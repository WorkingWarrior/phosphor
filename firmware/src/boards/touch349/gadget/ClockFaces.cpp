#ifdef BOARD_TOUCH349
#include "ClockFaces.h"
#include "Theme.h"
#include "ThemeFonts.h"

using namespace vfd;
using px::rgb;

namespace {
constexpr uint16_t WHITE = rgb(255, 255, 255);
constexpr uint16_t GREEN = rgb(102, 204, 68);
constexpr uint16_t ORANGE = rgb(255, 153, 0);
constexpr uint16_t YELLOW = rgb(184, 199, 111);
constexpr uint16_t LIGHT_GREEN = rgb(154, 210, 132);
const char *const BELL[] = { "..#..", ".###.", ".###.", "#####", "..#.." };
// Only the bright C64 colors: the dark blue, purple and light red of the mockup's wash are the sky's own bands, and
// the time dissolved into them.
const uint16_t WASH[] = { WHITE, LIGHT_GREEN, YELLOW, WHITE, rgb(112, 164, 178), rgb(149, 149, 149) };
constexpr int WASH_N = sizeof(WASH) / sizeof(WASH[0]);
const uint16_t CHROME[] = {
  rgb(255, 255, 255), rgb(255, 255, 255), rgb(255, 255, 255), rgb(255, 255, 255),
  rgb(255, 255, 238), rgb(255, 255, 238), rgb(255, 255, 238), rgb(255, 255, 238),
  rgb(255, 238, 221), rgb(255, 238, 204), rgb(255, 238, 187), rgb(255, 221, 187),
  rgb(255, 221, 170), rgb(255, 221, 153), rgb(255, 204, 136), rgb(255, 204, 119),
  rgb(255, 187, 102), rgb(187, 102, 68), rgb(170, 68, 68), rgb(238, 119, 102),
  rgb(255, 136, 102), rgb(238, 119, 119), rgb(238, 119, 119), rgb(238, 102, 119),
  rgb(221, 102, 136), rgb(221, 85, 136), rgb(221, 85, 136), rgb(204, 85, 136),
  rgb(204, 85, 153), rgb(187, 68, 153), rgb(170, 68, 153), rgb(170, 68, 153),
  rgb(153, 68, 153), rgb(136, 51, 136), rgb(136, 51, 136), rgb(119, 51, 136)
};

// Keep the mockup's own lowercase and Turkish glyphs, rather than VFD's accent synthesis.
uint32_t codepoint(const char *&s)
{
  uint8_t c = *s++;
  if (c < 128) return c;
  int n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
  uint32_t cp = c & (0x3f >> n);
  while (n-- && *s) cp = (cp << 6) | (*s++ & 0x3f);
  return cp;
}

void label(const String &s, int x, int y, uint16_t color, bool shadow = false)
{
  const Font &font = *style().font;
  if (&font == &VFD_FONT) {
    text(s.c_str(), x, y, 2, { color });
    return;
  }
  auto draw = [&](int dx, int dy, uint16_t c) {
    const char *p = s.c_str();
    int cx = x;
    while (*p) {
      const uint8_t *rows = font.rows(codepoint(p));
      if (!rows) rows = font.rows('?');
      if (rows) for (int r = 0; r < 8; r++) for (int col = 0; col < 8; col++)
        if (rows[r] & (0x80 >> col)) rect(cx + col * 2 + dx, y + r * 2 + dy, 2, 2, c);
      cx += 16;
    }
  };
  if (shadow) draw(2, 2, 0);
  draw(0, 0, color);
}

void battery(int x, int y, int pct, uint16_t color, uint16_t fill)
{
  if (pct < 0) return;
  pct = constrain(pct, 0, 100);
  rect(x, y, 20, 2, color);
  rect(x, y + 8, 20, 2, color);
  rect(x, y, 2, 10, color);
  rect(x + 18, y, 2, 10, color);
  rect(x + 20, y + 2, 2, 6, color);
  rect(x + 2, y + 2, ((8 * pct + 50) / 100) * 2, 6, fill);
  label(String(pct) + "%", x + 32, y - 2, color);
}

void bigTime(const String &s, bool amiga)
{
  const Font &font = amiga ? AMIGA_FONT : C64_FONT;
  int sx = amiga ? 10 : 12, sy = 10, y = amiga ? 14 : 16;
  // Three passes over the whole mask: shadows must not cover adjacent glyphs.
  auto draw = [&](int pass) {
    int x = amiga ? 192 : 32;
    for (unsigned i = 0; i < s.length(); i++) {
      char ch = s[i];
      const uint8_t *rows = font.rows(ch);
      int gx = ch == ':' ? x - 2 * sx : x;
      if (rows) for (int r = 0; r < 8; r++) {
        uint8_t bits = ch == '0' && r >= 2 && r <= 5 ? rows[1] : rows[r];
        for (int c = 0; c < 8; c++) {
          if (!(bits & (0x80 >> c))) continue;
          int xx = gx + c * sx, yy = y + r * sy;
          if (pass == 0) rect(xx + (amiga ? 6 : 4), yy + (amiga ? 6 : 4), sx, sy,
                              amiga ? rgb(17, 0, 34) : 0);
          else if (pass == 1) rect(xx - 2, yy - 2, sx + 4, sy + 4, 0);
          else for (int j = 0; j < sy; j += 2) {
            int row = (yy + j - y) / 2;
            rect(xx, yy + j, sx, 2, amiga ? CHROME[min(row, 35)] : WASH[(row / 2) % WASH_N]);
          }
        }
      }
      x += (ch == ':' ? 2 : 7) * sx + 8;
    }
  };
  draw(0);
  draw(1);
  draw(2);
}

void ehb()
{
  for (int y = 110; y < 166; y++) for (int x = 340; x < 632; x++) {
    uint16_t &c = canvas()[y * W + x];
    c = (c & 0xf7de) >> 1;
  }
  rect(340, 110, 292, 2, rgb(153, 153, 170));
  rect(340, 110, 2, 56, rgb(153, 153, 170));
  rect(340, 164, 292, 2, 0);
  rect(630, 110, 2, 56, 0);
}

void atariTime(const String &s)
{
  static const uint8_t DIGITS[10][5] = {
    { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 },
    { 5, 5, 7, 1, 1 }, { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 },
    { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 }
  };
  // Cool rows over the warm sunset bands, one color per block row like the 2600 changed colors per line.
  static const uint16_t COLORS[] = { rgb(236, 236, 236), rgb(164, 224, 252), rgb(144, 204, 232),
                                      rgb(124, 180, 212), rgb(104, 156, 192) };
  const uint16_t SHADOW = rgb(0, 24, 124);
  for (int pass = 0; pass < 2; pass++) {
    int x = 24, d = pass ? 0 : 4;
    for (unsigned i = 0; i < s.length(); i++) {
      char ch = s[i];
      if (ch == ':') {
        for (int y : { 28, 56 }) rect(x + d, y + d, 10, 10, pass ? COLORS[(y - 14) / 14] : SHADOW);
        x += 24;
      } else if (ch >= '0' && ch <= '9') {
        for (int r = 0; r < 5; r++) for (int c = 0; c < 3; c++)
          if (DIGITS[ch - '0'][r] & (4 >> c))
            rect(x + c * 20 + d, 14 + r * 14 + d, 20, 14, pass ? COLORS[r] : SHADOW);
        x += 72;
      }
    }
  }
}
}  // namespace

void ClockFaces_Draw(const ClockFace &f)
{
  Style saved = style();
  bool amiga = theme::current() == theme::AMIGA, atari = theme::current() == theme::ATARI;
  setStyle({ atari ? &VFD_FONT : amiga ? &AMIGA_FONT : &C64_FONT, false, false, false });
  uint16_t alarm = f.alarm_lit ? (amiga ? ORANGE : atari ? rgb(252, 188, 148) : YELLOW)
                              : amiga ? rgb(119, 68, 34) : atari ? rgb(112, 72, 48) : rgb(68, 68, 68);
  if (amiga) {
    bigTime(f.time, true);
    ehb();
    const String &date = textWidth(f.long_date.c_str(), 2) <= 272 ? f.long_date : f.date;
    label(date, 352, 118, WHITE, true);
    sprite(BELL, 5, 352, 144, 2, alarm);
    label(f.alarm, 370, 142, alarm, true);
    // A ringing English message needs more room than the alarm time.
    battery(f.ringing ? 508 : 476, 144, f.battery, WHITE, GREEN);
  } else if (atari) {
    atariTime(f.time);
    rect(0, 150, W, 22, 0);
    label(f.date, 24, 154, rgb(236, 236, 236));
    sprite(BELL, 5, 256, 156, 2, alarm);
    label(f.alarm, 276, 154, alarm);
    battery(508, 156, f.battery, rgb(184, 252, 184), rgb(184, 252, 184));
  } else {
    bigTime(f.time, false);
    int b = theme::def().border_w;
    rect(b, 146, W - 2 * b, 18, 0);
    label(f.date, max(b + 2, 24), 148, WHITE);
    sprite(BELL, 5, 256, 150, 2, alarm);
    label(f.alarm, 274, 148, alarm);
    battery(508, 150, f.battery, LIGHT_GREEN, LIGHT_GREEN);
  }
  setStyle(saved);
}
#endif
