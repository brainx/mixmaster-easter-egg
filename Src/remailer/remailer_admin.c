/* Mixmaster 3.0 — remailer admin & abuse (NEW-2026)
   See MODERNIZATION.md
   $Id: remailer_admin.c $ */

#include "remailer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

int remailer_get_otherrequests_reply(BUFFER *reply, BUFFER *filename)
{
  FILE *f = NULL;
  int c;
  int err;
  BUFFER *path;

  path = buf_new();

  assert(filename);
  assert(reply);

  buf_rewind(filename);
  err = bufileft(filename, "remailer-");
  if (!err) {
    err = 1;
    goto end;
  }

  while ((c = buf_getc(filename)) != -1) {
    int ok = (c >= 'A' && c <= 'Z') ||
             (c >= 'a' && c <= 'z') ||
             (c >= '0' && c <= '9') ||
             c == '-';
    if (!ok) {
      err = 1;
      goto end;
    }
  }
  buf_rewind(filename);

  buf_appends(path, REQUESTDIR);
  buf_appends(path, "/");
  buf_cat(path, filename);

  f = mix_openfile(path->data, "r");
  if (f == NULL) {
    err = -1;
    goto end;
  }

  buf_read(reply, f);
  err = 0;
end:
  if (f)
    fclose(f);
  buf_free(path);
  return (err);
}

static int blockrequest_start(BUFFER *line)
/* If `line` starts (after optional leading whitespace) with "destination-block",
   leave line->ptr on the "block" token and return 1, else return 0.

   Anchoring is deliberate. The unanchored substring search this replaced matched
   the phrase anywhere in the body, so quoting or forwarding the remailer's own
   help text -- which documents the destination-block command -- was enough to
   trip AUTOBLOCK and block the sender's own address. It also walked line->ptr
   forward looking for "block" with no bound, relying on the outer search to
   guarantee a hit. */
{
  int i = 0;
  const int dlen = 12;		/* strlen("destination-") */

  while (i < line->length && (line->data[i] == ' ' || line->data[i] == '\t'))
    i++;
  if (!strileft((char *) line->data + i, "destination-"))
    return (0);
  if (!strileft((char *) line->data + i + dlen, "block"))
    return (0);
  line->ptr = i + dlen;
  return (1);
}

int remailer_blockrequest(BUFFER *message)
{
  int request = 0, num, i;
  BUFFER *from, *line, *field, *content, *addr, *remailer_addr, *copy_addr;
  BUFFER *scan;
  REMAILER remailer[MAXREM];
  FILE *f;
  char *destblklst = (char *)malloc(strlen(DESTBLOCK) + 1);
  char *destblk;

  from = buf_new();
  line = buf_new();
  field = buf_new();
  content = buf_new();
  addr = buf_new();
  remailer_addr = buf_new();
  copy_addr = buf_new();
  scan = buf_new();

  if (destblklst == NULL) {
    errlog(ERRORMSG, "Can't malloc %n bytes for destblklst.\n", strlen(DESTBLOCK) + 1);
    goto end;
  }

  /* A request may arrive in the Subject or in the body, so scan both. This used
     to append the subject onto `message` itself, which meant the copy handed to
     logmail() by the caller was archived with its own subject duplicated at the
     end. Build a scratch buffer instead and leave `message` untouched. */
  buf_rewind(message);
  while (buf_getheader(message, field, content) == 0)
    if (bufieq(field, "from"))
      buf_set(from, content);
    else if (bufieq(field, "subject")) {
      buf_cat(scan, content);
      buf_nl(scan);
    }
  buf_rest(scan, message);	/* message->ptr now sits at the body */
  buf_rewind(scan);

  while (buf_getline(scan, line) != -1)
    if (blockrequest_start(line)) {
      buf_clear(addr);
      request = 1;
      {
        int c = 0;

        while (c != ' ' && c != -1)
          c = tolower(buf_getc(line));
        while (c == ' ')
          c = buf_getc(line);
        if (c != -1)
          do {
            buf_appendc(addr, c);
            c = buf_getc(line);
          } while (c > ' ');
      }
      if (addr->length == 0) {
        rfc822_addr(from, addr);
        buf_chop(addr);
      }
      buf_set(copy_addr, addr);
      buf_sets(remailer_addr, REMAILERADDR);
      if (doblock(remailer_addr, copy_addr, 1)) {
        errlog(LOG, "Ignoring blocking request for %b from %b.\n", addr, from);
        request = 2;
        goto end;
      }
      num = mix2_rlist(remailer, NULL);
      for (i = 0; i < num; i++) {
        buf_sets(remailer_addr, remailer[i].addr);
        if (doblock(remailer_addr, copy_addr, 1)) {
          errlog(LOG, "Ignoring blocking request for %b from %b.\n", addr, from);
          request = 2;
          goto end;
        }
      }
      num = t1_rlist(remailer, NULL);
      for (i = 0; i < num; i++) {
        buf_sets(remailer_addr, remailer[i].addr);
        if (doblock(remailer_addr, copy_addr, 1)) {
          errlog(LOG, "Ignoring blocking request for %b from %b.\n", addr, from);
          request = 2;
          goto end;
        }
      }

      if (buf_ieq(addr, from))
        errlog(NOTICE, "Blocking request for %b\n", addr);
      else
        errlog(NOTICE, "Blocking request for %b from %b\n", addr, from);
      if (AUTOBLOCK) {
        buf_clear(line);
        rfc822_addr(addr, line);
        if (line->length == 0) {
          errlog(LOG, "Nothing to block after rfc822_addr().\n");
        } else if (bufleft(line, "/")) {
          errlog(LOG, "Ignoring blocking request: %b is a regex.\n", addr);
        } else {
          if (strchr(line->data, '@') && strchr(strchr(line->data, '@'), '.')) {
            strcpy(destblklst, DESTBLOCK);
            destblk = strtok(destblklst, " ");
            f = mix_openfile(destblk, "a");
            if (f != NULL) {
              lock(f);
              buf_chop(line);
              sendinfofile(BLOCKFILE, NULL, line, NULL);
              if (line->length)
                fprintf(f, "%s\n", line->data);
              else
                errlog(NOTICE, "%b already blocked.\n", addr);
              unlock(f);
              fclose(f);
            } else
              errlog(ERRORMSG, "Can't write to %s.\n", DESTBLOCK);
          } else
            errlog(WARNING, "Invalid address not added to %s: %b\n", DESTBLOCK, addr);
        }
      }
    }

end:
  free(destblklst);
  buf_free(from);
  buf_free(line);
  buf_free(field);
  buf_free(content);
  buf_free(addr);
  buf_free(remailer_addr);
  buf_free(copy_addr);
  buf_free(scan);

  return (request);
}
