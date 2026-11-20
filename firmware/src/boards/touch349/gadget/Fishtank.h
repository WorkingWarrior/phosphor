#pragma once

// The aquarium, in pixel art (Px): clownfish by their anemone, a blue tang, an angelfish, a goldfish, a royal gramma
// and a school of neon tetras over sand, kelp, rocks and a magenta coral, light shafts from the surface. The water
// stays level when the board tilts; a shake or a knock scares the fish; a tap drops a few food pellets.

#include <Arduino.h>

void Fishtank_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Fishtank_Feed(int screen_x);
