#pragma once

// The v1.1 machine themes: a scene redrawn as that machine's game art (Amiga-style painted 32 colors, C64-style
// multicolor and Atari 2600-style bands). The picture (assets/art/*.img in the asset pack, made by
// design/retro-proto/build_art.sh) is the still part; what moves is drawn by code on top in the machine's own pixel
// size and colors: snow, smoke, flames, a lighthouse beam, glints on the water. Scenes with actors (the aquarium's
// fish) draw them into the Px layer between RetroScene_Begin and RetroScene_End. Without the asset pack (an old partition
// table, or nothing written yet) RetroScene_Begin returns false and the scene draws itself the plain way.

#include <Arduino.h>

enum class RetroArt : uint8_t { Snow, Aquarium, Fire, Sea, World };

// Picture and effects only. false = the current theme has no picture for this scene: draw the usual one.
bool RetroScene_Draw(RetroArt scene, const char *corner_text);
// With actors: true = the picture is down and the Px layer is cleared; draw the actors into it, then call End.
bool RetroScene_Begin(RetroArt scene);
void RetroScene_End(const char *corner_text);
void RetroScene_Stir(float strength);  // 0..1, a tap or a shake: whirls the snow, flares the fire, roughens the sea

// Shares the scene background cache; false asks the clock to use its plain layout.
bool RetroWorld_Draw();

// Px pixels per block of the current scene's picture (1 Amiga, 2 C64, 4 2600): actors that must sit on the machine's
// grid snap to it (blocks are this wide and half as tall, at least 1).
int RetroScene_Block();

// Named app backdrop, sharing the single decoded PSRAM picture.
bool RetroPicture_Draw(const char *name);

// A small picture on its own (a ship, a figure) decoded into its own PSRAM buffer, so it can be drawn in the same frame
// as a backdrop. Pixels are palette indices, row by row; sx x sy screen pixels each. keyed = index 0 is transparent.
// Cached by name (a few at a time); nullptr when the asset pack or the sprite is missing.
struct RetroSprite {
  uint16_t w, h;
  uint8_t sx, sy;
  bool keyed;
  const uint16_t *pal;  // RGB565
  const uint8_t *idx;
};
const RetroSprite *RetroSprite_Get(const char *name);
