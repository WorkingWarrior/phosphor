#ifdef BOARD_TOUCH349

#include "Px.h"
#include "Vfd.h"

namespace px {

const uint16_t PALETTE[COLORS] = {
  rgb(5, 8, 14),                                                                      // CLR
  rgb(5, 8, 14),                                                                      // NIGHT
  rgb(12, 17, 27), rgb(22, 30, 44), rgb(38, 50, 68), rgb(70, 86, 108), rgb(122, 140, 160), rgb(196, 210, 220),
  rgb(8, 52, 66), rgb(56, 150, 168), rgb(118, 244, 226),                              // cyan
  rgb(96, 16, 64), rgb(250, 66, 178),                                                 // magenta
  rgb(56, 18, 8), rgb(138, 46, 10), rgb(226, 108, 22), rgb(255, 176, 58), rgb(255, 228, 150),  // amber
  rgb(4, 20, 34), rgb(6, 36, 56), rgb(12, 60, 86), rgb(30, 98, 128), rgb(90, 170, 190),        // water
  rgb(66, 52, 36), rgb(128, 102, 68), rgb(190, 160, 112),                             // sand
  rgb(10, 40, 30), rgb(24, 92, 62), rgb(70, 160, 96),                                 // plants
  rgb(36, 20, 12), rgb(76, 44, 24), rgb(128, 80, 44),                                 // wood
  rgb(66, 22, 18), rgb(118, 42, 30), rgb(214, 44, 50),                                // brick, red
  rgb(240, 212, 64), rgb(40, 96, 226), rgb(22, 40, 120), rgb(140, 72, 206), rgb(250, 124, 30),
};

uint8_t (*buf)[W] = nullptr;

void begin()
{
  if (buf) return;
  buf = (uint8_t (*)[W])ps_malloc(H * W);
  if (!buf) buf = (uint8_t (*)[W])malloc(H * W);
  memset(buf, 0, H * W);
}

void clear(uint8_t c)
{
  if (!buf) begin();
  memset(buf, c, H * W);
}

void rect(int x, int y, int w, int h, uint8_t c)
{
  int x0 = max(0, x), y0 = max(0, y), x1 = min(W, x + w), y1 = min(H, y + h);
  for (int yy = y0; yy < y1; yy++) memset(&buf[yy][x0], c, max(0, x1 - x0));
}

void disc(int cx, int cy, int r, uint8_t c)
{
  for (int y = -r; y <= r; y++) {
    int hw = (int)sqrtf((float)(r * r - y * y) + r * 0.6f);
    rect(cx - hw, cy + y, 2 * hw + 1, 1, c);
  }
}

void gradient(int y0, int y1, const uint8_t *cols, int n)
{
  static const uint8_t BAYER[2][2] = { { 0, 2 }, { 3, 1 } };
  float span = (float)(y1 - y0) / n;
  for (int y = y0; y < y1; y++) {
    float f = (y - y0) / span - 0.5f;
    int i = constrain((int)floorf(f), 0, n - 1);
    float frac = f - i;
    for (int x = 0; x < W; x++) {
      int k = (i < n - 1 && frac * 4 > BAYER[y & 1][x & 1] + 0.5f) ? i + 1 : i;
      buf[y][x] = cols[f < 0 ? 0 : k];
    }
  }
}

void sprite(const char *const *rows, int n, int x, int y, const char *key, const uint8_t *vals, bool flip)
{
  int nk = strlen(key);
  for (int j = 0; j < n; j++) {
    int len = strlen(rows[j]);
    for (int i = 0; i < len; i++) {
      const char *p = (const char *)memchr(key, rows[j][i], nk);
      if (!p) continue;
      set(x + (flip ? len - 1 - i : i), y + j, vals[p - key]);
    }
  }
}

// --- themes ---

static Theme cur = Theme::Natural;
static uint16_t pair_a[COLORS], pair_b[COLORS];  // even / odd checker cell of each palette color
static uint16_t pair_flat[COLORS];               // the lighter of the two, for flat blocks (2600)
static bool pairs_ready = false;

struct Rgb {
  float r, g, b;
};
static Rgb unpack(uint16_t c) { return { (float)((c >> 11) << 3), (float)(((c >> 5) & 63) << 2), (float)((c & 31) << 3) }; }
static float dist2(const Rgb &a, const Rgb &b)
{
  float dr = a.r - b.r, dg = a.g - b.g, db = a.b - b.b;
  return dr * dr * 0.3f + dg * dg * 0.59f + db * db * 0.11f;  // weighted by how much the eye cares
}

// Best single color or 50/50 checker pair from `set` for `want`; mixing two far-apart colors is penalized (noise).
static void bestPair(const Rgb &want, const Rgb *set, const uint16_t *set565, int n, uint16_t &a, uint16_t &b)
{
  float best = 1e12f;
  for (int i = 0; i < n; i++) {
    for (int j = i; j < n; j++) {
      Rgb mix = { (set[i].r + set[j].r) / 2, (set[i].g + set[j].g) / 2, (set[i].b + set[j].b) / 2 };
      float d = dist2(want, mix) + dist2(set[i], set[j]) * 0.08f;
      if (d < best) {
        best = d;
        a = set565[i];
        b = set565[j];
      }
    }
  }
}

static void buildPairs()
{
  if (cur == Theme::Natural) {
    for (int i = 0; i < COLORS; i++) pair_a[i] = pair_b[i] = PALETTE[i];
    return;
  }
  if (cur == Theme::Ega) {
    static const uint16_t EGA[16] = { rgb(0, 0, 0),       rgb(0, 0, 170),     rgb(0, 170, 0),    rgb(0, 170, 170),
                                      rgb(170, 0, 0),     rgb(170, 0, 170),   rgb(170, 85, 0),   rgb(170, 170, 170),
                                      rgb(85, 85, 85),    rgb(85, 85, 255),   rgb(85, 255, 85),  rgb(85, 255, 255),
                                      rgb(255, 85, 85),   rgb(255, 85, 255),  rgb(255, 255, 85), rgb(255, 255, 255) };
    Rgb set[16];
    for (int i = 0; i < 16; i++) set[i] = unpack(EGA[i]);
    for (int i = 0; i < COLORS; i++) {
      // EGA art is saturated: push each color away from its grey before matching, so dark water still reads blue
      Rgb c = unpack(PALETTE[i]);
      float g = c.r * 0.3f + c.g * 0.59f + c.b * 0.11f;
      auto boost = [&](float v) { return constrain((g + (v - g) * 2.0f) * 1.2f, 0.0f, 255.0f); };
      Rgb want = { boost(c.r), boost(c.g), boost(c.b) };
      bestPair(want, set, EGA, 16, pair_a[i], pair_b[i]);
    }
    return;
  }
  if (cur == Theme::C64) {
    // the measured colors of the 1980s home computer (the "Pepto" palette)
    static const uint16_t C64[16] = { rgb(0, 0, 0),       rgb(255, 255, 255), rgb(104, 55, 43),   rgb(112, 164, 178),
                                      rgb(111, 61, 134),  rgb(88, 141, 67),   rgb(53, 40, 121),   rgb(184, 199, 111),
                                      rgb(111, 79, 37),   rgb(67, 57, 0),     rgb(154, 103, 89),  rgb(68, 68, 68),
                                      rgb(108, 108, 108), rgb(154, 210, 132), rgb(108, 94, 181),  rgb(149, 149, 149) };
    Rgb set[16];
    for (int i = 0; i < 16; i++) set[i] = unpack(C64[i]);
    for (int i = 0; i < COLORS; i++) bestPair(unpack(PALETTE[i]), set, C64, 16, pair_a[i], pair_b[i]);
    return;
  }
  if (cur == Theme::Cga) {
    // palette 1, high intensity
    static const uint16_t CGA[4] = { rgb(0, 0, 0), rgb(85, 255, 255), rgb(255, 85, 255), rgb(255, 255, 255) };
    Rgb set[4];
    for (int i = 0; i < 4; i++) set[i] = unpack(CGA[i]);
    for (int i = 0; i < COLORS; i++) {
      Rgb c = unpack(PALETTE[i]);
      float g = c.r * 0.3f + c.g * 0.59f + c.b * 0.11f;
      auto boost = [&](float v) { return constrain((g + (v - g) * 1.6f) * 1.3f, 0.0f, 255.0f); };
      bestPair({ boost(c.r), boost(c.g), boost(c.b) }, set, CGA, 4, pair_a[i], pair_b[i]);
    }
    return;
  }
  if (cur == Theme::Amiga) {
    for (int i = 0; i < COLORS; i++) {
      Rgb c = unpack(PALETTE[i]);
      auto q = [](float v) { return (int)((int)(v + 8) / 17) * 17; };
      pair_a[i] = pair_b[i] = rgb(min(255, q(c.r)), min(255, q(c.g)), min(255, q(c.b)));
    }
    return;
  }
  // Vfd: two ramps, the colors of the clock and radio pages
  static const uint16_t CY[] = { rgb(0, 8, 8),     rgb(6, 28, 34),   rgb(10, 60, 70),
                                 rgb(30, 120, 130), rgb(70, 190, 190), rgb(118, 244, 226), rgb(210, 255, 248) };
  static const uint16_t MG[] = { rgb(30, 4, 22), rgb(70, 10, 48), rgb(130, 18, 86), rgb(200, 40, 140), rgb(250, 66, 178),
                                 rgb(255, 170, 222) };
  uint16_t all565[13];
  Rgb set[13];
  for (int i = 0; i < 7; i++) all565[i] = CY[i];
  for (int i = 0; i < 6; i++) all565[7 + i] = MG[i];
  for (int i = 0; i < 13; i++) set[i] = unpack(all565[i]);
  for (int i = 0; i < COLORS; i++) {
    Rgb c = unpack(PALETTE[i]);
    float lum = (c.r * 0.3f + c.g * 0.59f + c.b * 0.11f) / 255.0f;
    float warm = (c.r - c.b) / 255.0f + (c.r - c.g) / 510.0f;
    const uint16_t *ramp = warm > 0.12f ? MG : CY;
    int n = warm > 0.12f ? 6 : 7;
    float f = constrain(powf(lum, 0.8f) * 1.15f, 0.0f, 1.0f) * (n - 1);
    int k = (int)f;
    float fr = f - k;
    Rgb lo = unpack(ramp[k]), hi = unpack(ramp[min(k + 1, n - 1)]);
    Rgb want = { lo.r + (hi.r - lo.r) * fr, lo.g + (hi.g - lo.g) * fr, lo.b + (hi.b - lo.b) * fr };
    bestPair(want, set, all565, 13, pair_a[i], pair_b[i]);
  }
}

void setTheme(Theme t)
{
  if (t >= Theme::COUNT) t = Theme::Natural;
  cur = t;
  buildPairs();
  for (int i = 0; i < COLORS; i++) {  // the lighter of the two, for flat blocks
    int la = ((pair_a[i] >> 11) * 3 + ((pair_a[i] >> 5) & 63) * 3 + (pair_a[i] & 31) * 1);
    int lb = ((pair_b[i] >> 11) * 3 + ((pair_b[i] >> 5) & 63) * 3 + (pair_b[i] & 31) * 1);
    pair_flat[i] = lb > la ? pair_b[i] : pair_a[i];
  }
  pairs_ready = true;
}
Theme theme() { return cur; }
const char *themeName(Theme t)
{
  static const char *N[] = { "natural", "ega", "vfd", "c64", "cga", "amiga" };
  return N[(int)t < (int)Theme::COUNT ? (int)t : 0];
}
uint16_t ink(uint8_t c)
{
  if (!pairs_ready) setTheme(cur);
  return pair_a[c < COLORS ? c : 0];
}

void present()
{
  if (!pairs_ready) setTheme(cur);
  if (!buf) begin();
  uint16_t *cv = vfd::canvas();
  int wide = cur == Theme::C64 ? ~1 : ~0;  // C64 multicolor: every pixel twice as wide
  for (int y = 0; y < H; y++) {
    uint16_t *r0 = cv + (2 * y) * vfd::W, *r1 = r0 + vfd::W;
    const uint8_t *src = buf[y];
    for (int x = 0; x < W; x++) {
      uint8_t i = src[x & wide];
      uint16_t c = (((x & wide) ^ y) & 1) ? pair_b[i] : pair_a[i];
      r0[2 * x] = r0[2 * x + 1] = r1[2 * x] = r1[2 * x + 1] = c;
    }
  }
}

void overlay(int bw, int bh, uint16_t solid)
{
  if (!pairs_ready) setTheme(cur);
  if (!buf) begin();
  uint16_t *cv = vfd::canvas();
  int mx = ~(max(1, bw) - 1), my = ~(max(1, bh) - 1);
  bool flat = bw > 2;  // a 2600 block is one flat color: no checker between two
  for (int y = 0; y < H; y++) {
    uint16_t *r0 = cv + (2 * y) * vfd::W, *r1 = r0 + vfd::W;
    const uint8_t *src = buf[y & my];
    for (int x = 0; x < W; x++) {
      uint8_t i = src[x & mx];
      if (i == CLR) continue;
      uint16_t c = solid ? solid : flat ? pair_flat[i] : ((((x & mx) ^ y) & 1) ? pair_b[i] : pair_a[i]);
      r0[2 * x] = r0[2 * x + 1] = r1[2 * x] = r1[2 * x + 1] = c;
    }
  }
}

}  // namespace px

#endif
