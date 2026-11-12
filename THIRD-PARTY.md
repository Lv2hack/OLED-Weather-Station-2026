# Third-party code and fonts

These parts of the repository are not covered by this project's own licenses (LICENSE.md and
LICENSE-docs.txt). They keep their original licenses, reproduced or linked below.

## ThingPulse ESP8266 Weather Station example (MIT License)

- `WeatherStationFonts.h` (the Meteocons weather icon fonts as converted by Daniel Eichhorn)
- `WeatherStationImages.h` (the page indicator symbols)
- The layout of the time, current weather and forecast screens in `OLED-Weather-Station-2026.ino`,
  which started from the ThingPulse example

Source: https://github.com/ThingPulse/esp8266-weather-station

```
The MIT License (MIT)

Copyright (c) 2015-2018 by Daniel Eichhorn - ThingPulse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Meteocons icon font

The weather icons in `WeatherStationFonts.h` are the Meteocons font by Alessio Atzeni,
http://www.alessioatzeni.com/meteocons/ . See the author's site for its terms.

## DSEG7 Classic font

`DSEG7ClassicBoldFont.h` is the DSEG7 Classic Bold font by Keshikan, converted to the OLED font format.
DSEG is distributed under the SIL Open Font License 1.1: https://www.keshikan.net/fonts-e.html

## Libraries (not included in this repository)

The sketch uses these libraries, installed separately, each under its own license: ESP8266 Arduino core,
ThingPulse OLED driver (MIT), WiFiManager (MIT), ArduinoJson (MIT), Json Streaming Parser (MIT),
Polymorphic Buttons.

## Weather data

Weather and town search come from Open-Meteo (https://open-meteo.com). The free service is for
non-commercial use; commercial use needs an Open-Meteo subscription. Weather data by Open-Meteo.com,
licensed CC BY 4.0.
