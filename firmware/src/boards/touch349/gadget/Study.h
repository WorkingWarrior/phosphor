#pragma once

// Study room with a pomodoro, in pixel art (Px): someone in a hoodie and headphones studies at a desk under a lamp,
// snow (or rain, from the weather) falls past the window, a mug steams. On the right, the pomodoro: 25 min focus,
// 5 min break, 15 after every fourth. Tap starts/pauses, long press resets; a beep at every phase change.

#include <Arduino.h>

void Study_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Study_Tap();
void Study_Reset();
void Study_Tick();  // keeps the pomodoro running on other pages
