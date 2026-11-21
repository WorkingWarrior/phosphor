#ifdef BOARD_TOUCH349

#include "Fireplace.h"
#include "Px.h"
#include "RetroScene.h"
#include "Vfd.h"

using namespace px;

static constexpr int FX = 54, FY = 36, FW = 92, FH = 36;  // the firebox opening
static uint32_t frame_no = 0, awake_until = 0, flick_at = 0;
static float glow = 0.6f;

static float frand() { return (esp_random() & 0xffff) / 65535.0f; }
static uint32_t hash32(uint32_t v)
{
  v ^= v >> 16;
  v *= 0x7feb352d;
  v ^= v >> 15;
  v *= 0x846ca68b;
  return v ^ (v >> 16);
}

void Fireplace_Tap()
{
  awake_until = millis() + 4500;
  RetroScene_Stir(0.6f);  // with the theme's picture: the fire flares up
}

// Flame tongues, thick at the base and thin at the tip, three layers (outer, body, hot core) that flicker and sway.
static constexpr int TONGUES = 7;
static void flames()
{
  float t = frame_no * 0.05f;
  int base = FY + FH - 5;
  float total = 0;
  struct T {
    float x, w, h, ph;
  } tg[TONGUES];
  for (int i = 0; i < TONGUES; i++) {
    float centre = 1.0f - fabsf(i - (TONGUES - 1) / 2.0f) / (TONGUES / 2.0f);  // 1 in the middle, 0 at the ends
    float n = sinf(t * 3.1f + i * 1.7f) * 0.5f + sinf(t * 5.3f + i * 0.9f) * 0.3f + sinf(t * 1.3f + i) * 0.2f;
    tg[i] = { FX + 16 + i * (FW - 32) / (TONGUES - 1.0f) + sinf(t * 0.7f + i) * 1.5f, 6.5f + centre * 3.5f,
              (12 + centre * 16) * (0.82f + 0.18f * n), i * 1.3f };
    total += tg[i].h;
  }
  static const uint8_t LAYER[3] = { F2, F3, F5 };
  static const float KW[3] = { 1.0f, 0.62f, 0.3f }, KH[3] = { 1.0f, 0.78f, 0.45f };
  for (int l = 0; l < 3; l++) {
    for (int i = 0; i < TONGUES; i++) {
      float h = tg[i].h * KH[l], w = tg[i].w * KW[l];
      for (int y = 0; y < (int)h; y++) {
        float f = y / h;
        float hw = w * powf(1 - f, 0.62f) * (f < 0.12f ? 0.75f + f * 2 : 1);
        float sway = sinf(t * 4.2f + y * 0.28f + tg[i].ph) * f * 2.6f;
        int x0 = lroundf(tg[i].x + sway - hw), x1 = lroundf(tg[i].x + sway + hw);
        uint8_t c = LAYER[l];
        if (l == 0 && f > 0.72f) c = R2;  // the tips go dark red
        if (l == 2 && f < 0.35f) c = Y1;
        for (int x = x0; x <= x1; x++)
          if (x > FX && x < FX + FW - 1 && base - y > FY) set(x, base - y, c);
      }
    }
  }
  glow += (total / (TONGUES * 22.0f) - glow) * 0.3f;
  // sparks drifting up
  for (int k = 0; k < 4; k++) {
    uint32_t h = hash32(k * 97 + (frame_no / 40) * 13);
    float p = (frame_no % 40) / 40.0f;
    int x = FX + 20 + h % (FW - 40) + (int)(sinf(p * 6 + k) * 2), y = base - 6 - (int)(p * 28);
    if (y > FY + 1 && ((h >> 8) & 1)) set(x, y, p < 0.6f ? F4 : F2);
  }
}

static void lights()
{
  // a string of little bulbs across the top of the wall, slowly twinkling
  static const uint8_t BULB[] = { F4, M2, C3, G3 };
  for (int x = 0; x < W; x++) {
    int seg = x % 80;
    int y = 2 + (int)(sinf(seg / 80.0f * PI) * 4);
    set(x, y, N3);
    if (x % 10 == 5) {
      int i = (x / 10) % 4;
      bool on = ((frame_no / 14) + x / 10) % 5 != 0;
      set(x, y + 1, on ? BULB[i] : N2);
      set(x, y + 2, on ? BULB[i] : N2);
    }
  }
}

// A big window right of the fireplace: night, moon, snowy pines on a hill, snow falling.
static void bigWindow()
{
  const int x0 = 180, y0 = 11, w = 84, h = 40;
  rect(x0 - 3, y0 - 3, w + 6, h + 6, D2);
  for (int y = 0; y < h; y++) rect(x0, y0 + y, w, 1, y < 14 ? NIGHT : (y < 28 ? N1 : N2));
  for (int i = 0; i < 14; i++) {  // stars
    uint32_t r = hash32(i * 53 + 1);
    set(x0 + r % w, y0 + (r >> 8) % 16, (frame_no / 9 + i) % 17 ? N3 : N5);
  }
  disc(x0 + 64, y0 + 9, 4, N6);
  disc(x0 + 66, y0 + 8, 3, NIGHT);
  for (int x = 0; x < w; x++) {  // hill and snow
    int top = h - 8 + (int)(sinf(x * 0.07f) * 2);
    for (int y = top; y < h; y++) set(x0 + x, y0 + y, y == top ? N6 : N5);
  }
  static const int PINES[][2] = { { 8, 16 }, { 20, 22 }, { 31, 14 }, { 50, 19 }, { 62, 13 }, { 76, 17 } };
  for (auto &p : PINES) {
    int px0 = x0 + p[0], base = y0 + h - 7;
    for (int y = 0; y < p[1]; y++) {
      int hw = y * 5 / p[1] + (y % 4 == 3);
      for (int x = -hw; x <= hw; x++) set(px0 + x, base - p[1] + y, (y % 4 == 0 && abs(x) == hw) ? N5 : G1);
    }
    rect(px0, base, 1, 2, D1);
  }
  for (int i = 0; i < 26; i++) {  // falling snow
    uint32_t r = hash32(i * 31 + 7);
    int x = x0 + (int)(r % w + sinf(frame_no * 0.05f + i) * 1.5f) % w;
    int y = y0 + (int)((r >> 8) % h + frame_no * (0.15f + (r % 5) * 0.04f)) % h;
    set(x, y, N6);
  }
  // frame: mullions, sill, short curtains
  rect(x0 + w / 2 - 1, y0, 2, h, D2);
  rect(x0, y0 + h / 2 - 1, w, 2, D2);
  rect(x0 - 5, y0 + h + 3, w + 10, 2, D3);
  for (int y = y0 - 4; y < y0 + h + 4; y++) {
    int sw = 5 + (int)(sinf(y * 0.25f) * 1.2f);
    for (int x = 0; x < sw; x++) set(x0 - 7 + x, y, x % 3 ? M1 : R1), set(x0 + w + 6 - x, y, x % 3 ? M1 : R1);
  }
}

static void hearthThings()
{
  // poker stand left of the hearth, log basket right of it, a tall plant in the corner
  rect(26, 52, 1, 20, N4);
  rect(24, 71, 6, 2, N3);
  for (int k = 0; k < 3; k++) rect(25 + k * 2, 52, 1, 3, N5);
  rect(170, 63, 14, 10, D1);
  for (int x = 170; x < 184; x += 3) rect(x, 63, 1, 10, D2);
  for (int k = 0; k < 3; k++) disc(173 + k * 4, 61, 2, D3), set(173 + k * 4, 61, F1);
  rect(5, 62, 12, 10, R2);
  rect(4, 61, 14, 2, R1);
  for (int k = 0; k < 5; k++)
    for (int j = 0; j < 22; j++) {
      float a = -1.9f + k * 0.45f;
      set(11 + (int)(cosf(a) * j * 0.5f), 60 - (int)(fabsf(sinf(a)) * j) + j * j / 40, j % 3 ? G2 : G3);
    }
}

static void room()
{
  // wall: dark, faint vertical paper stripes; skirting; wooden floor with planks
  for (int y = 0; y < 72; y++)
    for (int x = 0; x < W; x++) buf[y][x] = (x % 12 == 0) ? D1 : N1;
  for (int x = 0; x < W; x++) buf[71][x] = D2;
  for (int y = 72; y < H; y++)
    for (int x = 0; x < W; x++) buf[y][x] = ((x + (y / 4) * 37) % 46 == 0 || y % 4 == 0) ? D1 : D2;
  // brick surround and mantel
  for (int y = 20; y < 72; y++) {
    for (int x = 36; x < 164; x++) {
      int row = (y - 20) / 4, off = row % 2 ? 6 : 0;
      bool mortar = (y - 20) % 4 == 3 || (x + off) % 12 == 0;
      buf[y][x] = mortar ? N2 : ((hash32(x / 12 * 13 + row) % 3) ? R1 : R2);
    }
  }
  rect(30, 14, 140, 4, D3);
  rect(32, 18, 136, 2, D2);
  rect(FX - 4, FY - 4, FW + 8, FH + 4, N2);
  rect(FX, FY, FW, FH, NIGHT);
  // hearth stone
  rect(32, 72, 136, 3, N3);
  rect(32, 75, 136, 1, N2);
  // bookshelf on the right
  rect(272, 8, 44, 64, D1);
  for (int s = 0; s < 3; s++) {
    int sy = 10 + s * 20;
    for (int b = 0, x = 275; x < 312; b++) {
      int w = 2 + hash32(s * 17 + b) % 3, h = 12 + hash32(s * 7 + b) % 6;
      static const uint8_t BOOKS[] = { R2, U2, G2, F2, P1, N4, D3 };
      rect(x, sy + 18 - h, w, h, BOOKS[hash32(s * 31 + b) % 7]);
      x += w + 1;
    }
    rect(272, sy + 18, 44, 2, D3);
  }
}

// The cat, built from shapes: a body that breathes, a round head, ears, a tail around the front.
static void cat()
{
  static uint8_t mask[30][70];
  memset(mask, 0, sizeof(mask));
  const int ox = 206, oy = 54;  // mask origin on screen
  float t = frame_no * 0.05f;
  bool awake = millis() < awake_until;
  float breath = sinf(t * 1.6f) * 0.7f;
  auto paint = [&](int x, int y, uint8_t v) {
    if (x >= 0 && x < 70 && y >= 0 && y < 30) mask[y][x] = v;
  };
  // body
  for (int y = 0; y < 30; y++) {
    for (int x = 0; x < 70; x++) {
      float dx = (x - 36) / 22.0f, dy = (y - 19) / (9.5f + breath);
      if (dx * dx + dy * dy <= 1 && y <= 25) paint(x, y, 1);
    }
  }
  // head: lifted when awake
  int hx = 13, hy = awake ? 14 : 18;
  for (int y = -7; y <= 7; y++)
    for (int x = -8; x <= 8; x++)
      if (x * x * 0.8f + y * y <= 49) paint(hx + x, hy + y, 1);
  for (int k = 0; k < 5; k++) {  // ears
    for (int x = -k / 2; x <= k / 2; x++) {
      paint(hx - 5 + x, hy - 10 + k, 1);
      paint(hx + 5 + x, hy - 10 + k, 1);
    }
  }
  // tail: from the back around the front, the tip flicking now and then
  uint32_t now = millis();
  if (now > flick_at + 4000) flick_at = now + 2000 + esp_random() % 6000;
  float flick = (now > flick_at && now < flick_at + 700) ? sinf((now - flick_at) * 0.02f) * 3 : 0;
  for (int k = 0; k < 40; k++) {
    float a = k / 40.0f;
    float x = 58 - a * 40, y = 24 + sinf(a * PI) * 1.5f;
    if (k > 32) y -= (k - 32) * 0.5f + flick * (k - 32) / 8.0f;
    for (int d = -1; d <= 1; d++) paint(lroundf(x), lroundf(y) + d, k > 35 ? 3 : 1);
  }
  // chest and muzzle white
  for (int y = -2; y <= 3; y++)
    for (int x = -3; x <= 3; x++)
      if (x * x + y * y < 10) paint(hx + x, hy + 3 + y, 3);
  // paint with stripes, outline in dark amber
  for (int y = 0; y < 30; y++) {
    for (int x = 0; x < 70; x++) {
      if (!mask[y][x]) continue;
      bool edge = y == 0 || x == 0 || !mask[y - 1][x] || !mask[y][x - 1] || (x < 69 && !mask[y][x + 1]) ||
                  (y < 29 && !mask[y + 1][x]);
      uint8_t c = mask[y][x] == 3 ? N6 : (((x * 2 + y) % 9 < 2 && x > 20) ? F2 : F3);
      if (edge) c = F1;
      set(ox + x, oy + y, c);
    }
  }
  // face
  set(ox + hx, oy + hy + 2, R3);  // nose
  if (awake && (millis() / 120) % 25 != 0) {
    for (int e : { -4, 3 }) {
      set(ox + hx + e, oy + hy - 1, Y1), set(ox + hx + e + 1, oy + hy - 1, Y1);
      set(ox + hx + e, oy + hy, Y1), set(ox + hx + e + 1, oy + hy, N1);
    }
  } else {
    for (int e : { -4, 3 }) set(ox + hx + e, oy + hy, F1), set(ox + hx + e + 1, oy + hy, F1);
  }
  set(ox + hx - 5, oy + hy - 9, R3), set(ox + hx + 5, oy + hy - 9, R3);  // inner ears
  // zzz while asleep
  if (!awake) {
    for (int i = 0; i < 3; i++) {
      float ph = fmodf(t * 0.18f + i / 3.0f, 1.0f);
      int zx = ox + hx + 6 + (int)(ph * 14), zy = oy + hy - 10 - (int)(ph * 22);
      uint8_t c = ph < 0.6f ? C2 : C1;
      for (int k = 0; k < 3; k++) set(zx + k, zy, c), set(zx + k, zy + 2, c);
      set(zx + 1, zy + 1, c);
    }
  }
}

void Fireplace_Draw(const char *corner_text)
{
  if (RetroScene_Draw(RetroArt::Fire, corner_text)) return;  // the theme's own picture of this scene
  frame_no++;
  room();
  lights();
  bigWindow();
  hearthThings();
  // rug
  for (int y = 74; y < 84; y++) {
    int inset = (84 - y) / 3;
    for (int x = 196 + inset; x < 286 - inset; x++) buf[y][x] = (y == 74 || y == 83) ? M1 : ((x + y) % 6 < 3 ? R2 : R1);
  }
  // firebox: soot-dark back, warmed bricks near the flames, then the flames and the logs over their base
  for (int y = FY; y < FY + FH; y++)
    for (int x = FX; x < FX + FW; x++)
      if (((x + y) & 1) == 0 && y > FY + FH / 2 && hash32(x * 7 + y) % 3 == 0) set(x, y, R1);
  flames();
  rect(FX + 10, FY + FH - 3, FW - 20, 3, F1);  // ember bed
  for (int x = FX + 12; x < FX + FW - 12; x += 3) set(x, FY + FH - 2, (hash32(x + frame_no / 5) & 1) ? F3 : F2);
  for (int k = 0; k < 3; k++) {
    int lx = FX + 14 + k * 22, ly = FY + FH - 6 + (k == 1 ? -3 : 0);
    rect(lx, ly, 34, 5, D2);
    rect(lx, ly, 34, 1, D3);
    for (int x = lx + 3; x < lx + 34; x += 7) set(x, ly + 2, F3);  // embers
  }
  // firelight on the floor and the hearth, breathing with the flames
  float g = constrain(glow, 0.2f, 1.2f);
  for (int y = 72; y < H; y++)
    for (int x = 16; x < 186; x++) {
      float d = fabsf(x - 100) / (60 + 30 * g) + (y - 72) / 22.0f;
      if (d < 1 && ((x + y + frame_no / 2) % 3 == 0)) set(x, y, d < 0.5f ? F2 : F1);
    }
  // mantel things: a candle and a small clock
  rect(52, 6, 4, 8, N6);
  set(53, 4 - ((frame_no / 3) % 2), F4), set(54, 5, F5), set(53, 5, F4);
  rect(126, 4, 14, 10, D3);
  disc(133, 9, 3, N5);
  set(133, 8, N1), set(133, 7, N1), set(134, 9, N1);
  rect(86, 8, 10, 6, G2);  // a small plant
  set(88, 6, G3), set(91, 5, G3), set(94, 6, G3);
  cat();

  present();
  if (corner_text && *corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 154, 2, vfd::Ink{ px::ink(F4) });
  }
}

#endif
