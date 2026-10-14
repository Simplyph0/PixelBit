#ifndef VIDEOPLAYER_H
#define VIDEOPLAYER_H

#include <LittleFS.h>
#include "config.h"
#include "game.h"
#include "pbvcodec.h"

#define MAX_CLIPS 16
#define CLIP_NAME_LEN 13          // 12 characters and the end
#define CLIP_DIR "/clips"
#define CLIP_SEEK_SECONDS 5

// Plays black and white videos (.pbv, see pbvcodec.h) from the console's flash
// file system, with no sound. Videos get there over Wi-Fi: the last entry of
// the list starts an access point with an upload page. A first-boot copy of the
// clip built into the firmware (demo_clip.h) is written to the file system once.
//
// Buttons. List: LEFT/RIGHT pick, UP plays (or opens the upload page), DOWN
// leaves the player. Playing: UP pauses, LEFT/RIGHT jump back and forward, DOWN
// goes back to the list. Upload page: DOWN stops it.
class VideoPlayer : public Game {
public:
  void init() override;
  void update() override;
  void draw() override;
  bool isGameOver() override { return false; }
  int getScore() override { return 0; }
  const char* getName() override { return "VIDEOS"; }
  bool hasScore() override { return false; }
  bool wantsExit() override { return exitRequested; }
  void stop() override;

private:
  enum Mode : uint8_t { MODE_LIST, MODE_PLAY, MODE_UPLOAD, MODE_ERROR };

  Mode mode = MODE_LIST;
  bool exitRequested = false;

  // list
  char names[MAX_CLIPS][CLIP_NAME_LEN];
  uint8_t clipCount = 0;
  uint8_t sel = 0;
  const char* notice = nullptr;
  unsigned long noticeUntil = 0;

  // playback
  File file;
  PbvHeader hdr;
  // One block of memory, taken when a clip opens and given back when it closes, so the
  // player costs no RAM while it is not playing: the picture being shown (in the OLED's
  // own layout) and the compressed frame being read
  uint8_t* frame = nullptr;                 // PBV_MAX_FRAME_BYTES
  uint8_t* payload = nullptr;               // PBV_MAX_PAYLOAD
  uint16_t decoded = 0;                     // frames decoded so far: the shown frame is decoded - 1
  uint16_t frameMs = 66;
  unsigned long nextAt = 0;
  unsigned long barUntil = 0;               // the progress bar shows until then
  bool paused = false;

  // upload page
  unsigned long uploadStartedAt = 0;

  void loadClips();
  bool openClip(uint8_t index);
  void closeClip();
  bool decodeNext();
  void seekBy(int seconds);
  void endClip();
  void startUpload();
  void stopUpload();

  void drawList();
  void drawVideo();
  void drawUpload();
  void drawError();
};

extern VideoPlayer videoGame;

#endif
