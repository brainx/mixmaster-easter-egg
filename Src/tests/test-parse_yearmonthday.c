/* Mixmaster 3.0 — regression test for parse_yearmonthday() (NEW-2026)
   Links the real Src/util.c rather than carrying its own copy of the function,
   so this actually exercises production code.

   $Id: test-parse_yearmonthday.c $ */

#include "../mix3.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int failures = 0;

static void expect(const char *input, time_t want)
{
  time_t got = parse_yearmonthday((char *) input);

  if (got == want)
    printf("ok: %-14s -> %ld\n", input, (long) got);
  else {
    printf("FAIL: %-12s -> %ld, wanted %ld\n", input, (long) got, (long) want);
    failures++;
  }
}

int main(void)
{
  const char *tz_before;
  char saved[256];

  /* The function temporarily forces TZ=GMT and is supposed to put the original
     back. Record it so we can check that it does -- the !HAVE_SETENV path used
     to restore TZ from a stack buffer that putenv() kept a pointer into. */
  tz_before = getenv("TZ");
  saved[0] = '\0';
  if (tz_before) {
    strncpy(saved, tz_before, sizeof(saved) - 1);
    saved[sizeof(saved) - 1] = '\0';
  }

  /* Values are UTC midnight; these two also guard main.c's startup assert. */
  expect("2003-04-01", 1049155200);
  expect("2003-04-02", 1049241600);
  expect("1970-01-01", 0);
  expect("2026-07-26", 1785024000);

  /* leap day must resolve, and the day after it must be 86400 later */
  expect("2024-02-29", 1709164800);
  expect("2024-03-01", 1709251200);

  /* unparseable input yields -1 rather than a garbage date */
  expect("not-a-date", -1);
  expect("", -1);

  {
    const char *tz_after = getenv("TZ");

    if (tz_before == NULL && tz_after == NULL)
      printf("ok: TZ still unset after parsing\n");
    else if (tz_before != NULL && tz_after != NULL && strcmp(saved, tz_after) == 0)
      printf("ok: TZ restored to %s\n", tz_after);
    else {
      printf("FAIL: TZ not restored (was %s, now %s)\n",
	     tz_before ? saved : "unset", tz_after ? tz_after : "unset");
      failures++;
    }
  }

  if (failures) {
    printf("\n%d check(s) failed.\n", failures);
    return (1);
  }
  printf("\nAll checks passed.\n");
  return (0);
}
