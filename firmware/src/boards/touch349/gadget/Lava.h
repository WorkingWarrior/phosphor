#pragma once

// Lava lamp for the bar stood upright (portrait), seen from inside: the whole screen is the liquid, no glass or lamp
// drawn. Big wax blobs heat up in the pool at the bottom, tear off, rise slowly, cool at the top and sink back, merging
// and splitting on the way. Upright whichever end is down. A tap cycles the colors (3 schemes).

#include <Arduino.h>

void Lava_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Lava_Tap();
