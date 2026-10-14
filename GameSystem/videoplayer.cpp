#include "videoplayer.h"
#include "display.h"
#include "input.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include "demo_clip.h"

VideoPlayer videoGame;

// ---------------------------------------------------------------------------
// Storage
// ---------------------------------------------------------------------------
static bool fsReady = false;

static bool goodNameChar(char c) {
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '-';
}

static bool goodName(const char* n) {
  size_t len = strlen(n);
  if (len == 0 || len > CLIP_NAME_LEN - 1) return false;
  for (size_t i = 0; i < len; i++) if (!goodNameChar(n[i])) return false;
  return true;
}

// The clip built into the firmware goes into the file system once (not again
// after someone deletes it)
static void seedDemo() {
  if (LittleFS.exists("/seeded")) return;
  const char* path = CLIP_DIR "/" DEMO_CLIP_NAME ".pbv";
  File f = LittleFS.open(path, "w");
  if (!f) return;
  bool ok = true;
  uint8_t buf[128];
  for (uint32_t off = 0; off < DEMO_CLIP_SIZE && ok; off += sizeof(buf)) {
    uint32_t n = DEMO_CLIP_SIZE - off;
    if (n > sizeof(buf)) n = sizeof(buf);
    for (uint32_t k = 0; k < n; k++) buf[k] = pgm_read_byte(DEMO_CLIP + off + k);
    ok = f.write(buf, n) == n;
  }
  f.close();
  if (!ok) { LittleFS.remove(path); return; }
  File m = LittleFS.open("/seeded", "w");
  if (m) { m.write((const uint8_t*)"1", 1); m.close(); }
}

static bool mountFs() {
  if (fsReady) return true;
  fsReady = LittleFS.begin();
  if (fsReady) {
    LittleFS.mkdir(CLIP_DIR);
    seedDemo();
  }
  return fsReady;
}

static uint32_t freeBytes() {
  fs::FSInfo info;
  if (!LittleFS.info(info) || info.totalBytes <= info.usedBytes) return 0;
  uint32_t free = info.totalBytes - info.usedBytes;
  return free > 32768 ? free - 32768 : 0;   // room for the file system's own bookkeeping and the upload's temporary copy
}

// "NAME.pbv" -> "NAME"; false for anything else (including the upload's temporary file)
static bool clipNameOf(const String& file, char* out) {
  size_t len = file.length();
  if (len < 5 || strcmp(file.c_str() + len - 4, ".pbv") != 0) return false;
  if (len - 4 > CLIP_NAME_LEN - 1) return false;
  memcpy(out, file.c_str(), len - 4);
  out[len - 4] = 0;
  return goodName(out);
}

static int countClips() {
  int n = 0;
  char name[CLIP_NAME_LEN];
  Dir d = LittleFS.openDir(CLIP_DIR);
  while (d.next()) if (clipNameOf(d.fileName(), name)) n++;
  return n;
}

void VideoPlayer::loadClips() {
  clipCount = 0;
  Dir d = LittleFS.openDir(CLIP_DIR);
  while (d.next() && clipCount < MAX_CLIPS) {
    if (clipNameOf(d.fileName(), names[clipCount])) clipCount++;
  }
  for (uint8_t i = 1; i < clipCount; i++) {          // alphabetical
    char tmp[CLIP_NAME_LEN];
    memcpy(tmp, names[i], CLIP_NAME_LEN);
    int j = i - 1;
    while (j >= 0 && strcmp(names[j], tmp) > 0) { memcpy(names[j + 1], names[j], CLIP_NAME_LEN); j--; }
    memcpy(names[j + 1], tmp, CLIP_NAME_LEN);
  }
  if (sel > clipCount) sel = clipCount;
}

// ---------------------------------------------------------------------------
// Upload page
// ---------------------------------------------------------------------------
static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>PixelBit videos</title>
<body style="font-family:sans-serif;max-width:480px;margin:1em auto;padding:0 1em">
<h2>PixelBit videos</h2>
<p id=free></p>
<ul id=list></ul>
<h3>Add a video</h3>
<p>A <b>.pbv</b> file made with <code>tools/video/pbv.py</code>. A console keeps up to 16.</p>
<input type=file id=f accept=".pbv"> <button onclick=up()>Upload</button>
<pre id=o></pre>
<script>
var free=0;
function load(){fetch('/clips').then(function(r){return r.json()}).then(function(j){
free=j.free;document.getElementById('free').textContent=Math.round(j.free/1024)+' KB free';
var l=document.getElementById('list');l.innerHTML='';
j.clips.forEach(function(c){var li=document.createElement('li');
li.textContent=c.n+' ('+Math.round(c.s/1024)+' KB) ';
var b=document.createElement('button');b.textContent='Delete';b.onclick=function(){del(c.n)};
li.appendChild(b);l.appendChild(li)});
if(!j.clips.length)l.textContent='No videos yet.'})}
function del(n){if(!confirm('Delete '+n+'?'))return;
fetch('/delete?name='+encodeURIComponent(n),{method:'POST'}).then(function(r){return r.text()})
.then(function(t){o.textContent=t;load()})}
function up(){var f=document.getElementById('f').files[0];
if(!f){o.textContent='Pick a file first';return}
if(f.size>free){o.textContent='Too big: '+Math.round(f.size/1024)+' KB, only '+Math.round(free/1024)+' KB free';return}
o.textContent='Uploading, the console screen freezes until it is done...';
var d=new FormData();d.append('file',f,f.name);
fetch('/upload',{method:'POST',body:d}).then(function(r){return r.text()})
.then(function(t){o.textContent=t;load()}).catch(function(){o.textContent='Upload failed'})}
load();
</script>
)HTML";

static ESP8266WebServer server(80);
static bool routesAdded = false;
static char apSsid[9], apPass[9];

static File upFile;
static bool upFailed = false;
static char upMsg[48];
static char upName[CLIP_NAME_LEN];

#define UPLOAD_TMP CLIP_DIR "/_up.tmp"

static void randomString(char* out, int len) {
  static const char ALPHABET[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  for (int i = 0; i < len; i++) out[i] = ALPHABET[ESP.random() % (sizeof(ALPHABET) - 1)];
  out[len] = 0;
}

static void upFail(const char* msg) {
  upFailed = true;
  snprintf(upMsg, sizeof(upMsg), "%s", msg);
  if (upFile) upFile.close();
  LittleFS.remove(UPLOAD_TMP);
}

// The file name from the browser, cut down to what the list can show
static void cleanName(const String& original, char* out) {
  const char* s = original.c_str();
  const char* cut = strrchr(s, '/');
  if (cut) s = cut + 1;
  cut = strrchr(s, '\\');
  if (cut) s = cut + 1;
  size_t n = 0;
  for (; *s && *s != '.' && n < CLIP_NAME_LEN - 1; s++) {
    if (goodNameChar(*s)) out[n++] = *s;
    else if (*s == ' ') out[n++] = '_';
  }
  if (n == 0) { strcpy(out, "CLIP"); return; }
  out[n] = 0;
}

static void handleRoot() { server.send_P(200, "text/html", PAGE); }

static void handleClips() {
  char json[40 + MAX_CLIPS * 40];           // on the stack: it is only needed while this runs
  int n = snprintf(json, sizeof(json), "{\"free\":%lu,\"clips\":[", (unsigned long)freeBytes());
  char name[CLIP_NAME_LEN];
  bool first = true;
  Dir d = LittleFS.openDir(CLIP_DIR);
  while (d.next() && n < (int)sizeof(json) - 40) {
    if (!clipNameOf(d.fileName(), name)) continue;
    n += snprintf(json + n, sizeof(json) - n, "%s{\"n\":\"%s\",\"s\":%lu}", first ? "" : ",", name, (unsigned long)d.fileSize());
    first = false;
  }
  snprintf(json + n, sizeof(json) - n, "]}");
  server.send(200, "application/json", json);
}

static void handleDelete() {
  String name = server.arg("name");
  if (!goodName(name.c_str())) { server.send(400, "text/plain", "No such video"); return; }
  char path[40];
  snprintf(path, sizeof(path), CLIP_DIR "/%s.pbv", name.c_str());
  bool removed = LittleFS.remove(path);
  server.send(removed ? 200 : 404, "text/plain", removed ? "Deleted" : "Could not delete");
}

static void handleUpload() {
  HTTPUpload& u = server.upload();
  if (u.status == UPLOAD_FILE_START) {
    upFailed = false;
    upMsg[0] = 0;
    if (upFile) upFile.close();
    cleanName(u.filename, upName);
    char dest[40];
    snprintf(dest, sizeof(dest), CLIP_DIR "/%s.pbv", upName);
    if (!mountFs()) { upFail("The console's storage is not working"); return; }
    if (!LittleFS.exists(dest) && countClips() >= MAX_CLIPS) { upFail("Too many videos (16). Delete one first."); return; }
    upFile = LittleFS.open(UPLOAD_TMP, "w");
    if (!upFile) upFail("Could not write to the console's storage");
  } else if (u.status == UPLOAD_FILE_WRITE) {
    if (upFailed || !upFile) return;
    if (upFile.write(u.buf, u.currentSize) != u.currentSize) upFail("The console's storage is full");
  } else if (u.status == UPLOAD_FILE_END) {
    if (upFailed || !upFile) return;
    upFile.close();
    // Only a real .pbv is kept
    File check = LittleFS.open(UPLOAD_TMP, "r");
    uint8_t head[PBV_HEADER_SIZE];
    PbvHeader h;
    bool ok = check && check.read(head, sizeof(head)) == sizeof(head) && pbvParseHeader(head, sizeof(head), h) &&
              check.size() >= h.dataOffset() + 2;
    if (check) check.close();
    if (!ok) { upFail("That is not a valid .pbv video"); return; }
    char dest[40];
    snprintf(dest, sizeof(dest), CLIP_DIR "/%s.pbv", upName);
    LittleFS.remove(dest);
    if (!LittleFS.rename(UPLOAD_TMP, dest)) { upFail("Could not save the video"); return; }
    snprintf(upMsg, sizeof(upMsg), "Saved %s", upName);
  } else if (u.status == UPLOAD_FILE_ABORTED) {
    upFail("Upload cancelled");
  }
}

static void handleUploadDone() {
  server.send(upFailed || !upMsg[0] ? 400 : 200, "text/plain", upMsg[0] ? upMsg : "Upload failed");
}

void VideoPlayer::startUpload() {
  randomString(apSsid, 8);
  randomString(apPass, 8);
  uploadStartedAt = millis();
  upMsg[0] = 0;
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPass, 1 + (ESP.random() % 11), 0, 1);   // WPA2, one device at a time
  if (!routesAdded) {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/clips", HTTP_GET, handleClips);
    server.on("/delete", HTTP_POST, handleDelete);
    server.on("/upload", HTTP_POST, handleUploadDone, handleUpload);
    routesAdded = true;
  }
  server.begin();
  mode = MODE_UPLOAD;
}

void VideoPlayer::stopUpload() {
  if (mode != MODE_UPLOAD) return;
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  if (upFile) { upFile.close(); LittleFS.remove(UPLOAD_TMP); }
  memset(apSsid, 0, sizeof(apSsid));
  memset(apPass, 0, sizeof(apPass));
}

// ---------------------------------------------------------------------------
// Playback
// ---------------------------------------------------------------------------
void VideoPlayer::closeClip() {
  if (file) file.close();
  free(frame);
  frame = payload = nullptr;
}

bool VideoPlayer::decodeNext() {
  if (decoded >= hdr.frames) return false;
  uint8_t lb[2];
  if (file.read(lb, 2) != 2) return false;
  uint16_t len = lb[0] | (lb[1] << 8);
  if (len > PBV_MAX_PAYLOAD || file.read(payload, len) != len) return false;
  if (!pbvDecodeFrame(payload, len, frame, hdr.frameBytes, decoded % hdr.keyEvery != 0)) return false;
  decoded++;
  return true;
}

// Opens a clip and decodes its first frame. On failure the list shows why.
bool VideoPlayer::openClip(uint8_t index) {
  closeClip();
  char path[40];
  snprintf(path, sizeof(path), CLIP_DIR "/%s.pbv", names[index]);
  file = LittleFS.open(path, "r");
  uint8_t head[PBV_HEADER_SIZE];
  if (!file || file.read(head, sizeof(head)) != sizeof(head) || !pbvParseHeader(head, sizeof(head), hdr) ||
      file.size() < hdr.dataOffset() + 2 || !file.seek(hdr.dataOffset())) {
    closeClip();
    notice = "BAD OR DAMAGED FILE";
    noticeUntil = millis() + 2500;
    return false;
  }
  frame = (uint8_t*)calloc(1, PBV_MAX_FRAME_BYTES + PBV_MAX_PAYLOAD);   // zeroed: the first key frame needs nothing before it
  if (!frame) {
    closeClip();
    notice = "OUT OF MEMORY";
    noticeUntil = millis() + 2500;
    return false;
  }
  payload = frame + PBV_MAX_FRAME_BYTES;
  decoded = 0;
  paused = false;
  barUntil = 0;
  frameMs = 1000 / hdr.fps;
  if (!decodeNext()) {
    closeClip();
    notice = "BAD OR DAMAGED FILE";
    noticeUntil = millis() + 2500;
    return false;
  }
  nextAt = millis() + frameMs;
  mode = MODE_PLAY;
  return true;
}

void VideoPlayer::endClip() {
  closeClip();
  mode = MODE_LIST;
}

// Jumps to the key frame at or before the target and decodes forward from there
void VideoPlayer::seekBy(int seconds) {
  long target = (long)decoded - 1 + (long)seconds * hdr.fps;
  if (target < 0) target = 0;
  if (target > (long)hdr.frames - 1) target = hdr.frames - 1;
  uint16_t key = target / hdr.keyEvery;
  uint8_t eb[4];
  if (!file.seek(PBV_HEADER_SIZE + 4UL * key) || file.read(eb, 4) != 4) { endClip(); return; }
  uint32_t off = eb[0] | (eb[1] << 8) | ((uint32_t)eb[2] << 16) | ((uint32_t)eb[3] << 24);
  if (off < hdr.dataOffset() || off >= file.size() || !file.seek(off)) { endClip(); return; }
  decoded = key * hdr.keyEvery;
  while ((long)decoded <= target) {
    if (!decodeNext()) { endClip(); return; }
  }
  nextAt = millis() + frameMs;
  barUntil = millis() + 1200;
}

// ---------------------------------------------------------------------------
// Game interface
// ---------------------------------------------------------------------------
void VideoPlayer::init() {
  exitRequested = false;
  notice = nullptr;
  sel = 0;
  mode = MODE_LIST;
  if (!mountFs()) { mode = MODE_ERROR; return; }
  loadClips();
}

void VideoPlayer::stop() {
  closeClip();
  stopUpload();
}

void VideoPlayer::update() {
  switch (mode) {
    case MODE_LIST: {
      int total = clipCount + 1;                       // the clips, then the upload page
      if (buttons.leftPressed) sel = (sel + total - 1) % total;
      else if (buttons.rightPressed) sel = (sel + 1) % total;
      else if (buttons.upPressed) {
        if (sel == clipCount) startUpload();
        else openClip(sel);
      } else if (buttons.downPressed) {
        exitRequested = true;
      }
      break;
    }

    case MODE_PLAY: {
      if (buttons.downPressed) { endClip(); break; }
      if (buttons.upPressed) {
        paused = !paused;
        if (!paused) nextAt = millis() + frameMs;
        barUntil = millis() + 1200;
      }
      if (buttons.leftPressed) { seekBy(-CLIP_SEEK_SECONDS); break; }
      if (buttons.rightPressed) { seekBy(CLIP_SEEK_SECONDS); break; }
      if (paused) break;

      // Show the frames that are due. A little behind is caught up by skipping
      // ahead; a lot behind is forgiven.
      if ((long)(millis() - nextAt) >= 0) {
        for (int n = 0; n < 4; n++) {
          if (!decodeNext()) { endClip(); return; }
          nextAt += frameMs;
          if ((long)(millis() - nextAt) < 0) break;
        }
        if ((long)(millis() - nextAt) > 250) nextAt = millis();
      }
      break;
    }

    case MODE_UPLOAD:
      server.handleClient();
      if (buttons.downPressed) {
        stopUpload();
        loadClips();
        sel = 0;
        mode = MODE_LIST;
      }
      break;

    case MODE_ERROR:
      if (buttons.downPressed) exitRequested = true;
      break;
  }
}

void VideoPlayer::draw() {
  switch (mode) {
    case MODE_LIST: drawList(); break;
    case MODE_PLAY: drawVideo(); break;
    case MODE_UPLOAD: drawUpload(); break;
    case MODE_ERROR: drawError(); break;
  }
}

// ---------------------------------------------------------------------------
// Screens
// ---------------------------------------------------------------------------
void VideoPlayer::drawList() {
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.print(F("VIDEOS"));
  char t[12];
  int total = clipCount + 1;
  snprintf(t, sizeof(t), "%d/%d", sel + 1, total);
  display.setCursor(SCREEN_WIDTH - 6 * strlen(t), 0);
  display.print(t);
  display.drawFastHLine(0, 9, SCREEN_WIDTH, SSD1306_WHITE);

  int first = sel - 1;
  if (first > total - 4) first = total - 4;
  if (first < 0) first = 0;
  for (int r = 0; r < 4 && first + r < total; r++) {
    int i = first + r, y = 12 + r * 10;
    bool on = i == sel;
    if (on) display.fillRect(0, y - 1, SCREEN_WIDTH, 10, SSD1306_WHITE);
    display.setTextColor(on ? SSD1306_BLACK : SSD1306_WHITE);
    display.setCursor(4, y);
    if (i < clipCount) display.print(names[i]);
    else display.print(F("+ UPLOAD VIDEOS"));
  }
  display.setTextColor(SSD1306_WHITE);

  if (notice && millis() < noticeUntil) drawCenteredText(notice, 55, 1);
  else if (clipCount == 0) drawCenteredText("NO VIDEOS YET", 55, 1);
  else drawCenteredText("LR=pick UP=go DN=exit", 55, 1);
}

// The frame is stored in the display's own layout, so a full-screen clip is one copy.
// Smaller ones are drawn scale x scale, centred.
void VideoPlayer::drawVideo() {
  clearDisplay();
  if (!frame) return;                       // not playing: nothing to show
  uint8_t* buf = display.getBuffer();
  if (hdr.scale == 1 && hdr.width == SCREEN_WIDTH && hdr.height == SCREEN_HEIGHT) {
    memcpy(buf, frame, PBV_MAX_FRAME_BYTES);
  } else {
    const int s = hdr.scale;
    const int ox = (SCREEN_WIDTH - hdr.width * s) / 2, oy = (SCREEN_HEIGHT - hdr.height * s) / 2;
    for (int x = 0; x < hdr.width; x++) {
      for (int p = 0; p < hdr.pages; p++) {
        uint8_t bits = frame[p * hdr.width + x];
        if (!bits) continue;
        for (int k = 0; k < 8; k++) {
          int y = p * 8 + k;
          if (y >= hdr.height) break;
          if (!(bits >> k & 1)) continue;
          for (int dy = 0; dy < s; dy++) {
            int sy = oy + y * s + dy;
            for (int dx = 0; dx < s; dx++) buf[(sy >> 3) * SCREEN_WIDTH + ox + x * s + dx] |= 1 << (sy & 7);
          }
        }
      }
    }
  }

  // Pause symbol and progress bar, for a moment after a button and while paused
  if (paused || millis() < barUntil) {
    int w = hdr.frames > 1 ? (int)((long)(decoded - 1) * 124 / (hdr.frames - 1)) : 0;
    display.fillRect(0, 60, SCREEN_WIDTH, 4, SSD1306_BLACK);
    display.drawRect(1, 60, 126, 4, SSD1306_WHITE);
    display.fillRect(2, 61, w, 2, SSD1306_WHITE);
    if (paused) {
      display.fillRect(3, 3, 12, 11, SSD1306_BLACK);
      display.fillRect(5, 5, 3, 7, SSD1306_WHITE);
      display.fillRect(10, 5, 3, 7, SSD1306_WHITE);
    }
  }
}

void VideoPlayer::drawUpload() {
  unsigned long up = (millis() - uploadStartedAt) / 1000;
  char line[24];
  clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  snprintf(line, sizeof(line), "VIDEO UPLOAD  %lu:%02lu", up / 60, up % 60);
  display.setCursor(0, 0);   display.print(line);
  snprintf(line, sizeof(line), "SSID %s", apSsid);
  display.setCursor(0, 11);  display.print(line);
  snprintf(line, sizeof(line), "PASS %s", apPass);
  display.setCursor(0, 22);  display.print(line);
  display.setCursor(0, 33);  display.print(F("http://192.168.4.1"));
  display.setCursor(0, 44);
  display.print(WiFi.softAPgetStationNum() > 0 ? F("Device: CONNECTED") : F("Device: not joined"));
  display.setCursor(0, 55);  display.print(F("DOWN: stop"));
}

void VideoPlayer::drawError() {
  clearDisplay();
  drawCenteredText("VIDEOS", 8, 1);
  drawCenteredText("STORAGE ERROR", 26, 1);
  drawCenteredText("The file system", 38, 1);
  drawCenteredText("did not start", 47, 1);
  drawCenteredText("DOWN: back", 57, 1);
}
