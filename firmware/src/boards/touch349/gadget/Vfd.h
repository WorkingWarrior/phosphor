#pragma once

// Drawing kit for the VFD look on the 640x172 bar (design/retro/mockups.html, direction A): a near-black glass, unlit
// "ghost" segments and dots, lit cyan with a one-pixel glow, and synthwave magenta for what matters. Everything draws
// into one RGB565 canvas; the board pushes it to the panel.

#include <Arduino.h>

namespace vfd {

constexpr int W = 640;
constexpr int H = 172;

// Palette (OKLCH from the mockup, converted to RGB565).
constexpr uint16_t BG = 0x0041;     // glass
constexpr uint16_t GHOST = 0x0904;  // unlit segment / dot
constexpr uint16_t FAINT = 0x0082;  // unlit dot of the aquarium matrix
constexpr uint16_t HALO = 0x0248;   // glow around lit cyan
constexpr uint16_t LIT = 0x77BC;    // lit cyan
constexpr uint16_t DIM = 0x44B1;    // secondary text
constexpr uint16_t HOT = 0xFA16;    // synthwave magenta
constexpr uint16_t HOTH = 0x6888;   // glow around magenta
constexpr uint16_t KELP = 0x4D72;
constexpr uint16_t NONE = 0;        // "no color" in Ink

struct Ink {
  uint16_t c;              // lit color
  uint16_t h = NONE;       // glow, NONE = no glow
  uint16_t ghost = NONE;   // unlit cells/segments, NONE = not drawn
};

constexpr Ink LIT_INK = { LIT, HALO };
constexpr Ink HOT_INK = { HOT, HOTH };
constexpr Ink DIM_INK = { DIM };
constexpr Ink GHOST_INK = { GHOST };

void begin(uint16_t *canvas);
uint16_t *canvas();
void clear(uint16_t color = BG);
// Drawing is clipped to this box (default: the whole screen).
void clip(int x0, int y0, int x1, int y1);
void noClip();
void rect(int x, int y, int w, int h, uint16_t color);
void frame(int x, int y, int w, int h, uint16_t color);  // 1 px outline

// Text in the 5x7 matrix: every font cell is a d x d dot at pitch p, glyphs advance 6 p. UTF-8 in; lower case is
// shown as upper case, Turkish letters keep their accents, other accented letters lose them. Returns the end x.
int text(const char *s, int x, int y, int p, int d, const Ink &ink);
inline int text(const char *s, int x, int y, int p, const Ink &ink) { return text(s, x, y, p, p, ink); }
int textWidth(const char *s, int p);
int glyphCount(const char *s);

// 7-segment digits with chamfered ends. s holds digits, ' ', '-' and ':'. Returns the end x.
int seg(const char *s, int x, int y, int w, int h, int t, int gap, const Ink &ink);

// Sprite from strings of '#' and '.', each '#' an s x s block.
void sprite(const char *const *rows, int n, int x, int y, int s, uint16_t color, bool flip = false);

}  // namespace vfd
