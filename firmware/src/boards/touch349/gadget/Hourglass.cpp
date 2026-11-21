#ifdef BOARD_TOUCH349

#include "Hourglass.h"
#include "Motion.h"
#include "Px.h"
#include "Settings.h"
#include "Radio.h"
#include "Theme.h"
#include "RetroScene.h"

using namespace px;

// Grid of big cells, each a 3x3 dot in a 4x4 slot (LED-matrix look), centered in the 86x320 portrait.
static constexpr int S = 4, CW = 21, CH = 72, NECK = 36;  // rows < NECK: the "upper" bulb with the left edge down
static constexpr int OX = (PW - CW * S) / 2, OY = (PH - CH * S) / 2;
static const int DURATIONS[] = { 5, 10, 15, 20 };

static uint8_t grid[CH][CW];  // 0 empty, 1/2 sand shades, | 0x80 moved this frame
static int8_t half[CH];       // inside a row: |x - CW/2| <= half
static int dur_idx = 1, dir = 1, total = 0;
static float acc = 0;
static bool ready = false, beeped = false;
static uint32_t last_ms = 0, seed = 0x2545f491;

static uint32_t rnd()
{
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

static bool inside(int x, int y) { return y >= 0 && y < CH && abs(x - CW / 2) <= half[y]; }

static void refill()
{
  memset(grid, 0, sizeof(grid));
  int cap = 0;
  for (int y = 0; y < NECK; y++) cap += 2 * half[y] + 1;
  int want = cap * 72 / 100;
  total = 0;
  // settled in the bulb that is up now, from the neck outward
  for (int k = 0; k < NECK && total < want; k++) {
    int y = dir > 0 ? NECK - 1 - k : NECK + k;
    for (int x = 0; x < CW && total < want; x++)
      if (inside(x, y)) grid[y][x] = 1 + (rnd() % 5 == 0), total++;
  }
  acc = 0;
  beeped = false;
}

static void hgInit()
{
  for (int y = 0; y < CH; y++) {
    float d = fabsf(y + 0.5f - NECK) / NECK;  // 0 at the neck, 1 at the ends
    float hw = 0.4f + 9.6f * powf(sinf(d * PI / 2), 0.8f);
    if (d > 0.9f) hw *= 1 - (d - 0.9f) * 2.5f;  // round off the ends
    half[y] = (int8_t)hw;
  }
  refill();
  ready = true;
}

static int upperCount()
{
  int n = 0;
  for (int y = 0; y < CH; y++)
    if ((dir > 0) == (y < NECK))
      for (int x = 0; x < CW; x++) n += grid[y][x] != 0;
  return n;
}

static void step(float side)
{
  for (int y = 0; y < CH; y++)
    for (int x = 0; x < CW; x++) grid[y][x] &= 0x7f;
  bool ltr = rnd() & 1;
  for (int k = 0; k < CH; k++) {
    int y = dir > 0 ? CH - 1 - k : k;  // start from the side the sand falls toward
    int ny = y + dir;
    if (ny < 0 || ny >= CH) continue;
    bool crossing = (dir > 0 && y == NECK - 1) || (dir < 0 && y == NECK);
    for (int i = 0; i < CW; i++) {
      int x = ltr ? i : CW - 1 - i;
      uint8_t g = grid[y][x];
      if (!g) continue;
      int tx = -1;
      if (inside(x, ny) && !grid[ny][x]) {
        tx = x;
      } else {
        int s = (side > 0.15f) ? 1 : (side < -0.15f) ? -1 : ((rnd() & 1) ? 1 : -1);
        if (inside(x + s, ny) && !grid[ny][x + s]) tx = x + s;
        else if (inside(x - s, ny) && !grid[ny][x - s]) tx = x - s;
      }
      if (tx < 0) continue;
      if (crossing) {
        if (acc < 1) continue;
        acc -= 1;
      }
      grid[ny][tx] = g | 0x80;
      grid[y][x] = 0;
    }
  }
}

// 3x5 digits, D and K, drawn 2x in the viewer's frame (upright whichever end is up).
static const uint8_t DIG[10][5] = { { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 },
                                    { 5, 5, 7, 1, 1 }, { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 2, 2, 2 },
                                    { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 } };
static const uint8_t LD[5] = { 6, 5, 5, 5, 6 }, LK[5] = { 5, 5, 6, 5, 5 };

static void text(const char *s, int y, uint8_t c)
{
  int w = 0;
  for (const char *p = s; *p; p++) w += (*p == ':' ? 1 : *p == ' ' ? 2 : 3) * 2 + 2;
  int x = (PW - w + 2) / 2;
  bool up = dir > 0;
  for (; *s; s++) {
    const uint8_t *g = *s >= '0' && *s <= '9' ? DIG[*s - '0'] : *s == 'D' ? LD : *s == 'K' ? LK : nullptr;
    if (*s == ':') {
      for (int dy : { 2, 6 })
        for (int k = 0; k < 4; k++) pset(x + (k & 1), y + dy + (k >> 1), c, up);
      x += 4;
      continue;
    }
    if (g)
      for (int r = 0; r < 5; r++)
        for (int b = 0; b < 3; b++)
          if (g[r] & (4 >> b))
            for (int k = 0; k < 4; k++) pset(x + b * 2 + (k & 1), y + r * 2 + (k >> 1), c, up);
    x += (*s == ' ' ? 2 : 3) * 2 + 2;
  }
}

// Portrait canvas coordinates, matching pset(..., true), without another scene buffer.
static void portraitRect(int x, int y, int w, int h, uint16_t color)
{
  int x0 = max(0, x), y0 = max(0, y);
  int x1 = min(vfd::H, x + w), y1 = min(vfd::W, y + h);
  if (x1 > x0 && y1 > y0) vfd::rect(vfd::W - y1, x0, y1 - y0, x1 - x0, color);
}

static void portraitText(const char *s, int y, uint16_t color)
{
  const vfd::Font &font = *vfd::style().font;
  const int p = 2;
  int x = (vfd::H - vfd::textWidth(s, p)) / 2;
  // These labels contain only digits, colon and MIN/DK, all native font glyphs.
  for (; *s; s++, x += font.adv * p) {
    const uint8_t *rows = font.rows((uint8_t)*s);
    if (!rows) continue;
    for (int pass = 0; pass < 2; pass++)
      for (int r = 0; r < font.h; r++)
        for (int c = 0; c < font.w; c++)
          if (rows[r] & (1 << (font.w - 1 - c))) {
            int d = pass ? 0 : 1;
            portraitRect(x + c * p - d, y + r * p - d, p + d * 2, p + d * 2, pass ? color : 0);
          }
  }
}

static void machineGlass(uint32_t secs, bool flowing)
{
  const theme::Id id = theme::current();
  const char *picture = id == theme::C64 ? "sand_c64" : id == theme::AMIGA ? "sand_amiga" : "sand_atari";
  if (!RetroPicture_Draw(picture)) vfd::clear(px::ink(NIGHT));
  uint16_t light, dark, glass, wood;
  if (id == theme::C64) {
    light = rgb(184, 199, 111); dark = rgb(111, 79, 37);
    glass = rgb(149, 149, 149); wood = rgb(154, 103, 89);
  } else if (id == theme::AMIGA) {
    light = rgb(238, 204, 119); dark = rgb(204, 153, 68);
    glass = rgb(187, 221, 238); wood = rgb(187, 119, 68);
  } else {
    light = rgb(236, 200, 120); dark = rgb(220, 180, 104);
    glass = rgb(236, 236, 236); wood = rgb(208, 128, 92);
  }
  for (int y = 0; y < CH; y++) {
    int left = CW / 2 - half[y] - 1, right = CW / 2 + half[y] + 1;
    portraitRect((OX + left * S) * 2, (OY + y * S) * 2, S * 2, S * 2, glass);
    portraitRect((OX + right * S) * 2, (OY + y * S) * 2, S * 2, S * 2, glass);
  }
  // Caps and posts sit outside the cells, so the simulation remains untouched.
  portraitRect(0, (OY - S) * 2, PW * 2, S * 2, wood);
  portraitRect(0, (OY + CH * S) * 2, PW * 2, S * 2, wood);
  portraitRect(0, OY * 2, 2, CH * S * 2, wood);
  portraitRect(PW * 2 - 2, OY * 2, 2, CH * S * 2, wood);
  for (int y = 0; y < CH; y++)
    for (int x = 0; x < CW; x++) {
      uint8_t g = grid[y][x];
      if (g) portraitRect((OX + x * S) * 2, (OY + y * S) * 2, S * 2, S * 2,
                          (g & 0x80) || (g & 0x7f) != 2 ? light : dark);
    }
  char label[12];
  snprintf(label, sizeof(label), "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  portraitText(label, 4, flowing ? glass : dark);
  snprintf(label, sizeof(label), L("%d MIN", "%d DK"), DURATIONS[dur_idx]);
  portraitText(label, PH * 2 - 20, light);
}

void Hourglass_Draw(const char *)
{
  if (!ready) hgInit();
  uint32_t now = millis();
  float dt = constrain((now - last_ms) / 1000.0f, 0.0f, 0.2f);
  last_ms = now;

  float gx, gy, gz;
  Motion_Gravity(gx, gy, gz);
  float along = -gx, side = gy;  // portrait frame: +x = screen +y, +y = screen -x
  bool flowing = fabsf(along) >= 0.4f;
  if (flowing) {
    int nd = along > 0 ? 1 : -1;
    if (nd != dir) {
      dir = nd;
      beeped = false;
    }
    acc = min(acc + total / (DURATIONS[dur_idx] * 60.0f) * dt, 2.0f);
    step(dir > 0 ? side : -side);
  }

  int up = upperCount();
  if (flowing && up == 0 && !beeped && total > 0) {
    beeped = true;
    Radio_Beep(1318, 110, 2, 90);
  }

  uint32_t secs = total ? (uint32_t)(up * DURATIONS[dur_idx] * 60.0f / total + 0.5f) : 0;
  if (theme::current() != theme::VFD) {
    machineGlass(secs, flowing);
    return;
  }

  clear(NIGHT);
  for (int y = 0; y < CH; y++)
    for (int x = 0; x < CW; x++) {
      uint8_t g = grid[y][x];
      if (!g) continue;
      uint8_t c = (g & 0x7f) == 2 ? R3 : M2;
      for (int k = 0; k < (S - 1) * (S - 1); k++) pset(OX + x * S + k % (S - 1), OY + y * S + k / (S - 1), c, true);
    }

  // time left at the top, the chosen duration at the bottom (viewer's frame)
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(secs / 60), (unsigned long)(secs % 60));
  text(buf, 3, flowing ? C3 : C1);
  snprintf(buf, sizeof(buf), settings::tr() ? "%d DK" : "%d MIN", DURATIONS[dur_idx]);
  text(buf, PH - 13, C2);
  present();
}

void Hourglass_Tap()
{
  dur_idx = (dur_idx + 1) % 4;
  if (!ready) hgInit();
  refill();
  Radio_Beep(1046, 30);
}

void Hourglass_Reset()
{
  if (!ready) hgInit();
  refill();
  Radio_Beep(523, 60);
}

void Hourglass_Tick() {}

#endif
