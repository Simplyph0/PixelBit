#include "savesync.h"
#include "display.h"
#include "highscore.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

static ESP8266WebServer server(80);
static bool running = false;
static bool routesAdded = false;
static unsigned long startedAt = 0;
static unsigned long lastDrawAt = 0;
static char ssid[9];        // 8 random characters
static char pass[9];        // 8 random characters

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>Game save</title>
<body style="font-family:sans-serif;max-width:480px;margin:1em auto;padding:0 1em">
<h2>Game save transfer</h2>
<h3>Export</h3>
<button onclick="location='/export'">Download save</button>
<h3>Import</h3>
<input type=file onchange="var f=this.files[0];f&&f.text().then(function(t){d.value=t})"><br>
<textarea id=d rows=4 style="width:100%" placeholder="GSAV1-..."></textarea><br>
<label><input type=checkbox id=m checked> Keep the best score per game (untick to overwrite)</label><br>
<button onclick=imp()>Import</button>
<h3>Factory reset</h3>
<p>Erases every high score on the console. Download a save first if you want a backup.</p>
<input id=r placeholder="type RESET to confirm" autocomplete=off>
<button onclick=rst()>Erase all scores</button>
<pre id=o></pre>
<script>
function rst(){
if(r.value!=='RESET'){o.textContent='Type RESET in the box first';return}
if(!confirm('Erase ALL saved scores? This cannot be undone.'))return;
fetch('/reset',{method:'POST'}).then(function(x){return x.text()}).then(function(t){o.textContent=t;r.value=''})}
function imp(){
fetch('/import',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
body:'mode='+(m.checked?'merge':'replace')+'&data='+encodeURIComponent(d.value)})
.then(function(r){return r.text()}).then(function(t){o.textContent=t})}
</script>
)HTML";

// Letters and digits, minus the look-alikes (0/O, 1/I)
static void randomString(char* out, int len) {
  static const char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for (int i = 0; i < len; i++) out[i] = ALPHABET[ESP.random() % (sizeof(ALPHABET) - 1)];
  out[len] = 0;
}

static void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

static void handleExport() {
  server.sendHeader("Content-Disposition", "attachment; filename=gamesave.gsv");
  server.send(200, "text/plain", highscoreManager.exportText());
}

static void handleImport() {
  const char* err = highscoreManager.importText(server.arg("data"), server.arg("mode") != "replace");
  server.send(err ? 400 : 200, "text/plain", err ? String("Import failed: ") + err : String("Save imported"));
}

static void handleReset() {
  highscoreManager.resetAllHighscores();
  LOG("sync: all highscores erased from the web page\n");
  server.send(200, "text/plain", "All scores erased");
}

// Boot-time setup: the radio stays off until the save link is opened, and the
// Wi-Fi settings are never written to flash (every mode change would wear it).
void syncInit() {
  WiFi.persistent(false);
  WiFi.mode(WIFI_OFF);
}

void syncBegin() {
  randomString(ssid, 8);
  randomString(pass, 8);

  startedAt = millis();
  lastDrawAt = 0;

  WiFi.mode(WIFI_AP);
  // WPA2, one connection at a time; hidden or visible per SYNC_HIDDEN_SSID
  WiFi.softAP(ssid, pass, 1 + (ESP.random() % 11), SYNC_HIDDEN_SSID, 1);

  if (!routesAdded) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/export", HTTP_GET, handleExport);
    server.on("/import", HTTP_POST, handleImport);
    server.on("/reset", HTTP_POST, handleReset);
    routesAdded = true;
  }
  server.begin();
  running = true;
  LOG("sync: AP started, SSID %s, password %s\n", ssid, pass);
}

void syncEnd() {
  if (!running) return;
  LOG("sync: AP stopped\n");
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  running = false;
  // Forget the credentials
  memset(ssid, 0, sizeof(ssid));
  memset(pass, 0, sizeof(pass));
}

bool syncUpdate() {
  if (!running) return false;
  server.handleClient();
  return true;   // no time limit: ends only with the exit combo
}

void syncDraw() {
  if (millis() - lastDrawAt < 500 && lastDrawAt != 0) return;
  lastDrawAt = millis();

  unsigned long up = (millis() - startedAt) / 1000;
  char line[24];

  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  snprintf(line, sizeof(line), "SAVE LINK     %lu:%02lu", up / 60, up % 60);
  display.setCursor(0, 0);   display.print(line);
  snprintf(line, sizeof(line), "SSID %s", ssid);
  display.setCursor(0, 11);  display.print(line);
  snprintf(line, sizeof(line), "PASS %s", pass);
  display.setCursor(0, 22);  display.print(line);
  display.setCursor(0, 33);  display.print(F("http://192.168.4.1"));
  display.setCursor(0, 44);
  display.print(WiFi.softAPgetStationNum() > 0 ? F("Device: CONNECTED") : F("Device: not joined"));
  display.setCursor(0, 55);  display.print(F("Double-tap UP: stop"));

  updateDisplay();
}
