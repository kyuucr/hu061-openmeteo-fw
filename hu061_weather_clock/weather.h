// Open-Meteo client: geocoding (city name -> coordinates + timezone) and
// current weather + 3-day forecast in a single request. No account or key needed.
#pragma once
#include <Arduino.h>

struct DayForecast {
  String date;      // "MM-DD"
  int code = -1;    // WMO weather code
  int tMin = 0, tMax = 0;
};

struct Weather {
  bool valid = false;
  int code = -1;          // WMO weather code
  bool isDay = true;
  int temp = 0, feels = 0, humidity = 0, wind = 0;
  long utcOffset = 0;     // seconds east of UTC at the location, incl. daylight saving
  String timezone;        // IANA name, e.g. "Asia/Jakarta"
  DayForecast days[3];
  unsigned long fetchedAt = 0;  // millis()
};

struct GeoResult {
  bool ok = false;
  String error;
  String name, country, timezone;
  float lat = 0, lon = 0;
};

// "Paris" or "Paris, FR" / "Paris, France" / "Springfield, Illinois".
GeoResult geocode(const String &query);

// Fetches into `out`. Returns false (and an error text) on failure; `out` is untouched then.
bool fetchWeather(float lat, float lon, bool fahrenheit, Weather &out, String &error);

// WMO code -> Meteocons font letter / short English text (fits the 128 px screen).
const char *wmoIcon(int code, bool day);
const char *wmoText(int code);
