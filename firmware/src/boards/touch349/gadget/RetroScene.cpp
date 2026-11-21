#ifdef BOARD_TOUCH349

#include "RetroScene.h"
#include <esp_heap_caps.h>
#include "Motion.h"
#include "Px.h"
#include "Theme.h"
#include "Vfd.h"

namespace art {
struct Image {
  uint16_t w, h;  // in machine pixels
  uint8_t sx, sy; // screen pixels per machine pixel
  uint8_t ncolors;
  const uint16_t *pal;
  const uint8_t *rle;  // (count, color index) pairs, row by row
  uint32_t rle_len;
};
#include "RetroArtData.h"
}  // namespace art

using px::rgb;

namespace {

enum Kind : uint8_t { PAINTED, LCD };

struct Machine {
  theme::Id id;
  const art::Image *img;
  Kind kind;
  int16_t wx0, wy0, wx1, wy1;  // the cabin window, screen px
  uint8_t win_lum;             // window pixels brighter than this are the fire
  bool win_always;             // the art lost the window: paint it every frame
  uint16_t win_on, win_alt;    // fire color, and its flicker
  int16_t chim_x, chim_y;      // where the smoke leaves the chimney, screen px
  uint16_t smoke_hi, smoke_lo; // young and old smoke
  uint16_t snow_near, snow_far;
  uint8_t flakes;   // how many
  uint8_t big_pct;  // share of near flakes drawn two machine pixels wide
  uint8_t step;     // move every n-th frame (the 2600 skips frames)
};

const Machine MACHINES[] = {
  { theme::AMIGA, &art::art_amiga, PAINTED, 160, 120, 178, 139, 150, false, rgb(255, 204, 119), rgb(255, 238, 187),
    104, 56, rgb(204, 204, 238), rgb(119, 119, 187), rgb(238, 238, 255), rgb(153, 153, 221), 110, 25, 1 },
  { theme::C64, &art::art_c64, PAINTED, 176, 130, 200, 138, 150, false, 0xBE2D /*yellow*/, 0x9B2B /*light red*/,
    120, 80, rgb(149, 149, 149), rgb(108, 108, 108), rgb(255, 255, 255), rgb(149, 149, 149), 80, 0, 1 },
  { theme::ATARI, &art::art_atari, PAINTED, 144, 128, 160, 134, 140, true, rgb(252, 188, 116), rgb(236, 140, 60),
    112, 92, rgb(170, 170, 170), rgb(110, 110, 110), rgb(236, 236, 236), rgb(236, 236, 236), 45, 0, 2 },
  { theme::LCD, &art::art_lcd, LCD, 0, 0, 0, 0, 0, false, 0, 0, 132, 56, 0, 0, 0, 0, 0, 0, 1 },
};

uint16_t *bg = nullptr;          // the decoded art, 640 x 172 RGB565 (PSRAM)
const art::Image *decoded = nullptr;
uint32_t frame_no = 0;
float burst = 0;                 // shake energy, decays

struct Flake {
  float x, y, v;   // machine px, machine px per second
  float floor;     // where it lands and melts into the snowfield
  bool near, big;
};
Flake fl[110];

float frand() { return (esp_random() & 0xffff) / 65535.0f; }
uint32_t hash32(uint32_t v)
{
  v ^= v >> 16;
  v *= 0x7feb352d;
  v ^= v >> 15;
  v *= 0x846ca68b;
  return v ^ (v >> 16);
}

int lum(uint16_t c) { return (((c >> 11) << 3) * 3 + (((c >> 5) & 63) << 2) * 6 + ((c & 31) << 3)) / 10; }

uint16_t mix(uint16_t a, uint16_t b, int t)  // t of 256 toward b
{
  int r = ((a >> 11) * (256 - t) + (b >> 11) * t) >> 8;
  int g = (((a >> 5) & 63) * (256 - t) + ((b >> 5) & 63) * t) >> 8;
  int bl = ((a & 31) * (256 - t) + (b & 31) * t) >> 8;
  return (r << 11) | (g << 5) | bl;
}

void decode(const art::Image *img)
{
  if (!bg) bg = (uint16_t *)heap_caps_malloc(vfd::W * vfd::H * 2, MALLOC_CAP_SPIRAM);
  if (!bg) return;
  uint32_t p = 0, n = (uint32_t)img->w * img->h;
  for (uint32_t i = 0; i + 1 < img->rle_len && p < n; i += 2) {
    uint16_t c = img->pal[img->rle[i + 1]];
    for (int k = 0; k < img->rle[i] && p < n; k++, p++) {
      int x = (p % img->w) * img->sx, y = (p / img->w) * img->sy;
      for (int yy = y; yy < y + img->sy && yy < vfd::H; yy++)
        for (int xx = x; xx < x + img->sx && xx < vfd::W; xx++) bg[yy * vfd::W + xx] = c;
    }
  }
  decoded = img;
}

void spawn(Flake &f, const art::Image *img, bool top)
{
  f.x = frand() * img->w;
  f.y = top ? -1 - frand() * 6 : frand() * img->h;
  f.near = frand() < 0.6f;
  f.big = f.near && (esp_random() % 100) < 25;
  f.v = (f.near ? 9 : 4.5f) * (0.7f + frand() * 0.6f);
  f.floor = img->h * (0.7f + frand() * 0.3f);
}

void block(int x, int y, int w, int h, uint16_t c)
{
  vfd::rect(x, y, w, h, c);
}

// ---- painted machines: Amiga, C64, 2600 ----------------------------------------------------------------------------

void painted(const Machine &m)
{
  const art::Image *img = m.img;
  const int sx = img->sx, sy = img->sy;
  uint16_t *cv = vfd::canvas();
  float t = frame_no * 0.05f;

  // the fire behind the window
  bool flick = ((frame_no / 3) % 7 == 0) || (hash32(frame_no / 2) % 23 == 0);
  if (m.win_always || flick) {
    for (int y = m.wy0; y < m.wy1; y++)
      for (int x = m.wx0; x < m.wx1; x++) {
        uint16_t &c = cv[y * vfd::W + x];
        if (lum(bg[y * vfd::W + x]) > m.win_lum) c = flick ? m.win_alt : m.win_on;
      }
  }

  // chimney smoke, puff by puff on the machine's pixel grid
  for (int i = 0; i < 8; i++) {
    float ph = fmodf(t * 0.11f + i / 8.0f, 1.0f);
    if (ph > 0.86f) continue;
    float xs = m.chim_x + ph * 30 + sinf(ph * 7 + i) * 3, ys = m.chim_y - ph * 40;
    int gx = (int)(xs / sx) * sx, gy = (int)(ys / sy) * sy;
    int size = ph > 0.18f && ph < 0.62f ? 2 : 1;
    uint16_t c = ph < 0.45f ? m.smoke_hi : m.smoke_lo;
    block(gx, gy, sx * size, sy * size, c);
  }

  // snow; a tilt blows it sideways, a shake whirls it
  float gx = 0, gy = 1, gz = 0;
  if (Motion_Ok()) Motion_Gravity(gx, gy, gz);
  bool move = frame_no % m.step == 0;
  float dt = 0.05f * m.step;
  for (int i = 0; i < m.flakes; i++) {
    Flake &f = fl[i];
    if (move) {
      float sway = sinf(t * 0.7f + i * 1.7f) * (f.near ? 1.2f : 0.6f);
      f.x += (sway + gx * 10 + sinf(t * 3 + i) * burst * 14) * dt * img->w / 320.0f;  // C64/2600 pixels are wider
      f.y += f.v * dt * (1 + burst * 1.5f);
      if (f.x < 0) f.x += img->w;
      if (f.x >= img->w) f.x -= img->w;
      if (f.y >= f.floor) spawn(f, img, true);
    }
    if (f.y < 0) continue;
    int x = (int)f.x * sx, y = (int)f.y * sy;
    int w = f.big ? 2 : 1;
    block(x, y, sx * w, sy * w, f.near ? m.snow_near : m.snow_far);
  }
}

// ---- LCD handheld: printed backdrop, black segments that are either on or off, ghosts of the off ones --------------

const uint16_t INK = rgb(34, 38, 36);

// One segment: a small star of machine pixels (2x2 screen px). On = ink, off = a faint ghost of it.
void segFlake(int x, int y, bool on)
{
  static const int8_t P[][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 2, 1 }, { 1, 2 } };
  uint16_t *cv = vfd::canvas();
  for (auto &p : P) {
    int xx = x + p[0] * 2, yy = y + p[1] * 2;
    for (int dy = 0; dy < 2; dy++)
      for (int dx = 0; dx < 2; dx++) {
        int X = xx + dx, Y = yy + dy;
        if ((unsigned)X >= vfd::W || (unsigned)Y >= vfd::H) continue;
        uint16_t &c = cv[Y * vfd::W + X];
        c = on ? INK : mix(bg[Y * vfd::W + X], INK, 26);
      }
  }
}

void segPuff(int x, int y, int r, bool on)  // a ring of machine pixels
{
  uint16_t *cv = vfd::canvas();
  for (int dy = -r; dy <= r; dy++)
    for (int dx = -r; dx <= r; dx++) {
      int d2 = dx * dx + dy * dy;
      if (d2 > r * r || d2 < (r - 1) * (r - 1)) continue;
      for (int k = 0; k < 4; k++) {
        int X = x + dx * 2 + (k & 1), Y = y + dy * 2 + (k >> 1);
        if ((unsigned)X >= vfd::W || (unsigned)Y >= vfd::H) continue;
        cv[Y * vfd::W + X] = on ? INK : mix(bg[Y * vfd::W + X], INK, 26);
      }
    }
}

float lcd_step = 0;

void lcd(const Machine &m)
{
  lcd_step += 0.05f / 0.38f * (1 + burst * 3);  // one tick every ~380 ms, faster when shaken
  int step = (int)lcd_step;
  // snow: columns of fixed flake segments, a lit one moves one row down per tick
  for (int c = 0; c < 27; c++) {
    int x = 6 + c * 24 + (c & 1) * 6, off = hash32(c * 7 + 3) % 6;
    for (int r = 0; r < 13; r++) {
      int y = 4 + r * 12 + (c & 1) * 5;
      if (y > vfd::H - 8) break;
      segFlake(x, y, ((r - step + off) % 6 + 6) % 6 == 0);
    }
  }
  // smoke: three puffs leave the chimney in turn
  static const int8_t PUFF[3][3] = { { 0, 0, 2 }, { 7, -11, 3 }, { 17, -24, 4 } };
  for (int i = 0; i < 3; i++) segPuff(m.chim_x + PUFF[i][0], m.chim_y + PUFF[i][1], PUFF[i][2], step % 4 == i);
}

}  // namespace

void RetroScene_Stir(float strength) { burst = max(burst, strength * 2.5f); }

bool RetroScene_Snow(const char *corner_text)
{
  if (theme::naturalScenes()) return false;
  const Machine *m = nullptr;
  for (auto &mm : MACHINES)
    if (mm.id == theme::current()) m = &mm;
  if (!m) return false;
  if (decoded != m->img) {
    decode(m->img);
    if (!bg) return false;
    for (auto &f : fl) spawn(f, m->img, false);
  }
  frame_no++;
  if (Motion_Ok()) {
    float shake = Motion_Shake();
    if (shake > 0.2f) RetroScene_Stir(shake);
  }
  burst *= 0.94f;

  vfd::noClip();
  memcpy(vfd::canvas(), bg, vfd::W * vfd::H * 2);
  if (m->kind == LCD)
    lcd(*m);
  else
    painted(*m);

  if (corner_text && *corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 4, 2, vfd::LIT_INK);  // the art is busy: the theme's brightest ink
  }
  return true;
}

#endif
