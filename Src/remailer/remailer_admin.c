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

int remailer_blockrequest(BUFFER *message)
{
  int request = 0, num, i;
  BUFFER *from, *line, *field, *content, *addr, *remailer_addr, *copy_addr;
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

  if (destblklst == NULL) {
    errlog(ERRORMSG, "Can't malloc %n bytes for destblklst.\n", strlen(DESTBLOCK) + 1);
    goto end;
  }

  buf_rewind(message);
  while (buf_getheader(message, field, content) == 0)
    if (bufieq(field, "from"))
      buf_set(from, content);
    else if (bufieq(field, "subject"))
      buf_cat(message, content);

  while (buf_getline(message, line) != -1)
    if (bufifind(line, "destination-block")) {
      buf_clear(addr);
      request = 1;
      {
        int c = 0;

        while (!strileft(line->data + line->ptr, "block"))
          line->ptr++;
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

  return (request);
}
