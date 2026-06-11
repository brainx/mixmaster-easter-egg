/* Mixmaster 3.0 — remailer dispatch pipeline (NEW-2026)
   See MODERNIZATION.md
   $Id: remailer_dispatch.c $ */

#include "remailer.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

extern int create_dummy_mailin(void);
extern void logmail(char *mailbox, BUFFER *message);

int remailer_classify(BUFFER *message, remailer_envelope_t *env)
{
  int err;
  BUFFER *field, *content, *block;

  field = buf_new();
  content = buf_new();
  block = buf_new();

  env->kind = REM_KIND_PLAIN;
  env->quoted_printable = 0;
  buf_clear(env->to);
  buf_clear(env->replyto);
  buf_sets(env->subject, "Subject: Re: your mail");
  buf_clear(env->otherrequest);

  buf_rewind(message);

  {
    FILE *f = mix_openfile(SOURCEBLOCK, "r");
    if (f != NULL) {
      buf_read(block, f);
      fclose(f);
    }
  }

  for (;;) {
    err = buf_getheader(message, field, content);
    if (err == 1) {
      while (buf_lookahead(message, field) == 1)
        buf_getheader(message, field, content);
      if (isline(field, HDRMARK))
        continue;
      else
        goto hdrend;
    }
    if (err == -1)
      goto hdrend;

    if ((bufieq(field, "from") || bufieq(field, "sender") || bufieq(field, "received")) &&
        doblock(content, block, 1) != 0) {
      env->kind = REM_KIND_NONE;
      goto end;
    }

    if (bufieq(field, "to"))
      buf_cat(env->to, content);
    else if (bufieq(field, "from") && env->replyto->length == 0)
      buf_set(env->replyto, content);
    else if (bufieq(field, "reply-to"))
      buf_set(env->replyto, content);
    else if (MIX && bufieq(field, "remailer-type") && bufileft(content, "mixmaster"))
      env->kind = REM_KIND_TYPE2;
    else if (bufieq(field, "subject")) {
      if (bufieq(content, "help") || bufieq(content, "remailer-help"))
        env->kind = REM_KIND_HELP;
      else if (bufieq(content, "remailer-stats"))
        env->kind = REM_KIND_STATS;
      else if (bufieq(content, "remailer-key"))
        env->kind = REM_KIND_KEY;
      else if (bufieq(content, "remailer-adminkey"))
        env->kind = REM_KIND_OPKEY;
      else if (bufieq(content, "remailer-conf"))
        env->kind = REM_KIND_CONF;
      else if (bufileft(content, "remailer-")) {
        env->kind = REM_KIND_OTHER;
        buf_set(env->otherrequest, content);
      } else if (bufileft(content, "destination-block"))
        env->kind = REM_KIND_BLOCK;
      else {
        buf_sets(env->subject, "Subject: ");
        if (!bufileft(content, "re:"))
          buf_appends(env->subject, "Re: ");
        buf_cat(env->subject, content);
      }
    } else if (bufieq(field, "test-to") || bufieq(field, "encrypted") ||
               bufieq(field, "anon-to") || bufieq(field, "request-remailing-to") ||
               bufieq(field, "remail-to") || bufieq(field, "anon-post-to") ||
               bufieq(field, "post-to") || bufieq(field, "anon-send-to") ||
               bufieq(field, "send-to") || bufieq(field, "remix-to") ||
               bufieq(field, "encrypt-to"))
      env->kind = REM_KIND_TYPE1;
    else if (bufieq(field, "content-transfer-encoding") && bufieq(content, "quoted-printable"))
      env->quoted_printable = 1;
  }

hdrend:
  if (env->quoted_printable)
    qp_decode_message(message);

  if (env->kind > REM_KIND_NONE && REMAIL == 0)
    env->kind = REM_KIND_DISABLED;

end:
  buf_free(field);
  buf_free(content);
  buf_free(block);
  return (env->kind == REM_KIND_NONE ? -1 : 0);
}

int remailer_dispatch(BUFFER *message, remailer_envelope_t *env)
{
  BUFFER *reply;
  int err = 0;

  reply = buf_new();

  switch (env->kind) {
  case REM_KIND_HELP:
    if (sendinfofile(HELPFILE, NULL, env->replyto, NULL) == -1)
      errlog(WARNING, "No help file available.\n");
    break;
  case REM_KIND_KEY:
    err = key(reply);
    if (err == 0)
      err = sendmail(reply, REMAILERNAME, env->replyto);
    break;
  case REM_KIND_OPKEY:
    err = adminkey(reply);
    if (err == 0)
      err = sendmail(reply, REMAILERNAME, env->replyto);
    break;
  case REM_KIND_STATS:
    err = stats(reply);
    if (err == 0)
      err = sendmail(reply, REMAILERNAME, env->replyto);
    break;
  case REM_KIND_CONF:
    err = conf(reply);
    if (err == 0)
      err = sendmail(reply, REMAILERNAME, env->replyto);
    break;
  case REM_KIND_OTHER:
    err = remailer_get_otherrequests_reply(reply, env->otherrequest);
    if (err == 0)
      err = sendmail(reply, REMAILERNAME, env->replyto);
    break;
  case REM_KIND_TYPE1:
    err = t1_decrypt(message);
    if (err != 0) {
      errlog(LOG, "Invalid type 1 message from %b\n", env->replyto);
      sendinfofile(USAGEFILE, USAGELOG, env->replyto, NULL);
      logmail(err == -2 ? MAILUSAGE : MAILERROR, message);
    } else
      create_dummy_mailin();
    break;
  case REM_KIND_TYPE2:
    err = t2_decrypt(message);
    if (err == -1) {
      errlog(LOG, "Invalid type 2 message from %b\n", env->replyto);
      sendinfofile(USAGEFILE, USAGELOG, env->replyto, NULL);
      logmail(MAILERROR, message);
    } else
      create_dummy_mailin();
    break;
  case REM_KIND_BLOCK:
    remailer_blockrequest(message);
    logmail(MAILBLOCK, message);
    break;
  case REM_KIND_DISABLED:
    errlog(ERRORMSG, "Remailer is disabled.\n");
    buf_sets(reply, "Subject: remailer error\n\nThe remailer is disabled.\n");
    sendmail(reply, REMAILERNAME, env->replyto);
    logmail(MAILERROR, message);
    break;
  default:
    if (strifind(env->replyto->data, "mailer-daemon")) {
      errlog(LOG, "Bounce mail from %b\n", env->replyto);
      logmail(MAILBOUNCE, message);
    } else if (bufifind(env->to, REMAILERADDR) && remailer_blockrequest(message)) {
      logmail(MAILBLOCK, message);
    } else if (bufifind(env->to, REMAILERADDR)) {
      errlog(LOG, "Non-remailer message from %b\n", env->replyto);
      if (AUTOREPLY)
        sendinfofile(USAGEFILE, USAGELOG, env->replyto, NULL);
      logmail(MAILUSAGE, message);
    } else if (bufifind(env->to, COMPLAINTS)) {
      errlog(WARNING, "Abuse complaint from %b\n", env->replyto);
      if (AUTOREPLY)
        sendinfofile(ABUSEFILE, NULL, env->replyto, env->subject);
      logmail(MAILABUSE, message);
    } else if (ANONADDR[0] && bufifind(env->to, ANONADDR)) {
      errlog(LOG, "Reply to anonymous message from %b\n", env->replyto);
      if (AUTOREPLY)
        sendinfofile(REPLYFILE, NULL, env->replyto, env->subject);
      logmail(MAILANON, message);
    } else {
      errlog(DEBUGINFO, "Mail from %b\n", env->replyto);
      logmail(MAILBOX, message);
    }
    err = 1;
  }

  buf_free(reply);
  return (err);
}

int remailer_process_message(BUFFER *message)
{
  remailer_envelope_t env;
  int err;

  mix_init(NULL);

  env.to = buf_new();
  env.replyto = buf_new();
  env.subject = buf_new();
  env.otherrequest = buf_new();

  if (remailer_classify(message, &env) == -1) {
    err = -1;
    goto end;
  }
  err = remailer_dispatch(message, &env);

end:
  buf_free(env.to);
  buf_free(env.replyto);
  buf_free(env.subject);
  buf_free(env.otherrequest);
  return (err);
}

/* Legacy entry point used by pool.c, main.c, remailer.c */
int mix_decrypt(BUFFER *message)
{
  return remailer_process_message(message);
}
