#pragma once

// Pixel-art layer for the ambient scenes (aquarium, forest cabin, fireplace, seaside, study): 320x86 logical pixels,
// each shown as a 2x2 block on the 640x172 canvas, colors from one shared palette. The palette stays in the VFD family:
// near-black blue-tinted night, the gadget's cyan and magenta, plus warm amber (fire, lamps, windows), water and sand.

#include <Arduino.h>

namespace px {

constexpr int W = 320;
constexpr int H = 86;

constexpr uint16_t rgb(int r, int g, int b) { return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3); }

// Palette indices. 0 is "transparent" in sprites and the night ground when drawn.
enum : uint8_t {
  CLR = 0,
  NIGHT,                    // ground
  N1, N2, N3, N4, N5, N6,   // blue-tinted neutrals, dark to frost
  C1, C2, C3,               // VFD cyan: glow, dim, lit
  M1, M2,                   // magenta: glow, hot
  F1, F2, F3, F4, F5,       // amber: ember to flame core
  W1, W2, W3, W4, W5,       // water, deep to light
  S1, S2, S3,               // sand
  G1, G2, G3,               // plants
  D1, D2, D3,               // wood
  R1, R2, R3,               // brick, red
  Y1, U1, U2, P1, O1,       // fish: yellow, blue, deep blue, violet, orange
  COLORS
};

extern const uint16_t PALETTE[COLORS];
extern uint8_t (*buf)[W];  // H rows, in PSRAM (begin() allocates it; internal RAM is the scarce kind)

void begin();

void clear(uint8_t c);
inline void set(int x, int y, uint8_t c)
{
  if ((unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H) buf[y][x] = c;
}
inline uint8_t get(int x, int y) { return (unsigned)x < (unsigned)W && (unsigned)y < (unsigned)H ? buf[y][x] : CLR; }
void rect(int x, int y, int w, int h, uint8_t c);
void disc(int cx, int cy, int r, uint8_t c);
// Vertical bands from top to bottom (a sky or water gradient) with a 2x2 ordered dither between neighbours.
void gradient(int y0, int y1, const uint8_t *cols, int n);
// Sprite from strings; each char is looked up in key/vals (same length), anything else is transparent.
void sprite(const char *const *rows, int n, int x, int y, const char *key, const uint8_t *vals, bool flip = false);
// Puts the scene on the canvas (2x2 per pixel), through the current theme.
void present();

// Themes recolor every scene at present(), as if the scene were loaded on that machine: Natural = the palette above;
// Ega = the 16 EGA colors with a checker dither between two of them where no single one fits (Sierra-era look); Vfd =
// the gadget's own cyan and magenta, cool colors to cyan, warm ones to magenta, by brightness; C64 = the 16 colors of
// the home computer with its double-wide pixels; Cga = black, cyan, magenta and white, dithered; Amiga = every color
// rounded to 12 bits (4 per channel).
enum class Theme : uint8_t { Natural, Ega, Vfd, C64, Cga, Amiga, COUNT };
void setTheme(Theme t);
Theme theme();
const char *themeName(Theme t);
uint16_t ink(uint8_t c);  // a palette color as the current theme shows it (for text drawn over a scene)

// Portrait scenes (held upright): 86 wide x 320 tall, drawn into the same buffer turned a quarter.
constexpr int PW = H;
constexpr int PH = W;
// left_down: the screen's left edge is at the bottom (else the right edge).
inline void pset(int x, int y, uint8_t c, bool left_down)
{
  if (left_down) set(W - 1 - y, x, c);
  else set(y, H - 1 - x, c);
}
inline uint8_t pget(int x, int y, bool left_down) { return left_down ? get(W - 1 - y, x) : get(y, H - 1 - x); }

}  // namespace px
