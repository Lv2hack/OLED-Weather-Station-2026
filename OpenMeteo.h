// -------------------------------------------------------------------------
//
//  OpenMeteo.h - weather and town search from Open-Meteo (open-meteo.com).
//  Free for non-commercial use, no API key. Plain HTTP, so no TLS on the ESP.
//
//  updateWeather() gets the current temperature and conditions, the daily
//  high/low and highest chance of precipitation, and the location's UTC
//  offset (with daylight saving applied), which the clock uses.
//
//  Copyright (c) 2026 Alex Tatistcheff
//  PolyForm Noncommercial License 1.0.0 (see LICENSE.md): free to use and change,
//  not for commercial use. Third-party parts: see THIRD-PARTY.md.
//
// -------------------------------------------------------------------------

#pragma once

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <JsonListener.h>
#include <JsonStreamingParser.h>
#include "Settings.h"

struct CurrentWeather {
  bool valid;
  float temp;
  int code;           // WMO weather code
  bool isDay;
};

struct DayWeather {
  bool valid;
  int dayOfWeek;      // 0 = Sunday
  float hi;
  float lo;
  int pop;            // highest chance of precipitation, percent; -1 = unknown
  int code;
};

const uint8_t FORECAST_DAYS = 3;
CurrentWeather currentWeather;
DayWeather forecastDays[FORECAST_DAYS];
time_t lastWeatherUpdate = 0;

// ---- Weather codes ------------------------------------------------------

// Meteocons font character for a WMO weather code
const char* weatherIcon(int code, bool day) {
  switch (code) {
    case 0:  return day ? "B" : "C";                 // clear
    case 1:  return day ? "H" : "4";                 // mainly clear
    case 2:  return day ? "N" : "5";                 // partly cloudy
    case 3:  return day ? "Y" : "%";                 // overcast
    case 45: case 48: return "M";                    // fog
    case 51: case 53: case 55: case 56: case 57:     // drizzle
    case 80: case 81: case 82: return day ? "R" : "8";   // showers
    case 61: case 63: case 65: case 66: case 67: return day ? "Q" : "7";  // rain
    case 71: case 73: case 75: case 77:
    case 85: case 86: return day ? "W" : "#";        // snow
    case 95: case 96: case 99: return day ? "P" : "6";   // thunderstorm
  }
  return ")";
}

String weatherText(int code) {
  switch (code) {
    case 0:  return "Clear";
    case 1:  return "Mainly clear";
    case 2:  return "Partly cloudy";
    case 3:  return "Overcast";
    case 45: return "Fog";
    case 48: return "Freezing fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 55: return "Heavy drizzle";
    case 56: case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Light showers";
    case 81: return "Showers";
    case 82: return "Heavy showers";
    case 85: case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Thunderstorm, hail";
  }
  return "";
}

// Day of the week for a date, 0 = Sunday
int dayOfWeek(int y, int m, int d) {
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

// ---- Forecast -------------------------------------------------------------

// Returns true on success. Sets utcOffsetChanged when the location's UTC offset
// is different from the saved one (daylight saving started or ended, or a new place).
bool updateWeather(bool &utcOffsetChanged) {
  utcOffsetChanged = false;
  if (!cfg.haveLocation || WiFi.status() != WL_CONNECTED) return false;

  char url[360];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,weather_code,is_day"
           "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max"
           "&timezone=auto&forecast_days=4%s",
           cfg.lat, cfg.lon, cfg.metric ? "" : "&temperature_unit=fahrenheit");

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(10000);
  if (!http.begin(client, url)) return false;
  int status = http.GET();
  if (status != 200) {
    Serial.printf("[Weather] HTTP %d\n", status);
    http.end();
    return false;
  }
  String body = http.getString();
  http.end();

  DynamicJsonBuffer jb(2048);
  JsonObject& root = jb.parseObject(body);
  if (!root.success() || !root.containsKey("daily")) {
    Serial.println("[Weather] could not read the reply");
    return false;
  }

  JsonObject& cur = root["current"];
  currentWeather.temp = cur["temperature_2m"] | 0.0;
  currentWeather.code = cur["weather_code"] | -1;
  currentWeather.isDay = (cur["is_day"] | 1) == 1;
  currentWeather.valid = cur.success();

  // Today before noon (local time at the location), tomorrow after
  const char* curTime = cur["time"] | "2000-01-01T00:00";     // "2026-11-12T09:00"
  int localHour = atoi(curTime + 11);
  int first = localHour >= 12 ? 1 : 0;

  JsonObject& daily = root["daily"];
  JsonArray& dates = daily["time"];
  JsonArray& codes = daily["weather_code"];
  JsonArray& his = daily["temperature_2m_max"];
  JsonArray& los = daily["temperature_2m_min"];
  JsonArray& pops = daily["precipitation_probability_max"];
  for (uint8_t i = 0; i < FORECAST_DAYS; i++) {
    uint8_t d = first + i;
    DayWeather &day = forecastDays[i];
    const char* date = dates[d] | "";
    day.valid = strlen(date) == 10 && his[d].success() && los[d].success();
    if (!day.valid) continue;
    day.dayOfWeek = dayOfWeek(atoi(date), atoi(date + 5), atoi(date + 8));
    day.hi = his[d].as<float>();
    day.lo = los[d].as<float>();
    day.pop = pops[d].success() ? pops[d].as<int>() : -1;
    day.code = codes[d] | -1;
  }

  int32_t offset = root["utc_offset_seconds"] | cfg.utcOffset;
  if (offset != cfg.utcOffset) {
    cfg.utcOffset = offset;
    utcOffsetChanged = true;
  }
  lastWeatherUpdate = time(nullptr);
  Serial.printf("[Weather] %.1f code %d, UTC offset %d s\n", currentWeather.temp, currentWeather.code, offset);
  return true;
}

// ---- Town search ------------------------------------------------------------

struct Place {
  String label;       // "Boise, Idaho, US"
  float lat;
  float lon;
};

const uint8_t MAX_PLACES = 5;

class PlaceListener : public JsonListener {
  public:
    Place *places;
    uint8_t count = 0;

    virtual void whitespace(char c) {}
    virtual void startDocument() {}
    virtual void endDocument() {}
    virtual void startArray() {}
    virtual void endArray() {}
    virtual void key(String key) { currentKey = key; }
    virtual void startObject() {
      depth++;
      if (depth == 2) { name = ""; admin1 = ""; country = ""; lat = 0; lon = 0; }
    }
    virtual void endObject() {
      // depth 2 = one entry of "results"
      if (depth == 2 && count < MAX_PLACES && name.length()) {
        String label = name;
        if (admin1.length() && admin1 != name) label += ", " + admin1;
        if (country.length()) label += ", " + country;
        places[count].label = label;
        places[count].lat = lat;
        places[count].lon = lon;
        count++;
      }
      depth--;
    }
    virtual void value(String value) {
      if (depth != 2) return;
      if (currentKey == "name") name = value;
      else if (currentKey == "admin1") admin1 = value;
      else if (currentKey == "country_code") country = value;
      else if (currentKey == "latitude") lat = value.toFloat();
      else if (currentKey == "longitude") lon = value.toFloat();
    }

  private:
    int depth = 0;
    String currentKey, name, admin1, country;
    float lat = 0, lon = 0;
};

String urlEncode(const String &s) {
  String out;
  const char *hex = "0123456789ABCDEF";
  for (unsigned int i = 0; i < s.length(); i++) {
    uint8_t c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
  }
  return out;
}

// Search for a town; returns the number of places found (up to MAX_PLACES).
// The reply lists every postcode of each place, so it is read as a stream.
uint8_t searchPlaces(const String &query, Place *places) {
  if (WiFi.status() != WL_CONNECTED || query.length() == 0) return 0;
  String url = "http://geocoding-api.open-meteo.com/v1/search?count=5&language=en&format=json&name=" + urlEncode(query);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(10000);
  http.useHTTP10(true);         // no chunked encoding, so the stream can be parsed directly
  if (!http.begin(client, url)) return 0;
  int status = http.GET();
  if (status != 200) {
    Serial.printf("[Search] HTTP %d\n", status);
    http.end();
    return 0;
  }

  PlaceListener listener;
  listener.places = places;
  JsonStreamingParser parser;
  parser.setListener(&listener);
  WiFiClient &stream = http.getStream();
  unsigned long started = millis();
  while ((stream.connected() || stream.available()) && millis() - started < 15000UL) {
    if (stream.available()) parser.parse(stream.read());
    else delay(1);
  }
  http.end();
  Serial.printf("[Search] \"%s\": %d places\n", query.c_str(), listener.count);
  return listener.count;
}
