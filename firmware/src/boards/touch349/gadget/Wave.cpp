#ifdef BOARD_TOUCH349

#include "Wave.h"
#include "Motion.h"
#include "Px.h"
#include "Radio.h"
#include "RetroScene.h"
#include "Theme.h"
#include "Vfd.h"

// ---------------------------------------------------------------------------------------------------------------
// Simulation (all in full-resolution screen pixels, 640x172)
// ---------------------------------------------------------------------------------------------------------------

static constexpr int FW = 640, FH = 172;
static constexpr int NC = 160;                // height-field columns
static constexpr float COLW = FW / (float)NC; // 4 px per column
static constexpr float LEVEL = 108.0f;         // surface y with the board upright (about half full)

static float dd[NC], vv[NC];  // displacement from the level surface, velocity (for the renderer)
static float slope = 0;  // kept 0: the shallow-water model carries the tilt itself
static uint32_t frame_no = 0;
static bool inited = false;

// Per-pixel surface, rebuilt every frame
static float sy[FW];       // surface y
static int16_t sI[FW];     // floor(sy)
static uint8_t sF[FW];     // fraction of sy, 0..255
static uint8_t thick[FW];  // foam band thickness (px), grows on steep or fast water

// Ship
static float ship_x = 320, ship_vx = 0, ship_y = LEVEL, ship_ang = 0;

// Foam bubbles hugging the surface and bubbles rising inside
struct Foam {
  float x, off, vx;
  uint8_t r;
};
struct Bub {
  float x, y, vy, ph;
  uint8_t r;
};
static constexpr int NF = 64, NB = 14;
static Foam foam[NF];
static Bub bubs[NB];

static float frand() { return (esp_random() & 0xffff) / 65535.0f; }
static inline int cl255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
static constexpr uint16_t pack565(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }

static float surf(float x)
{
  int i = (int)x;
  if (i < 0) i = 0;
  if (i > FW - 1) i = FW - 1;
  return sy[i];
}

static float eqAt(float x) { return LEVEL + slope * (x - FW / 2); }

// Shallow-water model: depth h per column, velocity u at the faces between columns (walls at both ends hold u = 0).
// Gravity across the glass pushes the water along the box when tilted, so a tilt sends a real wave (a bore) running
// to the low side; it hits the wall, climbs, and bounces back, several times, until friction settles it level.
static constexpr float BOTTOM = FH;           // the liquid reaches the bottom edge
static constexpr float H0 = BOTTOM - LEVEL;   // depth at rest
static float hh[NC], uu[NC + 1], flux[NC + 1];

static void depress(float centre, float amount, float width)
{
  float removed = 0;
  for (int i = 0; i < NC; i++) {
    float d = i - centre, k = amount * expf(-d * d / width);
    if (k > 0.01f) {
      float take = min(k, hh[i] - 4);
      hh[i] -= take;
      removed += take;
    }
  }
  for (int i = 0; i < NC; i++) hh[i] += removed / NC;  // the same water, pushed aside
}

void Wave_Tap(int screen_x)
{
  if (!inited) return;
  depress(screen_x / COLW, 14, 16);
  for (int k = 0, n = 0; k < NF && n < 8; k++) {  // a little foam burst
    if (foam[k].r >= 2 && (esp_random() & 3) == 0) {
      foam[k].x = constrain(screen_x + (frand() - 0.5f) * 40, 6.0f, 633.0f);
      foam[k].off = -1.0f;
      foam[k].vx = (frand() - 0.5f) * 1.4f;
      n++;
    }
  }
}

static void simStep(float gx, float gy)
{
  // units: px and frames; G sets the wave speed (about 18 px/frame at rest depth: a wave crosses in ~1.8 s)
  const int N = 8;
  const float dt = 1.0f / N, G = 340.0f / (0.75f * H0), FRICTION = 0.0035f, NU = 0.08f;
  gy = max(gy, 0.25f);
  gx = constrain(gx, -0.2f * gy, 0.2f * gy);  // steeper than this would leave one side dry
  for (int s = 0; s < N; s++) {
    for (int f = 1; f < NC; f++) {
      float slopeH = (hh[f] - hh[f - 1]) / COLW;
      float visc = (uu[f - 1] + uu[f + 1] - 2 * uu[f]) * NU;
      uu[f] += dt * (-G * gy * slopeH + G * gx) + visc;
      uu[f] *= 1 - FRICTION;
      uu[f] = constrain(uu[f], -12.0f, 12.0f);
    }
    uu[0] = uu[NC] = 0;
    for (int f = 1; f < NC; f++) {
      float hu = uu[f] > 0 ? hh[f - 1] : hh[f];  // upwind depth carries the water
      flux[f] = uu[f] * hu * dt / COLW;
      // never take more than a column holds
      if (flux[f] > 0) flux[f] = min(flux[f], hh[f - 1] * 0.45f);
      else flux[f] = max(flux[f], -hh[f] * 0.45f);
    }
    flux[0] = flux[NC] = 0;
    for (int i = 0; i < NC; i++) hh[i] = max(1.0f, hh[i] + flux[i] - flux[i + 1]);
  }
}

static void updateTank()
{
  if (hh[0] == 0)
    for (int i = 0; i < NC; i++) hh[i] = H0;
  float gx = 0, gy = 0.75f, gz = 0.6f;
  if (Motion_Ok()) Motion_Gravity(gx, gy, gz);
  float shake = Motion_Ok() ? Motion_Shake() : 0;
  if (shake > 0.12f) {
    int n = 1 + (int)(shake * 4);
    for (int k = 0; k < n; k++) uu[1 + esp_random() % (NC - 1)] += (frand() - 0.5f) * shake * 6.0f;
  }
  if (Motion_Ok() && Motion_TakeKnock()) depress(20 + esp_random() % (NC - 40), 10, 18);
  simStep(gx, gy);
  // hand the result to the renderer: displacement from the rest level, and a velocity for the foam
  slope = 0;
  for (int i = 0; i < NC; i++) {
    dd[i] = (BOTTOM - hh[i]) - LEVEL;
    vv[i] = (uu[i] + uu[i + 1]) * 0.25f;
  }
}

static void buildSurface()
{
  for (int x = 0; x < FW; x++) {
    float u = (x + 0.5f) / COLW - 0.5f;
    int i0 = (int)floorf(u);
    float f = u - i0;
    int i1 = i0 + 1;
    if (i0 < 0) i0 = 0;
    if (i1 > NC - 1) i1 = NC - 1;
    if (i0 > NC - 1) i0 = NC - 1;
    float d = dd[i0] + (dd[i1] - dd[i0]) * f;
    float v = vv[i0] + (vv[i1] - vv[i0]) * f;
    float y = eqAt(x) + d;
    y = constrain(y, 12.0f, FH - 6.0f);
    sy[x] = y;
    int yi = (int)floorf(y);
    sI[x] = yi;
    sF[x] = (uint8_t)((y - yi) * 255.0f);
    float steep = fabsf(dd[i1] - dd[i0]);
    thick[x] = (uint8_t)constrain(3.5f + fabsf(v) * 7.0f + steep * 1.1f, 3.5f, 11.0f);
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Ship: a bitmap built once (RGBA), drawn rotated with inverse mapping; and a tiny palette version for the pixel look
// ---------------------------------------------------------------------------------------------------------------

struct Texel {
  uint8_t r, g, b, a;
};
static constexpr int SHW = 150, SHH = 42;
static constexpr float PIV_X = 75, PIV_Y = 34;  // waterline, middle of the hull
static constexpr float SC = 2.0f;  // the ship is drawn twice its bitmap size
static Texel *ship = nullptr;

static void tx(int x, int y, int r, int g, int b, int a = 255)
{
  if ((unsigned)x >= (unsigned)SHW || (unsigned)y >= (unsigned)SHH) return;
  ship[y * SHW + x] = { (uint8_t)cl255(r), (uint8_t)cl255(g), (uint8_t)cl255(b), (uint8_t)cl255(a) };
}
static inline int lerpi(int a, int b, int n, int d) { return d <= 0 ? a : a + (b - a) * n / d; }
// Rectangle with a vertical gradient (x0..x1, y0..y1 inclusive)
static void vrect(int x0, int y0, int x1, int y1, const int *c0, const int *c1)
{
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      tx(x, y, lerpi(c0[0], c1[0], y - y0, y1 - y0), lerpi(c0[1], c1[1], y - y0, y1 - y0),
         lerpi(c0[2], c1[2], y - y0, y1 - y0));
}
// Rectangle with a horizontal 3-band shading (lit left, body, shadow right), for round funnels
static void funnel(int x0, int x1, int y0, int y1)
{
  int w = x1 - x0 + 1;
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      int k = (x - x0) * 6 / w;  // 0..5
      int r = k == 0 ? 236 : (k <= 3 ? 208 : (k == 4 ? 168 : 128));
      int g = k == 0 ? 104 : (k <= 3 ? 70 : (k == 4 ? 50 : 38));
      int b = k == 0 ? 72 : (k <= 3 ? 46 : (k == 4 ? 38 : 32));
      tx(x, y, r, g, b);
    }
  for (int x = x0; x <= x1; x++) {  // black cap
    for (int y = y0; y < y0 + 3; y++) tx(x, y, 24 + (x - x0) * 2, 26 + (x - x0) * 2, 34 + (x - x0) * 2);
    tx(x, y0 + 5, 236, 238, 240);  // white band
    tx(x, y0 + 6, 214, 218, 224);
  }
}

// A four-funnel ocean liner, bow to the right: long black hull with a gold line, red below the waterline, white
// decks, buff funnels with black tops raked aft, two masts with the aerial strung between them.
static int hullTop(int x) { return x > 124 ? 20 : (x < 18 ? 21 : 22); }

static void buildShip()
{
  ship = (Texel *)ps_malloc(sizeof(Texel) * SHW * SHH);
  if (!ship) return;
  memset(ship, 0, sizeof(Texel) * SHW * SHH);

  // hull: waterline y=34, keel y=39; rounded counter stern on the left, straight, slightly raked bow on the right
  for (int y = 20; y <= 39; y++) {
    float xl = y < 30 ? 4 + (y - 20) * 0.25f : 6.5f + (y - 30) * 1.6f;
    float xr = 146 - (y - 20) * 0.35f;
    for (int x = (int)xl; x <= (int)ceilf(xr); x++) {
      if (y < hullTop(x)) continue;
      float cov = fminf(fminf(x + 1 - xl, xr - x), 1.0f);
      if (cov <= 0) continue;
      int r, g, b;
      if (y == hullTop(x)) { r = 120; g = 124; b = 132; }  // deck edge catching light
      else if (y == 23) { r = 196; g = 160; b = 86; }      // gold line
      else if (y < 34) { int n = y - 22; r = lerpi(46, 16, n, 11); g = lerpi(48, 18, n, 11); b = lerpi(56, 24, n, 11); }
      else { int n = y - 34; r = lerpi(170, 96, n, 5); g = lerpi(44, 22, n, 5); b = lerpi(40, 26, n, 5); }
      float sh = 1.0f - 0.15f * fabsf(x - 75.0f) / 71.0f;
      tx(x, y, (int)(r * sh), (int)(g * sh), (int)(b * sh), (int)(cov * 255));
    }
  }
  for (int x = 10; x < 140; x += 3) tx(x, 27, 220, 196, 140), tx(x + 1, 30, 200, 170, 110);  // portholes, two rows
  // white superstructure: two decks of windows, boat deck on top, bridge forward
  const int white0[3] = { 246, 246, 244 }, white1[3] = { 196, 202, 212 };
  vrect(24, 14, 120, 21, white0, white1);
  vrect(108, 11, 120, 13, white0, white1);
  for (int x = 23; x <= 121; x++) tx(x, 13, 160, 166, 178);  // boat deck edge
  for (int x = 26; x < 118; x += 3) {
    tx(x, 16, 40, 56, 82), tx(x + 1, 16, 40, 56, 82);
    tx(x, 19, 52, 70, 98), tx(x + 1, 19, 52, 70, 98);
  }
  for (int x = 110; x < 120; x += 2) tx(x, 12, 36, 52, 80);  // bridge windows
  // lifeboats along the boat deck
  for (int k = 0; k < 8; k++) {
    int bx = 30 + k * 10 + (k > 3 ? 6 : 0);
    for (int x = bx; x < bx + 6; x++) tx(x, 11, 236, 236, 230), tx(x, 12, 190, 192, 196);
  }
  // funnels: buff, shaded round, black top, raked aft
  for (int f = 0; f < 4; f++) {
    int fx0 = 40 + f * 18;
    for (int y = 0; y <= 12; y++) {
      int sx = fx0 - (12 - y) / 4;
      for (int x = 0; x < 8; x++) {
        int k = x * 6 / 8;
        float sh = k == 0 ? 1.12f : (k <= 3 ? 1.0f : (k == 4 ? 0.8f : 0.62f));
        if (y < 3) tx(sx + x, y, (int)(30 * sh), (int)(28 * sh), (int)(30 * sh));
        else tx(sx + x, y, (int)(218 * sh), (int)(160 * sh), (int)(78 * sh));
      }
    }
  }
  // masts
  for (int y = 0; y <= 20; y++) tx(132 + (20 - y) / 10, y, 150, 140, 120);
  for (int y = 2; y <= 21; y++) tx(16 - (21 - y) / 10, y, 150, 140, 120);
}

// Pixel version, palette indices, waterline at row 17, pivot (37,17), same liner at half the size
static constexpr int PSW = 75, PSH = 21;
static constexpr float PPIV_X = 37, PPIV_Y = 17;
static uint8_t pship[PSH][PSW];

static void prect(int x0, int y0, int x1, int y1, uint8_t c)
{
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++)
      if ((unsigned)x < (unsigned)PSW && (unsigned)y < (unsigned)PSH) pship[y][x] = c;
}

static void buildPixelShip()
{
  memset(pship, px::CLR, sizeof(pship));
  for (int y = 10; y <= 20; y++) {  // hull
    int xl = y < 15 ? 2 : 3 + (y - 15), xr = 73 - (y - 10) / 4;
    for (int x = xl; x <= xr; x++) {
      int top = x > 62 ? 10 : 11;
      if (y < top) continue;
      pship[y][x] = y == top ? px::N4 : (y == 12 ? px::F3 : (y < 17 ? px::N2 : px::R2));
    }
  }
  for (int x = 5; x < 70; x += 3) pship[14][x] = px::N5;  // portholes
  prect(12, 7, 60, 10, px::N6);                         // decks
  prect(54, 5, 60, 6, px::N6);                          // bridge
  for (int x = 13; x < 59; x += 2) pship[8][x] = px::N3;  // windows
  for (int k = 0; k < 8; k++) prect(15 + k * 5 + (k > 3 ? 3 : 0), 6, 17 + k * 5 + (k > 3 ? 3 : 0), 6, px::N5);  // lifeboats
  for (int f = 0; f < 4; f++)  // funnels, raked aft
    for (int y = 0; y <= 6; y++) {
      int sx = 20 + f * 9 - (6 - y) / 3;
      prect(sx, y, sx + 3, y, y < 2 ? px::N1 : px::F3);
      if (y >= 2) pship[y][sx + 3] = px::F2;
    }
  for (int y = 0; y <= 10; y++) pship[y][66] = px::N4, pship[y + 1][8] = px::N4;  // masts
}

// ---------------------------------------------------------------------------------------------------------------
// Realistic render tables
// ---------------------------------------------------------------------------------------------------------------

static uint8_t wR[FH], wG[FH], wB[FH];   // liquid color by depth below the surface
static uint8_t fadeD[FH];                // light shaft strength by depth
static uint8_t depthQ[FH];               // depth below the surface in 1/16 shade, 0..127, for the machine renderers
static uint8_t oR[FH], oG[FH], oB[FH];   // oil color by row
static uint8_t eAdd[FW];                 // box edge reflections in the oil
static uint8_t glare[FW + FH / 3 + 8];   // diagonal glass glare, indexed by x + y/3
static uint8_t glareY[FH];
static uint8_t noise256[256];
static uint8_t shaft[FW + FH * 2 / 5 + 8];  // light shafts, rebuilt each frame, indexed by x + 2y/5
static const uint8_t BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

static void buildTables()
{
  // liquid: lighter at the surface, deep saturated blue at the bottom
  static const int stops[5][4] = { { 0, 44, 168, 238 }, { 26, 22, 108, 204 }, { 70, 9, 52, 150 }, { 120, 5, 30, 104 },
                                   { 171, 3, 18, 78 } };
  for (int d = 0; d < FH; d++) {
    int k = 0;
    while (k < 3 && d > stops[k + 1][0]) k++;
    int span = stops[k + 1][0] - stops[k][0];
    float t = span ? (float)(d - stops[k][0]) / span : 0;
    t = constrain(t, 0.0f, 1.0f);
    wR[d] = (uint8_t)(stops[k][1] + (stops[k + 1][1] - stops[k][1]) * t);
    wG[d] = (uint8_t)(stops[k][2] + (stops[k + 1][2] - stops[k][2]) * t);
    wB[d] = (uint8_t)(stops[k][3] + (stops[k + 1][3] - stops[k][3]) * t);
    fadeD[d] = (uint8_t)(255.0f * expf(-d / 55.0f));
    depthQ[d] = (uint8_t)min(127.0f, sqrtf((float)d) * 14.0f);
  }
  // oil: nearly black, a hint of blue, a touch lighter toward the liquid
  for (int y = 0; y < FH; y++) {
    float t = y / (float)(FH - 1);
    oR[y] = (uint8_t)(2 + 5 * t);
    oG[y] = (uint8_t)(5 + 11 * t);
    oB[y] = (uint8_t)(12 + 22 * t);
    glareY[y] = (uint8_t)(255 * (1.0f - 0.6f * t));
  }
  for (int x = 0; x < FW; x++) {
    float l = x, r = FW - 1 - x;
    float e = 24 * expf(-(l - 6) * (l - 6) / 3.0f) + 9 * expf(-(l - 14) * (l - 14) / 8.0f) + 6 * expf(-l / 30.0f) +
              24 * expf(-(r - 6) * (r - 6) / 3.0f) + 9 * expf(-(r - 14) * (r - 14) / 8.0f) + 6 * expf(-r / 30.0f);
    eAdd[x] = (uint8_t)constrain(e, 0.0f, 60.0f);
  }
  for (int k = 0; k < (int)sizeof(glare); k++) {
    float g = 9 * expf(-(k - 150) * (k - 150) / 500.0f) + 5 * expf(-(k - 176) * (k - 176) / 36.0f) +
              7 * expf(-(k - 470) * (k - 470) / 700.0f) + 3 * expf(-(k - 500) * (k - 500) / 30.0f);
    glare[k] = (uint8_t)constrain(g, 0.0f, 40.0f);
  }
  uint32_t h = 12345;
  for (int i = 0; i < 256; i++) {
    h = h * 1664525u + 1013904223u;
    noise256[i] = h >> 24;
  }
}

static void buildShafts()
{
  float t = frame_no * 0.05f;
  for (int k = 0; k < (int)sizeof(shaft); k++) {
    float p = sinf(k * 0.052f + t * 0.45f) * sinf(k * 0.019f - t * 0.28f);
    float v = (p - 0.15f) / 0.85f;
    shaft[k] = v > 0 ? (uint8_t)(v * 38) : 0;
  }
}

static uint16_t *cv = nullptr;

static inline void blendPx(int x, int y, int r, int g, int b, int a)  // a 0..256
{
  if ((unsigned)x >= (unsigned)FW || (unsigned)y >= (unsigned)FH || a <= 0) return;
  uint16_t &p = cv[y * FW + x];
  int pr = (p >> 8) & 0xF8, pg = (p >> 3) & 0xFC, pb = (p << 3) & 0xF8;
  pr += ((r - pr) * a) >> 8;
  pg += ((g - pg) * a) >> 8;
  pb += ((b - pb) * a) >> 8;
  p = pack565(cl255(pr), cl255(pg), cl255(pb));
}

// A glassy bubble: bright rim, faint fill, a highlight
static void bubble(float cx, float cy, int r, int rim, int fill, int cr, int cg, int cb)
{
  if (r <= 1) {
    blendPx((int)cx, (int)cy, cr, cg, cb, rim);
    blendPx((int)cx + 1, (int)cy, cr, cg, cb, rim / 2);
    blendPx((int)cx, (int)cy + 1, cr, cg, cb, rim / 2);
    return;
  }
  int x0 = (int)floorf(cx - r - 1), x1 = (int)ceilf(cx + r + 1), y0 = (int)floorf(cy - r - 1), y1 = (int)ceilf(cy + r + 1);
  for (int y = y0; y <= y1; y++)
    for (int x = x0; x <= x1; x++) {
      float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
      float d = sqrtf(dx * dx + dy * dy);
      if (d > r + 0.5f) continue;
      bool edge = d > r - 1.0f;
      int a = edge ? rim : fill;
      if (edge && d > r) a = a * 2 / 3;
      blendPx(x, y, cr, cg, cb, a);
    }
  blendPx((int)floorf(cx - r * 0.4f), (int)floorf(cy - r * 0.45f), 255, 255, 255, 230);
}

static void drawShipReal(float pxx, float pyy, float ang)
{
  if (!ship) return;
  float c = cosf(ang), s = sinf(ang);
  int rx = (int)ceilf((fabsf(c) * PIV_X + fabsf(s) * PIV_Y) * SC) + 1;
  int ry = (int)ceilf((fabsf(s) * PIV_X + fabsf(c) * PIV_Y) * SC) + 1;
  int ix = (int)lroundf(pxx), iy = (int)lroundf(pyy);
  float ox = pxx - ix, oy = pyy - iy;
  for (int dy = -ry; dy <= ry; dy++) {
    int y = iy + dy;
    if ((unsigned)y >= (unsigned)FH) continue;
    for (int dx = -rx; dx <= rx; dx++) {
      int x = ix + dx;
      if ((unsigned)x >= (unsigned)FW) continue;
      float rxp = dx + 0.5f - ox, ryp = dy + 0.5f - oy;
      float fx = (rxp * c + ryp * s) / SC + PIV_X - 0.5f;
      float fy = (-rxp * s + ryp * c) / SC + PIV_Y - 0.5f;
      int x0 = (int)floorf(fx), y0 = (int)floorf(fy);
      if (x0 < 0 || y0 < 0 || x0 >= SHW - 1 || y0 >= SHH - 1) continue;
      int fxi = (int)((fx - x0) * 256), fyi = (int)((fy - y0) * 256);
      const Texel *t00 = &ship[y0 * SHW + x0], *t10 = t00 + 1, *t01 = t00 + SHW, *t11 = t01 + 1;
      if (!(t00->a | t10->a | t01->a | t11->a)) continue;
      int w00 = (256 - fxi) * (256 - fyi) >> 8, w10 = fxi * (256 - fyi) >> 8, w01 = (256 - fxi) * fyi >> 8,
          w11 = fxi * fyi >> 8;
      int a00 = t00->a * w00, a10 = t10->a * w10, a01 = t01->a * w01, a11 = t11->a * w11;
      int A = a00 + a10 + a01 + a11;
      if (A < 256 * 6) continue;
      int r = (t00->r * a00 + t10->r * a10 + t01->r * a01 + t11->r * a11) / A;
      int g = (t00->g * a00 + t10->g * a10 + t01->g * a01 + t11->g * a11) / A;
      int b = (t00->b * a00 + t10->b * a10 + t01->b * a01 + t11->b * a11) / A;
      int alpha = A >> 8;  // 0..255
      // below the surface the hull is seen through the liquid: tinted and fading
      float depth = y + 0.5f - sy[x];
      if (depth > 0) {
        int k = (int)(depth * 40);
        if (k > 256) k = 256;
        int d = (int)depth;
        if (d > FH - 1) d = FH - 1;
        r += ((wR[d] - r) * k) >> 8;
        g += ((wG[d] - g) * k) >> 8;
        b += ((wB[d] - b) * k) >> 8;
        alpha = alpha * (256 - (k * 2 / 5)) >> 8;
      }
      blendPx(x, y, r, g, b, alpha + (alpha >> 7));
    }
  }
}

static void drawReal(const char *corner_text)
{
  cv = vfd::canvas();
  buildShafts();
  for (int y = 0; y < FH; y++) {
    uint16_t *row = cv + y * FW;
    const int orr = oR[y], og = oG[y], ob = oB[y], gy = glareY[y], sh = y * 2 / 5, gs = y / 3;
    for (int x = 0; x < FW; x++) {
      int s = sI[x];
      int ga = (glare[x + gs] * gy) >> 8;
      int dth = BAYER[((y & 3) << 2) | (x & 3)];
      if (y < s) {  // clear oil
        int a = eAdd[x] + ga;
        row[x] = pack565(cl255(orr + (a * 2 >> 2) + (dth >> 3)), cl255(og + (a * 3 >> 2) + (dth >> 3)),
                         cl255(ob + a + (dth >> 3)));
        continue;
      }
      int d = y - s;
      int r = wR[d], g = wG[d], b = wB[d];
      int a = (shaft[x + sh] * fadeD[d]) >> 8;
      r += (a >> 2) + (ga >> 1);
      g += (a * 3 >> 2) + (ga >> 1);
      b += a + ga;
      int th = thick[x];
      if (d < th) {  // foam band
        int f = (th - d) * 256 / th;
        f = f * f >> 8;
        f = f * (110 + (noise256[(x * 3 + d * 7) & 255] >> 1)) >> 8;
        f = f * 190 >> 8;
        if (d == 0) f = 235;
        else if (d == 1 && f < 120) f = 120;
        r += ((214 - r) * f) >> 8;
        g += ((238 - g) * f) >> 8;
        b += ((254 - b) * f) >> 8;
      }
      if (d == 0) {  // the surface row only half covers the pixel: blend with the oil above
        int cov = 255 - sF[x];
        int ea = eAdd[x] + ga;
        int orr2 = orr + (ea >> 1), og2 = og + (ea * 3 >> 2), ob2 = ob + ea;
        r = orr2 + (((r - orr2) * cov) >> 8);
        g = og2 + (((g - og2) * cov) >> 8);
        b = ob2 + (((b - ob2) * cov) >> 8);
      }
      row[x] = pack565(cl255(r + (dth >> 1)), cl255(g + (dth >> 2)), cl255(b + (dth >> 1)));
    }
  }

  // rising bubbles inside the liquid
  for (int i = 0; i < NB; i++) {
    const Bub &b = bubs[i];
    int xi = constrain((int)b.x, 0, FW - 1);
    if (b.y < sy[xi] + 3) continue;
    bubble(b.x, b.y, b.r, 120, 22, 170, 215, 255);
  }
  // ship, on top of the liquid body
  drawShipReal(ship_x, ship_y, ship_ang);
  // foam bubbles over everything at the surface
  for (int i = 0; i < NF; i++) {
    const Foam &f = foam[i];
    int xi = constrain((int)f.x, 0, FW - 1);
    bubble(f.x, sy[xi] + f.off, f.r, 215, 80, 222, 240, 255);
  }
  // glassy box edges on top of everything
  for (int y = 0; y < FH; y++) {
    blendPx(0, y, 175, 208, 240, 110);
    blendPx(1, y, 175, 208, 240, 52);
    blendPx(2, y, 175, 208, 240, 20);
    blendPx(FW - 1, y, 175, 208, 240, 110);
    blendPx(FW - 2, y, 175, 208, 240, 52);
    blendPx(FW - 3, y, 175, 208, 240, 20);
  }
  for (int x = 0; x < FW; x++) {
    blendPx(x, 0, 175, 208, 240, 100);
    blendPx(x, 1, 175, 208, 240, 36);
    blendPx(x, FH - 1, 175, 208, 240, 70);
  }
  if (corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 6, 2, vfd::Ink{ vfd::DIM });
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Pixel render (Ega / Vfd): 320x86 through Px
// ---------------------------------------------------------------------------------------------------------------

static void drawPixels(const char *corner_text)
{
  using namespace px;
  buildShafts();
  for (int x = 0; x < W; x++) {
    float s = sy[x * 2 + 1] * 0.5f;
    int si = (int)lroundf(s);
    int tp = thick[x * 2] / 3;
    for (int y = 0; y < H; y++) {
      uint8_t c;
      if (y < si) {  // night sky down to the water, with stars
        uint32_t h = (uint32_t)(x * 2654435761u) ^ (uint32_t)(y * 40503u);
        h ^= h >> 13;
        h *= 0x5bd1e995;
        h ^= h >> 15;
        c = (y > si - 10 && ((x + y) & 1)) ? N1 : NIGHT;
        if (h % 97 == 0) c = ((frame_no / 10 + x) % 13) ? N3 : N5;
        else if (h % 211 == 1) c = N5;
      } else {
        int d = y - si;
        int dz = d + (((x + y) & 1) ? 2 : 0);
        if (d == 0) c = N6;
        else if (d < tp) c = W5;
        else if (dz < tp + 3) c = W4;
        else if (dz < 17) c = W3;
        else if (dz < 38) c = W2;
        else c = W1;
        if (d > 4 && d < 44 && shaft[x * 2 + y * 4 / 5] > 30 && ((x + y) & 1) == 0) {
          if (c == W1) c = W2;
          else if (c == W2) c = W3;
          else if (c == W3) c = W4;
        }
      }
      buf[y][x] = c;
    }
  }
  for (int i = 0; i < NB; i++) {
    const Bub &b = bubs[i];
    int xi = constrain((int)b.x, 0, FW - 1);
    if (b.y < sy[xi] + 5) continue;
    set((int)(b.x * 0.5f), (int)(b.y * 0.5f), W5);
    if (b.r >= 3) set((int)(b.x * 0.5f) + 1, (int)(b.y * 0.5f), W4);
  }
  // ship
  float c = cosf(ship_ang), s = sinf(ship_ang);
  float sx = ship_x * 0.5f, sy_ = ship_y * 0.5f;
  int rx = (int)ceilf((fabsf(c) * PSW * 0.5f + fabsf(s) * PPIV_Y) * SC) + 1;
  int ry = (int)ceilf((fabsf(s) * PSW * 0.5f + fabsf(c) * PPIV_Y) * SC) + 1;
  int ix = (int)lroundf(sx), iy = (int)lroundf(sy_);
  for (int dy = -ry; dy <= ry; dy++)
    for (int dx = -rx; dx <= rx; dx++) {
      int x = ix + dx, y = iy + dy;
      if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) continue;
      float fx = (dx * c + dy * s) / SC + PPIV_X, fy = (-dx * s + dy * c) / SC + PPIV_Y;
      int sxp = (int)floorf(fx), syp = (int)floorf(fy);
      if ((unsigned)sxp >= (unsigned)PSW || (unsigned)syp >= (unsigned)PSH) continue;
      uint8_t pc = pship[syp][sxp];
      if (pc == CLR) continue;
      if (y > sy[x * 2 + 1] * 0.5f + 2.0f) continue;  // under the water line
      buf[y][x] = pc;
    }
  // foam dots on top
  for (int i = 0; i < NF; i++) {
    const Foam &f = foam[i];
    int xi = constrain((int)f.x, 0, FW - 1);
    int px_ = (int)(f.x * 0.5f), py_ = (int)((sy[xi] + f.off) * 0.5f);
    if (f.r >= 2) set(px_, py_, W5);
  }
  present();
  if (corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 6, 2, vfd::Ink{ px::ink(N5) });
  }
}


// ---------------------------------------------------------------------------------------------------------------
// Machine render (C64, Amiga, 2600): the night sky picture, the water on the machine's grid, the liner as a sprite
// ---------------------------------------------------------------------------------------------------------------

namespace {

enum Mach { M_C64, M_AMIGA, M_ATARI };
struct MachLook {
  const char *sky, *ship;
  int gw, gh;       // machine pixel in screen px
  float pivot_row;  // sprite row of the hull bottom edge that sits on the water; the rows below it are hidden
};
// Pivots measured on the sprites (assets/art/ship_<m>.img): where the lit hull ends and the black underwater strip begins.
const MachLook LOOKS[] = {
  { "sky_stars_c64", "ship_c64", 4, 2, 39 },
  { "sky_stars_amiga", "ship_amiga", 2, 2, 32 },
  { "sky_stars_atari", "ship_atari", 8, 4, 34 },
};

constexpr uint16_t c12(int r, int g, int b) { return pack565(r * 17, g * 17, b * 17); }
constexpr uint16_t WHITE = pack565(255, 255, 255);
// C64 (Pepto) and 2600 (Stella NTSC) colors
constexpr uint16_t C_BLACK = 0, C_BLUE = pack565(53, 40, 121), C_LBLUE = pack565(108, 94, 181),
                   C_CYAN = pack565(112, 164, 178);
constexpr uint16_t A_WATER[4] = { pack565(0x7c, 0x9c, 0xdc), pack565(0x50, 0x70, 0xbc), pack565(0x38, 0x54, 0xa8),
                                  pack565(0x1c, 0x38, 0x90) };
constexpr uint16_t A_WHITE = pack565(0xec, 0xec, 0xec);
// Amiga: 12-bit blue-greens from the surface down and light foam steps
constexpr uint16_t AM_WATER[8] = { c12(3, 9, 12), c12(2, 7, 11), c12(2, 6, 10), c12(1, 5, 9),
                                   c12(1, 4, 8),  c12(0, 3, 7),  c12(0, 2, 5),  c12(0, 1, 4) };
constexpr uint16_t AM_FOAM0 = c12(15, 15, 15), AM_FOAM1 = c12(11, 15, 15), AM_FOAM2 = c12(6, 13, 15);

// Screen pixels of a cell row start nearest to the surface at screen y s.
inline int snapRow(float s, int gh) { return (int)(s / gh + 0.5f) * gh; }

inline void cellFill(int x, int y, int w, int h, uint16_t c)
{
  if ((unsigned)(y + h - 1) >= (unsigned)FH || (unsigned)(x + w - 1) >= (unsigned)FW) return;
  for (int j = 0; j < h; j++) {
    uint16_t *p = cv + (y + j) * FW + x;
    for (int i = 0; i < w; i++) p[i] = c;
  }
}

uint16_t waterCell(int m, int cx, int cy, int d, int th, int shaftA)
{
  int v = depthQ[d];
  bool chk = (cx + cy) & 1;
  if (m == M_AMIGA) {
    if (d < th) {  // foam band, dithered into the water
      int fr = (th - d) * 16 / th + BAYER[((cy & 3) << 2) | (cx & 3)] - 8;
      if (d == 0 || fr >= 11) return AM_FOAM0;
      if (fr >= 6) return AM_FOAM1;
      if (fr >= 3) return AM_FOAM2;
    }
    v = max(0, v - (shaftA >> 1));
    return AM_WATER[min(7, (v + BAYER[((cy & 3) << 2) | (cx & 3)]) >> 4)];
  }
  if (m == M_C64) {
    if (d < th) {
      int fr = (th - d) * 16 / th;
      if (d == 0 || fr >= 9 || (fr >= 4 && chk)) return WHITE;
      if (fr >= 2 && !chk) return C_CYAN;
    }
    int band = v >> 5, fr = v & 31;
    if (fr >= 22 || (fr >= 11 && chk)) band++;
    if (shaftA > 18 && d > 3 && d < 50 && chk) band--;
    band = constrain(band, 0, 3);
    static const uint16_t BAND[3] = { C_CYAN, C_LBLUE, C_BLUE };
    return band < 3 ? BAND[band] : (chk ? C_BLUE : C_BLACK);
  }
  if (d < 4) return A_WHITE;  // the first block row is foam
  if (d < th && th >= 6 && d < 8 && ((cx * 7 + cy * 3) & 1)) return A_WHITE;
  return A_WATER[min(3, v >> 5)];
}

void drawMachineWater(int m)
{
  const MachLook &lk = LOOKS[m];
  for (int cx = 0; cx < FW / lk.gw; cx++) {
    int x0 = cx * lk.gw, xs = x0 + lk.gw / 2;
    int sp = snapRow(sy[xs], lk.gh), th = thick[xs];
    for (int y0 = sp; y0 < FH; y0 += lk.gh) {
      int d = y0 - sp;
      int a = m == M_ATARI ? 0 : (shaft[x0 + y0 * 2 / 5] * fadeD[d]) >> 8;
      cellFill(x0, y0, lk.gw, lk.gh, waterCell(m, cx, y0 / lk.gh, d, th, a));
    }
  }
}

void drawMachineShip(int m, const RetroSprite *sp)
{
  const MachLook &lk = LOOKS[m];
  float c = cosf(ship_ang), s = sinf(ship_ang);
  float hw = sp->w * sp->sx * 0.5f, hh = sp->h * sp->sy;
  int rx = (int)ceilf(fabsf(c) * hw + fabsf(s) * hh) + lk.gw, ry = (int)ceilf(fabsf(s) * hw + fabsf(c) * hh) + lk.gh;
  int cx0 = max(0, (int)(ship_x - rx) / lk.gw), cx1 = min(FW / lk.gw - 1, (int)(ship_x + rx) / lk.gw);
  int cy0 = max(0, (int)(ship_y - ry) / lk.gh), cy1 = min(FH / lk.gh - 1, (int)(ship_y + ry) / lk.gh);
  float isx = 1.0f / sp->sx, isy = 1.0f / sp->sy;
  for (int cy = cy0; cy <= cy1; cy++) {
    float dy = cy * lk.gh + lk.gh * 0.5f - ship_y;
    for (int cx = cx0; cx <= cx1; cx++) {
      float dx = cx * lk.gw + lk.gw * 0.5f - ship_x;
      // rotate in screen pixels, then into the sprite's own (maybe non-square) pixels
      int u = (int)floorf((dx * c + dy * s) * isx + sp->w * 0.5f);
      int v = (int)floorf((-dx * s + dy * c) * isy + lk.pivot_row);
      if ((unsigned)u >= sp->w || (unsigned)v >= sp->h) continue;
      uint8_t i = sp->idx[v * sp->w + u];
      if (sp->keyed && !i) continue;
      int x = cx * lk.gw, y = cy * lk.gh;
      if (y >= snapRow(sy[x + lk.gw / 2], lk.gh) + 2) continue;  // under the water line
      cellFill(x, y, lk.gw, lk.gh, sp->pal[i]);
    }
  }
}

void drawMachineParticles(int m, bool foamPass)
{
  const MachLook &lk = LOOKS[m];
  if (foamPass) {
    for (int i = 0; i < NF; i++) {
      const Foam &f = foam[i];
      if (f.r < 2 && m != M_AMIGA) continue;
      int xi = constrain((int)f.x, 0, FW - 1);
      cellFill((int)f.x / lk.gw * lk.gw, (int)(sy[xi] + f.off) / lk.gh * lk.gh, lk.gw, lk.gh,
               m == M_AMIGA ? (f.r >= 2 ? AM_FOAM0 : AM_FOAM1) : (m == M_C64 ? WHITE : A_WHITE));
    }
    return;
  }
  for (int i = 0; i < NB; i++) {
    const Bub &b = bubs[i];
    int xi = constrain((int)b.x, 0, FW - 1);
    if (b.y < sy[xi] + 5 || (m == M_ATARI && b.r < 2)) continue;
    uint16_t col = m == M_AMIGA ? AM_FOAM2 : (m == M_C64 ? (b.r >= 2 ? WHITE : C_CYAN) : A_WHITE);
    int gx = (int)b.x / lk.gw * lk.gw, gy = (int)b.y / lk.gh * lk.gh;
    cellFill(gx, gy, lk.gw, lk.gh, col);
    if (b.r >= 3 && m != M_ATARI) cellFill(gx + lk.gw, gy, lk.gw, lk.gh, col);
  }
}

}  // namespace

// false = the asset pack lacks this theme's sky or ship: the caller draws the plain pixel version.
static bool drawMachine(const char *corner_text, int m)
{
  const MachLook &lk = LOOKS[m];
  const RetroSprite *sp = RetroSprite_Get(lk.ship);
  if (!sp || !RetroPicture_Draw(lk.sky)) return false;
  cv = vfd::canvas();
  buildShafts();
  drawMachineWater(m);
  drawMachineParticles(m, false);
  drawMachineShip(m, sp);
  drawMachineParticles(m, true);
  if (corner_text && *corner_text) {  // the sky is busy: black outline, one font pixel wide
    int w = vfd::textWidth(corner_text, 2), x = vfd::W - 12 - w;
    for (int dy = -2; dy <= 2; dy += 2)
      for (int dx = -2; dx <= 2; dx += 2)
        if (dx || dy) vfd::text(corner_text, x + dx, 6 + dy, 2, vfd::Ink{ 0 });
    vfd::text(corner_text, x, 6, 2, vfd::LIT_INK);
  }
  return true;
}

// ---------------------------------------------------------------------------------------------------------------

static void waveInit()
{
  inited = true;
  buildTables();
  buildShip();
  buildPixelShip();
  for (int i = 0; i < NF; i++) {
    foam[i].x = frand() * (FW - 12) + 6;
    foam[i].off = -0.5f + frand() * 9.0f;
    foam[i].vx = (frand() - 0.5f) * 0.5f;
    foam[i].r = frand() < 0.45f ? 1 : (frand() < 0.7f ? 2 : 3);
  }
  for (int i = 0; i < NB; i++) {
    bubs[i].x = frand() * (FW - 40) + 20;
    bubs[i].y = FH - 6 - frand() * 80;
    bubs[i].vy = 0.25f + frand() * 0.5f;
    bubs[i].ph = frand() * 6.28f;
    bubs[i].r = 1 + (esp_random() % 3);
  }
  memset(dd, 0, sizeof(dd));
  memset(vv, 0, sizeof(vv));
  buildSurface();
}

static void updateShip()
{
  // the liner stays in the middle: it only rides up and down with the water and pitches with the slope under it
  ship_x = FW / 2;
  ship_vx = 0;
  float ty = 0;
  for (int k = -3; k <= 3; k++) ty += surf(ship_x + k * 40);
  ty /= 7;
  ship_y += (ty - ship_y) * 0.3f;
  float ta = atan2f(surf(ship_x + 120) - surf(ship_x - 120), 240.0f);
  ship_ang += (constrain(ta, -0.35f, 0.35f) - ship_ang) * 0.25f;
}

static void updateParticles()
{
  for (int i = 0; i < NF; i++) {
    Foam &f = foam[i];
    int xi = constrain((int)f.x, 2, FW - 3);
    float sl = (sy[xi + 2] - sy[xi - 2]) * 0.25f;
    f.x += f.vx + sl * 0.5f;
    if (f.x < 5) {
      f.x = 5;
      f.vx = fabsf(f.vx);
    }
    if (f.x > FW - 6) {
      f.x = FW - 6;
      f.vx = -fabsf(f.vx);
    }
    f.off += (frand() - 0.5f) * 0.25f;
    f.off = constrain(f.off, -1.5f, 3.0f + thick[xi] * 0.8f);
    f.vx *= 0.99f;
    if ((esp_random() & 511) == 0) {
      f.x = frand() * (FW - 12) + 6;
      f.vx = (frand() - 0.5f) * 0.5f;
    }
  }
  for (int i = 0; i < NB; i++) {
    Bub &b = bubs[i];
    b.y -= b.vy;
    b.ph += 0.15f;
    b.x += sinf(b.ph) * 0.3f;
    int xi = constrain((int)b.x, 0, FW - 1);
    if (b.y < sy[xi] + 3) {
      b.x = frand() * (FW - 40) + 20;
      b.y = FH - 4 - frand() * 20;
      b.vy = 0.25f + frand() * 0.5f;
      b.r = 1 + (esp_random() % 3);
    }
  }
}

// ---------------------------------------------------------------------------------------------------------------
// Hold: the fog horn and a puff of smoke from a funnel. Rarely a whale surfaces and spouts. Drawn in screen pixels
// over whatever renderer the theme uses, in the scene palette so every theme recolors it.
// ---------------------------------------------------------------------------------------------------------------

struct Puff {
  float x, y, vx, vy;
  uint8_t age;  // frames, 0 = unused
};
static Puff puffs[14];
static int horn_t = -1;         // frames since the horn sounded
static int whale_t = -1;        // frames since the whale surfaced
static uint32_t whale_next = 0; // frame_no of the next visit
static float whale_x = 160;
static int whale_dir = 1;

void Wave_Hold()
{
  horn_t = 0;
  if (Radio_Ready() && !Radio_On()) Radio_Beep(98, 1100);  // a long low blast, only if nothing else is playing
}

static void extrasStep()
{
  if (horn_t >= 0) {
    if (horn_t % 3 == 0) {  // a puff from the second funnel, raked and tilted with the ship
      float c = cosf(ship_ang), s = sinf(ship_ang), dx = -24, dy = -68;
      for (auto &p : puffs)
        if (!p.age) {
          p = { ship_x + dx * c - dy * s, ship_y + dx * s + dy * c, 0.5f + frand() * 0.3f, -(1.4f + frand() * 0.5f), 1 };
          break;
        }
    }
    if (++horn_t > 24) horn_t = -1;
  }
  for (auto &p : puffs) {
    if (!p.age) continue;
    p.x += p.vx;
    p.y += p.vy;
    p.vy *= 0.985f;
    p.vx += 0.01f;
    if (++p.age > 90) p.age = 0;
  }
  if (!whale_next) whale_next = frame_no + 3600 + esp_random() % 7200;  // 3 to 9 minutes
  if (whale_t < 0 && frame_no >= whale_next) {
    whale_t = 0;
    whale_dir = esp_random() & 1 ? 1 : -1;
    whale_x = (esp_random() & 1 ? 130 : 510) + frand() * 60;  // clear of the ship in the middle
  } else if (whale_t >= 0 && ++whale_t > 110) {
    whale_t = -1;
    whale_next = frame_no + 3600 + esp_random() % 7200;
  }
}

static void extrasDraw()
{
  using namespace px;
  for (auto &p : puffs) {
    if (!p.age) continue;
    int r = 2 + p.age / 14, x = (int)p.x, y = (int)p.y;
    uint16_t c = ink(p.age < 40 ? N5 : N4);
    if (p.age < 55) {
      vfd::rect(x - r, y - r, 2 * r, 2 * r, c);
    } else {  // thinning out: a checker of 2x2 cells
      for (int yy = -r; yy < r; yy += 2)
        for (int xx = -r; xx < r; xx += 2)
          if (((xx + yy) / 2) % 2 == 0) vfd::rect(x + xx, y + yy, 2, 2, c);
    }
  }
  if (whale_t < 0) return;
  float hf = whale_t < 25 ? whale_t / 25.0f : whale_t < 85 ? 1.0f : max(0.0f, (110 - whale_t) / 25.0f);
  hf = hf * hf * (3 - 2 * hf);
  const float HW = 50, HH = 26 * hf;
  uint16_t body = ink(N3), back = ink(N4), shine = ink(N5), foam = ink(N6), eye = ink(N1);
  int blow_x = (int)(whale_x + 0.35f * whale_dir * HW), blow_y = 0;
  for (int x = (int)(whale_x - HW); x <= (int)(whale_x + HW); x++) {
    float u = (x - whale_x) / HW;
    float h = HH * sqrtf(max(0.0f, 1 - u * u)) * (1 + 0.15f * u * whale_dir);
    if (fabsf(u + 0.45f * whale_dir) < 0.08f) h += 8 * hf * (1 - fabsf(u + 0.45f * whale_dir) / 0.08f);  // the dorsal fin
    int ix = constrain(x, 0, FW - 1), top = (int)(sy[ix] - h), n = (int)h + 1;
    if (n < 2) continue;
    vfd::rect(x, top, 1, n, body);
    vfd::rect(x, top, 1, 3, back);
    vfd::rect(x, top, 1, 1, shine);
    if (x == blow_x) blow_y = top;
  }
  int ex = (int)(whale_x + 0.72f * whale_dir * HW);
  vfd::rect(ex, (int)(sy[constrain(ex, 0, FW - 1)] - HH * 0.6f), 2, 2, eye);
  vfd::rect((int)(whale_x - HW) - 2, (int)sy[constrain((int)(whale_x - HW), 0, FW - 1)] - 1, 5, 2, foam);
  vfd::rect((int)(whale_x + HW) - 2, (int)sy[constrain((int)(whale_x + HW), 0, FW - 1)] - 1, 5, 2, foam);
  if (whale_t > 20 && whale_t < 85 && blow_y) {  // the spout: drops up and back down in a fan
    int n = whale_t < 40 ? (whale_t - 20) * 14 / 20 : whale_t > 70 ? (85 - whale_t) * 14 / 15 : 14;
    for (int i = 0; i < n; i++) {
      float tt = fmodf(whale_t * 0.9f + i * 3, 28.0f);
      int x = blow_x + (int)(((i % 5) - 2) * tt * 0.3f), y = blow_y - (int)(tt * (1.3f - tt * 0.024f));
      vfd::rect(x, y, 2, 2, i % 3 ? foam : ink(W5));
    }
  }
}

void Wave_Draw(const char *corner_text)
{
  if (!inited) waveInit();
  frame_no++;
  updateTank();
  buildSurface();
  updateShip();
  updateParticles();
  theme::Id id = theme::current();
  if (px::theme() == px::Theme::Natural) {
    drawReal(corner_text);
  } else if (id == theme::VFD || !drawMachine(corner_text, id == theme::C64 ? M_C64 : (id == theme::AMIGA ? M_AMIGA : M_ATARI))) {
    drawPixels(corner_text);
  }
  extrasStep();
  extrasDraw();
}

#endif  // BOARD_TOUCH349
