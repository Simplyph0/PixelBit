#ifndef FRAME_H
#define FRAME_H

#include "config.h"

// Ends a frame. Frames that redraw the screen (about 28 ms of it is sending the
// picture to the display) are evened out to FRAME_MS from start to start, so a
// game runs at the same speed whether a frame had a little or a lot to do.
// Frames with less to do wait FRAME_IDLE_MS at most, as before.
void frameWait();

#endif
