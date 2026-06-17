/* Mixmaster version 3.0  --  (C) 1999 - 2006 Anonymizer Inc. and others.

   Mixmaster may be redistributed and modified under certain conditions.
   This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
   ANY KIND, either express or implied. See the file COPYRIGHT for
   details.

   Remailer pool, logging, and Type II packet helpers.
   Message dispatch lives in remailer/remailer_dispatch.c.
   MODERNIZED-2026  see MODERNIZATION.md
   $Id: rem.c 934 2006-06-24 13:40:39Z rabbi $ */


#include "mix3.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef POSIX
#include <unistd.h>
#else /* end of POSIX */
#include <io.h>
#endif /* else if not POSIX */
#ifndef _MSC
#include <dirent.h>
#endif /* not _MSC */
#include <assert.h>

int create_dummy_mailin(void);

int create_dummy_mailin()
{
  while (rnd_number(100) < INDUMMYP) {
    errlog(DEBUGINFO, "Generating dummy message with incoming mail.\n");
    if (mix_encrypt(MSG_NULL, NULL, NULL, 1, NULL) == -1)
      return -1;
  }
  return 0;
}

int t2_decrypt(BUFFER *in)
{
  int err = 0;
  BUFFER *msg;

  msg = buf_new();
  do {
    err = mix_dearmor(in, msg);
    if (err != -1) {
      err = mix2_decrypt(msg);
    }
  }
  while (in->ptr + 1000 < in->length);	/* accept several packets in one message */

  buf_free(msg);
  return (err);
}

int mix_pool(BUFFER *msg, int type, long latent)
{
  char path[PATHMAX], pathtmp[PATHMAX];
  FILE *f;
  int err = -1;

  f = pool_new(latent > 0 ? "lat" : "msg", pathtmp, path);
  if (f != NULL) {
    if (latent > 0)
      fprintf(f, "%d %ld\n", type, latent + time(NULL));
    else
      fprintf(f, "%d 0\n", type);
    err = buf_write_sync(msg, f);
  }
  if (err == 0) {
    rename(pathtmp, path);
    errlog(DEBUGINFO, "Added message to pool.\n");
  }
  return (err);
}

int pool_packetfile(char *fname, BUFFER *mid, int packetnum)
     /* create a filename */
{
#ifdef SHORTNAMES
  sprintf(fname, "%s%cp%02x%02x%02x%01x.%02x", POOLDIR, DIRSEP,
	  mid->data[0], mid->data[1], mid->data[2], mid->data[3] & 15,
	  packetnum);
#else /* end of SHORTNAMES */
  sprintf(fname, "%s%cp%02x%02x%02x%02x%02x%02x%01x", POOLDIR, DIRSEP,
	  packetnum, mid->data[0], mid->data[1], mid->data[2], mid->data[3],
	  mid->data[4], mid->data[5] & 15);
#endif /* else if not SHORTNAMES */
  return (0);
}

void pool_packetexp(void)
{
  char *path;
  DIR *d;
  struct dirent *e;
  struct stat sb;

  d = opendir(POOLDIR);
  errlog(DEBUGINFO, "Checking for old parts.\n");
  if (d != NULL)
    for (;;) {
      e = readdir(d);
      if (e == NULL)
	break;
      if (e->d_name[0] == 'p' || e->d_name[0] == 'e' || e->d_name[0] == 't') {
	path=malloc(strlen(POOLDIR)+strlen(e->d_name)+strlen(DIRSEPSTR)+1);
	if (path) {
	 strcpy(path, POOLDIR);
	  strcat(path, DIRSEPSTR);
	  strcat(path, e->d_name);
	  if (stat(path, &sb) == 0 && time(NULL) - sb.st_mtime > PACKETEXP) {
	     if (e->d_name[0] == 'p') {
	        errlog(NOTICE, "Expiring incomplete partial message %s.\n",
	        e->d_name);
	     }
	     else if (e->d_name[0] == 'e') {
	        errlog(NOTICE, "Expiring old error message %s.\n",
	        e->d_name);
	     }
	     else if (e->d_name[0] == 't') {
	        errlog(NOTICE, "Expiring moldy temporary message %s.\n",
	        e->d_name);
	     }
	     unlink(path);
	  }
	free(path);
	}
      }
    }
  closedir(d);
}

void logmail(char *mailbox, BUFFER *message)
{
  time_t t;
  struct tm *tc;
  char line[LINELEN];

  /* mailbox is "|program", "user@host", "stdout", "Maildir/" or "filename" */
  buf_rewind(message);
  if (mailbox[0] == '\0')	/* default action */
    mailbox = MAILBOX;
  if (strieq(mailbox, "stdout"))
    buf_write(message, stdout);
  else if (mailbox[0] == '|') {
    FILE *p;

    errlog(DEBUGINFO, "Piping message to %s.\n", mailbox + 1);
    p = openpipe(mailbox + 1);
    if (p != NULL) {
      buf_write(message, p);
      closepipe(p);
    }
  } else if (strchr(mailbox, '@')) {
    BUFFER *field, *content;

    field = buf_new();
    content = buf_new();
    while (buf_getheader(message, field, content) == 0)
      if (bufieq(field, "x-loop") && bufifind(content, REMAILERADDR)) {
	errlog(WARNING, "Loop detected! Message not sent to %s.\n", mailbox);
	goto isloop;
      }
    buf_sets(content, mailbox);
    sendmail_loop(message, NULL, content);
  isloop:
    buf_free(field);
    buf_free(content);
  } else if (mailbox[strlen(mailbox)-1] == DIRSEP) {
    /* the user is requesting Maildir delivery */
    if(maildirWrite(mailbox, message, 1) != 0) {
      errlog(ERRORMSG, "Can't write to maildir %s\n", mailbox);
      return;
    }
  } else {
    FILE *mbox;

    mbox = mix_openfile(mailbox, "a");
    if (mbox == NULL) {
      errlog(ERRORMSG, "Can't write to mail folder %s\n", mailbox);
      return;
    }
    lock(mbox);
    if (!bufileft(message, "From ")) {
      t = time(NULL);
      tc = localtime(&t);
      strftime(line, LINELEN, "From Mixmaster %a %b %d %H:%M:%S %Y\n", tc);
      fprintf(mbox, "%s", line);
    }
    buf_write(message, mbox);
    fprintf(mbox, "\n\n");
    unlock(mbox);
    fclose(mbox);
  }
}

int idexp(void)
{
  FILE *f;
  long now, then;
  LOCK *i;
  idlog_t idbuf;
  long fpi = sizeof(idlog_t), fpo = sizeof(idlog_t);

  if (IDEXP == 0)
    return (0);

  f = mix_openfile(IDLOG, "rb+");
  if (f == NULL)
    return (-1);
  i = lockfile(IDLOG);
  now = time(NULL);
  if (fread(&idbuf, 1, sizeof(idlog_t), f) != sizeof(idlog_t)) { /* replace first line */
    fclose(f);
    unlockfile(i);
    return (-1);
  }
  then = idbuf.time;
  memset(idbuf.id,0,sizeof(idbuf.id));
  idbuf.time = now - IDEXP;
  fseek(f,0,SEEK_SET);
  fwrite(&idbuf,1,sizeof(idlog_t),f);
  fseek(f,fpi,SEEK_SET); /* this fseek does nothing, but MSVC CRT happilly reads past EOF (!!!) if we do not fseek here :-/ */
  while (fread(&idbuf, 1, sizeof(idlog_t), f) == sizeof(idlog_t)) {
    fpi+=sizeof(idlog_t);
    then = idbuf.time;
    if (now - then < IDEXP &&
      now - then > - SECONDSPERDAY * 180 )
      /* also expire packets that are dated more than half a year in the future.
       * That way we get rid of invalid packets introduced by the switch to a
       * binary id.log. */
    {
      fseek(f,fpo,SEEK_SET);
      fwrite(&idbuf,1,sizeof(idlog_t),f);
      fpo += sizeof(idlog_t);
      fseek(f,fpi,SEEK_SET);
    }
  }
#ifdef _MSC
    chsize(fileno(f),fpo);
#else /* end of _MSC */
    ftruncate(fileno(f),fpo);
#endif /* else if not _MSC */
  fclose(f);
  unlockfile(i);
  return (0);
}


int pgpmaxexp(void)
{
  FILE *f;
  BUFFER *b;
  long now, then;
  LOCK *i;
  char temp[LINELEN];

  f = mix_openfile(PGPMAXCOUNT, "rb+");
  if (f == NULL)
    return (-1);
  i = lockfile(PGPMAXCOUNT);
  b = buf_new();
  now = time(NULL);

  while (fgets(temp, sizeof(temp), f) != NULL)
    if (sscanf(temp, "%ld", &then) &&
	then >= now - SECONDSPERDAY)
      buf_appends(b, temp);

  fseek(f,0,SEEK_SET);

  buf_write(b, f);

#ifdef _MSC
    chsize(fileno(f),b->length);
#else /* end of _MSC */
    ftruncate(fileno(f),b->length);
#endif /* else if not _MSC */

  fclose(f);
  unlockfile(i);
  buf_free(b);
  return (0);
}
