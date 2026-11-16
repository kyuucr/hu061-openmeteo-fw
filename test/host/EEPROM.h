#pragma once
#include <cstring>
struct EEPROMClass { unsigned char m[512]; void begin(int) {} void commit() {}
  template <class T> void get(int a, T &v) { memcpy(&v, m + a, sizeof v); }
  template <class T> void put(int a, const T &v) { memcpy(m + a, &v, sizeof v); } };
static EEPROMClass EEPROM;
