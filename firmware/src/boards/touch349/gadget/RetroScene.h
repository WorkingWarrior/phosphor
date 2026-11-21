#pragma once

// The v1.1 machine themes: a scene redrawn as that machine's game art (Amiga-style painted 32 colors, C64-style
// multicolor and Atari 2600-style bands). The picture (assets/art/*.img in the asset pack, made by
// design/retro-proto/build_art.sh) is the still part; what moves is drawn by code on top in the machine's own pixel
// size and colors: snow, smoke, flames, a lighthouse beam, glints on the water. Scenes with actors (the aquarium's
// fish) draw them into the Px layer between RetroScene_Begin and RetroScene_End. Without the asset pack (an old partition
// table, or nothing written yet) RetroScene_Begin returns false and the scene draws itself the plain way.

#include <Arduino.h>

enum class RetroArt : uint8_t { Snow, Aquarium, Fire, Sea };

// Picture and effects only. false = the current theme has no picture for this scene: draw the usual one.
bool RetroScene_Draw(RetroArt scene, const char *corner_text);
// With actors: true = the picture is down and the Px layer is cleared; draw the actors into it, then call End.
bool RetroScene_Begin(RetroArt scene);
void RetroScene_End(const char *corner_text);
void RetroScene_Stir(float strength);  // 0..1, a tap or a shake: whirls the snow, flares the fire, roughens the sea
