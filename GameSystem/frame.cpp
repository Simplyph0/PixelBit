#include "frame.h"

static unsigned long frameStart = 0;

void frameWait() {
  unsigned long used = millis() - frameStart;
  unsigned long wait = used >= FRAME_MS ? 0 : FRAME_MS - used;
  if (wait > FRAME_IDLE_MS) wait = FRAME_IDLE_MS;
  delay(wait);
  frameStart = millis();
}
