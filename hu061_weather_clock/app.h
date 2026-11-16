// Shared state between the display, the web settings page and the main loop.
#pragma once
#include <Arduino.h>
#include <OLEDDisplayUi.h>
#include <SSD1306Wire.h>

#include "version.h"
#include "weather.h"

extern SSD1306Wire display;
extern OLEDDisplayUi ui;
extern Weather weather;

extern bool updateRequested;     // set by button / web page / timer, handled in loop()
extern String lastError;         // "" when the last update worked
extern unsigned long lastAttempt;
extern bool timeSynced;

// display.cpp
void setupUi();
void drawMessage(const String &l1, const String &l2 = "", const String &l3 = "", const String &l4 = "");
void drawProgress(int percent, const String &label);
void showInfoScreen(unsigned long ms);  // IP / URL screen (long button press)
bool infoScreenActive();
void drawInfoScreen();

// web.cpp
void setupWeb();
void handleWeb();

// main.cpp
void applyLocationTimezone();     // re-apply tz after settings/weather change
