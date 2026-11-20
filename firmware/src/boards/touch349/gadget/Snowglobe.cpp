#ifdef BOARD_TOUCH349

#include "Snowglobe.h"
#include "Motion.h"
#include "Px.h"
#include "Vfd.h"

using namespace px;

static constexpr int FLAKES = 420, FAR_FLAKES = 70;

static uint8_t *scene = nullptr;  // W x H colors of the still picture (PSRAM)
static uint8_t *solid = nullptr;  // W x H: 1 = scenery, 2 = settled snow
static uint32_t frame_no = 0;

struct Flake {
  float x, y, vx, vy;
  uint8_t state;  // 0 = not yet falling, 1 = falling, 2 = settled
};
static Flake flakes[FLAKES];
static int spawned = 0;

static float frand() { return (esp_random() & 0xffff) / 65535.0f; }
static uint32_t hash32(uint32_t v)
{
  v ^= v >> 16;
  v *= 0x7feb352d;
  v ^= v >> 15;
  v *= 0x846ca68b;
  return v ^ (v >> 16);
}

static float groundAt(int x) { return 70 - 7 * expf(-powf((x - 140) / 70.0f, 2)) + 1.5f * sinf(x * 0.05f); }

static inline void put(int x, int y, uint8_t c, bool hard)
{
  if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
  scene[y * W + x] = c;
  if (hard) solid[y * W + x] = 1;
}

static void pine(int cx, int base, int h)
{
  // trunk down into the snow
  for (int y = base - 3; y <= base + 1; y++) put(cx, y, D1, true), put(cx + 1, y, D2, true);
  int tiers = h / 7 + 1, top = base - 3 - h;
  for (int y = top; y < base - 3; y++) {
    int d = y - top, tier_d = d % 7 + d / 7 * 2;
    int hw = 1 + tier_d * 2 / 3 + d / 6;
    for (int x = -hw; x <= hw + 1; x++) {
      bool snow = (d % 7 == 0 && abs(x) < hw) || (x == -hw && d % 7 < 2);
      put(cx + x, y, snow ? N5 : ((x + d) % 5 == 0 ? G2 : G1), true);
    }
  }
  (void)tiers;
}

static void build()
{
  scene = (uint8_t *)ps_malloc(W * H);
  solid = (uint8_t *)ps_malloc(W * H);
  memset(solid, 0, W * H);
  // sky
  static const uint8_t SKY[] = { NIGHT, N1, N1, N2, N2 };
  gradient(0, H, SKY, 5);
  for (int y = 0; y < H; y++) memcpy(scene + y * W, buf[y], W);
  // mountains with snow caps
  for (int x = 0; x < W; x++) {
    int top = 34 + lroundf(9 * sinf(x * 0.018f + 1.2f) + 5 * sinf(x * 0.051f) + 2 * sinf(x * 0.13f));
    for (int y = top; y < H; y++) put(x, y, y < top + 2 + (int)(hash32(x) % 3) ? N4 : N2, false);
  }
  // the far forest
  for (int x = 0; x < W; x++) {
    int top = 54 + (int)(hash32(x / 3 * 7) % 5) - ((x / 3) % 2) * 3;
    for (int y = top; y < H; y++) put(x, y, y == top ? N3 : N1, false);
  }
  // the hill
  for (int x = 0; x < W; x++) {
    int top = lroundf(groundAt(x));
    for (int y = top; y < H; y++) {
      uint32_t h = hash32(x * 977 + y * 131);
      uint8_t c = y == top ? N6 : (y > top + 6 && ((x + y) & 1) ? N4 : N5);
      if (h % 41 == 0) c = N6;
      put(x, y, c, true);
    }
  }
  // the cabin
  int cx0 = 118, cx1 = 160, base = lroundf(groundAt(139));
  for (int y = base - 15; y <= base; y++)
    for (int x = cx0; x <= cx1; x++) put(x, y, (y - base) % 3 == 0 ? D1 : ((x * 7 + y) % 9 == 0 ? D3 : D2), true);
  for (int k = 0; k <= 13; k++) {  // roof, overhanging, snow on top
    for (int x = cx0 - 4 + k; x <= cx1 + 4 - k; x++) {
      bool edge = x <= cx0 - 2 + k || x >= cx1 + 2 - k;
      put(x, base - 16 - k, edge ? N6 : (k > 10 ? N6 : N1), true);
    }
  }
  for (int y = base - 34; y <= base - 22; y++) put(150, y, R2, true), put(151, y, R2, true), put(152, y, R1, true);
  put(150, base - 35, N6, true), put(151, base - 35, N6, true), put(152, base - 35, N6, true);
  for (int y = base - 11; y <= base - 4; y++)  // window
    for (int x = 125; x <= 136; x++) put(x, y, (x == 130 || x == 131 || y == base - 8) ? D1 : (y < base - 8 ? F5 : F4), true);
  for (int y = base - 12; y <= base; y++)  // door
    for (int x = 146; x <= 153; x++) put(x, y, x == 146 || x == 153 || y == base - 12 ? D1 : D2, true);
  // warm light spilling on the snow under the window
  for (int y = base + 1; y < base + 9; y++)
    for (int x = 116 + (y - base); x <= 146 - (y - base); x++)
      if ((x + y) % 2 == 0 || y < base + 4) put(x, y, y < base + 4 ? F4 : F3, true);
  // pines on the hill, standing in the snow
  const int PINES[][2] = { { 22, 26 }, { 52, 34 }, { 82, 22 }, { 190, 30 }, { 222, 38 }, { 256, 24 }, { 292, 32 } };
  for (auto &p : PINES) pine(p[0], lroundf(groundAt(p[0])), p[1]);
  // a little fence
  for (int x = 166; x < 186; x++) {
    int g = lroundf(groundAt(x));
    if (x % 5 == 0) put(x, g - 5, N6, true), put(x, g - 4, D2, true), put(x, g - 3, D2, true), put(x, g - 2, D2, true);
    put(x, g - 3, D3, true);
  }
}

void Snowglobe_Stir(float strength)
{
  for (auto &f : flakes) {
    if (f.state != 2 || frand() > strength) continue;
    f.state = 1;
    float a = frand() * 2 * PI, v = (6 + frand() * 22) * strength;
    f.vx = cosf(a) * v;
    f.vy = sinf(a) * v - 14 * strength;
  }
}

static bool blocked(int x, int y) { return (unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H || solid[y * W + x]; }

static void respawn(Flake &f, int sx, int sy)
{
  f.vx = f.vy = 0;
  f.state = 1;
  if (sy) {
    f.x = frand() * W;
    f.y = sy > 0 ? 0 : H - 1;
  } else {
    f.x = sx > 0 ? 0 : W - 1;
    f.y = frand() * 60;
  }
}

static void update()
{
  const float dt = 0.05f;
  float gx = 0, gy = 1, gz = 0;
  if (Motion_Ok()) {
    Motion_Gravity(gx, gy, gz);
    float shake = Motion_Shake();
    if (shake > 0.2f) Snowglobe_Stir(shake);
    if (Motion_TakeKnock()) Snowglobe_Stir(0.3f);
  }
  if (gx * gx + gy * gy < 0.01f) gy = 0.1f;
  int sx, sy, px_, py_;
  if (fabsf(gx) > fabsf(gy)) {
    sx = gx > 0 ? 1 : -1, sy = 0, px_ = 0, py_ = 1;
  } else {
    sx = 0, sy = gy > 0 ? 1 : -1, px_ = 1, py_ = 0;
  }

  // the snowfall starts slowly from an empty sky; once every flake is out, settled ones come back from the top
  if (spawned < FLAKES) {
    if (frame_no % 2 == 0) respawn(flakes[spawned++], sx, sy);
  } else if (frame_no % 3 == 0) {
    Flake &f = flakes[esp_random() % FLAKES];
    if (f.state == 2) {
      solid[(int)f.y * W + (int)f.x] = 0;
      respawn(f, sx, sy);
    }
  }

  float wind = sinf(frame_no * 0.01f) * 2;
  for (int i = 0; i < spawned; i++) {
    Flake &f = flakes[i];
    int x = (int)f.x, y = (int)f.y;
    if (f.state == 2) {
      if (!blocked(x + sx, y + sy)) {  // what held it is gone (the board turned)
        solid[y * W + x] = 0;
        f.state = 1;
      }
      continue;
    }
    f.vx += (gx * 9 + wind * 0.4f + (frand() - 0.5f) * 8) * dt;
    f.vy += (gy * 9 + (frand() - 0.5f) * 8) * dt;
    f.vx *= 0.9f;
    f.vy *= 0.9f;
    float nx = f.x + f.vx * dt * 3, ny = f.y + f.vy * dt * 3;
    if (nx < 0 || nx >= W || ny < -2 || ny >= H) {
      if ((sy > 0 && ny < 0) || (sy < 0 && ny >= H) || (sx > 0 && nx < 0) || (sx < 0 && nx >= W)) {
        f.x = constrain(nx, 0.0f, W - 0.01f);  // still entering from the far side
        f.y = constrain(ny, 0.0f, H - 0.01f);
      } else {
        respawn(f, sx, sy);  // left the picture: back to the start
      }
      continue;
    }
    int nxi = (int)nx, nyi = (int)ny;
    if ((nxi == x && nyi == y) || !blocked(nxi, nyi)) {
      f.x = nx;
      f.y = ny;
      continue;
    }
    // landed: slide down a slope if there is room, otherwise settle here
    int side = frand() < 0.5f ? 1 : -1;
    bool slid = false;
    for (int k = 0; k < 2 && !slid; k++, side = -side) {
      int dx = x + sx + px_ * side, dy = y + sy + py_ * side;
      if (!blocked(dx, dy) && !blocked(x + px_ * side, y + py_ * side)) {
        f.x = dx + 0.5f;
        f.y = dy + 0.5f;
        slid = true;
      }
    }
    if (slid) continue;
    if (!blocked(x, y)) {
      f.state = 2;
      f.x = x + 0.5f;
      f.y = y + 0.5f;
      solid[y * W + x] = 2;
    } else {
      respawn(f, sx, sy);
    }
  }
}

void Snowglobe_Draw(const char *corner_text)
{
  if (!scene) build();
  frame_no++;
  update();
  float t = frame_no * 0.05f;

  for (int y = 0; y < H; y++) memcpy(buf[y], scene + y * W, W);
  // stars and the moon
  for (int i = 0; i < 46; i++) {
    uint32_t h = hash32(i + 77);
    int x = h % W, y = (h >> 9) % 34;
    if (buf[y][x] <= N2 && ((frame_no / 8 + (h >> 20)) % 11) != 0) set(x, y, i % 4 ? N4 : N6);
  }
  disc(268, 13, 6, N5);
  disc(267, 12, 5, N6);
  set(265, 11, N5), set(269, 14, N5), set(270, 11, N5);
  // chimney smoke
  int base = lroundf(groundAt(139));
  for (int i = 0; i < 10; i++) {
    float ph = fmodf(t * 0.12f + i / 10.0f, 1.0f);
    int x = 151 + lroundf(ph * 22 + sinf(ph * 8 + i) * 2), y = base - 37 - (int)(ph * 30);
    uint8_t c = ph < 0.4f ? N4 : N3;
    set(x, y, c), set(x + 1, y, c);
    if (ph < 0.6f) set(x, y - 1, c);
  }
  // the window flickers a little, like a fire inside
  if ((frame_no / 3) % 7 == 0)
    for (int y = base - 11; y <= base - 9; y++)
      for (int x = 126; x <= 129; x++) set(x, y, F4);
  // far, slow flakes behind the forest line
  for (int i = 0; i < FAR_FLAKES && spawned > i * 5; i++) {
    uint32_t h = hash32(i * 31 + 9);
    int y = (int)((h >> 8) % 60 + t * (2 + h % 3)) % 60;
    int x = (int)(h % W + sinf(t * 0.5f + i) * 3) % W;
    if (buf[y][x] <= N3) set(x, y, N4);
  }
  for (int i = 0; i < spawned; i++) {
    const Flake &f = flakes[i];
    set((int)f.x, (int)f.y, N6);
  }

  present();
  if (corner_text && *corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 4, 2, vfd::DIM_INK);
  }
}

#endif
