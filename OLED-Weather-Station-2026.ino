// -------------------------------------------------------------------------
//
//  OLED Weather Station 2026
//
//  Alex Tatistcheff
//
//  A 128x64 OLED clock and weather station anyone can set up without
//  programming. Weather from Open-Meteo (no API key).
//
//  Setup: on first start (or when the saved Wi-Fi can't be found) the station
//  opens a hotspot named after the device ("OLED-Weather"). Connect a phone to
//  it, pick the Wi-Fi network and type your town. Everything else is on the
//  settings page: http://oled-weather.local or the address on the info screen.
//
//  Button (D7 to ground):
//    press         - info screen (settings address, Wi-Fi, uptime, last update)
//    double press  - update the weather now
//    hold 2 s      - Wi-Fi setup: hold again to open the setup hotspot, press to cancel
//    hold while powering up (5 s) - factory reset
//
//  History:  12/6/20  Started from the ThingPulse OLED weather demo
//            11/12/26 2026 version: settings page, Open-Meteo, set up from a phone,
//                     time zone from the weather location, night dimming,
//                     scroll direction and screen order, OTA updates
//
// -------------------------------------------------------------------------

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <LittleFS.h>
#include <time.h>

#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "PMButton.h"

#include "Settings.h"
#include "OpenMeteo.h"
#include "WebUI.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "DSEG7ClassicBoldFont.h"

// ---- Hardware ------------------------------------------------------------
const int I2C_DISPLAY_ADDRESS = 0x3c;
const int SDA_PIN = D3;
const int SDC_PIN = D2;
const int RES_PIN = D6;         // this display needs its reset pin pulsed at startup
const int BUTTON_PIN = D7;      // to ground

const int WIFI_PORTAL_TIMEOUT_SECS = 180;   // then restart and try the saved network again
const int INFO_SCREEN_SECS = 15;
const unsigned long WAKE_MS = 30000;        // screen on after a press while it is off at night

const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

SSD1306Wire display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
OLEDDisplayUi ui(&display);
PMButton button(BUTTON_PIN);
WiFiManager wifiManager;
WiFiManagerParameter townParam("town", "Your town, for the weather (e.g. Boise)", "", 64);
String hostStr;                 // must outlive WiFiManager/OTA, which keep a pointer

bool infoScreenActive = false;
unsigned long infoScreenStarted = 0;
unsigned long wakeUntil = 0;
int appliedBrightness = -1;
unsigned long nextWeatherMillis = 0;
int8_t shiftX = 0, shiftY = 0;  // pixel shift against burn-in
uint8_t frameCount = 1;
unsigned long screenShownSince = 0;

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawFooter(OLEDDisplay *display, OLEDDisplayUiState* state);

FrameCallback frames[3];
OverlayCallback overlays[] = { drawFooter };

// ---- Small helpers ---------------------------------------------------------

void showMessage(const String &l1, const String &l2 = "", const String &l3 = "", const String &l4 = "") {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 4, l1);
  display.drawString(64, 18, l2);
  display.drawString(64, 32, l3);
  display.drawString(64, 46, l4);
  display.display();
}

// POSIX TZ rule: the override if set, otherwise a fixed offset from the weather service
// (which already includes daylight saving and is refreshed with every weather update)
String posixTZ() {
  if (cfg.tzOverride[0]) return cfg.tzOverride;
  int32_t off = cfg.utcOffset;            // seconds east of UTC
  int32_t a = abs(off);
  int h = a / 3600, m = (a % 3600) / 60;
  char buff[24];
  // POSIX counts west as positive, so the sign is reversed: UTC-7 is "LOC7".
  // A plain letter name: the core's tzset() misreads "<-0700>7" as 700 hours.
  if (m) snprintf(buff, sizeof(buff), "LOC%s%d:%02d", off < 0 ? "" : "-", h, m);
  else   snprintf(buff, sizeof(buff), "LOC%s%d", off < 0 ? "" : "-", h);
  return buff;
}

void applyTimeZone() {
  String tz = posixTZ();
  setenv("TZ", tz.c_str(), 1);
  tzset();
  Serial.printf("Time zone %s\n", tz.c_str());
}

bool clockSet() {
  return time(nullptr) > 1600000000;
}

String timeString(bool withSeconds, bool withAmPm) {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int hour = cfg.clock24 ? t->tm_hour : (t->tm_hour + 11) % 12 + 1;
  char buff[16];
  if (withSeconds) snprintf(buff, sizeof(buff), "%02d:%02d:%02d", hour, t->tm_min, t->tm_sec);
  else             snprintf(buff, sizeof(buff), "%02d:%02d", hour, t->tm_min);
  String s = buff;
  if (withAmPm && !cfg.clock24) s += t->tm_hour >= 12 ? " PM" : " AM";
  return s;
}

// Local time of day in seconds, for the web page clock
long localSeconds() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  return t->tm_hour * 3600L + t->tm_min * 60 + t->tm_sec;
}

String tempString(float temp, int decimals) {
  return String(temp, decimals) + (cfg.metric ? "°C" : "°F");
}

bool isNight() {
  if (!cfg.nightOn || !clockSet() || cfg.nightStart == cfg.nightEnd) return false;
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int m = t->tm_hour * 60 + t->tm_min;
  if (cfg.nightStart < cfg.nightEnd) return m >= cfg.nightStart && m < cfg.nightEnd;
  return m >= cfg.nightStart || m < cfg.nightEnd;       // window over midnight
}

int wantedBrightness() {
  if ((long)(previewUntil - millis()) > 0) return previewLevel;    // "Try" button on the page
  if ((long)(wakeUntil - millis()) > 0) return cfg.brightness;
  return isNight() ? cfg.nightBrightness : cfg.brightness;
}

// Called at startup and whenever the settings page saves
void applySettings() {
  uint8_t n = 0;
  for (uint8_t i = 0; cfg.screens[i] && n < 3; i++) {
    switch (cfg.screens[i]) {
      case 'T': frames[n++] = drawDateTime; break;
      case 'C': frames[n++] = drawCurrentWeather; break;
      case 'F': frames[n++] = drawForecast; break;
    }
  }
  if (n == 0) frames[n++] = drawDateTime;
  ui.setFrames(frames, n);
  if (ui.getUiState()->currentFrame >= n) ui.switchToFrame(0);
  frameCount = n;
  if (n > 1) ui.enableIndicator();
  else       ui.disableIndicator();

  // loop() switches screens by the clock: the library counts animation frames and
  // adds a whole frame whenever one is even 1 ms late, so its timing ran about 20% fast
  ui.disableAutoTransition();
  ui.setTimePerFrame(60000);
  screenShownSince = millis();
  static const AnimationDirection directions[] = { SLIDE_UP, SLIDE_DOWN, SLIDE_LEFT, SLIDE_RIGHT };
  ui.setFrameAnimation(directions[cfg.scrollDir]);

  display.resetOrientation();
  if (cfg.flip) display.flipScreenVertically();

  applyTimeZone();
  appliedBrightness = -1;       // set again by loop()
}

void doWeatherUpdate() {
  bool offsetChanged;
  bool ok = updateWeather(offsetChanged);
  if (offsetChanged) {
    applyTimeZone();
    saveSettings();             // so the clock is right at the next start, before the first update
  }
  nextWeatherMillis = millis() + (ok ? cfg.updateMins * 60000UL : 60000UL);
  if (!ok && cfg.haveLocation) Serial.println("Weather update failed, retrying in 60 s");
}

// Town typed on the setup hotspot page: look it up and keep the first match
void applyTownFromPortal() {
  String town = townParam.getValue();
  town.trim();
  if (town.length() == 0) return;
  showMessage("Looking up", town);
  Place places[MAX_PLACES];
  if (searchPlaces(town, places) > 0) {
    strlcpy(cfg.town, places[0].label.c_str(), sizeof(cfg.town));
    cfg.lat = places[0].lat;
    cfg.lon = places[0].lon;
    cfg.haveLocation = true;
    saveSettings();
    showMessage("Location set:", places[0].label, "Change it on the", "settings page");
  } else {
    showMessage("Town not found:", town, "Set it on the", "settings page");
  }
  delay(3000);
}

void factoryReset() {
  Serial.println("Factory reset");
  showMessage("Factory reset", "Erasing settings...");
  LittleFS.remove(SETTINGS_FILE);
  wifiManager.resetSettings();
  delay(1000);
  ESP.restart();
}

// ---- Wi-Fi -----------------------------------------------------------------

void configModeCallback(WiFiManager *myWiFiManager) {
  Serial.print("WiFi setup hotspot open: ");
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.normalDisplay();
  display.displayOn();
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 0, "WiFi setup");
  display.drawString(64, 12, "Connect your phone to");
  display.drawString(64, 24, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 36, "then browse to");
  display.drawString(64, 48, WiFi.softAPIP().toString());
  display.display();
}

void startWifi() {
  showMessage("Connecting to WiFi", WiFi.SSID());

  // Station only. The chip's factory default also runs a hotspot named ESP-xxxxxx,
  // and that setting is kept in flash, so switch it off explicitly.
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);

  wifiManager.setHostname(hostStr.c_str());
  wifiManager.setConnectTimeout(30);
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SECS);
  wifiManager.setAPCallback(configModeCallback);
  wifiManager.addParameter(&townParam);
  if (!wifiManager.autoConnect(cfg.deviceName)) {
    Serial.println("WiFi setup timed out, restarting");
    showMessage("No WiFi", "Restarting...");
    delay(3000);
    ESP.restart();
  }
  WiFi.mode(WIFI_STA);   // close the setup hotspot if it was used
  Serial.printf("WiFi connected to %s, IP %s\n", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  applyTownFromPortal();
}

// Open the setup hotspot on demand (button or settings page), then restart
void openWifiSetup() {
  Serial.println("Opening WiFi setup hotspot");
  wifiManager.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_SECS);
  if (wifiManager.startConfigPortal(cfg.deviceName)) {
    WiFi.mode(WIFI_STA);
    applyTownFromPortal();
  }
  showMessage("Restarting...");
  delay(1000);
  ESP.restart();
}

// Button held: ask before opening the Wi-Fi setup hotspot
void confirmWifiSetup() {
  Serial.println("Button held: WiFi setup? Hold again to confirm, press to cancel");
  showMessage("Change WiFi network?", "Hold again: setup hotspot", "Press: cancel", "(cancels in 15 s)");
  display.invertDisplay();

  unsigned long started = millis();
  bool confirmed = false;
  while (millis() - started < 15000UL) {
    button.checkSwitch();
    if (button.held()) { confirmed = true; break; }
    if (button.clicked() || button.doubleClicked()) break;
    ArduinoOTA.handle();
    delay(10);
  }
  display.normalDisplay();
  infoScreenActive = false;
  if (confirmed) openWifiSetup();
  Serial.println("WiFi setup canceled");
}

// Button held while powering up: factory reset after 5 s
void checkFactoryResetAtBoot() {
  if (digitalRead(BUTTON_PIN) != LOW) return;
  unsigned long started = millis();
  while (digitalRead(BUTTON_PIN) == LOW) {
    int left = 5 - (millis() - started) / 1000;
    if (left <= 0) {
      showMessage("Factory reset", "Release the button");
      while (digitalRead(BUTTON_PIN) == LOW) delay(10);
      factoryReset();
    }
    showMessage("Keep holding for", "factory reset", String(left) + " s", "Release to cancel");
    delay(100);
  }
}

void startOTA() {
  ArduinoOTA.setHostname(hostStr.c_str());
  if (cfg.password[0]) ArduinoOTA.setPassword(cfg.password);
  ArduinoOTA.onStart([]() {
    Serial.println("OTA update starting");
    display.normalDisplay();
    display.displayOn();
    display.setBrightness(255);
    showMessage("Updating firmware...");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    display.drawProgressBar(4, 34, 120, 8, progress / (total / 100));
    display.display();
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update done, restarting");
    showMessage("Restarting...");
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA error %u\n", error);
    showMessage("Update failed");
    delay(2000);
    appliedBrightness = -1;
  });
  ArduinoOTA.begin();                       // also starts mDNS: <name>.local
  MDNS.addService("http", "tcp", 80);
  Serial.printf("OTA ready: %s.local\n", hostStr.c_str());
}

// ---- Screens ---------------------------------------------------------------

// Shown on the weather screens until a location is set
bool drawNoLocation(OLEDDisplay *display, int16_t x, int16_t y) {
  if (cfg.haveLocation && currentWeather.valid) return false;
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  if (!cfg.haveLocation) {
    display->drawString(64 + x, 4 + y, "Set your town at");
    display->drawString(64 + x, 18 + y, "http://" + WiFi.localIP().toString());
    display->drawString(64 + x, 32 + y, hostStr + ".local");
  } else {
    display->drawString(64 + x, 18 + y, "Getting the weather...");
  }
  return true;
}

void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  x += shiftX; y += shiftY;
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  char buff[24];
  const char* wday = WDAY_NAMES[t->tm_wday].c_str();
  switch (cfg.dateFormat) {
    case 1:  snprintf(buff, sizeof(buff), "%s, %02d/%02d/%04d", wday, t->tm_mday, t->tm_mon + 1, t->tm_year + 1900); break;
    case 2:  snprintf(buff, sizeof(buff), "%s, %04d-%02d-%02d", wday, t->tm_year + 1900, t->tm_mon + 1, t->tm_mday); break;
    default: snprintf(buff, sizeof(buff), "%s, %02d/%02d/%04d", wday, t->tm_mon + 1, t->tm_mday, t->tm_year + 1900); break;
  }
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 5 + y, buff);

  display->setFont(DSEG7_Classic_Bold_21);
  if (cfg.showSeconds) {
    display->setTextAlignment(TEXT_ALIGN_RIGHT);
    display->drawString(115 + x, 19 + y, timeString(true, false));
  } else {
    display->drawString(64 + x, 19 + y, timeString(false, false));
  }
}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  x += shiftX; y += shiftY;
  if (drawNoLocation(display, x, y)) return;
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, weatherText(currentWeather.code));

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(44 + x, 5 + y, tempString(currentWeather.temp, 1));

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(20 + x, 0 + y, weatherIcon(currentWeather.code, currentWeather.isDay));
}

// One forecast column, 40 px wide: day, icon, high/low, chance of precipitation
void drawForecastDay(OLEDDisplay *display, int x, int y, const DayWeather &day) {
  if (!day.valid) return;
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 1, WDAY_NAMES[day.dayOfWeek]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, weatherIcon(day.code, true));

  // High/low. No degree signs: on this font "°/" looks like "%"
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 30, String(lround(day.hi)) + "/" + String(lround(day.lo)));

  if (day.pop < 0) return;
  // Chance of precipitation with a small raindrop in front
  String pop = String(day.pop) + "%";
  int popWidth = display->getStringWidth(pop);
  int dropX = x + 20 - (popWidth + 7) / 2 + 2;
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(dropX + 5, y + 39, pop);
  int dropY = y + 42;
  display->setPixel(dropX, dropY);
  display->drawHorizontalLine(dropX - 1, dropY + 1, 3);
  display->drawHorizontalLine(dropX - 1, dropY + 2, 3);
  display->drawHorizontalLine(dropX - 2, dropY + 3, 5);
  display->drawHorizontalLine(dropX - 2, dropY + 4, 5);
  display->drawHorizontalLine(dropX - 2, dropY + 5, 5);
  display->drawHorizontalLine(dropX - 1, dropY + 6, 3);
}

void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  x += shiftX; y += shiftY;
  if (drawNoLocation(display, x, y)) return;
  for (uint8_t i = 0; i < FORECAST_DAYS; i++) {
    drawForecastDay(display, x + i * 44, y, forecastDays[i]);
  }
}

void drawFooter(OLEDDisplay *display, OLEDDisplayUiState* state) {
  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0 + shiftX, 54 + shiftY, timeString(cfg.showSeconds, false));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  display->drawString(127 + shiftX, 54 + shiftY, currentWeather.valid ? tempString(currentWeather.temp, 1) : String("--"));
  display->drawHorizontalLine(0, 52 + shiftY, 128);
}

void drawInfoScreen() {
  char buff[40];
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);

  if (WiFi.status() == WL_CONNECTED) {
    display.drawString(0, 0, "http://" + WiFi.localIP().toString());
    display.drawString(0, 12, hostStr + ".local");
    display.drawString(0, 24, WiFi.SSID() + "  " + String(WiFi.RSSI()) + " dBm");
  } else {
    display.drawString(0, 0, "WiFi not connected");
    display.drawString(0, 12, "Saved: " + WiFi.SSID());
  }
  unsigned long mins = millis() / 60000;
  snprintf(buff, sizeof(buff), "Up: %lud %luh %lum", mins / 1440, (mins / 60) % 24, mins % 60);
  display.drawString(0, 36, buff);

  if (!cfg.haveLocation) {
    display.drawString(0, 48, "Weather: no town set");
  } else if (lastWeatherUpdate == 0) {
    display.drawString(0, 48, "Weather: not updated yet");
  } else {
    struct tm* t = localtime(&lastWeatherUpdate);
    int hour = cfg.clock24 ? t->tm_hour : (t->tm_hour + 11) % 12 + 1;
    snprintf(buff, sizeof(buff), "Weather: %d:%02d%s", hour, t->tm_min,
             cfg.clock24 ? "" : (t->tm_hour >= 12 ? " PM" : " AM"));
    display.drawString(0, 48, buff);
  }
  display.display();
}

// Screen "off": a 2x2 dot in the middle of the screen, shown for 0.3 s every 2.5 s
void drawOffDot() {
  static int lastOn = -1;
  bool on = millis() % 2500 < 300;
  if (on == lastOn) return;
  lastOn = on;
  display.clear();
  if (on) display.fillRect(63, 31, 2, 2);
  display.display();
}

// ---- Setup and loop --------------------------------------------------------

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(RES_PIN, OUTPUT);
  digitalWrite(RES_PIN, LOW);
  delay(100);
  digitalWrite(RES_PIN, HIGH);

  display.init();
  display.setContrast(255);

  LittleFS.begin();
  loadSettings();
  hostStr = hostName();
  if (cfg.flip) display.flipScreenVertically();

  button.begin();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  checkFactoryResetAtBoot();

  startWifi();

  // Time from the network; the time zone comes from the weather location
  String tz = posixTZ();
  if (strcmp(cfg.ntpServer, "pool.ntp.org") == 0) configTime(tz.c_str(), cfg.ntpServer, "time.google.com");
  else configTime(tz.c_str(), cfg.ntpServer, "pool.ntp.org");
  showMessage("Getting the time...");
  unsigned long waitStart = millis();
  while (!clockSet() && millis() - waitStart < 20000UL) delay(100);
  Serial.printf("Clock %s after %lu ms\n", clockSet() ? "set" : "NOT set", millis() - waitStart);

  startOTA();
  startWebUI();

  ui.setTargetFPS(30);
  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);
  ui.setIndicatorPosition(BOTTOM);
  ui.setIndicatorDirection(LEFT_RIGHT);
  ui.setOverlays(overlays, 1);
  applySettings();
  ui.init();                    // initializes the display again, so orientation after it
  applySettings();

  if (cfg.haveLocation) {
    showMessage("Getting the weather...");
    doWeatherUpdate();
  }
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();

  if (factoryResetRequested) factoryReset();
  if (restartRequested) {
    delay(500);
    ESP.restart();
  }
  if (wifiSetupRequested) {
    wifiSetupRequested = false;
    openWifiSetup();
  }

  // Button
  bool dark = appliedBrightness == 0;
  button.checkSwitch();
  if (button.doubleClicked()) {
    if (dark) wakeUntil = millis() + WAKE_MS;
    else {
      Serial.println("Button: double press, updating weather");
      infoScreenActive = false;
      weatherUpdateRequested = true;
    }
  } else if (button.clicked()) {
    if (dark) wakeUntil = millis() + WAKE_MS;
    else {
      infoScreenActive = !infoScreenActive;
      infoScreenStarted = millis();
    }
  }
  if (button.held()) {
    if (dark) wakeUntil = millis() + WAKE_MS;
    else confirmWifiSetup();
  }

  // Weather, between screen transitions
  bool due = weatherUpdateRequested || (long)(millis() - nextWeatherMillis) >= 0;
  if (due && (dark || infoScreenActive || ui.getUiState()->frameState == FIXED)) {
    weatherUpdateRequested = false;
    doWeatherUpdate();
  }

  // Brightness, night dimming. "Off" keeps the panel on at the lowest level with
  // every pixel dark except a small dot in the middle that blinks every 2.5 s,
  // so you can tell the station is still running.
  int b = wantedBrightness();
  if (b != appliedBrightness) {
    display.displayOn();
    display.setBrightness(b == 0 ? 2 : map(b, 0, 100, 0, 255));
    appliedBrightness = b;
  }
  if (b == 0) {
    drawOffDot();
    delay(20);
    return;
  }

  // Pixel shift against burn-in: a 4-step square, one step every 2 minutes
  static const int8_t SHIFT_X[] = {0, 1, 1, 0};
  static const int8_t SHIFT_Y[] = {0, 0, -1, -1};
  uint8_t step = cfg.pixelShift ? (millis() / 120000UL) % 4 : 0;
  shiftX = SHIFT_X[step];
  shiftY = SHIFT_Y[step];

  if (infoScreenActive) {
    if (millis() - infoScreenStarted > INFO_SCREEN_SECS * 1000UL) {
      infoScreenActive = false;
      screenShownSince = millis();
    } else {
      drawInfoScreen();
      delay(20);
      return;
    }
  }

  // Next screen after cfg.screenSecs, counted from the end of the last transition
  if (ui.getUiState()->frameState == IN_TRANSITION) {
    screenShownSince = millis();
  } else if (frameCount > 1 && millis() - screenShownSince >= cfg.screenSecs * 1000UL) {
    ui.nextFrame();
    screenShownSince = millis();
  }

  int remainingTimeBudget = ui.update();
  if (remainingTimeBudget > 0) delay(remainingTimeBudget);
}
