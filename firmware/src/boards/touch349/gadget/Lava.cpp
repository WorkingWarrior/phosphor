#ifdef BOARD_TOUCH349

#include "Lava.h"
#include "Motion.h"
#include "Px.h"
#include "Theme.h"
#include "Vfd.h"

using namespace px;

static constexpr int C = 2;                      // cell = 2x2 portrait pixels (4x4 on screen)
static constexpr int GW = PW / C + 1, GH = PH / C;
static constexpr int NB = 6, NPOOL = 4;
static constexpr int NBALL = NPOOL + NB * 2;     // pool + blobs + their tails
static constexpr float T_IN = 0.35f;             // field level inside the wax

struct Blob {
  float x, y, vy, R, temp, rate, ph;
};
struct Ball {
  float x, y, R2;
};

static Blob blobs[NB];
static Ball balls[NBALL];
static int nballs = 0;
static bool inited = false, left_down = true;
static uint8_t scheme = 0;
static uint32_t frame_no = 0;

// wax edge, body, core; liquid top, bottom
static const uint8_t WAX[3][3] = { { M1, M2, F5 }, { F2, F4, F5 }, { C2, C3, N6 } };
static const uint8_t LIQ[3][2] = { { NIGHT, C1 }, { NIGHT, P1 }, { NIGHT, M1 } };

static float frand() { return (esp_random() & 0xffff) / 65535.0f; }

void Lava_Tap() { scheme = (scheme + 1) % 3; }

static void lavaInit()
{
  inited = true;
  for (int i = 0; i < NB; i++) {
    Blob &b = blobs[i];
    b.R = 30 + frand() * 14;
    b.rate = 0.7f + frand() * 0.6f;
    b.ph = frand() * 6.28f;
    b.x = 14 + frand() * (PW - 28);
    b.y = i < 2 ? 40 + i * 90 : PH - 6 - frand() * 10;  // two already on their way, the rest in the pool
    b.temp = i < 2 ? 0.3f : frand() * 0.5f;
    b.vy = 0;
  }
}

static void physics()
{
  float t = frame_no * 0.05f;
  for (int i = 0; i < NB; i++) {
    Blob &b = blobs[i];
    if (b.y > PH - 34) b.temp = min(1.0f, b.temp + 0.0035f * b.rate);  // the bulb heats the pool
    else b.temp = max(0.0f, b.temp - 0.0011f * b.rate);
    if (b.y < 30) b.temp = max(0.0f, b.temp - 0.004f);                   // cold at the top
    b.vy += -(b.temp - 0.5f) * 0.012f;
    b.vy *= 0.975f;
    b.vy = constrain(b.vy, -0.75f, 0.75f);
    b.y += b.vy;
    b.x += sinf(t * 0.23f + b.ph) * 0.06f;
    b.y = constrain(b.y, -b.R * 0.2f, (float)PH + 4);
    b.x = constrain(b.x, b.R * 0.45f, PW - b.R * 0.45f);
  }
  nballs = 0;
  for (int k = 0; k < NPOOL; k++) {
    float x = k * PW / (NPOOL - 1.0f), r = 30 + 4 * sinf(t * 0.3f + k * 2);
    balls[nballs++] = { x, PH + 10.0f, r * r };
  }
  for (int i = 0; i < NB; i++) {
    const Blob &b = blobs[i];
    float sq = 1 + fabsf(b.vy) * 0.25f;  // a moving blob is a bit narrower
    balls[nballs++] = { b.x, b.y, b.R * b.R / sq };
    float tail = fabsf(b.vy) * 16;     // and drags a tail behind it
    balls[nballs++] = { b.x, b.y - (b.vy > 0 ? 1 : -1) * tail * 0.9f, b.R * b.R * 0.35f * min(1.0f, tail / 6) };
  }
}

static float field(float x, float y)
{
  float f = 0;
  for (int i = 0; i < nballs; i++) {
    float dx = x - balls[i].x, dy = y - balls[i].y, d2 = dx * dx + dy * dy;
    if (d2 < balls[i].R2) {
      float k = 1 - d2 / balls[i].R2;
      f += k * k;
    }
  }
  return f;
}

// Machine looks: the simulation is sampled on a coarse 2x2 portrait grid once per frame, then every machine cell reads
// it by bilinear interpolation. A cell is cw x ch portrait pixels = one block of screen pixels: Amiga 2x2 (1x1),
// C64 4x2 (1x2), 2600 8x4 (2x4), so the blocks line up on the screen, not in portrait space.
struct Rgb {
  uint8_t r, g, b;
};

static float rowA[GW], rowB[GW];
static int rowJ = -2;

static void fillRow(float *row, int j)
{
  for (int i = 0; i < GW; i++) row[i] = field(i * C, j * C);
}

static void coarseRows(int j)
{
  if (j == rowJ) return;
  if (j == rowJ + 1) memcpy(rowA, rowB, sizeof(rowA));
  else fillRow(rowA, j);
  fillRow(rowB, j + 1);
  rowJ = j;
}

static void cell(int x, int y, int cw, int ch, uint16_t c)
{
  if (left_down) vfd::rect(vfd::W - 2 * (y + ch), 2 * x, 2 * ch, 2 * cw, c);
  else vfd::rect(2 * y, vfd::H - 2 * (x + cw), 2 * ch, 2 * cw, c);
}

static uint16_t c12(int r, int g, int b) { return rgb(r * 17, g * 17, b * 17); }

// n colors along stops (piecewise linear), 12 bit.
static void ramp(uint16_t *out, Rgb *rgbs, int n, const Rgb *stops, int ns)
{
  for (int i = 0; i < n; i++) {
    float t = i / (n - 1.0f) * (ns - 1);
    int k = min((int)t, ns - 2);
    float f = t - k;
    rgbs[i] = { (uint8_t)(stops[k].r + (stops[k + 1].r - stops[k].r) * f + 0.5f),
                (uint8_t)(stops[k].g + (stops[k + 1].g - stops[k].g) * f + 0.5f),
                (uint8_t)(stops[k].b + (stops[k + 1].b - stops[k].b) * f + 0.5f) };
    out[i] = c12(rgbs[i].r, rgbs[i].g, rgbs[i].b);
  }
}

// Amiga: wax edge, mid, core; liquid top, bottom (0..15 per channel)
static const Rgb AM_WAX[3][3] = { { { 8, 1, 5 }, { 14, 3, 8 }, { 15, 11, 6 } },
                                  { { 10, 4, 0 }, { 14, 8, 1 }, { 15, 14, 6 } },
                                  { { 1, 7, 9 }, { 3, 12, 14 }, { 14, 15, 15 } } };
static const Rgb AM_LIQ[3][2] = { { { 0, 1, 2 }, { 2, 9, 11 } }, { { 1, 0, 3 }, { 7, 3, 11 } }, { { 2, 0, 2 }, { 11, 2, 8 } } };
// C64 colors: wax edge, body, core; liquid dark, lit
static const uint16_t C64_WAX[3][3] = { { rgb(111, 61, 134), rgb(154, 103, 89), rgb(184, 199, 111) },
                                        { rgb(67, 57, 0), rgb(111, 79, 37), rgb(184, 199, 111) },
                                        { rgb(108, 94, 181), rgb(112, 164, 178), rgb(255, 255, 255) } };
static const uint16_t C64_LIQ[3][2] = { { rgb(0, 0, 0), rgb(53, 40, 121) },
                                        { rgb(0, 0, 0), rgb(111, 61, 134) },
                                        { rgb(0, 0, 0), rgb(104, 55, 43) } };
// 2600: two flat wax colors; liquid as 8 hue steps, one per band of block rows
static const uint16_t A_WAX[3][2] = { { rgb(192, 64, 128), rgb(252, 152, 96) },
                                      { rgb(212, 108, 32), rgb(252, 212, 96) },
                                      { rgb(80, 184, 200), rgb(200, 252, 252) } };
static const uint16_t A_LIQ[3][8] = {
  { rgb(0, 16, 40), rgb(0, 32, 72), rgb(0, 56, 104), rgb(0, 80, 136), rgb(0, 104, 160), rgb(0, 128, 184), rgb(0, 152, 200), rgb(16, 176, 216) },
  { rgb(24, 0, 40), rgb(40, 8, 72), rgb(64, 16, 104), rgb(88, 32, 136), rgb(112, 48, 160), rgb(136, 64, 176), rgb(152, 80, 192), rgb(168, 96, 208) },
  { rgb(40, 0, 24), rgb(72, 8, 40), rgb(104, 16, 64), rgb(136, 24, 88), rgb(160, 32, 112), rgb(184, 48, 136), rgb(204, 64, 152), rgb(220, 88, 172) }
};

static void machineLava(theme::Id id)
{
  static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
  const int cw = id == theme::ATARI ? 2 : 1, ch = id == theme::AMIGA ? 1 : id == theme::C64 ? 2 : 4;
  constexpr int NL = 6;
  uint16_t wax[NL], rim[NL], liq[NL];
  Rgb waxRgb[NL], liqRgb[NL];
  if (id == theme::AMIGA) {
    ramp(wax, waxRgb, NL, AM_WAX[scheme], 3);
    ramp(liq, liqRgb, NL, AM_LIQ[scheme], 2);
    for (int i = 0; i < NL; i++) rim[i] = c12(min(15, waxRgb[i].r + 6), min(15, waxRgb[i].g + 6), min(15, waxRgb[i].b + 6));
  }
  rowJ = -2;
  for (int cy = 0; cy < PH / ch; cy++) {
    int y = cy * ch;
    float v = (y + ch * 0.5f) * (1.0f / C);
    int j = (int)v;
    float fv = v - j, depth = (y + ch * 0.5f) / PH;
    coarseRows(j);
    for (int cx = 0; cx < PW / cw; cx++) {
      int x = cx * cw;
      float u = (x + cw * 0.5f) * (1.0f / C);
      int i = (int)u;
      float fu = u - i;
      float a = rowA[i] + (rowA[i + 1] - rowA[i]) * fu, b = rowB[i] + (rowB[i + 1] - rowB[i]) * fu;
      float f = a + (b - a) * fv;
      bool isWax = f > T_IN;
      float glow = (depth - 0.3f) * 1.4f + 0.3f * min(f, T_IN) / T_IN;  // the bulb lights the lower liquid, wax gives a halo
      uint16_t c;
      if (id == theme::AMIGA) {
        float bay = BAYER[y & 3][x & 3] * (1.0f / 16);
        if (isWax) {
          int lv = constrain((int)((f - T_IN) / (1.4f - T_IN) * (NL - 1) + bay), 0, NL - 1);
          // the rim toward the bulb (portrait bottom) catches the light
          c = (f < T_IN + 0.16f && b - a < -0.02f) ? rim[lv] : wax[lv];
        } else {
          c = liq[constrain((int)(glow * (NL - 1) + bay), 0, NL - 1)];
        }
      } else if (id == theme::C64) {
        bool odd = (cx + cy) & 1;
        if (isWax) {
          const uint16_t *w = C64_WAX[scheme];
          if (f < T_IN + 0.12f) c = w[0];
          else if (f < T_IN + 0.32f) c = odd ? w[1] : w[0];
          else if (f < 1.0f) c = w[1];
          else if (f < 1.3f) c = odd ? w[2] : w[1];
          else c = w[2];
        } else {
          c = glow < 0.2f ? C64_LIQ[scheme][0] : glow < 0.65f ? (odd ? C64_LIQ[scheme][1] : C64_LIQ[scheme][0]) : C64_LIQ[scheme][1];
        }
      } else {
        // flat: two wax colors, liquid steps by row only (like a background color register changed per scanline)
        c = isWax ? A_WAX[scheme][f > 1.0f] : A_LIQ[scheme][constrain((int)(depth * 8), 0, 7)];
      }
      cell(x, y, cw, ch, c);
    }
  }
}

void Lava_Draw(const char *)
{
  if (!inited) lavaInit();
  frame_no++;
  float gx, gy, gz;
  Motion_Gravity(gx, gy, gz);
  if (fabsf(gx) > 0.5f) left_down = gx < 0;
  physics();

  if (theme::current() != theme::VFD) {
    machineLava(theme::current());
    return;
  }

  const uint8_t *wax = WAX[scheme];
  for (int cy = 0; cy < GH; cy++) {
    float y = cy * C + C / 2.0f;
    float depth = y / PH;
    for (int cx = 0; cx < GW; cx++) {
      float x = cx * C + C / 2.0f;
      float f = field(x, y);
      uint8_t c;
      if (f > T_IN) {
        c = f > 1.15f ? wax[2] : (f > T_IN + 0.22f ? wax[1] : wax[0]);
      } else {
        // liquid: dark at the top, lit by the bulb toward the bottom; a soft rim of light around the wax
        static const uint8_t BAYER[4][4] = { { 0, 8, 2, 10 }, { 12, 4, 14, 6 }, { 3, 11, 1, 9 }, { 15, 7, 13, 5 } };
        float glow = (depth - 0.3f) * 1.4f + (f > T_IN * 0.6f ? 0.25f : 0);
        bool lit = BAYER[cy & 3][cx & 3] < glow * 16;
        c = lit ? LIQ[scheme][1] : LIQ[scheme][0];
      }
      for (int k = 0; k < C * C; k++) pset(cx * C + (k & 1), cy * C + (k >> 1), c, left_down);
    }
  }
  present();
}

#endif
