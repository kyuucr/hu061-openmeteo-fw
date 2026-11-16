#include <cassert>
#include <ctime>
#include "../../hu061_weather_clock/config.h"
static int fails = 0;
static void expectOffset(const char *in, long secondsEast) {
  String p = offsetToPosix(in);
  if (secondsEast == LONG_MIN) {  // expect rejection
    if (p.length()) { printf("FAIL %-8s -> '%s' (expected reject)\n", in, p.c_str()); fails++; }
    else printf("ok   %-8s rejected\n", in);
    return;
  }
  applyTimezone(p.c_str());
  time_t t = 1763200000;  // fixed UTC instant
  struct tm lt = *localtime(&t), ut = *gmtime(&t);
  long diff = (lt.tm_hour - ut.tm_hour) * 3600L + (lt.tm_min - ut.tm_min) * 60L +
              (lt.tm_mday - ut.tm_mday) * 86400L;
  if (diff > 50400) diff -= 86400 * 30;  // month wrap not expected for this instant
  bool ok = diff == secondsEast;
  printf("%s %-8s -> %-16s local-UTC = %+ld s\n", ok ? "ok  " : "FAIL", in, p.c_str(), diff);
  if (!ok) fails++;
}
int main() {
  expectOffset("+7", 7 * 3600);
  expectOffset("7", 7 * 3600);
  expectOffset("-5", -5 * 3600);
  expectOffset("+5:30", 5 * 3600 + 1800);
  expectOffset("-3:30", -(3 * 3600 + 1800));
  expectOffset("UTC+2", 2 * 3600);
  expectOffset("GMT-8", -8 * 3600);
  expectOffset("+14", 14 * 3600);
  expectOffset("0", 0);
  expectOffset("+15", LONG_MIN);
  expectOffset("abc", LONG_MIN);
  expectOffset("+5:3", LONG_MIN);
  expectOffset("CET-1CEST,M3.5.0,M10.5.0/3", LONG_MIN);  // not an offset -> used as POSIX rule
  // secondsToPosix round trip incl. Open-Meteo style values
  for (long off : {25200L, -18000L, 19800L, 20700L, 0L, -34200L}) {
    String p = secondsToPosix(off);
    applyTimezone(p.c_str());
    time_t t = 1763200000; struct tm lt = *localtime(&t), ut = *gmtime(&t);
    long diff = (lt.tm_hour - ut.tm_hour) * 3600L + (lt.tm_min - ut.tm_min) * 60L + (lt.tm_mday - ut.tm_mday) * 86400L;
    printf("%s secondsToPosix(%ld) = %s\n", diff == off ? "ok  " : "FAIL", off, p.c_str());
    if (diff != off) fails++;
  }
  // a real DST rule passed through unchanged
  applyTimezone("CET-1CEST,M3.5.0,M10.5.0/3");
  time_t summer = 1751371200; struct tm s = *localtime(&summer);  // 2025-07-01 12:00 UTC
  printf("%s CET rule in July -> %02d:00 local (expect 14)\n", s.tm_hour == 14 ? "ok  " : "FAIL", s.tm_hour);
  if (s.tm_hour != 14) fails++;
  printf(fails ? "\n%d FAILED\n" : "\nall passed\n", fails);
  return fails != 0;
}
