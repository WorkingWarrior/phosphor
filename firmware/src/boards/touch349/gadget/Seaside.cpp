#ifdef BOARD_TOUCH349

#include "Seaside.h"
#include <time.h>
#include "Motion.h"
#include "Px.h"
#include "RetroScene.h"
#include "Vfd.h"

using namespace px;

static constexpr int HORIZON = 38, SHORE = 64;
static constexpr int MOON_X = 130;  // moon and its reflection, clear of the palm
static uint32_t frame_no = 0;
static float big_wave = 0, boat_x = 60;
static uint32_t gull_t0 = 0;

static float frand01() { return (esp_random() & 0xffff) / 65535.0f; }
static uint32_t hash32(uint32_t v)
{
  v ^= v >> 16;
  v *= 0x7feb352d;
  v ^= v >> 15;
  v *= 0x846ca68b;
  return v ^ (v >> 16);
}

enum class Light { Day, Dusk, Night };

static Light lightNow()
{
  time_t t = time(nullptr);
  struct tm lt;
  localtime_r(&t, &lt);
  if (t < 100000) return Light::Night;  // no clock yet
  int h = lt.tm_hour;
  if (h >= 20 || h < 6) return Light::Night;
  if (h >= 17 || h < 8) return Light::Dusk;
  return Light::Day;
}

// ---- storm (hold), shooting star (tap on the night sky), the boat rocking with the tilt -----------------------------------

static bool retro = false;     // the theme's own picture is showing: only the old tap and a rougher sea apply
static bool storm = false;
static uint32_t storm_end = 0; // frame_no when the storm blows over by itself
static int flash_t = 0;        // lightning: frames left of the flash
static uint32_t flash_next = 0;
static int bolt_x[20];
struct Drop {
  float x, y;
};
static Drop rain[110];
static int star_t = -1;
static float star_x, star_y, star_dx, star_dy;
static float roll = 0, roll_v = 0;  // the boat's lean, radians

static void lightning()
{
  flash_t = 6;
  bolt_x[0] = 30 + esp_random() % 260;
  for (int i = 1; i < 20; i++) bolt_x[i] = constrain(bolt_x[i - 1] + (int)(esp_random() % 9) - 4, 2, W - 3);
}

void Seaside_Hold()
{
  storm = !storm;
  storm_end = frame_no + 1800;  // 90 s
  flash_next = frame_no + 20 + esp_random() % 40;
  if (storm)
    for (auto &d : rain) d = { (float)(esp_random() % (W + 40)), (float)(esp_random() % HORIZON) };
  RetroScene_Stir(1.5f);  // with the theme's picture: the sea gets rough
}

void Seaside_Tap(int sx, int sy)
{
  if (!retro && sy < 2 * HORIZON - 2) {
    if (storm) {  // a bolt right now
      lightning();
      return;
    }
    if (lightNow() == Light::Night) {  // a shooting star from where you tapped
      star_t = 0;
      star_x = constrain(sx / 2, 6, W - 6);
      star_y = constrain(sy / 2, 2, HORIZON - 12);
      star_dx = (star_x < W / 2 ? 1 : -1) * (2.0f + frand01() * 0.8f);
      star_dy = 0.7f + frand01() * 0.5f;
      return;
    }
  }
  big_wave = 1;
  RetroScene_Stir(0.6f);  // with the theme's picture: the sea gets rougher for a moment
}

// Dark cloud deck, rain and flashes instead of the sky of the hour.
static void stormSky()
{
  static const uint8_t DARK[] = { NIGHT, N1, N1, N2 }, FLASH[] = { N3, N4, N5, N5 };
  bool lit = flash_t == 6 || flash_t == 5 || flash_t == 3;
  gradient(0, HORIZON, lit ? FLASH : DARK, 4);
  for (int x = 0; x < W; x++) {
    int deck = 7 + (int)(4 * sinf(x * 0.03f + frame_no * 0.004f) + 3 * sinf(x * 0.08f - frame_no * 0.01f));
    for (int y = 0; y < deck; y++) set(x, y, lit ? N4 : N2);
    set(x, deck, ((x + deck) & 1) ? (lit ? N4 : N2) : (lit ? N5 : N3));
    int low = 20 + (int)(3 * sinf(x * 0.05f + 1 - frame_no * 0.006f));
    if (((x + low) & 1) == 0) set(x, low, lit ? N5 : N1);
  }
  if (lit && flash_t != 5)  // the bolt, forking once
    for (int y = 8; y < HORIZON; y++) {
      int n = (y - 8) * 19 / (HORIZON - 8), fr = (y - 8) * 19 % (HORIZON - 8);
      int x = bolt_x[n] + (bolt_x[n + 1] - bolt_x[n]) * fr / (HORIZON - 8);
      set(x, y, N6);
      if (y > 17 && y < 28) set(x + (y - 17) * 2 / 3 + 1, y, N5);
    }
}

static void stormRain()
{
  for (auto &d : rain) {
    d.y += 3.6f;
    d.x -= 1.3f;
    if (d.y > SHORE + 18 || d.x < -2) d = { (float)(esp_random() % (W + 40)), -(float)(esp_random() % 12) };
    int x = (int)d.x, y = (int)d.y;
    set(x, y, N4);
    set(x + 1, y - 2, N3);
  }
}

static void shootingStar()
{
  if (star_t < 0) return;
  const int LIFE = 20;
  float hx = star_x + star_dx * star_t, hy = star_y + star_dy * star_t;
  for (int k = 9; k >= 0; k--) {
    int x = lroundf(hx - star_dx * k * 0.55f), y = lroundf(hy - star_dy * k * 0.55f);
    if ((unsigned)x >= (unsigned)W || y < 0 || y >= HORIZON) continue;
    set(x, y, k == 0 ? N6 : k < 3 ? N5 : k < 6 ? N4 : N3);
  }
  if (++star_t > LIFE) star_t = -1;
}

static void sky(Light l)
{
  static const uint8_t DAY[] = { W3, W4, W4, W5, N5 };
  static const uint8_t DUSK[] = { N2, P1, M1, R2, F2, F3 };
  static const uint8_t NIGHTS[] = { NIGHT, NIGHT, NIGHT, N1 };
  if (l == Light::Day) gradient(0, HORIZON, DAY, 5);
  else if (l == Light::Dusk) gradient(0, HORIZON, DUSK, 6);
  else gradient(0, HORIZON, NIGHTS, 4);
  if (l == Light::Night) {
    for (int i = 0; i < 90; i++) {  // stars all the way down to the horizon
      uint32_t h = hash32(i * 977 + 3);
      int x = h % W, y = (h >> 9) % (HORIZON - 2);
      bool tw = ((frame_no / 8 + i) % 23) == 0;
      set(x, y, tw ? N4 : (i % 4 ? N3 : N5));
    }
    disc(MOON_X, 12, 5, N6);  // moon with a dark bite
    disc(MOON_X + 3, 10, 4, N2);
  } else if (l == Light::Dusk) {
    for (int y = HORIZON - 9; y < HORIZON; y++)
      for (int x = 150; x < 180; x++) {
        float d = (x - 165) * (x - 165) + (y - HORIZON) * (y - HORIZON) * 1.0f;
        if (d < 120) set(x, y, d < 50 ? F5 : F4);
      }
  } else {
    disc(250, 9, 5, F5);
    disc(250, 9, 3, Y1);
    // soft clouds
    for (int c = 0; c < 3; c++) {
      int cx = (int)(40 + c * 105 + frame_no * 0.03f * (c + 1)) % (W + 40) - 20, cy = 8 + c * 6;
      for (int k = -2; k <= 2; k++) disc(cx + k * 5, cy + (k & 1), 3 + (k == 0), N6);
      rect(cx - 12, cy + 2, 26, 2, N5);
    }
  }
}

// Sea in perspective: rows bunch up toward the horizon, each row carries long thin wave crests that drift sideways;
// far rows have short slow ripples, near rows long ones. Under the sun or moon a broad column of broken horizontal
// glints (wider near the viewer), not a thin line.
static void sea(Light l)
{
  uint8_t far = W2, mid = W3, near = W2, crest = W5, trough = W1, glint = N6;
  if (l == Light::Night) far = N1, mid = W1, near = W1, crest = N3, trough = W1, glint = N5;
  if (l == Light::Dusk) far = P1, mid = W2, near = N2, crest = F3, trough = N1, glint = F4;
  float t = frame_no * 0.05f;
  int gx = l == Light::Night ? MOON_X : (l == Light::Dusk ? 165 : 250);
  for (int y = HORIZON; y < SHORE; y++) {
    float d = (y - HORIZON + 1) / (float)(SHORE - HORIZON);  // 0 far .. 1 near
    uint8_t base = d < 0.25f ? far : (d < 0.7f ? mid : near);
    float freq = 0.5f / (0.15f + d * 1.6f);   // ripples shorter far away
    float drift = t * (0.6f + d * 1.5f) * (y & 1 ? 1 : -0.6f);
    float ph = hash32(y * 7) % 628 / 100.0f;
    // column of the sun/moon reflection
    float hw = 5 + d * 14;
    for (int x = 0; x < W; x++) {
      float w = sinf(x * freq + drift + ph) + 0.45f * sinf(x * freq * 2.3f - drift * 1.7f + ph * 2);
      uint8_t c = base;
      // crests only on every other row and only the tallest part: clean dashes instead of speckle
      if (w > 1.15f && ((y - HORIZON) & 1)) c = crest;
      (void)trough;
      float dx = fabsf(x - gx);
      if (dx < hw && w > 0.35f - (1 - dx / hw) * 0.6f) c = glint;
      buf[y][x] = c;
    }
  }
}

static void beach(Light l)
{
  float t = frame_no * 0.05f;
  big_wave *= 0.985f;
  uint8_t sand = l == Light::Night ? S1 : S2, dry = l == Light::Night ? S1 : S3, wet = l == Light::Night ? N2 : S1;
  float reach = 4 + 3 * sinf(t * 0.9f) + big_wave * 9;  // how far up the sand the water comes
  for (int x = 0; x < W; x++) {
    float wob = sinf(x * 0.06f + t * 0.7f) * 1.5f + sinf(x * 0.017f - t * 0.3f) * 1.5f;
    int edge = SHORE + (int)(reach + wob);
    for (int y = SHORE; y < H; y++) {
      uint8_t c;
      if (y == edge - 3 && sinf(x * 0.11f + t * 1.3f) > 0.2f) c = l == Light::Night ? N4 : N6;  // second, broken foam line
      else if (y < edge - 1) c = l == Light::Night ? W1 : W4;
      else if (y <= edge) c = N6;  // foam
      else if (y < edge + 4 + (int)(big_wave * 4)) c = wet;
      else c = ((x * 7 + y * 13) % 29 == 0) ? dry : sand;
      buf[y][x] = c;
    }
  }
}

// Lighthouse on a rock out in the sea, nearer than the horizon so it stands bigger and its lamp sits well below the top
static void lighthouse(Light l)
{
  const int cx = 236, base = HORIZON + 14, top = 21;
  for (int y = base - 6; y < base + 4; y++) {  // rock
    int hw = 9 + (y - base + 6) * 2;
    for (int x = cx - hw; x <= cx + hw; x++) set(x, y, ((x * 3 + y * 5) % 7) ? N2 : N3);
  }
  for (int y = top; y < base - 6; y++) {  // tower, red and white bands, a little wider at the foot
    int hw = 4 + (y - top) / 9;
    uint8_t c = ((y - top) / 6) % 2 ? R2 : N6;
    for (int x = cx - hw; x <= cx + hw; x++) set(x, y, x == cx + hw ? (c == N6 ? N4 : R1) : c);
  }
  rect(cx - 6, top - 1, 13, 1, N3);  // gallery
  rect(cx - 4, top - 7, 9, 6, N2);   // lantern room
  bool lit = l != Light::Day;
  rect(cx - 2, top - 6, 5, 4, lit ? F5 : N4);
  rect(cx - 5, top - 8, 11, 1, N3);  // roof
  rect(cx - 1, top - 10, 3, 2, N3);
  if (lit) {  // the beam sweeps left and back
    float a = sinf(frame_no * 0.03f);
    int len = (int)(fabsf(a) * 150) + 10, dir = a < 0 ? -1 : 1;
    for (int k = 5; k < len; k++) {
      int spread = k / 14;
      for (int s = -spread; s <= spread; s++) {
        int x = cx + dir * k, y = top - 4 + s;
        if (((x + y) & 1) == 0 || k < 30) set(x, y, k < 40 ? F4 : F2);
      }
    }
  }
}

// The boat's colour at (sx, sy) from its waterline centre, 0 = nothing there.
static uint8_t boatPixel(int sx, int sy, Light l)
{
  uint8_t hull = l == Light::Night ? N1 : D2, sail = l == Light::Night ? N4 : N6;
  if (sy >= 0 && sy <= 1 && sx >= -7 && sx <= 6) return hull;
  if (sy == 2 && sx >= -5 && sx <= 4) return hull;
  if (sy >= -11 && sy <= -1) {
    int y = -1 - sy;  // 0 at the deck, up the mast
    if (y == 10) return l == Light::Night && sx == 0 ? F4 : CLR;
    if (sx == 0) return N2;
    if (sx > 0 && sx <= (10 - y) * 6 / 10) return sail;
    if (sx < 0 && y < 7 && -sx <= (7 - y) / 2) return sail;
  }
  return CLR;
}

// It rocks on its own, a lot in a storm, and swings with the board's tilt like a spring.
static void boat(Light l)
{
  boat_x += 0.04f;
  if (boat_x > W + 20) boat_x = -20;
  float gx = 0, gy = 1, gz = 0;
  if (Motion_Ok()) Motion_Gravity(gx, gy, gz);
  float target = -constrain(gx, -0.7f, 0.7f) * 0.6f + sinf(frame_no * 0.08f) * (storm ? 0.2f : 0.03f);
  roll_v += (target - roll) * 0.1f - roll_v * 0.07f;
  roll += roll_v;
  int bx = (int)boat_x, by = HORIZON + 4 + (int)(sinf(frame_no * 0.08f) * (storm ? 2.4f : 0.8f));
  float c = cosf(roll), sn = sinf(roll);
  for (int dy = -14; dy <= 5; dy++)
    for (int dx = -13; dx <= 13; dx++) {
      int sx = lroundf(dx * c + (dy - 1) * sn), sy = lroundf(-dx * sn + (dy - 1) * c) + 1;
      uint8_t col = boatPixel(sx, sy, l);
      if (col) set(bx + dx, by + dy, col);
    }
}

static void gulls()
{
  uint32_t now = millis();
  if (now - gull_t0 > 24000) gull_t0 = now;
  float p = (now - gull_t0) / 24000.0f;
  for (int g = 0; g < 2; g++) {
    int x = (int)(-20 + p * (W + 40)) - g * 14, y = 16 + g * 5 + (int)(sinf(p * 20 + g) * 2);
    bool up = ((frame_no / 4) + g) % 2;
    set(x, y, N1), set(x - 1, y - (up ? 1 : 0), N1), set(x + 1, y - (up ? 1 : 0), N1);
    set(x - 2, y - (up ? 2 : 0), N1), set(x + 2, y - (up ? 2 : 0), N1);
  }
}

// A little dog on the sand: sits and wags for a while, then dashes off somewhere along the beach and sits again.
static const char *const DOG_SIT[] = {
  "........e...",
  ".......eBB..",
  ".......BoBBk",
  "........BBBB",
  "......BBB...",
  ".....BBBw...",
  "....BBBBw...",
  "...BBBBBw...",
  "..BBBBBBB...",
  "..BBB.BB.B..",
};
static const char *const DOG_RUN[2][7] = {
  {
    "...........e...",
    "..........eBB..",
    "..........BoBBk",
    "..BBBBBBBBBBB..",
    "..BBBBBBBBBw...",
    ".BB.......BB...",
    "B...........B..",
  },
  {
    "...........e...",
    "..........eBB..",
    "..........BoBBk",
    "..BBBBBBBBBBB..",
    "..BBBBBBBBBw...",
    "...BB....BB....",
    "...B......B....",
  },
};
static const char DOG_KEY[] = "Beokw";
static float dog_x = 100, dog_to = 100;
static bool dog_left = false, dog_running = false;
static uint32_t dog_sit_until = 300;

static void dog(Light l)
{
  // a white dog with brown ears: stands out on the sand in every theme
  const uint8_t vals[] = { N5, D2, N1, N1, N6 };
  (void)l;
  const int floor_y = 84;
  if (dog_running) {
    float d = dog_to - dog_x;
    if (fabsf(d) < 1.3f) {
      dog_running = false;
      dog_sit_until = frame_no + 240 + hash32(frame_no) % 240;  // sit 12..24 s
    } else {
      dog_x += d > 0 ? 1.3f : -1.3f;
    }
  } else if (frame_no > dog_sit_until) {
    do dog_to = 50 + hash32(frame_no * 31 + (uint32_t)dog_x) % 190;
    while (fabsf(dog_to - dog_x) < 40);
    dog_left = dog_to < dog_x;
    dog_running = true;
  }
  int x = (int)dog_x;
  if (dog_running) {
    int f = (frame_no / 3) % 2;
    int w = 15, ox = dog_left ? x - w / 2 : x - w / 2;
    sprite(DOG_RUN[f], 7, ox, floor_y - 7 - (f ? 1 : 0), DOG_KEY, vals, dog_left);
    // tail streaming out behind
    int tx = dog_left ? ox + w - 1 : ox, dir = dog_left ? 1 : -1;
    set(tx + dir * 1, floor_y - 5 - f, vals[1]), set(tx + dir * 2, floor_y - 6 - f, vals[1]);
  } else {
    int w = 12, ox = x - w / 2;
    sprite(DOG_SIT, 10, ox, floor_y - 10, DOG_KEY, vals, dog_left);
    bool wag = (frame_no / 4) % 2;  // tail wag
    int tx = dog_left ? ox + w - 2 : ox + 1, dir = dog_left ? 1 : -1;
    if (wag) set(tx, floor_y - 3, vals[1]), set(tx + dir, floor_y - 4, vals[1]), set(tx + dir * 2, floor_y - 5, vals[1]);
    else set(tx, floor_y - 2, vals[1]), set(tx + dir, floor_y - 2, vals[1]), set(tx + dir * 2, floor_y - 3, vals[1]);
  }
}

static void palm(Light l)
{
  uint8_t trunk = l == Light::Night ? N3 : D2, leaf = l == Light::Night ? N3 : G2, leaf2 = l == Light::Night ? N4 : G3;
  float sway = sinf(frame_no * 0.04f) * 1.2f;
  int tx = 0, ty = 0;
  for (int k = 0; k < 44; k++) {
    float a = k / 44.0f;
    tx = 30 + (int)(a * a * 14 + a * sway * 2);
    ty = 82 - k;
    rect(tx - 1, ty, 3, 1, (k % 4 == 0 && l != Light::Night) ? D1 : trunk);
  }
  static const float ANG[] = { -2.7f, -2.2f, -1.6f, -0.9f, -0.4f, 0.1f };
  for (int f = 0; f < 6; f++) {
    float an = ANG[f] + sway * 0.05f;
    for (int k = 0; k < 18; k++) {
      float x = tx + cosf(an) * k, y = ty + sinf(an) * k + k * k * 0.04f;
      set((int)x, (int)y, leaf);
      if (k > 3 && k % 2 == 0) set((int)x, (int)y + 1, leaf2), set((int)x, (int)y + 2, leaf);
    }
  }
  disc(tx + 1, ty + 2, 1, D1);
}

void Seaside_Draw(const char *corner_text)
{
  retro = RetroScene_Draw(RetroArt::Sea, corner_text);
  if (retro) return;  // the theme's own picture of this scene
  frame_no++;
  Light l = lightNow();
  if (storm && (int32_t)(frame_no - storm_end) > 0) storm = false;
  if (storm) {
    l = Light::Night;
    big_wave = max(big_wave, 0.7f);  // a rough sea
    if (flash_t > 0) flash_t--;
    else if (frame_no >= flash_next) {
      lightning();
      flash_next = frame_no + 40 + esp_random() % 120;
    }
    stormSky();
  } else {
    flash_t = 0;
    sky(l);
  }
  sea(l);
  lighthouse(l);
  boat(l);
  beach(l);
  dog(l);
  palm(l);
  if (l != Light::Night) gulls();
  if (storm) stormRain();
  else shootingStar();
  present();
  if (corner_text && *corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 154, 2, vfd::Ink{ l == Light::Day ? px::ink(N1) : px::ink(C3) });
  }
}

#endif
