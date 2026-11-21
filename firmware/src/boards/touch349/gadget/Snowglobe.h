#pragma once

// The forest cabin at night, in pixel art (Px): mountains under the moon, a pine forest, a log cabin with a warm
// window on a snowy hill. Snow starts from an empty sky and falls the way the board is tilted, piling up on the roof,
// the trees and the ground; a shake, a knock or a tap stirs it up again. Hold for the snow globe: the cabin as a small
// diorama in a glass dome, snow drifting as if in water. Dec 1 - Jan 6 the cabin gets lights (SettingsUi_Holidays).

#include <Arduino.h>

void Snowglobe_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Snowglobe_Stir(float strength);            // 0..1
void Snowglobe_Tap(int screen_x, int screen_y); // the cabin window: lamp on/off; anywhere else Snowglobe_Stir(0.6)
void Snowglobe_Hold();                          // cabin <-> snow globe
void Snowglobe_Show(const char *what);          // "sleigh" | "star": now, for screenshots
