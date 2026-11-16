#include "weather.h"

#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

// Both endpoints are HTTPS. Like the stock firmware we don't validate the certificate
// (the ESP8266 has no CA store); weather data isn't sensitive and nothing secret is sent.
static bool httpsGetJson(const String &url, JsonDocument &doc, const JsonDocument &filter,
                         String &error) {
  BearSSL::WiFiClientSecure client;
  client.setInsecure();
  // Default TLS buffers (16 KB receive): shrinking them only works with servers that
  // support max-fragment-length negotiation, which we can't count on.
  HTTPClient http;
  http.useHTTP10(true);  // no chunked encoding -> we can parse straight from the stream
  http.setTimeout(10000);
  if (!http.begin(client, url)) {
    error = "bad URL";
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    error = code > 0 ? "HTTP " + String(code) : http.errorToString(code);
    http.end();
    return false;
  }
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    error = String("JSON: ") + err.c_str();
    return false;
  }
  return true;
}

static String urlEncode(const String &s) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (unsigned char c : s) {
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += hex[c >> 4];
      out += hex[c & 15];
    }
  }
  return out;
}

static bool startsWithIgnoreCase(const String &a, const String &prefix) {
  if (prefix.length() == 0 || prefix.length() > a.length()) return false;
  return a.substring(0, prefix.length()).equalsIgnoreCase(prefix);
}

GeoResult geocode(const String &query) {
  GeoResult r;
  String name = query, qualifier;
  int comma = query.indexOf(',');
  if (comma >= 0) {
    name = query.substring(0, comma);
    qualifier = query.substring(comma + 1);
  }
  name.trim();
  qualifier.trim();
  if (name.length() < 2) {
    r.error = "City name too short";
    return r;
  }

  String url = "https://geocoding-api.open-meteo.com/v1/search?count=10&language=en&format=json&name=" +
               urlEncode(name);
  JsonDocument filter;
  JsonObject f = filter["results"].add<JsonObject>();
  for (const char *k : {"name", "latitude", "longitude", "timezone", "country", "country_code", "admin1"})
    f[k] = true;
  JsonDocument doc;
  if (!httpsGetJson(url, doc, filter, r.error)) return r;

  JsonArray results = doc["results"];
  if (results.isNull() || results.size() == 0) {
    r.error = "No place called \"" + name + "\" found";
    return r;
  }
  JsonObject pick;
  if (qualifier.length() == 0) {
    pick = results[0];  // results are ordered by population
  } else {
    for (JsonObject c : results) {
      if (qualifier.equalsIgnoreCase(c["country_code"].as<String>()) ||
          startsWithIgnoreCase(c["country"].as<String>(), qualifier) ||
          startsWithIgnoreCase(c["admin1"].as<String>(), qualifier)) {
        pick = c;
        break;
      }
    }
    if (pick.isNull()) {
      r.error = "Found \"" + name + "\", but none in \"" + qualifier + "\"";
      return r;
    }
  }
  r.ok = true;
  r.name = pick["name"].as<String>();
  r.country = pick["country"].as<String>();
  r.timezone = pick["timezone"].as<String>();
  r.lat = pick["latitude"];
  r.lon = pick["longitude"];
  return r;
}

bool fetchWeather(float lat, float lon, bool fahrenheit, Weather &out, String &error) {
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(lat, 4) +
               "&longitude=" + String(lon, 4) +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,"
               "weather_code,wind_speed_10m,is_day"
               "&daily=weather_code,temperature_2m_max,temperature_2m_min"
               "&timezone=auto&forecast_days=3";
  if (fahrenheit) url += "&temperature_unit=fahrenheit&wind_speed_unit=mph";

  JsonDocument filter;
  filter["utc_offset_seconds"] = true;
  filter["timezone"] = true;
  filter["current"] = true;
  filter["daily"] = true;
  JsonDocument doc;
  if (!httpsGetJson(url, doc, filter, error)) return false;

  JsonObject cur = doc["current"];
  JsonObject daily = doc["daily"];
  if (cur.isNull() || daily.isNull()) {
    error = "unexpected reply";
    return false;
  }
  Weather w;
  w.valid = true;
  w.code = cur["weather_code"] | -1;
  w.isDay = (cur["is_day"] | 1) == 1;
  w.temp = lroundf(cur["temperature_2m"] | 0.0f);
  w.feels = lroundf(cur["apparent_temperature"] | 0.0f);
  w.humidity = cur["relative_humidity_2m"] | 0;
  w.wind = lroundf(cur["wind_speed_10m"] | 0.0f);
  w.utcOffset = doc["utc_offset_seconds"] | 0L;
  w.timezone = doc["timezone"].as<String>();
  for (int i = 0; i < 3; i++) {
    String date = daily["time"][i].as<String>();  // "YYYY-MM-DD"
    w.days[i].date = date.length() >= 10 ? date.substring(5) : date;
    w.days[i].code = daily["weather_code"][i] | -1;
    w.days[i].tMax = lroundf(daily["temperature_2m_max"][i] | 0.0f);
    w.days[i].tMin = lroundf(daily["temperature_2m_min"][i] | 0.0f);
  }
  w.fetchedAt = millis();
  out = w;
  return true;
}

// Meteocons letters (same font as the stock firmware / ThingPulse demo):
// B sun, C moon, H sun+cloud, 4 moon+cloud, N cloud, 5 cloud (night), Y clouds,
// M fog, Q light rain, 7 light rain (night), R rain, 8 rain (night),
// W snow, # snow (night), P thunder, 6 thunder (night), ) unknown.
const char *wmoIcon(int code, bool day) {
  switch (code) {
    case 0: return day ? "B" : "C";
    case 1: return day ? "H" : "4";
    case 2: return day ? "N" : "5";
    case 3: return "Y";
    case 45: case 48: return "M";
    case 51: case 53: case 55: case 56: case 57: return day ? "Q" : "7";
    case 61: case 63: case 65: case 66: case 67:
    case 80: case 81: case 82: return day ? "R" : "8";
    case 71: case 73: case 75: case 77: case 85: case 86: return day ? "W" : "#";
    case 95: case 96: case 99: return day ? "P" : "6";
    default: return ")";
  }
}

const char *wmoText(int code) {
  switch (code) {
    case 0: return "Clear";
    case 1: return "Mostly clear";
    case 2: return "Partly cloudy";
    case 3: return "Overcast";
    case 45: case 48: return "Fog";
    case 51: case 53: case 55: return "Drizzle";
    case 56: case 57: return "Icy drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Icy rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: case 81: case 82: return "Showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Storm + hail";
    default: return "--";
  }
}
