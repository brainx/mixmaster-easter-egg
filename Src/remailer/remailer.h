/* Mixmaster 3.0 — remailer server architecture (NEW-2026)
   See MODERNIZATION.md and ARCHITECTURE.md
   $Id: remailer.h $ */

#ifndef _REMAILER_H
#define _REMAILER_H

#include "../mix3.h"

/* Message kinds detected from incoming mail */
typedef enum {
  REM_KIND_NONE = 0,
  REM_KIND_HELP,
  REM_KIND_STATS,
  REM_KIND_KEY,
  REM_KIND_OPKEY,
  REM_KIND_CONF,
  REM_KIND_OTHER,
  REM_KIND_TYPE1,
  REM_KIND_TYPE2,
  REM_KIND_BLOCK,
  REM_KIND_DISABLED,
  REM_KIND_PLAIN
} remailer_kind_t;

typedef struct remailer_envelope {
  remailer_kind_t kind;
  BUFFER *to;
  BUFFER *replyto;
  BUFFER *subject;
  BUFFER *otherrequest;
  int quoted_printable;
} remailer_envelope_t;

/* Pipeline: classify → dispatch → pool/outbound */
int remailer_classify(BUFFER *message, remailer_envelope_t *env);
int remailer_dispatch(BUFFER *message, remailer_envelope_t *env);
int remailer_process_message(BUFFER *message);

/* Admin / abuse */
int remailer_get_otherrequests_reply(BUFFER *reply, BUFFER *filename);
int remailer_blockrequest(BUFFER *message);

/* Server lifecycle */
int remailer_server_init(const char *mixdir);
int remailer_server_tick(int force);
int remailer_server_ingest(void);
int remailer_server_flush_pool(void);
int remailer_server_daemon(int nodetach);
void remailer_server_shutdown(void);

#endif /* _REMAILER_H */
