# OLED Weather Station 2026

A 128x64 OLED clock and weather station that anyone can set up from a phone, no programming needed.
Weather comes from [Open-Meteo](https://open-meteo.com) (free for non-commercial use, no API key).

## Screens

- **Time**: date and a large clock
- **Current weather**: temperature, conditions and icon
- **Forecast**: 3 days with high/low, chance of precipitation and icon

The bottom line always shows the time and temperature.

## Setup

1. Power it up. If it has no Wi-Fi network yet, the screen asks you to connect a phone to the
   **OLED-Weather** hotspot.
2. Pick your Wi-Fi network, enter the password and type your town. The station looks the town up
   and keeps the first match.
3. Everything else is on the settings page: `http://oled-weather.local`, or the address shown when
   you press the button.

The time zone and daylight saving follow the weather location automatically.

## Settings page

Location search, °F/°C, 12/24 hour, seconds, date format, seconds per screen, screen order,
scroll direction, brightness (full/dim), night mode (no change / dim / screen off with a small
blinking dot), upside down, pixel shift against burn-in, weather update interval, time server,
time zone override, device name, password (user `admin`, also used for OTA updates), change
Wi-Fi network, restart, factory reset.

## Button

| Action | Result |
|---|---|
| Press | Info screen: settings address, Wi-Fi, uptime, last weather update |
| Double press | Update the weather now |
| Hold 2 s, then hold again | Open the Wi-Fi setup hotspot (a press cancels) |
| Hold while powering up (5 s) | Factory reset |
| Any press while the screen is off at night | Screen on for 30 s |

## Hardware

- WeMos/LOLIN D1 mini (ESP8266)
- 2.42" 128x64 OLED, SSD1309, I2C: SDA on D3, SCL on D2 (reset on D6 for 7-pin displays)
- Push button from D7 to GND

**[BUILD.md](BUILD.md)** has the parts list, wiring diagram, software setup and troubleshooting.
A 3D-printed case is coming.

## Building

Arduino IDE with the ESP8266 core (2.7.4), board "LOLIN(WEMOS) D1 R2 & mini" with a flash layout
that includes a file system (e.g. 4MB, FS: 2MB). Libraries:

- ESP8266 and ESP32 OLED driver for SSD1306 displays (ThingPulse) 4.1
- WiFiManager (tzapu) 2.0
- ArduinoJson 5.13
- Json Streaming Parser (squix78)
- Polymorphic Buttons (PMButton)

After the first USB upload, updates can go over Wi-Fi: pick the `oled-weather` network port in the IDE.

## License

You're welcome to build, use, change and share this for personal, educational or other non-commercial
purposes. Making and selling these, or other commercial use, needs my permission.

- **Code:** [PolyForm Noncommercial 1.0.0](LICENSE.md)
- **Build guide, diagrams and case designs:** [CC BY-NC-SA 4.0](LICENSE-docs.txt)
- **Third-party parts** (ThingPulse example code and fonts, MIT; DSEG7 font, SIL OFL; Meteocons icons)
  keep their own licenses: see [THIRD-PARTY.md](THIRD-PARTY.md).

Weather data by [Open-Meteo.com](https://open-meteo.com), whose free service is for non-commercial use.
