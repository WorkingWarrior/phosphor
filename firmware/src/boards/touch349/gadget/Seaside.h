#pragma once

// Seaside in pixel art (Px): the sky follows the hour (day, dawn/dusk, night with moon and stars), a lighthouse on a
// rock sweeps its beam at night, a sailboat drifts, gulls fly by day, waves roll in and wash up the sand. A tap sends
// a bigger wave.

#include <Arduino.h>

void Seaside_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Seaside_Tap();
