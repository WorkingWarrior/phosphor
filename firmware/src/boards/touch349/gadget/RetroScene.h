#pragma once

// Prototype of the v1.1 machine themes: the snow cabin redrawn as that machine's game art (Amiga-style painted 32
// colors, C64-style multicolor, Atari 2600-style bands, LCD handheld-style printed backdrop), an illustrated
// background (RetroArtData.h, made by design/retro-proto/convert.py) with snow, chimney smoke and the window's fire
// drawn by code on top, in the machine's own pixel size and colors.

#include <Arduino.h>

bool RetroScene_Snow(const char *corner_text);  // false = the current theme has no art; draw the usual scene
void RetroScene_Stir(float strength);           // 0..1, a tap or a shake
