// -------------------------------------------------------------------------
//
//  Settings.h - everything the owner can change, kept in /settings.json on
//  the ESP's flash (LittleFS). Changed from the web page or the setup hotspot.
//
// -------------------------------------------------------------------------

#pragma once

#include <LittleFS.h>
#include <ArduinoJson.h>

struct Settings {
  // Device
  char deviceName[33];      // setup hotspot name; also the network name and OTA port (lower case)
  char password[33];        // protects the web page (user "admin") and OTA updates; empty = none

  // Location
  char town[81];            // shown on the web page, e.g. "Boise, Idaho, US"
  float lat;
  float lon;
  bool haveLocation;

  // Time
  int32_t utcOffset;        // seconds, from the last weather update (includes daylight saving)
  char tzOverride[48];      // POSIX TZ rule, empty = automatic from the weather service
  char ntpServer[48];

  // Units and clock
  bool metric;
  bool clock24;
  bool showSeconds;
  uint8_t dateFormat;       // 0 = MM/DD/YYYY, 1 = DD/MM/YYYY, 2 = YYYY-MM-DD

  // Screens
  uint8_t screenSecs;
  char screens[4];          // order of screens: T = time, C = current weather, F = forecast
  uint8_t scrollDir;        // 0 = up, 1 = down, 2 = left, 3 = right

  // Display
  uint8_t brightness;       // percent: 100 = full, 1 = dim (this OLED barely changes in between)
  bool flip;                // upside down
  bool pixelShift;          // move everything a pixel now and then against burn-in
  bool nightOn;
  uint16_t nightStart;      // minutes after midnight
  uint16_t nightEnd;
  uint8_t nightBrightness;  // 1 = dim, 0 = screen off (a small dot blinks)

  // Weather
  uint8_t updateMins;
};

Settings cfg;
const char* SETTINGS_FILE = "/settings.json";

void defaultSettings() {
  memset(&cfg, 0, sizeof(cfg));
  strlcpy(cfg.deviceName, "OLED-Weather", sizeof(cfg.deviceName));
  strlcpy(cfg.ntpServer, "pool.ntp.org", sizeof(cfg.ntpServer));
  cfg.metric = false;
  cfg.clock24 = false;
  cfg.showSeconds = true;
  cfg.dateFormat = 0;
  cfg.screenSecs = 6;
  strlcpy(cfg.screens, "TCF", sizeof(cfg.screens));
  cfg.scrollDir = 1;
  cfg.brightness = 100;
  cfg.flip = false;
  cfg.pixelShift = true;
  cfg.nightOn = false;
  cfg.nightStart = 22 * 60;
  cfg.nightEnd = 7 * 60;
  cfg.nightBrightness = 1;
  cfg.updateMins = 10;
}

// Keep values in range, whatever the file or the web page sent
void checkSettings() {
  if (cfg.deviceName[0] == 0) strlcpy(cfg.deviceName, "OLED-Weather", sizeof(cfg.deviceName));
  if (cfg.ntpServer[0] == 0) strlcpy(cfg.ntpServer, "pool.ntp.org", sizeof(cfg.ntpServer));
  if (cfg.dateFormat > 2) cfg.dateFormat = 0;
  cfg.screenSecs = constrain(cfg.screenSecs, 2, 120);
  if (cfg.scrollDir > 3) cfg.scrollDir = 1;
  // Screens: only T, C, F, each at most once
  char clean[4] = "";
  uint8_t n = 0;
  for (uint8_t i = 0; cfg.screens[i] && i < 3; i++) {
    char c = toupper(cfg.screens[i]);
    if ((c == 'T' || c == 'C' || c == 'F') && !strchr(clean, c)) {
      clean[n++] = c;
      clean[n] = 0;
    }
  }
  if (n == 0) strlcpy(clean, "T", sizeof(clean));
  strlcpy(cfg.screens, clean, sizeof(cfg.screens));
  cfg.brightness = constrain(cfg.brightness, 1, 100);
  cfg.nightBrightness = constrain(cfg.nightBrightness, 0, 100);
  cfg.nightStart %= 1440;
  cfg.nightEnd %= 1440;
  cfg.updateMins = constrain(cfg.updateMins, 5, 120);
  if (cfg.utcOffset < -14 * 3600 || cfg.utcOffset > 14 * 3600) cfg.utcOffset = 0;
}

bool loadSettings() {
  defaultSettings();
  File f = LittleFS.open(SETTINGS_FILE, "r");
  if (!f) {
    Serial.println("No settings file, using defaults");
    return false;
  }
  DynamicJsonBuffer jb(1024);
  JsonObject& o = jb.parseObject(f);
  f.close();
  if (!o.success()) {
    Serial.println("Settings file unreadable, using defaults");
    return false;
  }
  strlcpy(cfg.deviceName, o["deviceName"] | cfg.deviceName, sizeof(cfg.deviceName));
  strlcpy(cfg.password, o["password"] | "", sizeof(cfg.password));
  strlcpy(cfg.town, o["town"] | "", sizeof(cfg.town));
  cfg.lat = o["lat"] | 0.0;
  cfg.lon = o["lon"] | 0.0;
  cfg.haveLocation = o["haveLocation"] | false;
  cfg.utcOffset = o["utcOffset"] | 0;
  strlcpy(cfg.tzOverride, o["tzOverride"] | "", sizeof(cfg.tzOverride));
  strlcpy(cfg.ntpServer, o["ntpServer"] | cfg.ntpServer, sizeof(cfg.ntpServer));
  cfg.metric = o["metric"] | cfg.metric;
  cfg.clock24 = o["clock24"] | cfg.clock24;
  cfg.showSeconds = o["showSeconds"] | cfg.showSeconds;
  cfg.dateFormat = o["dateFormat"] | cfg.dateFormat;
  cfg.screenSecs = o["screenSecs"] | cfg.screenSecs;
  strlcpy(cfg.screens, o["screens"] | cfg.screens, sizeof(cfg.screens));
  cfg.scrollDir = o["scrollDir"] | cfg.scrollDir;
  cfg.brightness = o["brightness"] | cfg.brightness;
  cfg.flip = o["flip"] | cfg.flip;
  cfg.pixelShift = o["pixelShift"] | cfg.pixelShift;
  cfg.nightOn = o["nightOn"] | cfg.nightOn;
  cfg.nightStart = o["nightStart"] | cfg.nightStart;
  cfg.nightEnd = o["nightEnd"] | cfg.nightEnd;
  cfg.nightBrightness = o["nightBrightness"] | cfg.nightBrightness;
  cfg.updateMins = o["updateMins"] | cfg.updateMins;
  checkSettings();
  return true;
}

bool saveSettings() {
  DynamicJsonBuffer jb(1024);
  JsonObject& o = jb.createObject();
  o["deviceName"] = cfg.deviceName;
  o["password"] = cfg.password;
  o["town"] = cfg.town;
  o["lat"] = cfg.lat;
  o["lon"] = cfg.lon;
  o["haveLocation"] = cfg.haveLocation;
  o["utcOffset"] = cfg.utcOffset;
  o["tzOverride"] = cfg.tzOverride;
  o["ntpServer"] = cfg.ntpServer;
  o["metric"] = cfg.metric;
  o["clock24"] = cfg.clock24;
  o["showSeconds"] = cfg.showSeconds;
  o["dateFormat"] = cfg.dateFormat;
  o["screenSecs"] = cfg.screenSecs;
  o["screens"] = cfg.screens;
  o["scrollDir"] = cfg.scrollDir;
  o["brightness"] = cfg.brightness;
  o["flip"] = cfg.flip;
  o["pixelShift"] = cfg.pixelShift;
  o["nightOn"] = cfg.nightOn;
  o["nightStart"] = cfg.nightStart;
  o["nightEnd"] = cfg.nightEnd;
  o["nightBrightness"] = cfg.nightBrightness;
  o["updateMins"] = cfg.updateMins;
  File f = LittleFS.open(SETTINGS_FILE, "w");
  if (!f) {
    Serial.println("Could not write settings file");
    return false;
  }
  o.printTo(f);
  f.close();
  Serial.println("Settings saved");
  return true;
}

// Network name from the device name: lower case, letters, digits and dashes
String hostName() {
  String h;
  for (const char* p = cfg.deviceName; *p; p++) {
    char c = tolower(*p);
    if (isalnum(c)) h += c;
    else if (h.length() && h[h.length() - 1] != '-') h += '-';
  }
  while (h.endsWith("-")) h.remove(h.length() - 1);
  if (h.length() == 0) h = "oled-weather";
  return h;
}
