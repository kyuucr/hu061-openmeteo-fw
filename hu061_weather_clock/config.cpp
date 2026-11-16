#include "config.h"

#include <EEPROM.h>
#include <time.h>

Settings cfg;

// Layout: [magic u32][version u16][Settings][checksum u32]. A different magic (e.g. the
// stock firmware's data, which is still in this sector) simply means "use defaults".
static const uint32_t MAGIC = 0x48553631;  // "HU61"
static const uint16_t VERSION = 1;
static const int EEPROM_SIZE = 512;
static_assert(sizeof(Settings) + 10 <= EEPROM_SIZE, "Settings too large");

static uint32_t checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;  // FNV-1a
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}

static void setDefaults() {
  memset(&cfg, 0, sizeof(cfg));
  strcpy(cfg.tz, "auto");
  strcpy(cfg.activeTz, "UTC0");
  cfg.refreshMin = 30;
}

void loadSettings() {
  EEPROM.begin(EEPROM_SIZE);
  uint32_t magic, sum;
  uint16_t version;
  EEPROM.get(0, magic);
  EEPROM.get(4, version);
  EEPROM.get(6, cfg);
  EEPROM.get(6 + sizeof(Settings), sum);
  if (magic != MAGIC || version != VERSION ||
      sum != checksum(reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg))) {
    setDefaults();
    return;
  }
  // Defensive: make sure every string is terminated and values are sane.
  cfg.city[sizeof(cfg.city) - 1] = cfg.place[sizeof(cfg.place) - 1] = 0;
  cfg.tz[sizeof(cfg.tz) - 1] = cfg.activeTz[sizeof(cfg.activeTz) - 1] = 0;
  cfg.password[sizeof(cfg.password) - 1] = 0;
  if (cfg.refreshMin < 10 || cfg.refreshMin > 180) cfg.refreshMin = 30;
}

void saveSettings() {
  EEPROM.put(0, MAGIC);
  EEPROM.put(4, VERSION);
  EEPROM.put(6, cfg);
  EEPROM.put(6 + sizeof(Settings), checksum(reinterpret_cast<const uint8_t *>(&cfg), sizeof(cfg)));
  EEPROM.commit();  // one flash write for the whole block
}

String secondsToPosix(long off) {
  // POSIX TZ signs are inverted: UTC+7 is written "<+07>-7".
  long a = labs(off);
  int h = a / 3600, m = (a % 3600) / 60;
  char buf[24];
  snprintf(buf, sizeof(buf), "<%c%02d%02d>%c%d:%02d", off >= 0 ? '+' : '-', h, m,
           off >= 0 ? '-' : '+', h, m);
  return buf;
}

String offsetToPosix(const String &in) {
  String s = in;
  s.trim();
  if (s.startsWith("UTC") || s.startsWith("GMT")) s = s.substring(3);
  if (s.length() == 0 || s.length() > 6) return "";
  int sign = 1, i = 0;
  if (s[0] == '+' || s[0] == '-') {
    sign = s[0] == '-' ? -1 : 1;
    i = 1;
  }
  int h = 0, m = 0, digits = 0;
  for (; i < (int)s.length() && isdigit(s[i]); i++, digits++) h = h * 10 + (s[i] - '0');
  if (digits == 0 || digits > 2) return "";
  if (i < (int)s.length()) {
    if (s[i] != ':' || s.length() - i != 3 || !isdigit(s[i + 1]) || !isdigit(s[i + 2])) return "";
    m = (s[i + 1] - '0') * 10 + (s[i + 2] - '0');
  }
  if (h > 14 || m > 59) return "";
  return secondsToPosix(sign * (h * 3600L + m * 60L));
}

void applyTimezone(const char *posixTz) {
  setenv("TZ", posixTz, 1);
  tzset();
}
