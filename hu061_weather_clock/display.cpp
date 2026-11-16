// Screens. Layout follows the stock HU-061 firmware (itself the ThingPulse
// WeatherStationDemo): 3 frames sliding every 5 s + a bottom bar.
#include <ESP8266WiFi.h>
#include <time.h>

#include "app.h"
#include "config.h"
#include "fonts.h"

SSD1306Wire display(0x3c, 0 /*SDA=GPIO0*/, 2 /*SCL=GPIO2*/);
OLEDDisplayUi ui(&display);

static const char *WDAY[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static unsigned long infoUntil = 0;

static String unitSuffix() { return cfg.fahrenheit ? "°F" : "°C"; }

void drawMessage(const String &l1, const String &l2, const String &l3, const String &l4) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, l1);
  display.drawString(0, 13, l2);
  display.drawString(0, 32, l3);
  display.drawString(0, 45, l4);
  display.display();
}

void drawProgress(int percent, const String &label) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, label);
  display.drawProgressBar(2, 28, 124, 10, percent);
  display.display();
}

void showInfoScreen(unsigned long ms) { infoUntil = millis() + ms; }
bool infoScreenActive() { return infoUntil && (long)(millis() - infoUntil) < 0; }

void drawInfoScreen() {
  String status = lastError.length() ? "Last update failed" : (weather.valid ? "Weather OK" : "No data yet");
  drawMessage("Settings page:", "http://" + WiFi.localIP().toString(), "weatherclock.local",
              status + "  v" FW_VERSION);
}

// Frame 1: date + time
static void drawDateTime(OLEDDisplay *d, OLEDDisplayUiState *, int16_t x, int16_t y) {
  time_t now = time(nullptr);
  struct tm *t = localtime(&now);
  char buf[40];
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_16);
  if (!timeSynced) {
    d->drawString(64 + x, 5 + y, "Syncing time...");
    return;
  }
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d  %s", t->tm_year + 1900, t->tm_mon + 1, t->tm_mday, WDAY[t->tm_wday]);
  d->drawString(64 + x, 5 + y, buf);
  d->setFont(ArialMT_Plain_24);
  if (cfg.clock12h) {
    int h = t->tm_hour % 12;
    snprintf(buf, sizeof(buf), "%d:%02d %s", h ? h : 12, t->tm_min, t->tm_hour < 12 ? "AM" : "PM");
  } else {
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t->tm_hour, t->tm_min, t->tm_sec);
  }
  d->drawString(64 + x, 22 + y, buf);
}

// Frame 2: current weather
static void drawCurrentWeather(OLEDDisplay *d, OLEDDisplayUiState *, int16_t x, int16_t y) {
  if (!weather.valid) {
    d->setFont(ArialMT_Plain_10);
    d->setTextAlignment(TEXT_ALIGN_CENTER);
    d->drawString(64 + x, 10 + y, cfg.hasLocation ? "Waiting for weather..." : "No location set.");
    d->drawString(64 + x, 24 + y, cfg.hasLocation ? "" : "Long-press the button");
    d->drawString(64 + x, 36 + y, cfg.hasLocation ? "" : "for the settings page");
    return;
  }
  d->setFont(Meteocons_Plain_36);
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->drawString(32 + x, y, wmoIcon(weather.code, weather.isDay));

  d->setTextAlignment(TEXT_ALIGN_LEFT);
  d->setFont(ArialMT_Plain_24);
  d->drawString(60 + x, 3 + y, String(weather.temp) + unitSuffix());
  d->setFont(ArialMT_Plain_10);
  d->drawString(62 + x, 26 + y, String(weather.feels) + unitSuffix() + " | " + String(weather.humidity) + "%");

  d->setTextAlignment(TEXT_ALIGN_CENTER);
  String wind = String(weather.wind) + (cfg.fahrenheit ? "mph" : "km/h");
  d->drawString(64 + x, 38 + y, String(wmoText(weather.code)) + " | " + wind);
}

// One column of frame 3
static void drawDay(OLEDDisplay *d, int x, int y, const DayForecast &day) {
  d->setTextAlignment(TEXT_ALIGN_CENTER);
  d->setFont(ArialMT_Plain_10);
  d->drawString(x + 20, y, day.date);
  d->setFont(Meteocons_Plain_21);
  d->drawString(x + 20, y + 12, wmoIcon(day.code, true));
  d->setFont(ArialMT_Plain_10);
  d->drawString(x + 20, y + 34, String(day.tMin) + "|" + String(day.tMax));
}

// Frame 3: 3-day forecast
static void drawForecast(OLEDDisplay *d, OLEDDisplayUiState *, int16_t x, int16_t y) {
  if (!weather.valid) return;
  for (int i = 0; i < 3; i++) drawDay(d, x + 44 * i, y, weather.days[i]);
}

// Bottom bar on every frame
static void drawOverlay(OLEDDisplay *d, OLEDDisplayUiState *state) {
  d->setColor(WHITE);
  d->setFont(ArialMT_Plain_10);
  d->setTextAlignment(TEXT_ALIGN_LEFT);
  if (state->currentFrame == 0) {
    if (weather.valid) d->drawString(6, 54, String(weather.temp) + unitSuffix());
  } else if (timeSynced) {
    time_t now = time(nullptr);
    struct tm *t = localtime(&now);
    char buf[12];
    if (cfg.clock12h) {
      int h = t->tm_hour % 12;
      snprintf(buf, sizeof(buf), "%d:%02d%s", h ? h : 12, t->tm_min, t->tm_hour < 12 ? "a" : "p");
    } else {
      snprintf(buf, sizeof(buf), "%02d:%02d", t->tm_hour, t->tm_min);
    }
    d->drawString(6, 54, buf);
  }
  d->setTextAlignment(TEXT_ALIGN_RIGHT);
  String place = cfg.hasLocation ? String(cfg.place) : String("no location");
  if (lastError.length()) place = "! " + place;  // last update failed
  while (place.length() > 3 && d->getStringWidth(place) > 66) place.remove(place.length() - 1);
  d->drawString(128, 54, place);
  d->drawHorizontalLine(0, 52, 128);
}

static FrameCallback frames[] = {drawDateTime, drawCurrentWeather, drawForecast};
static OverlayCallback overlays[] = {drawOverlay};

void setupUi() {
  ui.setTargetFPS(30);
  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setFrameAnimation(SLIDE_LEFT);
  ui.setFrames(frames, 3);
  ui.setTimePerFrame(5000);
  ui.setOverlays(overlays, 1);
  ui.init();
}
