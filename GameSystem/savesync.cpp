#include "savesync.h"
#include "display.h"
#include "highscore.h"
#include "chat.h"
#include "contacts.h"
#include "gamelink.h"
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
<h3>Chat contacts</h3>
<p>Give another console's id (the 5 letters on its CHAT screen) a name that shows up in chat. Up to 8; names are 1 to 10 characters: letters, digits, space and - _ . ! ? + *</p>
<p>This console: <b id=me></b></p>
<ul id=cl></ul>
<input id=ci placeholder="id, e.g. NVDKH" size=12 maxlength=5 autocomplete=off>
<input id=ca placeholder="name" size=12 maxlength=10 autocomplete=off>
<button onclick=addc()>Save contact</button>
<h3>Factory reset</h3>
<p>Erases every high score on the console. Download a save first if you want a backup.</p>
<input id=r placeholder="type RESET to confirm" autocomplete=off>
<button onclick=rst()>Erase all scores</button>
<pre id=o></pre>
<script>
function post(u,b){return fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b}).then(function(r){return r.text()})}
function loadc(){fetch('/contacts').then(function(r){return r.json()}).then(function(j){
me.textContent=j.me;cl.innerHTML='';
j.contacts.forEach(function(c){var li=document.createElement('li');li.textContent=c.id+'  '+c.alias+'  ';
var b=document.createElement('button');b.textContent='remove';b.onclick=function(){post('/contactdel','id='+c.id).then(function(t){o.textContent=t;loadc()})};
li.appendChild(b);cl.appendChild(li)});
if(!j.contacts.length)cl.textContent='No contacts yet.'})}
function addc(){post('/contact','id='+encodeURIComponent(ci.value)+'&alias='+encodeURIComponent(ca.value)).then(function(t){o.textContent=t;ci.value='';ca.value='';loadc()})}
loadc();
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

// Chat contacts (contacts.h). The answers are plain text; ids and aliases only hold characters
// that need no escaping (see contactCheckAlias()).
static void handleContacts() {
  char me[LINK_ID_LEN + 1];
  linkOwnId(me);
  String out = String("{\"me\":\"") + me + "\",\"contacts\":[";
  for (int i = 0; i < MAX_CONTACTS; i++) {
    uint8_t id[3];
    const char* alias;
    if (!contactAt(i, id, &alias)) break;
    char text[LINK_ID_LEN + 1];
    linkIdToText(id, text);
    if (i) out += ",";
    out += String("{\"id\":\"") + text + "\",\"alias\":\"" + alias + "\"}";
  }
  out += "]}";
  server.send(200, "application/json", out);
}

static void handleContactSet() {
  uint8_t id[3];
  if (!linkIdFromText(server.arg("id").c_str(), id)) {
    server.send(400, "text/plain", "Not a console id: it has 5 letters and digits (no 0, 1, I or O)");
    return;
  }
  char alias[48];
  strncpy(alias, server.arg("alias").c_str(), sizeof(alias) - 1);
  alias[sizeof(alias) - 1] = 0;
  const char* err = contactSet(id, alias);
  server.send(err ? 400 : 200, "text/plain", err ? String(err) : String("Saved ") + alias);
}

static void handleContactDelete() {
  uint8_t id[3];
  bool ok = linkIdFromText(server.arg("id").c_str(), id) && contactRemove(id);
  server.send(ok ? 200 : 400, "text/plain", ok ? "Removed" : "No such contact");
}

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

  chatReleaseRadio();   // chat may have the radio open: the access point needs it
  WiFi.mode(WIFI_AP);
  // WPA2, one connection at a time; hidden or visible per SYNC_HIDDEN_SSID
  WiFi.softAP(ssid, pass, 1 + (ESP.random() % 11), SYNC_HIDDEN_SSID, 1);

  if (!routesAdded) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/export", HTTP_GET, handleExport);
    server.on("/import", HTTP_POST, handleImport);
    server.on("/reset", HTTP_POST, handleReset);
    server.on("/contacts", HTTP_GET, handleContacts);
    server.on("/contact", HTTP_POST, handleContactSet);
    server.on("/contactdel", HTTP_POST, handleContactDelete);
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

bool syncDraw() {
  if (millis() - lastDrawAt < 500 && lastDrawAt != 0) return false;
  lastDrawAt = millis();

  unsigned long up = (millis() - startedAt) / 1000;
  char line[24];

  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  snprintf(line, sizeof(line), "WI-FI PAGE    %lu:%02lu", up / 60, up % 60);
  display.setCursor(0, 0);   display.print(line);
  snprintf(line, sizeof(line), "SSID %s", ssid);
  display.setCursor(0, 11);  display.print(line);
  snprintf(line, sizeof(line), "PASS %s", pass);
  display.setCursor(0, 22);  display.print(line);
  display.setCursor(0, 33);  display.print(F("http://192.168.4.1"));
  display.setCursor(0, 44);
  display.print(WiFi.softAPgetStationNum() > 0 ? F("Device: CONNECTED") : F("Device: not joined"));
  display.setCursor(0, 55);  display.print(F("LEFT: stop"));
  return true;
}
