// Settings page, served on the home network at http://<clock-ip>/ and http://weatherclock.local/
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>

#include "app.h"
#include "config.h"

static ESP8266WebServer server(80);
static ESP8266HTTPUpdateServer updater;
static String flashMsg;  // one-shot message shown at the top of the page
static bool flashIsError = false;

static String esc(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (char c : s) {
    switch (c) {
      case '&': o += F("&amp;"); break;
      case '<': o += F("&lt;"); break;
      case '>': o += F("&gt;"); break;
      case '"': o += F("&quot;"); break;
      case '\'': o += F("&#39;"); break;
      default: o += c;
    }
  }
  return o;
}

static bool authorised() {
  if (!cfg.password[0]) return true;
  if (server.authenticate("admin", cfg.password)) return true;
  server.requestAuthentication(BASIC_AUTH, "Weather clock", "Wrong password");
  return false;
}

static void redirectHome(const String &msg, bool isError = false) {
  flashMsg = msg;
  flashIsError = isError;
  server.sendHeader("Location", "/");
  server.send(303);
}

static const char PAGE_HEAD[] PROGMEM = R"(<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Weather clock</title><style>
body{font-family:system-ui,sans-serif;max-width:30em;margin:auto;padding:1em;background:#fafafa;color:#222}
@media(prefers-color-scheme:dark){body{background:#151515;color:#ddd}input,select{background:#222;color:#ddd}}
h1{font-size:1.3em}fieldset{border:1px solid #8884;border-radius:8px;margin:0 0 1em}
label{display:block;margin:.6em 0 .2em;font-weight:600}input,select{width:100%;padding:.5em;box-sizing:border-box;
border:1px solid #8886;border-radius:6px;font-size:1em}small{color:#888}.row{display:flex;gap:.5em}
.row>*{flex:1}button{padding:.7em 1em;border:0;border-radius:6px;background:#1f6feb;color:#fff;font-size:1em;
margin:.3em 0}button.s{background:#666}.msg{padding:.7em;border-radius:6px;background:#2a7a2a33}
.err{background:#c0303033}table{width:100%;border-collapse:collapse}td{padding:.2em 0}td:last-child{text-align:right}
</style></head><body><h1>Weather clock</h1>)";

static void handleRoot() {
  if (!authorised()) return;
  String h;
  h.reserve(4200);
  h += FPSTR(PAGE_HEAD);
  if (flashMsg.length()) {
    h += "<p class='msg";
    h += flashIsError ? " err'>" : "'>";
    h += esc(flashMsg) + "</p>";
    flashMsg = "";
  }

  // Status
  h += F("<fieldset><legend>Status</legend><table>");
  auto row = [&](const String &k, const String &v) { h += "<tr><td>" + k + "</td><td>" + esc(v) + "</td></tr>"; };
  row("Location", cfg.hasLocation ? String(cfg.place) + " (" + String(cfg.lat, 3) + ", " + String(cfg.lon, 3) + ")" : "not set");
  if (weather.valid) {
    row("Now", String(weather.temp) + (cfg.fahrenheit ? " °F, " : " °C, ") + wmoText(weather.code));
    row("Updated", String((millis() - weather.fetchedAt) / 60000) + " min ago");
    row("Time zone", weather.timezone + " (UTC" + (weather.utcOffset >= 0 ? "+" : "-") +
                         String(labs(weather.utcOffset) / 3600) +
                         (labs(weather.utcOffset) % 3600 ? ":" + String(labs(weather.utcOffset) % 3600 / 60) : "") + ")");
  }
  if (lastError.length()) row("Last error", lastError);
  time_t now = time(nullptr);
  char tbuf[32];
  strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M", localtime(&now));
  row("Clock", timeSynced ? String(tbuf) : String("not synced yet"));
  row("Wi-Fi", WiFi.SSID() + " (" + String(WiFi.RSSI()) + " dBm)");
  row("Firmware", "v" FW_VERSION ", free RAM " + String(ESP.getFreeHeap()) + " B");
  h += F("</table><form method=post action=/refresh><button>Update weather now</button></form></fieldset>");

  // Settings form
  h += F("<form method=post action=/save><fieldset><legend>Location</legend>"
         "<label>City</label><input name=city maxlength=47 placeholder='e.g. Jakarta or Paris, FR' value='");
  h += esc(cfg.city);
  h += F("'><small>Add a country or region after a comma if the wrong place is found.</small>"
         "<label>or coordinates</label><div class=row><input name=lat placeholder='latitude'"
         " inputmode=decimal><input name=lon placeholder='longitude' inputmode=decimal></div>"
         "<small>Leave empty to use the city name.</small></fieldset>");

  h += F("<fieldset><legend>Time</legend><label>Time zone</label><input name=tz maxlength=47 value='");
  h += esc(cfg.tz);
  h += F("'><small><b>auto</b> = from the location, incl. daylight saving (recommended). Or a fixed "
         "offset like <b>+7</b> / <b>-5:30</b>, or a POSIX rule like <b>CET-1CEST,M3.5.0,M10.5.0/3</b>.</small>"
         "<label>Clock</label><select name=clock><option value=24");
  h += cfg.clock12h ? "" : " selected";
  h += F(">24-hour</option><option value=12");
  h += cfg.clock12h ? " selected" : "";
  h += F(">12-hour (AM/PM)</option></select></fieldset>");

  h += F("<fieldset><legend>Weather</legend><label>Units</label><select name=units><option value=c");
  h += cfg.fahrenheit ? "" : " selected";
  h += F(">°C, km/h</option><option value=f");
  h += cfg.fahrenheit ? " selected" : "";
  h += F(">°F, mph</option></select><label>Update every (minutes)</label>"
         "<input name=refresh type=number min=10 max=180 value=");
  h += String(cfg.refreshMin);
  h += F("></fieldset><fieldset><legend>Security</legend><label>Settings password</label>"
         "<input name=pw type=password maxlength=23 placeholder='");
  h += cfg.password[0] ? F("(unchanged)") : F("(none)");
  h += F("'><small>Protects this page and firmware updates (user name: admin). "
         "Type a single <b>-</b> to remove it.</small></fieldset><button>Save</button></form>");

  h += F("<fieldset><legend>Device</legend>"
         "<form method=get action=/update><button class=s>Firmware update</button></form>"
         "<form method=post action=/restart><button class=s>Restart</button></form>"
         "<form method=post action=/wifi-reset onsubmit=\"return confirm('Forget Wi-Fi and start the "
         "setup hotspot?')\"><button class=s>Change Wi-Fi network</button></form></fieldset></body></html>");
  server.send(200, "text/html; charset=utf-8", h);
}

static void handleSave() {
  if (!authorised()) return;
  String msg;
  bool err = false;

  // Location: explicit coordinates win, otherwise geocode the city if it changed
  String city = server.arg("city"), lat = server.arg("lat"), lon = server.arg("lon");
  city.trim(); lat.trim(); lon.trim();
  if (lat.length() && lon.length()) {
    float la = lat.toFloat(), lo = lon.toFloat();
    if (la < -90 || la > 90 || lo < -180 || lo > 180 || (la == 0 && lat[0] != '0') ) {
      msg = "Coordinates out of range. "; err = true;
    } else {
      cfg.lat = la; cfg.lon = lo; cfg.hasLocation = true;
      snprintf(cfg.place, sizeof(cfg.place), "%.2f,%.2f", la, lo);
      strlcpy(cfg.city, "", sizeof(cfg.city));
      msg = "Location set to coordinates. ";
    }
  } else if (city.length() && (city != cfg.city || !cfg.hasLocation)) {
    GeoResult g = geocode(city);
    if (!g.ok) {
      msg = g.error + ". Location unchanged. "; err = true;
    } else {
      cfg.lat = g.lat; cfg.lon = g.lon; cfg.hasLocation = true;
      strlcpy(cfg.city, city.c_str(), sizeof(cfg.city));
      strlcpy(cfg.place, g.name.c_str(), sizeof(cfg.place));
      msg = "Found " + g.name + ", " + g.country + " (" + g.timezone + "). ";
    }
  }

  // Time zone
  String tz = server.arg("tz");
  tz.trim();
  if (tz.length() == 0) tz = "auto";
  if (tz.indexOf('/') >= 0) {
    // newlib only understands POSIX rules; "Asia/Jakarta" would silently become UTC
    msg += "Names like \"" + tz + "\" aren't supported; use auto (it follows the location). "; err = true;
  } else if (!tz.equalsIgnoreCase("auto") && offsetToPosix(tz).length() == 0 &&
             !(isalpha(tz[0]) || tz[0] == '<')) {
    msg += "Time zone \"" + tz + "\" not understood, kept \"" + String(cfg.tz) + "\". "; err = true;
  } else {
    strlcpy(cfg.tz, tz.equalsIgnoreCase("auto") ? "auto" : tz.c_str(), sizeof(cfg.tz));
  }

  cfg.clock12h = server.arg("clock") == "12";
  cfg.fahrenheit = server.arg("units") == "f";
  int r = server.arg("refresh").toInt();
  cfg.refreshMin = constrain(r ? r : 30, 10, 180);
  String pw = server.arg("pw");
  if (pw == "-") cfg.password[0] = 0;
  else if (pw.length()) strlcpy(cfg.password, pw.c_str(), sizeof(cfg.password));
  if (pw.length()) msg += "Restart for the password change to apply to firmware updates too. ";

  saveSettings();
  applyLocationTimezone();
  updateRequested = true;  // fetch with the new location / units right away
  redirectHome(msg + "Saved.", err);
}

void setupWeb() {
  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/refresh", HTTP_POST, [] {
    if (!authorised()) return;
    updateRequested = true;
    redirectHome("Updating the weather...");
  });
  server.on("/restart", HTTP_POST, [] {
    if (!authorised()) return;
    server.send(200, "text/html", "<meta http-equiv=refresh content='8;url=/'>Restarting...");
    delay(500);
    ESP.restart();
  });
  server.on("/wifi-reset", HTTP_POST, [] {
    if (!authorised()) return;
    server.send(200, "text/plain",
                "Wi-Fi forgotten. Connect your phone to the 'HU061-Setup' hotspot to choose a network.");
    delay(500);
    WiFiManager().resetSettings();
    ESP.restart();
  });
  server.onNotFound([] { server.send(404, "text/plain", "Not found"); });
  // Firmware upload page at /update (accepts .bin or .bin.gz). Credentials are only
  // enforced when a password is set; they are read when the page is set up, so a
  // password change takes effect after a restart.
  if (cfg.password[0]) updater.setup(&server, "/update", "admin", cfg.password);
  else updater.setup(&server, "/update");
  server.begin();
}

void handleWeb() { server.handleClient(); }
