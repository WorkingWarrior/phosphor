#pragma once

// Wave machine, the desk toy with a tank of blue liquid under a layer of clear oil and a little steamship riding the
// waves. The surface is a 1-D height field (waves travel, bounce off the walls and die out slowly); tilting the board
// tilts the level and the liquid sloshes over, a shake churns it, a knock splashes it, a tap kicks a wave at that x.
// Natural theme: full-resolution realistic render. Ega/Vfd themes: the same simulation as chunky pixel art (Px).

#include <Arduino.h>

void Wave_Draw(const char *corner_text);  // advances one frame (call every ~50 ms) and draws
void Wave_Tap(int screen_x);              // kicks a wave at that x (screen 0..639)
// Hold: the ship's fog horn (a low Radio_Beep, only when the radio is not playing) and a puff of smoke from a funnel.
// Now and then, rarely, a whale surfaces and spouts.
void Wave_Hold();
