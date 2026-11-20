#ifdef BOARD_TOUCH349

#include "Lava.h"
#include "Motion.h"
#include "Px.h"

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

void Lava_Draw(const char *)
{
  if (!inited) lavaInit();
  frame_no++;
  float gx, gy, gz;
  Motion_Gravity(gx, gy, gz);
  if (fabsf(gx) > 0.5f) left_down = gx < 0;
  physics();

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
