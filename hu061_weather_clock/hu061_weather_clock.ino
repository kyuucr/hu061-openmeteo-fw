// HU-061 weather clock -- Open-Meteo firmware (Arduino sketch: setup/loop and app logic;
// the other modules are the .cpp/.h files in this folder).
//
// Replaces the stock QWeather firmware (no account, phone number or API key needed).
// Same screens as the original; location, time zone, units and refresh interval are
// set on a web page served by the clock. Hardware: ESP-01S + 0.96" SSD1306
// (SDA=GPIO0, SCL=GPIO2), button on GPIO3 (RX).
//
// Button: short press = update weather now, hold 2 s = show the settings address,
//         hold while powering on = Wi-Fi setup hotspot "HU061-Setup".

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <time.h>

#include "app.h"
#include "config.h"

static const int BUTTON_PIN = 3;           // RX pin; Serial is TX-only
static const char *SETUP_AP = "HU061-Setup";
static const char *HOSTNAME = "weatherclock";

Weather weather;
bool updateRequested = true;               // fetch as soon as we're online
String lastError;
unsigned long lastAttempt = 0;
bool timeSynced = false;

// ---------------------------------------------------------------- time zone
// "auto": follow the offset Open-Meteo reports for the location (includes daylight
// saving; picked up at the next update after a change). Otherwise the user's offset
// or POSIX rule. The applied string is saved so the clock is right after a reboot
// even before the first weather update.
void applyLocationTimezone() {
  String tz;
  if (strcmp(cfg.tz, "auto") == 0) {
    if (weather.valid) tz = secondsToPosix(weather.utcOffset);
    else tz = cfg.activeTz;
  } else {
    tz = offsetToPosix(cfg.tz);
    if (tz.length() == 0) tz = cfg.tz;  // full POSIX rule
  }
  applyTimezone(tz.c_str());
  if (tz != cfg.activeTz) {
    strlcpy(cfg.activeTz, tz.c_str(), sizeof(cfg.activeTz));
    saveSettings();
  }
}

// ---------------------------------------------------------------- weather
static void updateWeather(bool showProgress) {
  lastAttempt = millis();
  if (!cfg.hasLocation) {
    lastError = "";  // not an error: the screens ask the user to set a location
    return;
  }
  if (showProgress) drawProgress(50, "Updating weather...");
  String err;
  bool ok = false;
  for (int attempt = 0; attempt < 3 && !ok; attempt++) {
    ok = fetchWeather(cfg.lat, cfg.lon, cfg.fahrenheit, weather, err);
    if (!ok) {
      Serial.printf("weather update failed (%s), try %d\n", err.c_str(), attempt + 1);
      delay(500);
    }
  }
  lastError = ok ? "" : err;
  if (ok) {
    Serial.printf("weather: %d, code %d, %s (offset %ld)\n", weather.temp, weather.code,
                  weather.timezone.c_str(), weather.utcOffset);
    applyLocationTimezone();
  }
  if (showProgress) drawProgress(100, ok ? "Done" : "Update failed");
}

// ---------------------------------------------------------------- Wi-Fi
static void connectWiFi(bool forcePortal) {
  WiFiManager wm;
  wm.setDebugOutput(false);
  wm.setTitle("Weather clock");
  wm.setHostname(HOSTNAME);
  wm.setShowInfoUpdate(false);
  const char *menu[] = {"wifi", "exit"};
  wm.setMenu(menu, 2);
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(300);  // give up after 5 min, then retry with saved Wi-Fi
  wm.setAPCallback([](WiFiManager *) {
    drawMessage("Wi-Fi setup", "Join hotspot:", SETUP_AP, "then open 192.168.4.1");
  });

  bool ok = forcePortal ? wm.startConfigPortal(SETUP_AP) : wm.autoConnect(SETUP_AP);
  if (!ok) {
    drawMessage("No Wi-Fi.", "Restarting...");
    delay(3000);
    ESP.restart();
  }
}

// ---------------------------------------------------------------- button
static void handleButton() {
  static bool down = false, longDone = false;
  static unsigned long since = 0;
  bool pressed = digitalRead(BUTTON_PIN) == LOW;
  if (pressed && !down) {
    down = true;
    longDone = false;
    since = millis();
  } else if (pressed && down && !longDone && millis() - since > 2000) {
    longDone = true;
    showInfoScreen(15000);  // hold 2 s: settings address
  } else if (!pressed && down) {
    down = false;
    if (!longDone && millis() - since > 30) {  // short press (debounced)
      if (infoScreenActive()) showInfoScreen(0);
      else updateRequested = true;
    }
  }
}

// ---------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200, SERIAL_8N1, SERIAL_TX_ONLY);  // RX pin is the button
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.println("\nHU-061 Open-Meteo firmware v" FW_VERSION);

  display.init();
  display.setContrast(120, 241, 64);
  loadSettings();
  applyTimezone(cfg.activeTz);

  bool forcePortal = false;
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(50);
    forcePortal = digitalRead(BUTTON_PIN) == LOW;
  }
  drawMessage("Weather clock v" FW_VERSION, forcePortal ? "Starting Wi-Fi setup..." : "Connecting to Wi-Fi...");
  connectWiFi(forcePortal);

  MDNS.begin(HOSTNAME);
  MDNS.addService("http", "tcp", 80);
  setupWeb();

  configTime(cfg.activeTz, "pool.ntp.org", "time.google.com", "time.cloudflare.com");
  // Wait (up to 15 s) for the internet time, so we don't show 1970 like the stock firmware.
  unsigned long t0 = millis();
  while (time(nullptr) < 1700000000 && millis() - t0 < 15000) {
    drawMessage("Connected: " + WiFi.SSID(), "Syncing time...", "Settings page:", "http://" + WiFi.localIP().toString());
    delay(250);
  }
  timeSynced = time(nullptr) >= 1700000000;
  if (!cfg.hasLocation) {
    drawMessage("Set your location at:", "http://" + WiFi.localIP().toString(), "or weatherclock.local");
    delay(5000);
  }

  setupUi();
  updateWeather(true);
  updateRequested = false;
}

void loop() {
  handleWeb();
  MDNS.update();
  handleButton();
  if (!timeSynced && time(nullptr) >= 1700000000) timeSynced = true;

  // Normal interval, but retry after 5 min if the last update failed.
  unsigned long interval = lastError.length() ? 5 * 60000UL : cfg.refreshMin * 60000UL;
  if (millis() - lastAttempt > interval) updateRequested = true;
  // Only fetch while a frame is standing still so the slide animation doesn't stutter.
  if (updateRequested && ui.getUiState()->frameState == FIXED) {
    updateRequested = false;
    updateWeather(false);
  }

  if (infoScreenActive()) {
    drawInfoScreen();
    delay(50);
    return;
  }
  int remaining = ui.update();
  if (remaining > 0) delay(remaining > 20 ? 20 : remaining);  // stay responsive to web/button
}
