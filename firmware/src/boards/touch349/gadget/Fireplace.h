#pragma once

// Fireplace and a sleeping cat, in pixel art (Px): a brick fireplace with a living fire, a candle and a clock on the
// mantel, books, a cat curled up on the rug breathing slowly, its tail flicking now and then. A tap wakes the cat for
// a few seconds.

#include <Arduino.h>

void Fireplace_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Fireplace_Tap();
