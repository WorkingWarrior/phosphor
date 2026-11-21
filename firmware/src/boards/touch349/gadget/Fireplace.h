#pragma once

// Fireplace and a sleeping cat, in pixel art (Px): a brick fireplace with a living fire, a candle and a clock on the
// mantel, books, a cat curled up on the rug breathing slowly, its tail flicking now and then. A tap wakes the cat for
// a few seconds.

#include <Arduino.h>

void Fireplace_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
// Tap (screen x, y): on the fire a burst of sparks, elsewhere it wakes the cat for a few seconds.
void Fireplace_Tap(int screen_x, int screen_y);
// Hold: a log goes on and the flames flare up for a few seconds. Tilt rolls a yarn ball a kitten chases (in Draw).
void Fireplace_Hold();
