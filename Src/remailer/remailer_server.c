/* Mixmaster 3.0 — remailer server lifecycle (NEW-2026)
   See MODERNIZATION.md
   $Id: remailer_server.c $ */

#include "remailer.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int remailer_server_init(const char *mixdir)
{
  mix_init(mixdir);
  if (!REMAIL) {
    errlog(ERRORMSG, "Remailer disabled (set REMAIL y in mix.cfg).\n");
    return -1;
  }
  mix_check_timeskew();
  return 0;
}

int remailer_server_tick(int force)
{
  return mix_regular(force);
}

int remailer_server_ingest(void)
{
  return process_mailin();
}

int remailer_server_flush_pool(void)
{
  return mix_send();
}

int remailer_server_daemon(int nodetach)
{
#ifdef UNIX
  if (!nodetach) {
    int pid;

    fprintf(stderr, "Detaching.\n");
    pid = fork();
    if (pid > 0)
      exit(0);
    if (setsid() < 0) {
      fprintf(stderr, "setsid() failed.\n");
      exit(1);
    }
    pid = fork();
    if (pid > 0)
      exit(0);
  }
  if (chdir(MIXDIR) < 0) {
    if (chdir("/") < 0) {
      fprintf(stderr, "Cannot chdir to mixdir or /.\n");
      exit(1);
    }
  }
  if (write_pidfile(PIDFILE)) {
    fprintf(stderr, "Aborting.\n");
    exit(1);
  }
  if (!nodetach) {
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
  }
#endif /* UNIX */

  mix_daemon();

#ifdef UNIX
  clear_pidfile(PIDFILE);
#endif /* UNIX */
  return 0;
}

void remailer_server_shutdown(void)
{
  mix_exit();
}
