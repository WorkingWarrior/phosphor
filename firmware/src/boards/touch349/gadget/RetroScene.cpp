#ifdef BOARD_TOUCH349

#include "RetroScene.h"
#include <esp_heap_caps.h>
#include "Motion.h"
#include "Px.h"
#include "Theme.h"
#include "Vfd.h"
#include "../../../core/Assets.h"

namespace art {
struct Image {
  uint16_t w, h;   // in machine pixels
  uint8_t sx, sy;  // screen pixels per machine pixel
  uint8_t ncolors;
  const uint16_t *pal;
  const uint8_t *rle;  // (count, color index) pairs, row by row
  uint32_t rle_len;
};
}  // namespace art

using px::rgb;

namespace {

// One picture of one scene for one theme, with the places and colors its effects need. The numbers in p and c mean
// something per scene:
//   Snow  p: window x0 y0 x1 y1, window brightness threshold, paint the window always (0/1), chimney x y,
//            flakes, % of big flakes, move every n-th frame   c: fire, fire flicker, young smoke, old smoke,
//            near snow, far snow
//   Fire  p: firebox x0 y0 x1 y1, candle x y, glow x0 y0 x1 y1   c: flame core, flame body, flame tip, glow
//   Sea   p: lamp x y, sea y0 y1, foam y0 y1   c: beam, glint
//   Aquarium p: surface y0 y1   c: light shafts
// Coordinates are screen pixels (640 x 172).
struct Entry {
  RetroArt scene;
  theme::Id id;
  const char *art;  // name in the asset pack (assets/art/<name>.img, written by design/retro-proto/build_art.sh)
  int16_t p[12];
  uint16_t c[6];
};

const Entry ENTRIES[] = {
#include "RetroArtTable.h"
};
constexpr int ENTRY_COUNT = sizeof(ENTRIES) / sizeof(ENTRIES[0]);
art::Image images[ENTRY_COUNT];  // filled from the pack on first use; w == 0 = not loaded
inline const art::Image *imgOf(const Entry &e) { return &images[&e - ENTRIES]; }

uint16_t *bg = nullptr;  // the decoded picture, 640 x 172 RGB565 (PSRAM)
const art::Image *decoded = nullptr;
const Entry *cur = nullptr;
uint32_t frame_no = 0;
float burst = 0;  // stir energy, decays
bool actors = false;

struct Flake {
  float x, y, v;  // machine px, machine px per second
  float floor;    // where it lands and melts into the snowfield
  bool near, big;
};
Flake fl[110];

struct Flame {
  float x, y, v, life;  // screen px; life 1 -> 0
};
Flame flames[60];

float frand() { return (esp_random() & 0xffff) / 65535.0f; }
uint32_t hash32(uint32_t v)
{
  v ^= v >> 16;
  v *= 0x7feb352d;
  v ^= v >> 15;
  v *= 0x846ca68b;
  return v ^ (v >> 16);
}

int lum(uint16_t c) { return (((c >> 11) << 3) * 3 + (((c >> 5) & 63) << 2) * 6 + ((c & 31) << 3)) / 10; }

uint16_t mix(uint16_t a, uint16_t b, int t)  // t of 256 toward b
{
  int r = ((a >> 11) * (256 - t) + (b >> 11) * t) >> 8;
  int g = (((a >> 5) & 63) * (256 - t) + ((b >> 5) & 63) * t) >> 8;
  int bl = ((a & 31) * (256 - t) + (b & 31) * t) >> 8;
  return (r << 11) | (g << 5) | bl;
}

inline uint16_t *cv() { return vfd::canvas(); }

void decode(const art::Image *img)
{
  if (!bg) bg = (uint16_t *)heap_caps_malloc(vfd::W * vfd::H * 2, MALLOC_CAP_SPIRAM);
  if (!bg) return;
  uint32_t p = 0, n = (uint32_t)img->w * img->h;
  for (uint32_t i = 0; i + 1 < img->rle_len && p < n; i += 2) {
    uint16_t c = img->pal[img->rle[i + 1]];
    for (int k = 0; k < img->rle[i] && p < n; k++, p++) {
      int x = (p % img->w) * img->sx, y = (p / img->w) * img->sy;
      for (int yy = y; yy < y + img->sy && yy < vfd::H; yy++)
        for (int xx = x; xx < x + img->sx && xx < vfd::W; xx++) bg[yy * vfd::W + xx] = c;
    }
  }
  decoded = img;
}

// n x n machine pixels at screen position x, y, snapped to the machine's grid.
void mpx(const art::Image *img, float x, float y, int n, uint16_t c)
{
  int gx = (int)(x / img->sx) * img->sx, gy = (int)(y / img->sy) * img->sy;
  vfd::rect(gx, gy, img->sx * n, img->sy * n, c);
}

// Row band [y0, y1) shifted sideways by a slow sine, a machine row at a time: water that moves.
void ripple(const art::Image *img, int y0, int y1, float amp, float speed)
{
  float t = frame_no * 0.05f;
  for (int y = y0; y < y1 && y < vfd::H; y += img->sy) {
    int d = lroundf(sinf(t * speed + y * 0.45f) * amp) * img->sx;
    if (!d) continue;
    for (int yy = y; yy < y + img->sy && yy < vfd::H; yy++) {
      uint16_t *dst = cv() + yy * vfd::W;
      const uint16_t *src = bg + yy * vfd::W;
      for (int x = 0; x < vfd::W; x++) dst[x] = src[min(vfd::W - 1, max(0, x - d))];
    }
  }
}

// ---- snow ---------------------------------------------------------------------------------------------------------

void spawnFlake(Flake &f, const Entry &e, bool top)
{
  const art::Image *img = imgOf(e);
  f.x = frand() * img->w;
  f.y = top ? -1 - frand() * 6 : frand() * img->h;
  f.near = frand() < 0.6f;
  f.big = f.near && (int)(esp_random() % 100) < e.p[9];
  f.v = (f.near ? 9 : 4.5f) * (0.7f + frand() * 0.6f);
  f.floor = img->h * (0.7f + frand() * 0.3f);
}

void snowPainted(const Entry &e)
{
  const art::Image *img = imgOf(e);
  const int16_t *p = e.p;
  float t = frame_no * 0.05f;

  // the fire behind the window
  bool flick = ((frame_no / 3) % 7 == 0) || (hash32(frame_no / 2) % 23 == 0);
  if (p[5] || flick) {
    for (int y = p[1]; y < p[3]; y++)
      for (int x = p[0]; x < p[2]; x++)
        if (lum(bg[y * vfd::W + x]) > p[4]) cv()[y * vfd::W + x] = flick ? e.c[1] : e.c[0];
  }
  // chimney smoke, puff by puff on the machine's grid
  for (int i = 0; i < 8; i++) {
    float ph = fmodf(t * 0.11f + i / 8.0f, 1.0f);
    if (ph > 0.86f) continue;
    float x = p[6] + ph * 30 + sinf(ph * 7 + i) * 3, y = p[7] - ph * 40;
    mpx(img, x, y, ph > 0.18f && ph < 0.62f ? 2 : 1, ph < 0.45f ? e.c[2] : e.c[3]);
  }
  // snow; a tilt blows it sideways, a stir whirls it
  float gx = 0, gy = 1, gz = 0;
  if (Motion_Ok()) Motion_Gravity(gx, gy, gz);
  int step = max(1, (int)p[10]);
  bool move = frame_no % step == 0;
  float dt = 0.05f * step;
  int n = min((int)p[8], (int)(sizeof(fl) / sizeof(fl[0])));
  for (int i = 0; i < n; i++) {
    Flake &f = fl[i];
    if (move) {
      float sway = sinf(t * 0.7f + i * 1.7f) * (f.near ? 1.2f : 0.6f);
      f.x += (sway + gx * 10 + sinf(t * 3 + i) * burst * 14) * dt * img->w / 320.0f;  // C64/2600 pixels are wider
      f.y += f.v * dt * (1 + burst * 1.5f);
      if (f.x < 0) f.x += img->w;
      if (f.x >= img->w) f.x -= img->w;
      if (f.y >= f.floor) spawnFlake(f, e, true);
    }
    if (f.y < 0) continue;
    int w = f.big ? 2 : 1;
    vfd::rect((int)f.x * img->sx, (int)f.y * img->sy, img->sx * w, img->sy * w, f.near ? e.c[4] : e.c[5]);
  }
}

// ---- fire ---------------------------------------------------------------------------------------------------------

void firePainted(const Entry &e)
{
  const art::Image *img = imgOf(e);
  const int16_t *p = e.p;
  // the room breathes with the fire: the lit surfaces around the hearth warm up and cool down
  float pulse = 0.5f + 0.3f * sinf(frame_no * 0.31f) + 0.2f * sinf(frame_no * 0.83f + 1) + burst * 0.4f;
  int amt = (int)(constrain(pulse, 0.0f, 1.0f) * 70);
  for (int y = p[7]; y < p[9]; y++)  // no rect (C64, 2600): blending would make colors the machine never had
    for (int x = p[6]; x < p[8]; x++) {
      uint16_t b = bg[y * vfd::W + x];
      if (lum(b) > 70) cv()[y * vfd::W + x] = mix(b, e.c[3], amt);
    }
  // flames: tongues rise from the logs, a hot core low and thin tips high
  int x0 = p[0], y0 = p[1], x1 = p[2], y1 = p[3];
  for (auto &f : flames) {
    if (f.life <= 0) {
      f.x = x0 + (x1 - x0) * (0.15f + 0.7f * (frand() + frand()) / 2);
      f.y = y1 - 2;
      f.v = (18 + frand() * 22) * (1 + burst);
      f.life = 1;
    }
    f.y -= f.v * 0.05f;
    f.x += (frand() - 0.5f) * 2.2f;
    f.life -= 0.05f * (36.0f / max(8, y1 - y0)) * (0.8f + frand() * 0.4f) / (1 + burst * 0.6f);
    if (f.life <= 0 || f.y < y0) {
      f.life = 0;
      continue;
    }
    uint16_t c = f.life > 0.66f ? e.c[0] : (f.life > 0.33f ? e.c[1] : e.c[2]);
    mpx(img, f.x, f.y, f.life > 0.5f ? 2 : 1, c);
  }
  // the candle
  if (p[4]) {
    mpx(img, p[4], p[5] - img->sy, 1, e.c[0]);
    if (hash32(frame_no / 2) % 3) mpx(img, p[4], p[5] - 2 * img->sy, 1, e.c[2]);
  }
}

// ---- sea ----------------------------------------------------------------------------------------------------------

void seaPainted(const Entry &e)
{
  const art::Image *img = imgOf(e);
  const int16_t *p = e.p;
  ripple(img, p[2], p[3], 1 + burst, 1.1f);
  ripple(img, p[4], p[5], 2 + burst * 2, 0.8f);
  // glints on the water
  for (int i = 0; i < 26; i++) {
    uint32_t h = hash32(i * 977 + frame_no / 4);
    if (h % 3) continue;
    float x = (h >> 4) % vfd::W, y = p[2] + (h >> 14) % max(1, p[3] - p[2]);
    mpx(img, x, y, 1, e.c[1]);
  }
  // the lighthouse beam sweeps around: long when it points across the bay, short when it turns toward us or away
  float a = frame_no * 0.045f, ca = cosf(a);
  int len = (int)(fabsf(ca) * 330), dir = ca > 0 ? 1 : -1;
  int strength = (int)(60 + 40 * fabsf(sinf(a * 0.5f)));
  for (int d = 2; d < len; d++) {
    int x = p[0] + dir * d;
    if (x < 0 || x >= vfd::W) break;
    int hw = 1 + d * 7 / 100, fade = strength * (len - d) / len;
    for (int y = p[1] - hw; y <= p[1] + hw; y++) {
      if ((unsigned)y >= vfd::H) continue;
      uint16_t &c = cv()[y * vfd::W + x];
      if (img->sx == 4) {  // C64 / 2600: no blending, a dither of the beam color that thins out with distance
        int mx = x / img->sx, my = y / img->sy;
        bool on = d < len / 2 ? ((mx + my) & 1) == 0 : ((mx & 1) == 0 && (my & 1) == 0);
        if (on && d < len * 9 / 10) c = e.c[0];
      } else {
        c = mix(c, e.c[0], fade);
      }
    }
  }
  mpx(img, p[0], p[1], 1, e.c[0]);  // the lamp itself
}

// ---- aquarium, under the fish -------------------------------------------------------------------------------------

void tankUnder(const Entry &e)
{
  const int16_t *p = e.p;
  ripple(imgOf(e), p[0], p[1], 1, 1.4f);
  if (!e.c[0]) return;  // the picture has its own light shafts
  float t = frame_no * 0.05f;
  for (int r = 0; r < 4; r++) {  // light shafts drifting through the water
    float x0 = 80 + r * 156 + sinf(t * 0.3f + r * 2) * 28;
    for (int y = p[1]; y < vfd::H - 20; y++) {
      int cx = lroundf(x0 + (y - p[1]) * 0.45f), w = 14 + r % 3 * 4;
      for (int x = cx; x < cx + w; x++) {
        if ((unsigned)x >= vfd::W || ((x / imgOf(e)->sx + y / imgOf(e)->sy) & 1)) continue;
        uint16_t &c = cv()[y * vfd::W + x];
        c = mix(c, e.c[0], 40);
      }
    }
  }
}


// An .img blob: u16 w, u16 h, u8 sx, u8 sy, u8 colors, u8 0, u32 rle bytes, then the RGB565 palette and the RLE.
const art::Image *image(int i)
{
  art::Image &im = images[i];
  if (im.w) return &im;
  uint32_t size = 0;
  const uint8_t *b = assets::find(ENTRIES[i].art, &size);
  if (!b || size < 12) return nullptr;
  uint32_t rle_len;
  memcpy(&im.h, b + 2, 2);
  memcpy(&rle_len, b + 8, 4);
  im.sx = b[4];
  im.sy = b[5];
  im.ncolors = b[6];
  if (12 + im.ncolors * 2 + rle_len > size || !im.sx || !im.sy) return nullptr;
  im.pal = (const uint16_t *)(b + 12);
  im.rle = b + 12 + im.ncolors * 2;
  im.rle_len = rle_len;
  memcpy(&im.w, b, 2);  // last: w != 0 marks it loaded
  return &im;
}

const Entry *find(RetroArt scene, const art::Image **img)
{
  if (!assets::ok()) return nullptr;  // no pack: the scene draws itself the old way
  for (int i = 0; i < ENTRY_COUNT; i++) {
    const Entry &e = ENTRIES[i];
    if (e.scene == scene && e.id == theme::current()) {
      *img = image(i);
      return *img ? &e : nullptr;
    }
  }
  return nullptr;
}

}  // namespace

void RetroScene_Stir(float strength) { burst = max(burst, strength * 2.5f); }

bool RetroScene_Begin(RetroArt scene)
{
  const art::Image *img = nullptr;
  const Entry *e = find(scene, &img);
  if (!e) return false;
  if (decoded != img) decode(img);
  if (!bg) return false;
  if (cur != e) {
    cur = e;
    for (auto &f : fl) spawnFlake(f, *e, false);
  }
  frame_no++;
  if (Motion_Ok()) {
    float shake = Motion_Shake();
    if (shake > 0.2f) RetroScene_Stir(shake);
  }
  burst *= 0.94f;

  vfd::noClip();
  memcpy(vfd::canvas(), bg, vfd::W * vfd::H * 2);
  if (scene == RetroArt::Aquarium) tankUnder(*e);
  px::clear(px::CLR);
  actors = true;
  return true;
}

void RetroScene_End(const char *corner_text)
{
  if (!cur) return;
  const Entry &e = *cur;
  if (actors) px::overlay(imgOf(e)->sx == 4);
  actors = false;
  switch (e.scene) {
    case RetroArt::Snow: snowPainted(e); break;
    case RetroArt::Fire: firePainted(e); break;
    case RetroArt::Sea: seaPainted(e); break;
    case RetroArt::Aquarium: break;
  }
  if (corner_text && *corner_text) {
    int w = vfd::textWidth(corner_text, 2);
    vfd::text(corner_text, vfd::W - 12 - w, 4, 2, vfd::LIT_INK);  // the picture is busy: the theme's brightest ink
  }
}

bool RetroScene_Draw(RetroArt scene, const char *corner_text)
{
  if (!RetroScene_Begin(scene)) return false;
  actors = false;  // nothing on top
  RetroScene_End(corner_text);
  return true;
}

#endif
