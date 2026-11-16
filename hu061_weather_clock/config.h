// Persistent settings, stored in the EEPROM-emulation flash sector.
#pragma once
#include <Arduino.h>

struct Settings {
  char city[48];        // what the user typed, e.g. "Jakarta" or "Paris, FR"
  char place[40];       // resolved display name, e.g. "Jakarta"
  float lat;
  float lon;
  bool hasLocation;
  char tz[48];          // "auto", "+7", "-5:30", or a POSIX TZ string
  char activeTz[48];    // TZ string currently applied (survives reboots)
  bool fahrenheit;
  bool clock12h;
  uint16_t refreshMin;  // 10..180
  char password[24];    // optional HTTP password for the settings page ("" = none)
};

extern Settings cfg;

void loadSettings();   // loads or initialises defaults
void saveSettings();

// "+7" / "-5:30" / "UTC+2" -> POSIX TZ for a fixed offset; returns "" if not an offset.
String offsetToPosix(const String &s);
// Seconds east of UTC -> POSIX TZ ("<+0700>-7:00").
String secondsToPosix(long offsetSeconds);
// Applies a TZ string to the C library (localtime etc.).
void applyTimezone(const char *posixTz);
