#ifdef BOARD_TOUCH349

#include "GameRunner.h"
#include <Preferences.h>
#include <esp_random.h>
#include "../../../core/state.h"
#include "Radio.h"
#include "Settings.h"
#include "Theme.h"
#include "Vfd.h"

using namespace vfd;

// Endless runner. A small courier runs right over a scrolling ground; tap = short hop, hold = high jump (the jump starts
// the moment the finger lands and is cut short when it lifts: the press length sets the height). Score = distance.
//
// World units: everything (speed, jump, obstacle sizes) is in sprite cells, drawn sc pixels per cell (4, the C64 theme 5),
// so the game plays the same in every theme.
//   jump: v0 = 82 cells/s, gravity 220 -> 15.3 cells high, 0.75 s in the air. Lifting the finger cuts the climb to
//         46 cells/s, but not before 60 ms: shortest hop ~9.3 cells (air time 0.56 s), a press of ~0.1 s or more gets
//         over the tall crate.
//   obstacles: puddle 1.5 high, crate 6, tall crate 13, low bird 1.5..7.5 (hit box 2..6.5, 5 narrower); each needs the
//         box of the runner (feet at y + 1) above its top while it passes.
//   spacing rule: the edge-to-edge gap between two obstacles is at least 0.80 s of running at the current speed
//         (x1.05 for the speed gained while it scrolls in), 0.95 s next to a tall crate, plus a random 0..0.9 s.
//         0.80 s is landing from the shortest hop (0.31 s after clearing) plus reaction and the next take-off.
//   speed: 60 cells/s, +0.35 per second of running, at most 108.

namespace {

constexpr float G = 220.0f, V0 = 82.0f, V_CUT = 46.0f;
constexpr uint32_t MIN_HOLD_MS = 60;
constexpr float SPEED0 = 60.0f, SPEED_GAIN = 0.35f, SPEED_MAX = 108.0f;
constexpr float GAP_MIN_S = 0.80f, GAP_TALL_S = 0.95f, GAP_RAND_S = 0.9f, GAP_SPEEDUP = 1.05f;
constexpr int GROUND_Y = 146;     // feet, in pixels
constexpr int RUNNER_SX = 44;     // the runner's left edge, in pixels
constexpr int MAX_OBS = 8;

enum class Rs : uint8_t { Ready, Run, Dead };
enum Type : uint8_t { PUDDLE, CRATE, TALL, BIRD, TYPES };

// Sprites: '#' main color, 'o' detail color, '.' empty.
const char *const RUN_A[] = { "...####..", "..######.", "..#####o.", "..######.", "...####..", "oo.####..", ".o#####.#", "..#####.#", "..#####..", "..##.##..", ".##...##.", "##.....##" };
const char *const RUN_B[] = { "...####..", "..######.", "..#####o.", "..######.", "...####..", "oo.####..", ".o#####.#", "..#####.#", "..#####..", "...###...", "...#.#...", "..##.##.." };
const char *const RUN_J[] = { "...####..", "..######.", "..#####o.", "..######.", "...####..", "oo.####..", ".o#####.#", "..#####.#", "..#####..", ".#######.", ".##...##.", "........." };
const char *const PUDDLE_S[] = { "...########...", ".############." };
const char *const CRATE_S[] = { "########", "#oo..oo#", "#.oooo.#", "#.oooo.#", "#oo..oo#", "########" };
const char *const TALL_S[] = { "########", "#oo..oo#", "#.oooo.#", "#.oooo.#", "#oo..oo#", "########", "########", "#oo..oo#", "#.oooo.#", "#.oooo.#", "#.oooo.#", "#oo..oo#", "########" };
const char *const BIRD_A[] = { "o.........o", "oo.......oo", ".ooo###ooo.", "oo.#o####..", "...#####...", "....#.#...." };
const char *const BIRD_B[] = { "...........", "...#####...", "ooo#o####oo", ".ooo####oo.", "...#####...", "....#.#...." };

struct Dim {
  const char *const *rows;
  int n, w;
  float bottom, hit_b, hit_t;  // sprite bottom above the ground, hit box bottom and top (cells above the ground)
  float inset;                 // the hit box is this much narrower than the sprite on each side
};
const Dim DIMS[TYPES] = {
  { PUDDLE_S, 2, 14, 0, 0, 1.2f, 0.5f },
  { CRATE_S, 6, 8, 0, 0, 5.5f, 0.5f },
  { TALL_S, 13, 8, 0, 0, 12.5f, 0.5f },
  { BIRD_A, 6, 11, 1.5f, 2.0f, 6.5f, 2.5f },
};

struct Ob {
  float x;  // world, left edge
  uint8_t type;
  bool alive;
};

Preferences prefs;
bool prefs_open = false;
Rs rs = Rs::Ready;
Ob obs[MAX_OBS];
float dist = 0, speed = SPEED0, run_s = 0, y = 0, vy = 0, next_x = 0;
bool on_ground = true, held = false, cut_pending = false, buffered = false, show_fps = false;
uint8_t pending_type = CRATE;
uint32_t jump_ms = 0, cut_at = 0, buffer_until = 0, last_ms = 0, dead_ms = 0, score_hi = 0, score = 0, last_milestone = 0;
bool new_hi = false;
float ema_gap = 33, ema_draw = 10;
uint32_t last_draw_ms = 0;

int sc() { return theme::current() == theme::C64 ? 5 : 4; }
float viewCells() { return (float)W / sc(); }

void openPrefs()
{
  if (prefs_open) return;
  prefs.begin("games", false);
  prefs_open = true;
}

float rnd() { return (esp_random() & 0xFFFF) / 65536.0f; }

uint8_t pickType()
{
  float r = rnd();
  if (run_s < 6) return r < 0.3f ? PUDDLE : CRATE;
  if (run_s < 14) return r < 0.25f ? PUDDLE : r < 0.65f ? CRATE : BIRD;
  return r < 0.2f ? PUDDLE : r < 0.5f ? CRATE : r < 0.75f ? BIRD : TALL;
}

void spawn()
{
  for (Ob &o : obs) {
    if (o.alive) continue;
    o = { next_x, pending_type, true };
    uint8_t t = pending_type;
    pending_type = pickType();
    bool tall = t == TALL || pending_type == TALL;
    float gap = speed * GAP_SPEEDUP * ((tall ? GAP_TALL_S : GAP_MIN_S) + GAP_RAND_S * rnd());
    next_x += DIMS[t].w + gap;
    return;
  }
}

void reset()
{
  dist = 0, speed = SPEED0, run_s = 0, y = 0, vy = 0;
  on_ground = true, held = false, cut_pending = false, buffered = false;
  for (Ob &o : obs) o.alive = false;
  pending_type = CRATE;
  next_x = 90;
  score = last_milestone = 0;
  new_hi = false;
}

void startJump()
{
  vy = V0;
  on_ground = false;
  jump_ms = millis();
  cut_pending = false;
  buffered = false;
  Radio_Beep(740, 18);
}

void startRun()
{
  reset();
  rs = Rs::Run;
  last_ms = millis();
  state.dirty = true;
}

void die()
{
  rs = Rs::Dead;
  dead_ms = millis();
  openPrefs();
  if (score > score_hi) {
    score_hi = score;
    new_hi = true;
    prefs.putUInt("rn_hi", score_hi);
  }
  Radio_Beep(150, 220);
  state.dirty = true;
}

bool hits()
{
  float rx0 = dist + 1.5f, rx1 = dist + 7.5f, rb = y + 1.0f, rt = y + 11.0f;
  for (const Ob &o : obs) {
    if (!o.alive) continue;
    const Dim &d = DIMS[o.type];
    float ox0 = o.x + d.inset, ox1 = o.x + d.w - d.inset;
    if (rx1 > ox0 && rx0 < ox1 && rb < d.hit_t && rt > d.hit_b) return true;
  }
  return false;
}

void step(float dt)
{
  run_s += dt;
  speed = min(SPEED_MAX, SPEED0 + SPEED_GAIN * run_s);
  dist += speed * dt;
  uint32_t now = millis();
  if (cut_pending && !held && (int32_t)(now - cut_at) >= 0) {
    if (vy > V_CUT) vy = V_CUT;
    cut_pending = false;
  }
  if (!on_ground) {
    vy -= G * dt;
    y += vy * dt;
    if (y <= 0) {
      y = 0, vy = 0, on_ground = true, cut_pending = false;
      if (buffered && (int32_t)(now - buffer_until) < 0) {
        startJump();
        if (!held) cut_pending = true, cut_at = now + MIN_HOLD_MS;
      }
      buffered = false;
    }
  }
  for (Ob &o : obs) {
    if (o.alive && o.x + DIMS[o.type].w < dist - 4) o.alive = false;
  }
  while (next_x < dist + viewCells() + 12) spawn();
  score = (uint32_t)(dist / 5.0f);
  if (score / 100 > last_milestone) {
    last_milestone = score / 100;
    Radio_Beep(1318, 22);
  }
  if (hits()) die();
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------------------------------

struct Look {
  uint16_t body, accent, band;  // band: color of every second row (2600), 0 = none
  bool outline;                 // VFD: only the rim is drawn, glowing
  uint16_t halo;
};

// Draws a sprite at pixel (x, y) with its feet/bottom at the given row.
void drawSprite(const char *const *rows, int n, int x, int ybottom, int s, const Look &lk, bool flip = false)
{
  int top = ybottom - n * s;
  for (int pass = lk.outline && style().glow ? 0 : 1; pass < 2; pass++) {
    for (int j = 0; j < n; j++) {
      int len = strlen(rows[j]);
      for (int i = 0; i < len; i++) {
        char c = rows[j][i];
        if (c == '.') continue;
        int ii = flip ? len - 1 - i : i;
        if (lk.outline && c == '#') {  // rim only: a body pixel with an empty neighbour
          bool edge = i == 0 || i == len - 1 || j == 0 || j == n - 1 || rows[j][i - 1] == '.' || rows[j][i + 1] == '.' ||
                      (int)strlen(rows[j - 1]) <= i || rows[j - 1][i] == '.' || (int)strlen(rows[j + 1]) <= i || rows[j + 1][i] == '.';
          if (!edge) continue;
        }
        if (pass == 0) {
          rect(x + ii * s - 1, top + j * s - 1, s + 2, s + 2, lk.halo);
        } else {
          uint16_t col = c == 'o' ? lk.accent : (lk.band && (j & 1)) ? lk.band : lk.body;
          rect(x + ii * s, top + j * s, s, s, col);
        }
      }
    }
  }
}

uint16_t mix(uint16_t a, uint16_t b, int t)  // t 0..255 of b
{
  int ar = (a >> 11) & 31, ag = (a >> 5) & 63, ab = a & 31, br = (b >> 11) & 31, bg = (b >> 5) & 63, bb = b & 31;
  int r = ar + (br - ar) * t / 255, g = ag + (bg - ag) * t / 255, bl = ab + (bb - ab) * t / 255;
  return (r << 11) | (g << 5) | bl;
}

uint16_t rgb12(int r, int g, int b)  // 4 bits per channel, like the Amiga chipset
{
  return px::rgb((r >> 4) * 17, (g >> 4) * 17, (b >> 4) * 17);
}

void drawBackground(float d, int s)
{
  int cell = (int)(d * s);
  switch (theme::current()) {
    case theme::C64: {
      clear();
      for (int k = 0; k < 3; k++) {  // chunky clouds
        int cx = ((k * 230 + 60 - cell / 6) % 760 + 760) % 760 - 100;
        rect(cx, 30 + k * 16, 64, 12, 0xFFFF);
        rect(cx + 12, 22 + k * 16, 40, 8, 0xFFFF);
      }
      rect(0, GROUND_Y, W, 8, px::rgb(88, 141, 67));
      rect(0, GROUND_Y + 8, W, H - GROUND_Y - 8, px::rgb(111, 79, 37));
      for (int k = 0; k < 14; k++) {
        int x = ((k * 61 - cell) % 700 + 700) % 700 - 30;
        rect(x, GROUND_Y + 14 + (k % 3) * 6, 8, 4, px::rgb(67, 57, 0));
      }
      break;
    }
    case theme::AMIGA: {  // copper bars: the sky is a gradient written scanline by scanline, 12-bit colors
      for (int yy = 0; yy < GROUND_Y; yy += 4) {
        int t = yy * 255 / GROUND_Y;
        uint16_t c = t < 128 ? rgb12(0, 20 + t / 3, 90 + t) : rgb12((t - 128) * 2, 50 + (t - 128), 200 - (t - 128) * 2);
        if (t > 190) c = rgb12(255, 110 + (t - 190), 40);
        rect(0, yy, W, 4, c);
      }
      Apps_Disc(470, GROUND_Y - 6, 30, rgb12(255, 221, 0));
      for (int k = 0; k < 4; k++) rect(430, GROUND_Y - 22 + k * 6, 80, 2 + k, rgb12(255, 85, 34));  // the sun is striped
      rect(0, GROUND_Y, W, H - GROUND_Y, rgb12(51, 17, 0));
      rect(0, GROUND_Y, W, 3, rgb12(255, 136, 0));
      for (int k = 0; k < 10; k++) {
        int x = ((k * 77 - cell) % 700 + 700) % 700 - 20;
        rect(x, GROUND_Y + 10 + (k % 3) * 5, 14, 2, rgb12(136, 51, 0));
      }
      break;
    }
    case theme::ATARI: {  // flat sky bands and a striped ground, nothing finer than a 2600 could do
      clear(px::rgb(0, 0, 0));
      for (int k = 0; k < 6; k++) rect(0, 24 + k * 14, W, 5, k < 3 ? px::rgb(24, 26, 80) : px::rgb(80, 26, 24));
      rect(300, 30, 54, 54, px::rgb(252, 188, 116));
      for (int k = 0; k < 5; k++) rect(300, 42 + k * 10, 54, 3, px::rgb(0, 0, 0));
      for (int yy = GROUND_Y; yy < H; yy += 6) rect(0, yy, W, 6, ((yy - GROUND_Y) / 6) & 1 ? px::rgb(60, 130, 60) : px::rgb(92, 186, 92));
      for (int k = 0; k < 9; k++) {
        int x = ((k * 83 - cell) % 700 + 700) % 700 - 20;
        rect(x, GROUND_Y + 6, 24, 6, px::rgb(40, 90, 40));
      }
      break;
    }
    default: {  // VFD: dark glass, a glowing horizon, dim ticks that slide by
      clear();
      rect(0, GROUND_Y - 1, W, 4, HALO);
      rect(0, GROUND_Y, W, 1, LIT);
      for (int k = 0; k < 12; k++) {
        int x = ((k * 58 - cell) % 700 + 700) % 700 - 30;
        rect(x, GROUND_Y + 7 + (k % 3) * 6, 18, 2, GHOST);
      }
      for (int k = 0; k < 6; k++) rect(((k * 131 - cell / 8) % 760 + 760) % 760 - 60, 24 + (k * 37) % 70, 3, 3, GHOST);  // far stars
      break;
    }
  }
}

Look runnerLook()
{
  using px::rgb;
  switch (theme::current()) {
    case theme::C64: return { 0xFFFF, rgb(184, 105, 98), 0, false, 0 };
    case theme::AMIGA: return { 0xFFFF, rgb12(255, 136, 0), 0, false, 0 };
    case theme::ATARI: return { rgb(252, 188, 116), rgb(236, 236, 236), rgb(208, 128, 92), false, 0 };
    default: return { LIT, HOT, 0, true, HALO };
  }
}

Look obstacleLook(int type)
{
  using px::rgb;
  switch (theme::current()) {
    case theme::C64:
      if (type == BIRD) return { rgb(184, 105, 98), 0xFFFF, 0, false, 0 };
      if (type == PUDDLE) return { rgb(112, 164, 178), 0xFFFF, 0, false, 0 };
      return { rgb(184, 199, 111), rgb(111, 79, 37), 0, false, 0 };
    case theme::AMIGA:
      if (type == BIRD) return { rgb12(51, 0, 68), rgb12(255, 255, 255), 0, false, 0 };
      if (type == PUDDLE) return { rgb12(0, 170, 255), rgb12(255, 255, 255), 0, false, 0 };
      return { rgb12(255, 136, 0), rgb12(51, 17, 0), 0, false, 0 };
    case theme::ATARI:
      if (type == BIRD) return { rgb(160, 160, 200), rgb(236, 236, 236), rgb(120, 120, 170), false, 0 };
      if (type == PUDDLE) return { rgb(68, 120, 200), rgb(160, 200, 252), 0, false, 0 };
      return { rgb(200, 72, 48), rgb(252, 188, 116), rgb(160, 52, 36), false, 0 };
    default:
      if (type == PUDDLE) return { LIT, DIM, 0, true, HALO };
      return { HOT, DIM, 0, true, HOTH };
  }
}

void drawObstacles(float d, int s)
{
  bool wing = (millis() / 150) & 1;
  for (const Ob &o : obs) {
    if (!o.alive) continue;
    const Dim &dm = DIMS[o.type];
    int x = RUNNER_SX + (int)((o.x - d) * s);
    if (x > W || x + dm.w * s < 0) continue;
    const char *const *rows = o.type == BIRD && wing ? BIRD_B : dm.rows;
    drawSprite(rows, dm.n, x, GROUND_Y - (int)(dm.bottom * s), s, obstacleLook(o.type));
  }
}

void drawRunner(int s)
{
  const char *const *rows = !on_ground ? RUN_J : (((int)(dist / 6.0f)) & 1) ? RUN_B : RUN_A;
  if (rs == Rs::Ready) rows = RUN_B;
  int yb = GROUND_Y - (int)(y * s);
  Look lk = runnerLook();
  if (rs == Rs::Dead) {  // the runner flashes the accent color
    lk.body = lk.accent;
    lk.band = 0;
  }
  drawSprite(rows, 12, RUNNER_SX, yb, s, lk);
}

void drawHud()
{
  char b[24];
  Ink score_ink = LIT_INK, dim = DIM_INK;
  if (theme::current() != theme::VFD) {
    score_ink = Ink{ 0xFFFF };
    dim = theme::current() == theme::ATARI ? Ink{ px::rgb(236, 236, 236) } : Ink{ 0xFFFF };
  }
  snprintf(b, sizeof(b), "%05lu", (unsigned long)score);
  text(b, W - 22 - textWidth(b, 4), 14, 4, score_ink);
  snprintf(b, sizeof(b), "%s %05lu", L("HI", "REKOR"), (unsigned long)score_hi);
  text(b, W - 22 - textWidth(b, 2), 46, 2, dim);
  if (show_fps) {
    snprintf(b, sizeof(b), "%dMS %dMS", (int)ema_gap, (int)ema_draw);
    text(b, 20, 14, 2, dim);
  }
}

// A dark box behind the messages, so they read over the copper sky and the clouds.
void panel(int y0, int y1)
{
  uint16_t fill = BG, edge = DIM;
  switch (theme::current()) {
    case theme::C64: edge = HALO; break;
    case theme::AMIGA: fill = px::rgb(0, 0, 34), edge = 0xFFFF; break;
    case theme::ATARI: fill = 0, edge = px::rgb(160, 160, 200); break;
    default: break;
  }
  rect(100, y0, 440, y1 - y0, fill);
  frame(100, y0, 440, y1 - y0, edge);
  frame(101, y0 + 1, 438, y1 - y0 - 2, edge);
}

void centered(const char *s, int p, int yy, const Ink &ink) { text(s, (W - textWidth(s, p)) / 2, yy, p, ink); }

void runnerDraw(const DateTime &)
{
  uint32_t t0 = millis();
  int s = sc();
  drawBackground(dist, s);
  drawObstacles(dist, s);
  drawRunner(s);
  drawHud();
  Ink hot = HOT_INK, lit = LIT_INK;
  if (theme::current() != theme::VFD) {
    hot = Ink{ (uint16_t)(theme::current() == theme::ATARI ? px::rgb(252, 188, 116) : theme::current() == theme::AMIGA ? rgb12(255, 136, 0) : px::rgb(184, 199, 111)) };
    lit = Ink{ 0xFFFF };
  }
  if (rs == Rs::Ready) {
    panel(36, 126);
    centered(L("TAP TO RUN", "DOKUN: KOŞ"), 5, 52, lit);
    centered(L("TAP: HOP   HOLD: HIGH JUMP", "DOKUN: ZIPLA  BASILI: YÜKSEK"), 2, 100, hot);
  } else if (rs == Rs::Dead) {
    panel(24, 136);
    centered(L("GAME OVER", "OYUN BİTTİ"), 5, 36, hot);
    char b[32];
    snprintf(b, sizeof(b), "%s %lu", new_hi ? L("NEW BEST", "YENİ REKOR") : L("SCORE", "SKOR"), (unsigned long)score);
    centered(b, 3, 82, lit);
    if (millis() - dead_ms > 600) centered(L("TAP TO RETRY", "DOKUN: TEKRAR"), 2, 112, hot);
  }
  if (theme::current() == theme::C64) {
    const theme::Def &d = theme::def();
    rect(0, 0, W, d.border_w, d.border);
    rect(0, H - d.border_w, W, d.border_w, d.border);
    rect(0, 0, d.border_w, H, d.border);
    rect(W - d.border_w, 0, d.border_w, H, d.border);
  }
  uint32_t now = millis();
  ema_draw += ((float)(now - t0) - ema_draw) * 0.1f;
  if (last_draw_ms) ema_gap += ((float)(now - last_draw_ms) - ema_gap) * 0.1f;
  last_draw_ms = now;
}

// ---------------------------------------------------------------------------------------------------------------------
// Input and loop
// ---------------------------------------------------------------------------------------------------------------------

void runnerEnter()
{
  openPrefs();
  score_hi = prefs.getUInt("rn_hi", 0);
  reset();
  rs = Rs::Ready;
  last_draw_ms = 0;
}

void runnerTouch(bool down, int, int)
{
  uint32_t now = millis();
  if (down) {
    held = true;
    if (rs == Rs::Ready || (rs == Rs::Dead && now - dead_ms > 600)) {
      startRun();
    } else if (rs == Rs::Run) {
      if (on_ground) {
        startJump();
      } else {  // a touch just before landing jumps on landing
        buffered = true;
        buffer_until = now + 120;
      }
    }
  } else {
    held = false;
    if (rs == Rs::Run && !on_ground && vy > V_CUT) {
      cut_pending = true;
      cut_at = max(now, jump_ms + MIN_HOLD_MS);
    }
  }
}

bool runnerGesture(Gesture g, int, int)
{
  if (g == Gesture::LongPress) {
    if (rs != Rs::Run) show_fps = !show_fps;  // frame times, for testing
    return true;
  }
  // taps and up/down swipes (a sloppy tap) were already used as the touch edge; they must not reach the volume control
  return g == Gesture::Tap || g == Gesture::SwipeUp || g == Gesture::SwipeDown;
}

void runnerTick()
{
  uint32_t now = millis();
  if (rs == Rs::Run) {
    float dt = (now - last_ms) / 1000.0f;
    last_ms = now;
    if (dt > 0.1f) dt = 0.1f;
    while (dt > 0 && rs == Rs::Run) {  // small steps keep the jump arc and the collision test honest
      float h = min(dt, 0.008f);
      step(h);
      dt -= h;
    }
  }
}

uint32_t runnerFrameMs() { return rs == Rs::Run ? 1 : 80; }

void iconRunner(int x, int yy, uint16_t c)
{
  for (int j = 0; j < 12; j++) {
    for (int i = 0; i < 9; i++) {
      if (RUN_A[j][i] != '.') rect(x + 6 + i * 4, yy + j * 4, 4, 4, c);
    }
  }
}

}  // namespace

const App APP_RUNNER = { "runner", "RUNNER", "KOŞU", iconRunner, true, true, runnerEnter, nullptr, runnerDraw, runnerGesture, runnerTick, runnerFrameMs, nullptr, nullptr, runnerTouch };

#endif
