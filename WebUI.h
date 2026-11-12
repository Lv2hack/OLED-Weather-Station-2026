// -------------------------------------------------------------------------
//
//  WebUI.h - settings page on port 80 (http://<name>.local or the IP shown
//  on the info screen). If a password is set, log in as "admin".
//
//  GET  /               the page
//  GET  /api/status     time, Wi-Fi, weather state
//  GET  /api/settings   current settings (the password is never sent)
//  POST /api/settings   change settings (form fields, only those sent)
//  GET  /api/search?q=  town search
//  POST /api/action     do=update | wifi | restart | reset
//
//  Copyright (c) 2026 Alex Tatistcheff
//  PolyForm Noncommercial License 1.0.0 (see LICENSE.md): free to use and change,
//  not for commercial use. Third-party parts: see THIRD-PARTY.md.
//
// -------------------------------------------------------------------------

#pragma once

#include <ESP8266WebServer.h>
#include "Settings.h"
#include "OpenMeteo.h"

ESP8266WebServer server(80);

// Set by the page, done by loop() so the reply goes out first
bool weatherUpdateRequested = false;
bool wifiSetupRequested = false;
bool restartRequested = false;
bool factoryResetRequested = false;
unsigned long previewUntil = 0;   // brightness preview from the page
int previewLevel = 0;

// In the main sketch
void applySettings();
String timeString(bool withSeconds, bool withAmPm);
long localSeconds();

static const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Weather Station</title>
<style>
:root{--bg:#f4f5f7;--card:#fff;--text:#1d2228;--muted:#69727d;--line:#dde1e6;--accent:#1f6feb;--bad:#c62828}
@media (prefers-color-scheme:dark){:root{--bg:#15181c;--card:#1f2328;--text:#e6e9ed;--muted:#98a1ab;--line:#353b42;--accent:#4c8dff;--bad:#ff6b6b}}
*{box-sizing:border-box}
body{margin:0;background:var(--bg);color:var(--text);font:16px/1.4 system-ui,sans-serif}
main{max-width:560px;margin:0 auto;padding:16px}
h1{font-size:22px;margin:4px 0 14px}
section{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:14px 16px;margin-bottom:14px}
h2{font-size:15px;text-transform:uppercase;letter-spacing:.04em;color:var(--muted);margin:0 0 10px}
label{display:block;margin:10px 0 4px;font-weight:600}
.hint{color:var(--muted);font-size:13px;font-weight:400}
input,select,button{font:inherit;color:inherit}
input[type=text],input[type=password],input[type=number],input[type=time],select{width:100%;padding:8px;border:1px solid var(--line);border-radius:6px;background:var(--bg)}
.row{display:flex;gap:10px}.row>*{flex:1}
.check{display:flex;align-items:center;gap:8px;margin:10px 0;font-weight:600}
.check input{width:20px;height:20px}
button{padding:9px 14px;border-radius:6px;border:1px solid var(--line);background:var(--bg);cursor:pointer}
button.primary{background:var(--accent);border-color:var(--accent);color:#fff;font-weight:600}
button.danger{color:var(--bad)}
.buttons{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}
#results button{display:block;width:100%;text-align:left;margin-top:6px}
table{border-collapse:collapse;width:100%}td{padding:3px 0;vertical-align:top}td:first-child{color:var(--muted);width:40%}
#msg{position:sticky;bottom:0;padding:10px;text-align:center;font-weight:600;display:none;background:var(--accent);color:#fff;border-radius:6px}
</style></head><body><main>
<h1 id="title">Weather Station</h1>

<section><h2>Status</h2><table id="status"></table></section>

<section><h2>Location</h2>
<div id="town" style="font-weight:600"></div>
<label>Find your town <span class="hint">e.g. "Boise" or "Boise, Idaho"</span></label>
<div class="row"><input type="text" id="q"><button style="flex:0" onclick="search()">Search</button></div>
<div id="results"></div>
<details style="margin-top:10px"><summary class="hint">Enter latitude/longitude instead</summary>
<div class="row"><div><label>Latitude</label><input type="number" step="any" id="lat"></div>
<div><label>Longitude</label><input type="number" step="any" id="lon"></div></div>
<div class="buttons"><button onclick="setLatLon()">Use these</button></div></details>
</section>

<form id="f" onsubmit="save();return false">
<section><h2>Clock and units</h2>
<label>Temperature</label><select name="metric"><option value="0">°F</option><option value="1">°C</option></select>
<label>Clock</label><select name="clock24"><option value="0">12 hour</option><option value="1">24 hour</option></select>
<div class="check"><input type="checkbox" name="showSeconds">Show seconds</div>
<label>Date</label><select name="dateFormat"><option value="0">MM/DD/YYYY</option><option value="1">DD/MM/YYYY</option><option value="2">YYYY-MM-DD</option></select>
</section>

<section><h2>Screens</h2>
<label>Seconds per screen</label><input type="number" name="screenSecs" min="2" max="120">
<label>Order <span class="hint">pick "none" to leave a screen out</span></label>
<div class="row"><select id="s0"></select><select id="s1"></select><select id="s2"></select></div>
<label>Scroll direction</label><select name="scrollDir"><option value="0">Up</option><option value="1">Down</option><option value="2">Left</option><option value="3">Right</option></select>
</section>

<section><h2>Display</h2>
<label>Brightness</label><select name="brightness"><option value="100">Full</option><option value="1">Dim</option></select>
<div class="check"><input type="checkbox" name="flip">Upside down</div>
<div class="check"><input type="checkbox" name="pixelShift">Move the picture a pixel now and then <span class="hint">(slows burn-in)</span></div>
<label>At night</label><select id="nightMode"><option value="0">No change</option><option value="1">Dim</option><option value="2">Screen off (a small dot blinks)</option></select>
<div class="row"><div><label>From</label><input type="time" id="ns"></div><div><label>Until</label><input type="time" id="ne"></div></div>
<div class="buttons"><button type="button" onclick="preview()">Try the night setting (10 s)</button></div>
<p class="hint">With the screen off at night, a button press wakes it for 30 seconds.</p>
</section>

<section><h2>Advanced</h2>
<label>Device name <span class="hint">hotspot and network name; restarts</span></label><input type="text" name="deviceName" maxlength="32">
<label>Password <span class="hint">for this page (user "admin") and updates; restarts. Leave empty to keep it, enter "-" to remove it</span></label><input type="password" name="password" maxlength="32" autocomplete="new-password">
<label>Weather update every (minutes)</label><input type="number" name="updateMins" min="5" max="120">
<label>Time server</label><input type="text" name="ntpServer" maxlength="47">
<label>Time zone rule <span class="hint">leave empty to follow the weather location (recommended). Otherwise a POSIX rule, e.g. MST7MDT,M3.2.0,M11.1.0</span></label><input type="text" name="tzOverride" maxlength="47">
</section>
<button class="primary" style="width:100%;padding:12px">Save settings</button>
</form>

<section style="margin-top:14px"><h2>Wi-Fi and maintenance</h2>
<div class="buttons">
<button onclick="act('update','Updating the weather...')">Update weather now</button>
<button onclick="if(confirm('Open the setup hotspot to choose a Wi-Fi network? The station leaves your network until it is set up (or 3 minutes pass).'))act('wifi','Setup hotspot opening. Connect to it to choose the network.')">Change Wi-Fi network</button>
<button onclick="act('restart','Restarting...')">Restart</button>
<button class="danger" onclick="if(confirm('Erase all settings and the Wi-Fi network? The station starts over with the setup hotspot.'))act('reset','Erased. Restarting with the setup hotspot.')">Factory reset</button>
</div></section>
<div id="msg"></div>
</main>
<script>
const $=id=>document.getElementById(id),F=$('f');
const SCR={T:'Time',C:'Current weather',F:'Forecast'};
function msg(t,ms){$('msg').textContent=t;$('msg').style.display='block';if(ms)setTimeout(()=>$('msg').style.display='none',ms)}
function hm(m){return String(Math.floor(m/60)).padStart(2,'0')+':'+String(m%60).padStart(2,'0')}
function mins(v){const p=(v||'0:0').split(':');return p[0]*60+ +p[1]}
async function post(url,data){const r=await fetch(url,{method:'POST',body:new URLSearchParams(data)});return r.json()}
async function status(){
 const s=await (await fetch('/api/status')).json();
 $('title').textContent=s.name;
 clk={secs:s.secs,at:performance.now(),h24:s.clock24};
 const rows=[['Time','<span id="clk">'+s.time+'</span>'],['Weather',s.weather],['Updated',s.updated],['Wi-Fi',s.ssid+' ('+s.rssi+' dBm)'],['Address','http://'+s.host+'.local or http://'+s.ip],['Up',s.uptime]];
 $('status').innerHTML=rows.map(r=>'<tr><td>'+r[0]+'</td><td>'+r[1]+'</td></tr>').join('');
}
async function load(){
 const c=await (await fetch('/api/settings')).json();
 $('town').textContent=c.haveLocation?c.town:'No location set yet';
 $('lat').value=c.haveLocation?c.lat:'';$('lon').value=c.haveLocation?c.lon:'';
 for(const k of ['metric','clock24','dateFormat','scrollDir'])F[k].value=+c[k];
 for(const k of ['showSeconds','flip','pixelShift'])F[k].checked=c[k];
 F.brightness.value=c.brightness<50?'1':'100';
 $('nightMode').value=!c.nightOn?'0':(c.nightBrightness==0?'2':'1');
 for(const k of ['screenSecs','deviceName','updateMins','ntpServer','tzOverride'])F[k].value=c[k];
 F.password.placeholder=c.passwordSet?'(set)':'(none)';
 $('ns').value=hm(c.nightStart);$('ne').value=hm(c.nightEnd);
 for(let i=0;i<3;i++){const s=$('s'+i);s.innerHTML='<option value="">none</option>'+Object.keys(SCR).map(k=>'<option value="'+k+'">'+SCR[k]+'</option>').join('');s.value=c.screens[i]||''}
}
let clk=null;
function tickClock(){
 if(!clk||!$('clk'))return;
 const t=Math.floor(clk.secs+(performance.now()-clk.at)/1000)%86400,h=Math.floor(t/3600),m=Math.floor(t/60)%60,s=t%60,p=n=>String(n).padStart(2,'0');
 $('clk').textContent=clk.h24?p(h)+':'+p(m)+':'+p(s):p((h+11)%12+1)+':'+p(m)+':'+p(s)+(h<12?' AM':' PM');
}
setInterval(tickClock,250);
async function save(){
 const d=new FormData(F),o={};
 for(const [k,v] of d.entries())o[k]=v;
 for(const k of ['showSeconds','flip','pixelShift'])o[k]=F[k].checked?1:0;
 const nm=$('nightMode').value;o.nightOn=nm>0?1:0;if(nm>0)o.nightBrightness=nm==2?0:1;
 o.screens=[0,1,2].map(i=>$('s'+i).value).join('');
 o.nightStart=mins($('ns').value);o.nightEnd=mins($('ne').value);
 if(!o.password)delete o.password;
 const r=await post('/api/settings',o);
 msg(r.restart?'Saved. Restarting... reload this page in 20 seconds.':'Saved.',r.restart?0:2500);
 if(!r.restart){load();setTimeout(status,3000)}
}
async function search(){
 const q=$('q').value.trim();if(!q)return;
 $('results').textContent='Searching...';
 const r=await (await fetch('/api/search?q='+encodeURIComponent(q))).json();
 if(!r.length){$('results').textContent='Nothing found. Try just the town name.';return}
 $('results').innerHTML='';
 r.forEach(p=>{const b=document.createElement('button');b.textContent=p.label;b.onclick=()=>setPlace(p.label,p.lat,p.lon);$('results').appendChild(b)});
}
async function setPlace(label,lat,lon){
 await post('/api/settings',{town:label,lat:lat,lon:lon});
 $('results').innerHTML='';$('q').value='';msg('Location saved. Updating the weather...',3000);
 load();setTimeout(status,5000);
}
function setLatLon(){const a=parseFloat($('lat').value),b=parseFloat($('lon').value);if(isNaN(a)||isNaN(b))return;setPlace(a.toFixed(4)+', '+b.toFixed(4),a,b)}
async function preview(){const nm=$('nightMode').value;if(nm==0){msg('Pick Dim or Screen off first',2500);return}await post('/api/action',{do:'preview',level:nm==2?0:1});msg(nm==2?'Screen off for 10 seconds (watch for the dot)':'Dim for 10 seconds',10000)}
async function act(what,text){await post('/api/action',{do:what});msg(text)}
load();status();setInterval(status,10000);
</script></body></html>)HTML";

bool checkAuth() {
  if (cfg.password[0] == 0) return true;
  if (server.authenticate("admin", cfg.password)) return true;
  server.requestAuthentication(BASIC_AUTH, "Weather Station");
  return false;
}

String jsonEscape(const String &s) {
  String out;
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if ((uint8_t)c < 0x20) out += ' ';
    else out += c;
  }
  return out;
}

void handleStatus() {
  if (!checkAuth()) return;
  char buff[40];
  unsigned long mins = millis() / 60000;
  snprintf(buff, sizeof(buff), "%lud %luh %lum", mins / 1440, (mins / 60) % 24, mins % 60);
  String updated = "not yet";
  if (lastWeatherUpdate) {
    long ago = (time(nullptr) - lastWeatherUpdate) / 60;
    updated = ago < 1 ? String("just now") : String(ago) + " min ago";
  }
  String weather = currentWeather.valid
      ? String(currentWeather.temp, 1) + (cfg.metric ? " °C, " : " °F, ") + weatherText(currentWeather.code)
      : String(cfg.haveLocation ? "waiting" : "set a location below");
  String json = "{\"name\":\"" + jsonEscape(cfg.deviceName) + "\",\"host\":\"" + hostName() +
                "\",\"ip\":\"" + WiFi.localIP().toString() + "\",\"ssid\":\"" + jsonEscape(WiFi.SSID()) +
                "\",\"rssi\":" + String(WiFi.RSSI()) + ",\"uptime\":\"" + buff +
                "\",\"time\":\"" + timeString(true, true) + "\",\"weather\":\"" + jsonEscape(weather) +
                "\",\"updated\":\"" + updated + "\",\"secs\":" + String(localSeconds()) +
                ",\"clock24\":" + (cfg.clock24 ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleGetSettings() {
  if (!checkAuth()) return;
  DynamicJsonBuffer jb(1024);
  JsonObject& o = jb.createObject();
  o["deviceName"] = cfg.deviceName;
  o["passwordSet"] = cfg.password[0] != 0;
  o["town"] = cfg.town;
  o["lat"] = cfg.lat;
  o["lon"] = cfg.lon;
  o["haveLocation"] = cfg.haveLocation;
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
  String json;
  o.printTo(json);
  server.send(200, "application/json", json);
}

void handlePostSettings() {
  if (!checkAuth()) return;
  bool restart = false;
  bool newLocation = false;
  bool newUnits = false;

  if (server.hasArg("lat") && server.hasArg("lon")) {
    cfg.lat = server.arg("lat").toFloat();
    cfg.lon = server.arg("lon").toFloat();
    cfg.haveLocation = true;
    strlcpy(cfg.town, server.arg("town").c_str(), sizeof(cfg.town));
    newLocation = true;
  }
  if (server.hasArg("deviceName") && server.arg("deviceName") != cfg.deviceName) {
    strlcpy(cfg.deviceName, server.arg("deviceName").c_str(), sizeof(cfg.deviceName));
    restart = true;
  }
  if (server.hasArg("password")) {
    String p = server.arg("password");
    strlcpy(cfg.password, p == "-" ? "" : p.c_str(), sizeof(cfg.password));
    restart = true;
  }
  if (server.hasArg("metric")) {
    bool m = server.arg("metric") == "1";
    newUnits = m != cfg.metric;
    cfg.metric = m;
  }
  if (server.hasArg("clock24")) cfg.clock24 = server.arg("clock24") == "1";
  if (server.hasArg("showSeconds")) cfg.showSeconds = server.arg("showSeconds") == "1";
  if (server.hasArg("dateFormat")) cfg.dateFormat = server.arg("dateFormat").toInt();
  if (server.hasArg("screenSecs")) cfg.screenSecs = server.arg("screenSecs").toInt();
  if (server.hasArg("scrollDir")) cfg.scrollDir = server.arg("scrollDir").toInt();
  if (server.hasArg("screens")) strlcpy(cfg.screens, server.arg("screens").c_str(), sizeof(cfg.screens));
  if (server.hasArg("brightness")) cfg.brightness = server.arg("brightness").toInt();
  if (server.hasArg("flip")) cfg.flip = server.arg("flip") == "1";
  if (server.hasArg("pixelShift")) cfg.pixelShift = server.arg("pixelShift") == "1";
  if (server.hasArg("nightOn")) cfg.nightOn = server.arg("nightOn") == "1";
  if (server.hasArg("nightStart")) cfg.nightStart = server.arg("nightStart").toInt();
  if (server.hasArg("nightEnd")) cfg.nightEnd = server.arg("nightEnd").toInt();
  if (server.hasArg("nightBrightness")) cfg.nightBrightness = server.arg("nightBrightness").toInt();
  if (server.hasArg("updateMins")) cfg.updateMins = server.arg("updateMins").toInt();
  if (server.hasArg("ntpServer") && server.arg("ntpServer") != cfg.ntpServer) {
    strlcpy(cfg.ntpServer, server.arg("ntpServer").c_str(), sizeof(cfg.ntpServer));
    restart = true;
  }
  if (server.hasArg("tzOverride")) strlcpy(cfg.tzOverride, server.arg("tzOverride").c_str(), sizeof(cfg.tzOverride));

  checkSettings();
  saveSettings();
  applySettings();
  if (newLocation || newUnits) weatherUpdateRequested = true;
  if (restart) restartRequested = true;
  server.send(200, "application/json", restart ? "{\"ok\":true,\"restart\":true}" : "{\"ok\":true,\"restart\":false}");
}

void handleSearch() {
  if (!checkAuth()) return;
  Place places[MAX_PLACES];
  uint8_t n = searchPlaces(server.arg("q"), places);
  String json = "[";
  for (uint8_t i = 0; i < n; i++) {
    if (i) json += ",";
    json += "{\"label\":\"" + jsonEscape(places[i].label) + "\",\"lat\":" + String(places[i].lat, 4) +
            ",\"lon\":" + String(places[i].lon, 4) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleAction() {
  if (!checkAuth()) return;
  String what = server.arg("do");
  if (what == "update") weatherUpdateRequested = true;
  else if (what == "wifi") wifiSetupRequested = true;
  else if (what == "restart") restartRequested = true;
  else if (what == "reset") factoryResetRequested = true;
  else if (what == "preview") {
    previewLevel = constrain(server.arg("level").toInt(), 0, 100);
    previewUntil = millis() + 10000;
  }
  else { server.send(400, "application/json", "{\"ok\":false}"); return; }
  server.send(200, "application/json", "{\"ok\":true}");
}

void startWebUI() {
  server.on("/", HTTP_GET, []() {
    if (!checkAuth()) return;
    server.send_P(200, "text/html", PAGE);
  });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/settings", HTTP_GET, handleGetSettings);
  server.on("/api/settings", HTTP_POST, handlePostSettings);
  server.on("/api/search", HTTP_GET, handleSearch);
  server.on("/api/action", HTTP_POST, handleAction);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
}
