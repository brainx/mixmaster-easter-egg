/* Mixmaster 3.0 — regression test for the stats() day window (NEW-2026)
   Links the real Src/stats.c; see FIXPLAN.md (F1).

   stats() summarises the last 80 days out of int msg[7][80] / int pool[2][80].
   The start index came from `havestats`, the oldest timestamp in stats.log or
   id.log, which is recorded outside the guard that bounds the array writes. The
   clamp meant to cap it at 79 was written as a ternary whose value was discarded:

       for ((i = (today - havestats) / SECONDSPERDAY) > 79 ? 79 : i; i >= 1; i--)

   so any record older than 80 days indexed both arrays past their ends and the
   out-of-bounds words were formatted into the reply mailed to the requester.

   This test writes a stats.log whose oldest record is 400 days old and asserts
   the summary stops at 79 daily rows. Run it under -fsanitize=address to also
   catch the out-of-bounds read directly.

   $Id: test-stats-window.c $ */

#include "../mix3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

static int failures = 0;

static void ok(int cond, const char *what)
{
  printf("%s: %s\n", cond ? "ok" : "FAIL", what);
  if (!cond)
    failures++;
}

/* count the "<dd> <Mon>: " rows in the per-day section of the reply */
static int count_daily_rows(BUFFER *b)
{
  BUFFER *line;
  int rows = 0, inday = 0;

  line = buf_new();
  buf_rewind(b);
  while (buf_getline(b, line) != -1) {
    if (bufifind(line, "messages per day")) {
      inday = 1;
      continue;
    }
    if (inday && line->length > 7 && line->data[2] == ' ' && line->data[6] == ':')
      rows++;
  }
  buf_free(line);
  return (rows);
}

int main(void)
{
  BUFFER *reply;
  FILE *f;
  long now, old;
  int rows;
  char dir[] = "/tmp/mixstatstestXXXXXX";
  char path[PATHMAX];

  rnd_initialized();

  if (mkdtemp(dir) == NULL) {
    printf("FAIL: could not create a temporary mixdir\n");
    return (1);
  }

  /* Point the library at a throwaway spool. mix_init() reads mix.cfg from here;
     an absent one just leaves the defaults, which is all this test needs. */
  setenv("MIXPATH", dir, 1);
  mix_init(dir);

  /* REMAIL must be on for stats_out(), and stats() reads STATS from the mixdir */
  REMAIL = 1;
  STATSDETAILS = 1;

  now = time(NULL);
  old = now - 400 * SECONDSPERDAY;

  snprintf(path, sizeof(path), "%s/%s", dir, STATS);
  f = fopen(path, "w");
  if (f == NULL) {
    printf("FAIL: could not write %s\n", path);
    return (1);
  }
  fprintf(f, "%ld\n", now);		/* last-updated line */
  fprintf(f, "2 5 %ld d\n", old);	/* the out-of-range record */
  fprintf(f, "2 3 %ld\n", now - 3600);	/* one recent record */
  fclose(f);

  reply = buf_new();
  ok(stats(reply) == 0, "stats() succeeds with a 400-day-old record");

  rows = count_daily_rows(reply);
  printf("     (per-day rows in reply: %d)\n", rows);
  ok(rows <= 79, "per-day section is clamped to at most 79 rows");
  ok(rows > 0, "per-day section is not empty");

  /* the reply should be a normal-sized summary, not hundreds of rows of stack */
  ok(reply->length < 8192, "reply stays a sane size");

  buf_free(reply);

  snprintf(path, sizeof(path), "%s/%s", dir, STATS);
  unlink(path);
  rmdir(dir);

  if (failures) {
    printf("\n%d check(s) failed.\n", failures);
    return (1);
  }
  printf("\nAll checks passed.\n");
  return (0);
}
