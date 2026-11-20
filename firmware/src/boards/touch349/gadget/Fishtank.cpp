#ifdef BOARD_TOUCH349

#include "Fishtank.h"
#include "Motion.h"
#include "Px.h"
#include "Vfd.h"

using namespace px;

static constexpr float LEVEL = 10;  // surface row with the board upright
static constexpr int SAND = 74;     // top of the sand, roughly

// Fish sprites, facing right; the tail is on the left.
static const char *const CLOWN[] = { "....kook....", "k..oowoowoo.", "koooowoowoeo", "koooowoowooo", "k..oowoowoo.",
                                     "....kook...." };
static const char *const TANG[] = { ".....dddd.....", "y...dbbbbbd...", "yy.dbbddbbbbd.", "yyybbbbbddbbeb",
                                    "yyybbbbbbbbbbb", "yy.dbbbbbbbbd.", "y...dbbbbbd...", ".....dddd....." };
static const char *const TETRA[] = { ".ssssss.", "scccccce", ".rrrsss." };
static const char *const ANGEL[] = { "..s........", "..ss.......", "..sss......", "...sgss....", "...sgssss..",
                                     "s..sgssgsse", "ssssgssgsss", "s..sgssgss.", "...sgssss..", "...sgss....",
                                     "..sss......", "..ss.......", "..s........" };
static const char *const GOLD[] = { "......aa....", "AA..aaaaaa..", ".AAaaaaaaaea", "..Aaaaaaaaaa", ".AAaaaaaaaa.",
                                    "AA..aaaaa...", "......a....." };
static const char *const GRAMMA[] = { "...yyppp..", "y.yyypppp.", "yyyyypppep", "y.yyypppp.", "...yyppp.." };
// The big one: a slow humphead wrasse.
static const char *const BIG[] = { "..............GGGG......", "...........GGGGGGGGG....", "G........GGGHGGGGGGGGG..", "GG.....GGGGGHGGHGGGGGGG.", "GGG..GGGHGGGGGGGHGGGGeGG", "GGGGGGGGGGHGGGGGGGGGGGGG", "GGGGGGGHGGGGHGGGGGGGGGkk", "GGG..GGGGGGGGGGGHGGGGGG.", "GG.....vGGGGGGGGGGGGGG..", "G.......vvGGGGGGGGGG....", "...........vvvGGGv......", ".............vv........." };
static const char *const FISH_KEY = "owkebdyscrgaApGHv";
static const uint8_t FISH_VALS[] = { O1, N6, N1, N1, U1, U2, Y1, N5, C3, R3, N3, F4, F3, P1, G2, G3, W4 };

struct Kind {
  const char *const *rows;
  uint8_t n, w, tail;
  float speed;
};
enum { K_CLOWN, K_TANG, K_TETRA, K_ANGEL, K_GOLD, K_GRAMMA, K_BIG };
static const Kind KINDS[] = { { CLOWN, 6, 12, 3, 0.45f },  { TANG, 8, 14, 3, 0.55f }, { TETRA, 3, 8, 2, 0.75f },
                              { ANGEL, 13, 11, 3, 0.25f }, { GOLD, 7, 12, 4, 0.35f }, { GRAMMA, 5, 10, 2, 0.42f },
                              { BIG, 12, 24, 4, 0.18f } };

struct Fish {
  uint8_t kind;
  float x, y, ty;
  int dir;  // +1 right, -1 left
  uint16_t retarget;
  float home;  // x it likes to stay near (clownfish: the anemone), < 0 = anywhere
};
static Fish fish[] = {
  { K_CLOWN, 70, 52, 52, 1, 40, 82 },  { K_CLOWN, 96, 58, 58, -1, 90, 82 }, { K_TANG, 200, 30, 30, -1, 60, -1 },
  { K_ANGEL, 150, 24, 24, 1, 120, -1 }, { K_GOLD, 250, 44, 44, -1, 70, -1 }, { K_GRAMMA, 40, 36, 36, 1, 50, -1 },
  { K_TETRA, 120, 38, 38, 1, 30, -1 }, { K_TETRA, 110, 34, 34, 1, 0, -1 },   { K_TETRA, 108, 42, 42, 1, 0, -1 },
  { K_TETRA, 98, 38, 38, 1, 0, -1 },    { K_TETRA, 124, 44, 44, 1, 0, -1 },   { K_TETRA, 102, 31, 31, 1, 0, -1 },
  { K_TETRA, 114, 47, 47, 1, 0, -1 },  // followers start in formation (see SCHOOL_DX/DY)
  { K_BIG, 200, 44, 44, -1, 120, -1 },
};
static constexpr int FISH_COUNT = sizeof(fish) / sizeof(fish[0]);
static constexpr int SCHOOL_LEAD = 6;  // the tetras after it follow it at these offsets
static const int8_t SCHOOL_DX[] = { 0, 10, 12, 22, -4, 18, 6 }, SCHOOL_DY[] = { 0, -4, 4, 0, 6, -7, 9 };

struct Dot {
  float x, y;
  uint16_t life;  // frames left, 0 = unused
};
static Dot bubbles[16];
static Dot food[9];
static uint32_t frame_no = 0;
static float slope = 0, slope_v = 0, chop = 0;
static uint32_t panic_until = 0;

static float frand() { return (esp_random() & 0xffff) / 65535.0f; }
static uint32_t hash32(uint32_t v)
{
  v ^= v >> 16;
  v *= 0x7feb352d;
  v ^= v >> 15;
  v *= 0x846ca68b;
  return v ^ (v >> 16);
}

static float surfaceAt(float x)
{
  float t = frame_no * 0.05f;
  return LEVEL + slope * (x - W / 2) + chop * sinf(x * 0.09f + t * 5) + 0.35f * sinf(x * 0.05f - t * 1.1f);
}

static float sandAt(int x) { return SAND + 2 * sinf(x * 0.045f) + sinf(x * 0.13f + 1); }

static void updateWater()
{
  const float dt = 0.05f;
  float gx = 0, gy = 1, gz = 0;
  if (Motion_Ok()) Motion_Gravity(gx, gy, gz);
  float target = constrain(-gx / max(0.3f, gy), -0.1f, 0.1f);  // calm: the surface leans, it does not slosh much
  slope_v += ((target - slope) * 18 - slope_v * 4.5f) * dt;
  slope += slope_v * dt;
  float shake = Motion_Ok() ? Motion_Shake() : 0;
  chop = min(1.4f, max(chop * expf(-dt / 0.8f), shake * 1.2f));
  bool knock = Motion_Ok() && Motion_TakeKnock();
  if (shake > 0.3f || knock) {
    panic_until = millis() + 2200;
  }
}

static int nearestFood(const Fish &f)
{
  int best = -1;
  float best_d = 1e9;
  for (int i = 0; i < 9; i++) {
    if (!food[i].life) continue;
    float d = fabsf(food[i].x - f.x) + fabsf(food[i].y - f.y);
    if (d < best_d) {
      best_d = d;
      best = i;
    }
  }
  return best;
}

static void swim(Fish &f, int idx)
{
  const Kind &k = KINDS[f.kind];
  float max_y = sandAt((int)f.x + k.w / 2) - k.n - 1;
  float min_y = min(max_y, surfaceAt(f.x + k.w / 2) + 2);
  bool panic = millis() < panic_until;
  if (f.kind == K_TETRA && idx != SCHOOL_LEAD) {
    const Fish &lead = fish[SCHOOL_LEAD];
    int o = idx - SCHOOL_LEAD;
    float tx = lead.x - lead.dir * SCHOOL_DX[o], ty = lead.y + SCHOOL_DY[o];
    if (panic) {  // the school bursts apart, then gathers again
      tx += sinf(idx * 2.1f + frame_no * 0.25f) * 30;
      ty += cosf(idx * 1.7f + frame_no * 0.2f) * 12;
    }
    float sp = panic ? 2.4f : 0.9f;
    f.x += constrain(tx - f.x, -sp, sp);
    f.y += constrain(ty - f.y, -sp * 0.5f, sp * 0.5f);
    f.y = constrain(f.y, min_y, max_y);
    if (fabsf(tx - f.x) > 1.5f) f.dir = tx > f.x ? 1 : -1;
    return;
  }
  int target = f.kind == K_TETRA ? -1 : nearestFood(f);
  if (panic) {  // startled: dashes, turns at random
    if (f.retarget-- == 0 || f.retarget > 40) {
      f.retarget = 6 + esp_random() % 18;
      f.dir = esp_random() & 1 ? 1 : -1;
      f.ty = min_y + frand() * max(0.0f, max_y - min_y);
    }
    f.x += f.dir * k.speed * 3.6f;
    f.y += constrain(f.ty - f.y, -0.9f, 0.9f);
  } else if (target >= 0) {
    Dot &p = food[target];
    float mouth = f.x + (f.dir > 0 ? k.w - 1 : 0);
    if (fabsf(p.x - mouth) > 1.5f) f.dir = p.x > mouth ? 1 : -1;
    f.ty = constrain(p.y - k.n / 2.0f, min_y, max_y);
    if (fabsf(p.x - mouth) < 2 && fabsf(p.y - (f.y + k.n / 2.0f)) < 2.5f) p.life = 0;  // eaten
    f.x += f.dir * k.speed * 1.7f;
    f.y += constrain(f.ty - f.y, -0.4f, 0.4f);
  } else {
    f.x += f.dir * k.speed;
    if (f.retarget-- == 0) {
      f.retarget = 50 + esp_random() % 160;
      f.ty = min_y + 2 + frand() * max(0.0f, max_y - min_y - 4);
      if (f.home >= 0) {  // clownfish keep close to the anemone
        f.ty = constrain(46 + frand() * 14, min_y, max_y);
        f.dir = f.x < f.home ? 1 : -1;
        f.retarget = 30 + esp_random() % 60;
      } else if (frand() < 0.3f) {
        f.dir = -f.dir;
      }
    }
    f.y += constrain(f.ty - f.y, -0.15f, 0.15f);
    f.y += sinf((frame_no + idx * 37) * 0.07f) * 0.05f;
  }
  if (f.x < 2) {
    f.x = 2;
    f.dir = 1;
  } else if (f.x > W - k.w - 2) {
    f.x = W - k.w - 2;
    f.dir = -1;
  }
  f.y = constrain(f.y, min_y, max_y);
}

// A fish with its tail beating (the tail columns move up and down).
static void drawFish(const Fish &f, int idx)
{
  const Kind &k = KINDS[f.kind];
  bool flip = f.dir < 0;
  bool panic = millis() < panic_until;
  float beat = sinf(frame_no * (panic ? 1.1f : 0.45f) + idx * 1.3f);
  int x0 = lroundf(f.x), y0 = lroundf(f.y);
  int nk = strlen(FISH_KEY);
  for (int j = 0; j < k.n; j++) {
    for (int i = 0; i < k.w; i++) {
      const char *p = (const char *)memchr(FISH_KEY, k.rows[j][i], nk);
      if (!p) continue;
      int dy = i < k.tail ? lroundf(beat * (k.tail - i) / (float)k.tail) : 0;
      set(x0 + (flip ? k.w - 1 - i : i), y0 + j + dy, FISH_VALS[p - FISH_KEY]);
    }
  }
}

void Fishtank_Feed(int screen_x)
{
  float x = constrain(screen_x / 2, 8, W - 8);
  for (int n = 0, i = 0; i < 9 && n < 3; i++) {
    if (!food[i].life) {
      food[i] = { x + (n - 1) * 3.0f, surfaceAt(x) + 1 + n * 2.0f, 500 };
      n++;
    }
  }
}

static void lighten(int x, int y)
{
  uint8_t c = get(x, y);
  if (c >= W1 && c < W4) set(x, y, c + 1);
}

static void plant(int x, int h, float t, uint8_t dark, uint8_t lit, int seed)
{
  for (int k = 0; k < h; k++) {
    float sway = sinf(k * 0.22f + t + seed) * 2.2f * k / h;
    int y = (int)sandAt(x) - k, xx = x + lroundf(sway);
    set(xx, y, dark);
    set(xx + 1, y, k % 7 < 4 ? lit : dark);
    if (k % 6 == 3) {  // a leaf
      int s = (k / 6) % 2 ? 1 : -1;
      for (int l = 1; l <= 3; l++) set(xx + (s > 0 ? 1 : 0) + s * l, y - l / 2, l == 3 ? lit : dark);
    }
  }
}

void Fishtank_Draw(const char *corner_text)
{
  frame_no++;
  float t = frame_no * 0.05f;
  updateWater();

  // water, deep at the bottom; dark air above the surface
  static const uint8_t WATER[] = { W4, W3, W2, W2, W1 };
  gradient(0, H, WATER, 5);
  float surf[W];
  for (int x = 0; x < W; x++) {
    surf[x] = surfaceAt(x);
    int s = lroundf(surf[x]);
    for (int y = 0; y < s && y < H; y++) buf[y][x] = y < 2 ? N2 : N1;
    set(x, s, W5);
    if ((x + frame_no / 3) % 9 < 5) set(x, s + 1, W4);
  }
  // light shafts from the surface, drifting
  for (int r = 0; r < 4; r++) {
    float x0 = 40 + r * 78 + sinf(t * 0.3f + r * 2) * 14;
    for (int y = (int)LEVEL + 2; y < SAND; y++) {
      int cx = lroundf(x0 + (y - LEVEL) * 0.45f);
      int w = 7 + r % 3 * 2;
      for (int x = cx; x < cx + w; x++) {
        if (((x + y) & 1) && (y < 50 || ((x * 3 + y) % 4 == 0))) lighten(x, y);
      }
    }
  }
  // far rocks in the haze
  for (int x = 0; x < W; x++) {
    int top = 58 + lroundf(6 * sinf(x * 0.021f + 0.5f) + 3 * sinf(x * 0.067f));
    for (int y = top; y < SAND + 2; y++) set(x, y, W1);
  }
  // sand with grain and moving caustics
  for (int x = 0; x < W; x++) {
    int top = (int)sandAt(x);
    for (int y = top; y < H; y++) {
      uint32_t h = hash32(x * 131 + y * 7919);
      uint8_t c = y == top ? S3 : (h % 11 == 0 ? S1 : (h % 17 == 0 ? S3 : S2));
      float caustic = sinf(x * 0.21f + t * 1.7f) + sinf(x * 0.13f - y * 0.5f + t * 1.1f);
      if (y < top + 4 && caustic > 1.5f) c = S3;
      set(x, y, c);
    }
  }
  // kelp behind, rocks, the anemone, the coral
  plant(18, 52, t, G1, G2, 1);
  plant(26, 40, t * 1.1f, G1, G2, 4);
  plant(232, 58, t * 0.9f, G1, G2, 2);
  plant(300, 46, t, G1, G2, 7);
  auto rock = [](int cx, int r, int h) {
    for (int y = -h; y <= 0; y++) {
      int hw = (int)(r * sqrtf(1 - (float)(y * y) / (h * h)));
      for (int x = -hw; x <= hw; x++) {
        int sy = (int)sandAt(cx) + y + 1;
        uint8_t c = (x < -hw / 2 || y > -2) ? N2 : (x > hw / 3 && y < -h / 2 ? N4 : N3);
        set(cx + x, sy, c);
      }
    }
  };
  rock(170, 16, 9);
  rock(186, 9, 6);
  rock(272, 12, 7);
  // anemone: swaying tentacles with magenta tips
  for (int k = 0; k < 13; k++) {
    int bx = 74 + k * 2, len = 9 + (k * 5) % 6;
    for (int l = 0; l < len; l++) {
      float sway = sinf(t * 1.6f + k * 0.6f + l * 0.25f) * l * 0.25f;
      set(bx + lroundf(sway), (int)sandAt(bx) - 1 - l, l == len - 1 ? M2 : (l > len - 4 ? P1 : N5));
    }
  }
  // coral: a branching magenta fan
  for (int b = 0; b < 6; b++) {
    float a = -1.2f + b * 0.48f;
    int x = 214, y = (int)sandAt(214);
    for (int l = 0; l < 16 - abs(b - 2) * 2; l++) {
      x += lroundf(sinf(a + sinf(l * 0.7f + b) * 0.3f) * 1.2f);
      y -= 1;
      set(x, y, l % 5 == 4 ? M2 : M1);
      if (l % 4 == 2) set(x + (b % 2 ? 1 : -1), y, M2);
    }
  }
  // bubbles from the rock, popping at the surface
  if (frame_no % 9 == 0) {
    for (auto &b : bubbles) {
      if (!b.life) {
        b = { 172.0f + frand() * 3, sandAt(172) - 10, 300 };
        break;
      }
    }
  }
  for (auto &b : bubbles) {
    if (!b.life) continue;
    b.y -= 0.55f;
    b.x += sinf(b.y * 0.4f) * 0.25f;
    b.life--;
    if (b.y < surfaceAt(b.x) + 1) {
      b.life = 0;
      continue;
    }
    int x = lroundf(b.x), y = lroundf(b.y);
    if (b.y < 40) {  // grown on the way up: a ring
      set(x, y - 1, W5);
      set(x - 1, y, W5);
      set(x + 1, y, W5);
      set(x, y + 1, W4);
    } else {
      set(x, y, W5);
    }
  }
  // food: straight down, then lies on the sand for a while
  for (auto &p : food) {
    if (!p.life) continue;
    p.life--;
    if (p.y < sandAt(p.x) - 1) p.y += 0.22f;
    set(lroundf(p.x), lroundf(p.y), F4);
  }
  for (int i = 0; i < FISH_COUNT; i++) swim(fish[i], i);
  for (int i = 0; i < FISH_COUNT; i++) drawFish(fish[i], i);
  // front kelp and grass
  plant(132, 30, t * 1.2f, G2, G3, 3);
  plant(312, 26, t * 1.3f, G2, G3, 5);
  for (int g = 0; g < 14; g++) {
    int x = (g * 53 + 11) % W, hgt = 3 + g % 4;
    for (int l = 0; l < hgt; l++) set(x + lroundf(sinf(t + g) * l * 0.3f), (int)sandAt(x) - l, l == hgt - 1 ? G3 : G2);
  }

  present();
  if (corner_text && *corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 2, 2, vfd::DIM_INK);
  }
}

#endif
