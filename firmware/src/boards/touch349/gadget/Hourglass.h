#pragma once

// Hourglass for the bar stood upright (portrait): chunky dot-matrix sand only, no glass drawn; the shape shows in how
// the sand piles. Fixed to the device like a real one: turn it over and the sand runs back; lying flat pauses it. The
// neck meters the grains so they all pass in the chosen time (5/10/15/20 min), with a beep at the end.
// Tap = next duration + refill, long press = refill with the same duration.

#include <Arduino.h>

void Hourglass_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Hourglass_Tap();
void Hourglass_Reset();
void Hourglass_Tick();  // no-op: the sand only runs while the page is shown
