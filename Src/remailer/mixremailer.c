/* Mixmaster 3.0 — dedicated remailer server binary (NEW-2026)
   See MODERNIZATION.md
   $Id: mixremailer.c $ */

#include "../mix3.h"
#include "remailer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void ensure_passphrase(void)
{
  BUFFER *pass;

  if (PASSPHRASE[0] != '\0' || !isatty(fileno(stdin)))
    return;
  pass = buf_new();
  user_pass(pass);
  strncpy(PASSPHRASE, (char *)pass->data, LINELEN);
  PASSPHRASE[LINELEN - 1] = '\0';
  strncpy(ENTEREDPASSPHRASE, PASSPHRASE, LINELEN);
  ENTEREDPASSPHRASE[LINELEN - 1] = '\0';
  buf_free(pass);
}

static void usage(const char *prog)
{
  fprintf(stderr,
          "Mixmaster %s — remailer server\n\n"
          "Usage: %s [options]\n\n"
          "  -R, --read-mail       Process one message from stdin\n"
          "  -I, --store-mail      Store stdin message in mailin pool\n"
          "  -S, --send            Force outbound pool flush\n"
          "  -M, --maintain        Run scheduled maintenance (ingest + pool)\n"
          "  -D, --daemon          Run as background daemon\n"
          "      --no-detach       Daemon mode, keep terminal attached\n"
          "  -G, --generate-key    Generate remailer keys\n"
          "  -K, --update-keys     Update remailer keys\n"
          "  -h, --help            Show this help\n"
          "  -V, --version         Show version\n",
          VERSION, prog);
}

int main(int argc, char *argv[])
{
  int readmail = 0, storemail = 0, sendpool = 0, maintain = 0;
  int daemon = 0, nodetach = 0, keygen = 0;
  int help = 0, version = 0;
  BUFFER *msg;
  int ret = 0;
  char *p;

  msg = buf_new();

  for (int i = 1; i < argc; i++) {
    p = argv[i];
    if (p[0] == '-' && p[1] == '-') {
      p += 2;
      if (streq(p, "help"))
        help = 1;
      else if (streq(p, "version"))
        version = 1;
      else if (streq(p, "read-mail"))
        readmail = 1;
      else if (streq(p, "store-mail"))
        storemail = 1;
      else if (streq(p, "send"))
        sendpool = 1;
      else if (streq(p, "maintain"))
        maintain = 1;
      else if (streq(p, "daemon"))
        daemon = 1;
      else if (streq(p, "no-detach"))
        nodetach = 1;
      else if (streq(p, "generate-key"))
        keygen = 2;
      else if (streq(p, "update-keys"))
        keygen = 1;
      else {
        fprintf(stderr, "%s: unknown option --%s\n", argv[0], p);
        return 1;
      }
    } else if (p[0] == '-' && p[1]) {
      while (*++p) {
        switch (*p) {
        case 'R': readmail = 1; break;
        case 'I': storemail = 1; break;
        case 'S': sendpool = 1; break;
        case 'M': maintain = 1; break;
        case 'D': daemon = 1; break;
        case 'G': keygen = 2; break;
        case 'K': keygen = 1; break;
        case 'h': help = 1; break;
        case 'V': version = 1; break;
        default:
          fprintf(stderr, "%s: unknown option -%c\n", argv[0], *p);
          return 1;
        }
      }
    }
  }

  if (help) {
    usage(argv[0]);
    ret = 0;
    goto done;
  }
  if (version) {
    printf("Mixmaster %s (remailer server)\n", VERSION);
    ret = 0;
    goto done;
  }

  if (!readmail && !storemail && !sendpool && !maintain && !daemon && !keygen) {
    usage(argv[0]);
    ret = argc == 1 ? 0 : 1;
    goto done;
  }

  if (remailer_server_init(NULL) != 0) {
    fprintf(stderr, "Hint: run ./scripts/setup-mixdir.sh and set REMAIL y in Mix/mix.cfg\n");
    ret = 1;
    goto done;
  }

  if (keygen) {
    ensure_passphrase();
    keymgt(keygen);
  }

  if (readmail || storemail) {
    ensure_passphrase();
    if (buf_read(msg, stdin) == -1) {
      fprintf(stderr, "Can't read message from stdin.\n");
      ret = 1;
    } else if (readmail)
      ret = remailer_process_message(msg) == 0 ? 0 : 1;
    else
      pool_add(msg, "inf");
  }

  if (sendpool)
    remailer_server_flush_pool();

  if (maintain)
    remailer_server_tick(0);

  if (daemon) {
    ensure_passphrase();
    remailer_server_daemon(nodetach);
  }

done:
  remailer_server_shutdown();
  buf_free(msg);
  return ret;
}
