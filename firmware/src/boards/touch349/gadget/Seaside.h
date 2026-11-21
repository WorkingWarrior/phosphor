#pragma once

// Seaside in pixel art (Px): the sky follows the hour (day, dawn/dusk, night with moon and stars), a lighthouse on a
// rock sweeps its beam at night, a sailboat drifts, gulls fly by day, waves roll in and wash up the sand. A tap sends
// a bigger wave, except on the night sky: a shooting star there (in a storm: a lightning bolt). Hold = storm on/off
// (rain, dark clouds, lightning, rough sea). Tilting the board rocks the boat.

#include <Arduino.h>

void Seaside_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Seaside_Tap(int screen_x, int screen_y);
void Seaside_Hold();
