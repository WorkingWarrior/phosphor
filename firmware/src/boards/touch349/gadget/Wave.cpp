#ifdef BOARD_TOUCH349

#include "Wave.h"
#include "Motion.h"
#include "Px.h"
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
static inline uint16_t pack565(int r, int g, int b) { return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)); }

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

void Wave_Draw(const char *corner_text)
{
  if (!inited) waveInit();
  frame_no++;
  updateTank();
  buildSurface();
  updateShip();
  updateParticles();
  if (px::theme() == px::Theme::Natural) {
    drawReal(corner_text);
  } else {
    drawPixels(corner_text);
  }
}

#endif  // BOARD_TOUCH349
