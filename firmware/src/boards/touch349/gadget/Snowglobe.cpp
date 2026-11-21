#ifdef BOARD_TOUCH349

#include "Snowglobe.h"
#include "Motion.h"
#include "Px.h"
#include "RetroScene.h"
#include "SettingsUi.h"
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

static uint8_t *t_scene = nullptr, *t_solid = nullptr;  // what put() draws into: the cabin or the globe

static inline void put(int x, int y, uint8_t c, bool hard)
{
  if ((unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H) return;
  t_scene[y * W + x] = c;
  if (hard) t_solid[y * W + x] = 1;
}

static int mountainTop(int x) { return 34 + lroundf(9 * sinf(x * 0.018f + 1.2f) + 5 * sinf(x * 0.051f) + 2 * sinf(x * 0.13f)); }

static bool globe = false;  // the snow globe view (hold), else the cabin
static void globeStir(float strength);

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
  t_scene = scene;
  t_solid = solid;
  // sky
  static const uint8_t SKY[] = { NIGHT, N1, N1, N2, N2 };
  gradient(0, H, SKY, 5);
  for (int y = 0; y < H; y++) memcpy(scene + y * W, buf[y], W);
  // mountains with snow caps
  for (int x = 0; x < W; x++) {
    int top = mountainTop(x);
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
  RetroScene_Stir(strength);
  if (globe) {
    globeStir(strength);
    return;
  }
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

// ---------------------------------------------------------------------------------------------------------------------
// Holidays (Dec 1 - Jan 6 or as set): bulbs along the cabin's roof, a wreath, a decorated pine, a sleigh past the moon.
// A shooting star now and then, all year.
// ---------------------------------------------------------------------------------------------------------------------

static const uint8_t BULBS[] = { R3, Y1, G3, U1 };

// Bulb i in its color; each one rests now and then on its own slow beat, so the string twinkles instead of blinking.
static uint8_t bulb(uint32_t i)
{
  uint32_t beat = frame_no / 14 + hash32(i * 13 + 5) % 9;
  return beat % 9 == 0 ? N3 : BULBS[i % 4];
}

static void holidayCabin(int base)
{
  uint32_t i = 0;
  for (int x = 116; x <= 162; x += 3) set(x, base - 15, bulb(i++));  // under the eaves
  for (int k = 2; k <= 12; k += 3) {                                  // up both slopes
    set(114 + k, base - 16 - k, bulb(i++));
    set(164 - k, base - 16 - k, bulb(i++));
  }
  static const char *const WREATH[] = { ".gg.", "g..g", "g..g", ".rr." };
  static const uint8_t WREATH_C[] = { G3, R3 };
  sprite(WREATH, 4, 148, base - 10, "gr", WREATH_C);

  // the pine past the fence is theirs: ornaments and a star
  int cx = 190, pb = lroundf(groundAt(cx)), top = pb - 3 - 30;
  for (int y = top + 4; y < pb - 4; y++) {
    for (int x = cx - 12; x <= cx + 13; x++) {
      uint8_t c = scene[y * W + x];
      uint32_t h = hash32(x * 7919 + y * 104729);
      if ((c == G1 || c == G2) && h % 15 == 0) set(x, y, bulb(h >> 8));
    }
  }
  static const char *const STAR[] = { ".yy.", "yffy", ".yy." };
  const uint8_t star_c[] = { Y1, (frame_no / 10) % 3 ? F5 : Y1 };
  sprite(STAR, 3, cx - 1, top - 3, "yf", star_c);
}

static uint32_t star_next_ms = 0;
static int star_t = -1;
static float star_x, star_y, star_dx, star_dy;

static void shootingStar()
{
  uint32_t now = millis();
  if (star_t < 0) {
    if (!star_next_ms) star_next_ms = now + 15000 + esp_random() % 30000;
    if ((int32_t)(now - star_next_ms) < 0) return;
    star_t = 0;
    star_x = 50 + frand() * 200;
    star_y = 2 + frand() * 8;
    star_dx = (frand() < 0.5f ? -1 : 1) * (2.2f + frand());
    star_dy = 0.8f + frand() * 0.5f;
  }
  const int LIFE = 18;
  float hx = star_x + star_dx * star_t, hy = star_y + star_dy * star_t;
  for (int k = 9; k >= 0; k--) {
    int x = lroundf(hx - star_dx * k * 0.55f), y = lroundf(hy - star_dy * k * 0.55f);
    if ((unsigned)x >= (unsigned)W || y < 0 || y >= mountainTop(x)) continue;
    uint8_t c = k == 0 ? (star_t > LIFE - 5 ? N4 : N6) : k < 3 ? N5 : k < 6 ? N4 : N3;
    set(x, y, c);
  }
  if (++star_t > LIFE) {
    star_t = -1;
    star_next_ms = now + 30000 + esp_random() % 60000;
  }
}

static float sleigh_x = -1000;
static uint32_t sleigh_next_ms = 0;

static void sleigh()
{
  static const char *const DEER[2][7] = {
    { "a.a.......", ".aa.......", "bbb.......", ".bbbbbbbb.", "..bbbbbbb.", "..b....b..", ".b......b." },
    { "a.a.......", ".aa.......", "bbb.......", ".bbbbbbbb.", "..bbbbbbb.", "..b...b...", "...b...b.." },
  };
  static const char *const SLEIGH[] = { ".......w....", "......rr....", "..l..rrr.ss.", "c.cRRRRRRRR.",
                                        ".cRRRRRRRRR.", "..RRRRRRRR..", "nnnnnnnnnnnn" };
  static const uint8_t DEER_C[] = { S3, D3 };
  static const uint8_t SLEIGH_C[] = { N6, R3, F5, D3, R3, R2, N5 };
  uint32_t now = millis();
  if (sleigh_x < -60) {
    if (!sleigh_next_ms) sleigh_next_ms = now + 20000 + esp_random() % 30000;
    if ((int32_t)(now - sleigh_next_ms) < 0) return;
    sleigh_next_ms = 0;
    sleigh_x = W + 4;
  }
  sleigh_x -= 0.9f;  // right to left, about 18 px/s: across the moon
  if (sleigh_x < -60) {
    sleigh_next_ms = now + 180000 + esp_random() % 120000;
    return;
  }
  float t = frame_no * 0.05f;
  int x = lroundf(sleigh_x), leg = (frame_no / 3) % 2;
  for (int d = 0; d < 2; d++) sprite(DEER[leg ^ d], 7, x + d * 12, 11 + lroundf(2 * sinf(t * 1.7f + d * 0.8f)), "ab", DEER_C);
  sprite(SLEIGH, 7, x + 25, 11 + lroundf(2 * sinf(t * 1.7f + 1.9f)), "wrlscRn", SLEIGH_C);
}

// ---------------------------------------------------------------------------------------------------------------------
// The snow globe (hold): the cabin as a little diorama in a glass dome on a turned wooden base, in a dark room.
// The flakes drift slowly as if in water; a shake, a knock or a tap swirls them, and they settle again.
// ---------------------------------------------------------------------------------------------------------------------

static constexpr int GCX = 160, GCY = 37, GR = 33;  // the dome
static constexpr int GBASE = GCY + 24;              // where the base hides the rest of the sphere
static constexpr int TABLE = 80;
static constexpr int GFLAKES = 170;

static uint8_t *gscene = nullptr, *gsolid = nullptr;
static Flake gfl[GFLAKES];
static float spin = 0;  // the swirl of the water, + or - for its direction
static bool globe_ready = false, shake_pending = false;
static int mini_base = 0;  // the diorama cabin's ground line

// The room around it: a window on the snowy night at the left, a candle and a mug of something warm at the right.
static constexpr int WX0 = 34, WX1 = 98, WY0 = 10, WY1 = 60;  // the window panes
static constexpr int CANDLE_X = 238, MUG_X = 258;

static bool inPane(int x, int y)
{
  return x >= WX0 && x <= WX1 && y >= WY0 && y <= WY1 && abs(x - (WX0 + WX1) / 2) > 1 && abs(y - (WY0 + WY1) / 2) > 1;
}

static float outsideHill(int x) { return WY1 - 7 - 3.5f * sinf(x * 0.09f + 0.5f); }

static void buildRoom()
{
  for (int y = 0; y < TABLE; y++)  // the wall, a faint stripe in the paper
    for (int x = 0; x < W; x++) put(x, y, (x % 12 == 0 && (y & 1)) ? N1 : NIGHT, true);
  for (int y = WY0 - 3; y <= WY1 + 3; y++) {
    for (int x = WX0 - 3; x <= WX1 + 3; x++) {
      if (inPane(x, y)) {
        float hill = outsideHill(x);
        put(x, y, y >= hill ? (y == (int)hill ? N6 : N5) : y < WY0 + 22 ? N2 : (((x + y) & 1) ? N2 : N3), true);
      } else {
        put(x, y, (x == WX0 - 3 || y == WY0 - 3) ? D3 : (x == WX1 + 3 || y == WY1 + 3) ? D1 : D2, true);
      }
    }
  }
  const int FIRS[][2] = { { 46, 11 }, { 58, 7 }, { 84, 9 } };  // outside, dark against the snow
  for (const auto &f : FIRS) {
    int base = lroundf(outsideHill(f[0]));
    for (int k = 0; k < f[1]; k++)
      for (int x = f[0] - k / 2; x <= f[0] + k / 2; x++)
        if (inPane(x, base - f[1] + k)) put(x, base - f[1] + k, G1, true);
  }
  for (int x = WX0 - 7; x <= WX1 + 7; x++) put(x, WY1 + 4, D3, true), put(x, WY1 + 5, D1, true);  // the sill
  // the candle in its brass holder
  for (int x = CANDLE_X - 5; x <= CANDLE_X + 5; x++) put(x, TABLE - 2, F3, true), put(x, TABLE - 1, D1, true);
  for (int y = TABLE - 14; y <= TABLE - 3; y++)
    for (int x = CANDLE_X - 2; x <= CANDLE_X + 2; x++) put(x, y, x == CANDLE_X + 2 ? N5 : N6, true);
  put(CANDLE_X, TABLE - 15, N1, true);
  // the mug
  for (int y = TABLE - 11; y < TABLE; y++)
    for (int x = MUG_X; x <= MUG_X + 11; x++)
      put(x, y, y == TABLE - 11 ? R3 : y == TABLE - 10 && x > MUG_X && x < MUG_X + 11 ? D1 : x == MUG_X + 1 ? R3 : x == MUG_X + 11 ? R1 : R2, true);
  const int HANDLE[][2] = { { 12, 3 }, { 13, 3 }, { 14, 4 }, { 14, 5 }, { 14, 6 }, { 13, 7 }, { 12, 7 } };
  for (const auto &h : HANDLE) put(MUG_X + h[0], TABLE - 11 + h[1], R2, true);
}

static float out_x[24], out_y[24];

static void drawRoom(bool holidays)
{
  // snow falling past the window
  for (int i = 0; i < 24; i++) {
    if (out_y[i] <= 0 || out_y[i] > outsideHill((int)out_x[i])) {
      out_x[i] = WX0 + frand() * (WX1 - WX0);
      out_y[i] = WY0 - frand() * 30;
    }
    out_y[i] += 0.22f + (i % 3) * 0.06f;
    out_x[i] += 0.12f * sinf(frame_no * 0.04f + i);
    if (inPane((int)out_x[i], (int)out_y[i])) set((int)out_x[i], (int)out_y[i], i % 3 ? N5 : N6);
  }
  // the candle: its flame and a warm light on the wall
  uint32_t fl = hash32(frame_no / 3) % 4;
  int fy = TABLE - 16;
  for (int y = fy - 12; y <= fy + 8; y++)
    for (int x = CANDLE_X - 14; x <= CANDLE_X + 14; x++) {
      if (y >= TABLE - 15 && abs(x - CANDLE_X) <= 2) continue;
      int d2 = (x - CANDLE_X) * (x - CANDLE_X) + (y - fy) * (y - fy) * 2;
      if (get(x, y) != NIGHT && get(x, y) != N1) continue;
      if (d2 < 30 + fl * 6) set(x, y, D2);
      else if (d2 < 110 + fl * 14 && ((x + y) & 1)) set(x, y, D1);
    }
  set(CANDLE_X - 1, fy, F4), set(CANDLE_X, fy, F5), set(CANDLE_X + 1, fy, F4);
  set(CANDLE_X, fy - 1, Y1);
  if (fl) set(CANDLE_X + (fl == 3 ? 1 : 0), fy - 2, F5);
  if (fl >= 2) set(CANDLE_X, fy - 3, F3);
  // steam off the mug
  for (int w = 0; w < 2; w++)
    for (int k = 0; k < 11; k++) {
      if (hash32(k * 31 + w * 7 + frame_no / 4) % 5 == 0) continue;
      int x = MUG_X + 4 + w * 4 + lroundf(1.5f * sinf(k * 0.6f - frame_no * 0.12f + w * 2));
      set(x, TABLE - 13 - k, k < 6 ? N3 : N2);
    }
  if (!holidays) return;
  // a string of bulbs across the window, and a small present by the globe
  uint32_t i = 200;
  for (int x = WX0 - 2; x <= WX1 + 2; x += 4)
    set(x, WY0 - 1 + lroundf(4 * sinf(PI * (x - WX0 + 2) / (WX1 - WX0 + 4))), bulb(i++));
  px::rect(204, TABLE - 7, 11, 7, R2);
  px::rect(204, TABLE - 7, 11, 1, R3);
  px::rect(209, TABLE - 7, 2, 7, F5);
  px::rect(204, TABLE - 4, 11, 1, F5);
  set(207, TABLE - 9, F5), set(208, TABLE - 8, F5), set(212, TABLE - 9, F5), set(211, TABLE - 8, F5);
}

static float moundAt(int x) { return GBASE - 8 - 3.5f * expf(-powf((x - GCX) / 13.0f, 2)) + 0.6f * sinf(x * 0.35f); }

static bool inDome(int x, int y)
{
  int dx = x - GCX, dy = y - GCY;
  return dx * dx + dy * dy < (GR - 1) * (GR - 1) && y < GBASE;
}

static void buildGlobe()
{
  gscene = (uint8_t *)ps_malloc(W * H);
  gsolid = (uint8_t *)ps_malloc(W * H);
  t_scene = gscene;
  t_solid = gsolid;
  memset(gscene, NIGHT, W * H);
  memset(gsolid, 1, W * H);  // everything outside the water is a wall to the flakes
  buildRoom();
  for (int y = 0; y < GBASE; y++) {
    for (int x = GCX - GR; x <= GCX + GR; x++) {
      int dx = x - GCX, dy = y - GCY;
      float d = sqrtf(dx * dx + dy * dy);
      if (inDome(x, y)) {
        int band = y - (GCY - GR);
        put(x, y, band < 18 ? N2 : band < 30 ? (((x + y) & 1) ? N2 : N1) : N1, false);
        gsolid[y * W + x] = 0;
      } else if (d < GR) {
        put(x, y, N3, false);  // the glass edge, faint
      }
    }
  }
  // the snowy mound
  for (int x = GCX - GR; x <= GCX + GR; x++) {
    int top = lroundf(moundAt(x));
    for (int y = top; y < GBASE; y++)
      if (inDome(x, y)) put(x, y, y == top || hash32(x * 31 + y * 7) % 37 == 0 ? N6 : N5, true);
  }
  // the cabin, small
  int b = mini_base = lroundf(moundAt(GCX));
  for (int y = b - 7; y <= b; y++)
    for (int x = 153; x <= 167; x++) put(x, y, (y - b) % 2 == 0 ? D1 : D2, true);
  for (int k = 0; k <= 5; k++)
    for (int x = 151 + k; x <= 169 - k; x++) put(x, b - 8 - k, (x <= 152 + k || x >= 168 - k || k >= 4) ? N6 : N1, true);
  for (int y = b - 15; y <= b - 10; y++) put(164, y, R2, true), put(165, y, R1, true);
  put(164, b - 16, N6, true), put(165, b - 16, N6, true);
  for (int y = b - 5; y <= b - 3; y++)
    for (int x = 155; x <= 157; x++) put(x, y, y == b - 5 ? F5 : F4, true);
  for (int y = b - 5; y <= b; y++)
    for (int x = 161; x <= 163; x++) put(x, y, D1, true);
  for (int x = 154; x <= 158; x++) put(x, b + 1, (x & 1) ? F3 : N6, true);
  pine(143, lroundf(moundAt(143)), 11);
  pine(177, lroundf(moundAt(177)), 9);
  // the turned wooden base with a brass plate
  for (int r = 0; r < TABLE - GBASE; r++) {
    int y = GBASE + r, hw = r < 3 ? 25 : r < 5 ? 27 : 27 + (r - 5) * 3 / 13;
    for (int x = GCX - hw; x <= GCX + hw; x++) {
      int dx = x - GCX;
      uint8_t c = (r == 0 || r == 3) ? D3 : (r == TABLE - GBASE - 1 || abs(dx) >= hw - 1) ? D1 : D2;
      if (c == D2 && abs(dx + hw / 2) <= 1) c = D3;                // the light on the turned wood
      if (c == D2 && hash32(x * 5 + r * 977) % 13 == 0) c = D1;  // grain
      put(x, y, c, true);
    }
  }
  for (int y = GBASE + 8; y <= GBASE + 12; y++)
    for (int x = GCX - 9; x <= GCX + 9; x++) {
      bool edge = y == GBASE + 8 || y == GBASE + 12 || x == GCX - 9 || x == GCX + 9;
      put(x, y, edge ? D1 : y == GBASE + 9 ? F5 : F4, true);
    }
  for (int x = GCX - 5; x <= GCX + 5; x++) put(x, GBASE + 10, F3, true);
  // the table
  for (int y = TABLE; y < H; y++)
    for (int x = 0; x < W; x++)
      put(x, y, y == TABLE ? D3 : (y == H - 1 || hash32(x / 3 * 17 + y * 101) % 9 == 0) ? D1 : D2, true);
}

static bool gblocked(int x, int y) { return (unsigned)x >= (unsigned)W || (unsigned)y >= (unsigned)H || gsolid[y * W + x]; }

static void globeStir(float s)
{
  if (!globe_ready) return;
  float dir = fabsf(spin) < 0.2f ? ((esp_random() & 1) ? 1.0f : -1.0f) : (spin > 0 ? 1.0f : -1.0f);
  spin = dir * min(2.0f, max(fabsf(spin), 0.6f + s));
  for (auto &f : gfl) {
    if (f.state == 2) {
      if (frand() > s) continue;
      gsolid[(int)f.y * W + (int)f.x] = 0;
      f.state = 1;
    }
    float a = frand() * 2 * PI, v = (8 + frand() * 26) * s;
    f.vx += cosf(a) * v;
    f.vy += sinf(a) * v - 20 * s;
  }
}

static void globeInit()
{
  for (auto &f : gfl) {
    int x, y;
    do {
      x = GCX - GR + esp_random() % (2 * GR);
      y = GCY - GR + esp_random() % (GBASE - (GCY - GR));
    } while (gblocked(x, y));
    f = { x + 0.5f, y + 0.5f, 0, 0, 1 };
  }
  globe_ready = true;
}

static void globeUpdate()
{
  const float dt = 0.05f;
  float gx = 0, gy = 1, gz = 0;
  if (Motion_Ok()) {
    Motion_Gravity(gx, gy, gz);
    float shake = Motion_Shake();
    if (shake > 0.2f) globeStir(shake);
    if (Motion_TakeKnock()) globeStir(0.3f);
  }
  if (gx * gx + gy * gy < 0.01f) gy = 0.1f;
  int sx, sy, px_, py_;
  if (fabsf(gx) > fabsf(gy)) {
    sx = gx > 0 ? 1 : -1, sy = 0, px_ = 0, py_ = 1;
  } else {
    sx = 0, sy = gy > 0 ? 1 : -1, px_ = 1, py_ = 0;
  }
  spin *= 0.985f;
  float swirl = fabsf(spin);
  for (auto &f : gfl) {
    int x = (int)f.x, y = (int)f.y;
    if (f.state == 2) {
      if (!gblocked(x + sx, y + sy)) {  // what held it is gone (the globe turned)
        gsolid[y * W + x] = 0;
        f.state = 1;
      }
      continue;
    }
    // water: slow sinking, the swirl carries the flakes round the dome and a little inwards
    float dx = f.x - GCX, dy = f.y - GCY, r = sqrtf(dx * dx + dy * dy) + 1;
    float ax = gx * 10 - dy / r * spin * 34 - dx / r * swirl * 4 + (frand() - 0.5f) * 10;
    float ay = gy * 10 + dx / r * spin * 34 - dy / r * swirl * 4 + (frand() - 0.5f) * 10;
    f.vx = (f.vx + ax * dt) * 0.93f;
    f.vy = (f.vy + ay * dt) * 0.93f;
    float nx = f.x + f.vx * dt, ny = f.y + f.vy * dt;
    int nxi = (int)nx, nyi = (int)ny;
    if ((nxi == x && nyi == y) || !gblocked(nxi, nyi)) {
      f.x = nx;
      f.y = ny;
      continue;
    }
    if (f.vx * f.vx + f.vy * f.vy > 64 || swirl > 0.35f) {  // still stirred: bounce off
      f.vx *= -0.35f;
      f.vy *= -0.35f;
      continue;
    }
    int side = frand() < 0.5f ? 1 : -1;
    bool slid = false;
    for (int k = 0; k < 2 && !slid; k++, side = -side) {
      int ddx = x + sx + px_ * side, ddy = y + sy + py_ * side;
      if (!gblocked(ddx, ddy) && !gblocked(x + px_ * side, y + py_ * side)) {
        f.x = ddx + 0.5f;
        f.y = ddy + 0.5f;
        slid = true;
      }
    }
    if (slid || gblocked(x, y)) continue;
    f.state = 2;
    f.vx = f.vy = 0;
    f.x = x + 0.5f;
    f.y = y + 0.5f;
    gsolid[y * W + x] = 2;
  }
}

// The glass shows only by its light: a soft arc upper left, one bright point, a faint one on the right.
static void glassShine()
{
  for (float a = 3.49f; a < 4.36f; a += 0.025f) {
    bool mid = a > 3.65f && a < 4.2f;
    for (int k = 3; k <= 4; k++) set(GCX + lroundf((GR - k) * cosf(a)), GCY + lroundf((GR - k) * sinf(a)), mid && k == 3 ? N5 : N4);
  }
  px::rect(GCX + lroundf((GR - 10) * cosf(3.8f)), GCY + lroundf((GR - 10) * sinf(3.8f)), 2, 2, N6);
  for (float a = 5.6f; a < 6.0f; a += 0.03f) set(GCX + lroundf((GR - 3) * cosf(a)), GCY + lroundf((GR - 3) * sinf(a)), N3);
}

static void holidayGlobe()
{
  int b = mini_base;
  uint32_t i = 100;
  for (int x = 153; x <= 167; x += 2) set(x, b - 7, bulb(i++));
  for (int k = 1; k <= 5; k += 2) set(151 + k, b - 8 - k, bulb(i++)), set(169 - k, b - 8 - k, bulb(i++));
  int top = lroundf(moundAt(177)) - 3 - 9;
  static const char *const STAR[] = { ".y.", "yfy", ".y." };
  const uint8_t star_c[] = { Y1, (frame_no / 10) % 3 ? F5 : Y1 };
  sprite(STAR, 3, 177, top - 3, "yf", star_c);
}

static void drawGlobe(const char *corner_text)
{
  if (!gscene) buildGlobe();
  if (!globe_ready) globeInit();
  if (shake_pending) {
    shake_pending = false;
    globeStir(1.0f);  // picked up and shaken
  }
  frame_no++;
  globeUpdate();
  for (int y = 0; y < H; y++) memcpy(buf[y], gscene + y * W, W);
  if ((frame_no / 3) % 7 == 0) set(156, mini_base - 4, F5);  // the fire inside
  bool holidays = SettingsUi_Holidays();
  drawRoom(holidays);
  if (holidays) holidayGlobe();
  for (const auto &f : gfl) set((int)f.x, (int)f.y, N6);
  glassShine();
  present();
  if (corner_text && *corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 4, 2, vfd::DIM_INK);
  }
}

void Snowglobe_Show(const char *what)
{
  if (!strcmp(what, "sleigh")) sleigh_next_ms = millis(), sleigh_x = -1000;
  else if (!strcmp(what, "star")) star_next_ms = millis(), star_t = -1;
}

void Snowglobe_Hold()
{
  globe = !globe;
  if (globe) shake_pending = true;
}

void Snowglobe_Draw(const char *corner_text)
{
  if (globe) {
    drawGlobe(corner_text);
    return;
  }
  if (RetroScene_Draw(RetroArt::Snow, corner_text)) return;  // the theme has its own art for this scene
  if (!scene) build();
  t_scene = scene;
  t_solid = solid;
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
  bool holidays = SettingsUi_Holidays();
  if (holidays) holidayCabin(base);
  shootingStar();
  if (holidays) sleigh();
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
