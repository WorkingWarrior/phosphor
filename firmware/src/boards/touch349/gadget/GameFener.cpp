#ifdef BOARD_TOUCH349

#include "GameFener.h"
#include <Preferences.h>
#include <esp_random.h>
#include "../../../core/state.h"
#include "Radio.h"
#include "Settings.h"
#include "Theme.h"
#include "Vfd.h"

using namespace vfd;

// Fener Bekçisi. A Game & Watch style LCD game: fixed segments, every one drawn every frame (unlit = ghost, lit = lit),
// ships walk one position left per tick, the keeper lights the lane a ship is about to hit. Original art and names;
// from Nintendo's games only the genre rules are used. The fairness rules and every number are in design/fener-bekcisi.md
// and were measured with design/fener-bekcisi-mc.py; step() keeps the order of that script on purpose.

// ---------------------------------------------------------------------------------------------------------------------
// Tuning: every number that changes how the game plays
// ---------------------------------------------------------------------------------------------------------------------

namespace FenerTuning {
struct Pace {
  float t0;      // tick (ms) at score 0
  float per_h;   // ms less per hundred points
  float per_w;   // ms less per point inside the hundred
  float t_min;   // fastest tick
  float p0;      // spawn chance per tick at score 0
  float per_h_p; // more chance per hundred points
  uint8_t max_lo, max_hi;  // ships on screen below / from 500 points
};
constexpr Pace MODES[2] = {
  { 650, 30, 0.9f, 320, 0.38f, 0.02f, 3, 4 },  // Game A
  { 520, 25, 0.7f, 260, 0.48f, 0.02f, 4, 5 },  // Game B
};
constexpr int POSITIONS = 5;          // P5 (spawn, right) .. P1 (danger, in front of the rocks)
constexpr int SPAWN_POS = POSITIONS + 1;  // a ship is born one tick before it shows at P5 (as in the Monte Carlo script)
constexpr int GAP_POS = POSITIONS - 1;    // a lane takes a new ship only when P4 and P5 (and the birth place) are empty
constexpr int FAST_TICK_MS = 400;     // below this tick a second ship may only follow in the lane of the previous one
constexpr int MAX_MISS = 3;
constexpr int WRAP = 1000;            // the score shows 3 digits and rolls over
constexpr int MIN_HUNDRED_AFTER_WRAP = 5;  // after 999 the speed keeps at least the 500 level
constexpr int BONUS_AT[2] = { 300, 700 };  // misses are cleared here
constexpr int BONUS_SAVES = 20;       // double points for this many saves when there was nothing to clear
constexpr int FAST_SHIPS_FROM = 500;  // the second ship limit starts here
constexpr uint32_t READY_MS = 1400;       // 2 beeps, then go
constexpr uint32_t CRASH_FREEZE_MS = 1200;
constexpr uint32_t OVER_BLINK_MS = 1200;  // all segments blink 3 times
constexpr uint32_t OVER_TOTAL_MS = 2600;  // then the title screen
constexpr uint32_t AUTO_PAUSE_MS = 30000;
constexpr uint32_t RESUME_DELAY_MS = 700; // first tick after a pause comes at least this late
constexpr int LANE_SPLIT_Y[2] = { 69, 117 };  // y below which a touch belongs to lane 0 / lane 1, else lane 2
}  // namespace FenerTuning
using namespace FenerTuning;

// ---------------------------------------------------------------------------------------------------------------------
// Sprites (each '#' is an s x s block)
// ---------------------------------------------------------------------------------------------------------------------

static const char *const SHIP_FAR[] = { "........#..", "..##..###..", "..##..###..", "###########", ".#########.", "..#######.." };
static const char *const SHIP_MID[] = { "...........#...", "..........#....", "....###..###...", "....###..###...", "..###########..",
                                        "##############.", ".############..", "..##########..." };
static const char *const SHIP_NEAR[] = { "..........#..", ".........#...", "...###.###...", "...###.###...", ".###########.", "#############",
                                         ".###########.", "..#########.." };
static const char *const TURN_FAR[] = { "......##", ".....###", "...####.", "..####..", ".####...", "###....." };
static const char *const TURN_MID[] = { "........##", "......####", ".....####.", "...####...", "..####....", ".####.....", "####......", "##........" };
static const char *const TURN_NEAR[] = { "..........##", "........####", "......#####.", "....#####...", "..#####.....", ".#####......", "#####.......", "###........." };
static const char *const WRECK[] = { "....#.........", "....#...#.....", "..#.##..##.#..", ".#####..#####.", "#######.######", ".###...#####.." };
static const char *const ROCK[] = { "......##......", "....######..#.", "..##########.#", ".#############", "##############" };
static const char *const KEEPER[] = { "..###..", ".#####.", "..###..", "..###..", ".#####.", "#.###.#", "#.###.#", "..###..", "..#.#..", "..#.#..", ".##.##." };
static const char *const LAMP[] = { "..#..", ".###.", "#####", "#####", "#####", ".###." };
static const char *const RING[] = { "..#####..", ".##...##.", "##.....##", "#.......#", "#.......#", "#.......#", "##.....##", ".##...##.", "..#####.." };
static const char *const ICON[] = { "............", "...#.##.#...", "....####....", "....#..#....", "....####....", ".....##.....",
                                    "....####....", ".....##.....", "....####....", "...######...", "...#....#...", "..########.." };

template <size_t N> static constexpr int rowsOf(const char *const (&)[N]) { return (int)N; }
#define SPR(a) a, rowsOf(a)

// ---------------------------------------------------------------------------------------------------------------------
// Layout (640 x 172)
// ---------------------------------------------------------------------------------------------------------------------

static constexpr int LANES = 3;
static constexpr int PX[POSITIONS] = { 190, 300, 410, 520, 615 };  // x centers of P1..P5
static constexpr int LANE_CY[LANES] = { 46, 93, 140 };             // window / keeper centers
static constexpr int LANE_WL[LANES] = { 56, 104, 152 };            // waterline: ships stand on it
static constexpr int SHIP_S[LANES] = { 2, 2, 3 };                  // far, mid, near
static constexpr int TOWER_X = 18, TOWER_W = 80, TOWER_TOP = 32;
static constexpr int WIN_X = 50, WIN_W = 42, WIN_H = 34;
static constexpr int BEAM_X0 = 92, BEAM_X1 = 246, BEAM_H0 = 3, BEAM_H1 = 20;
static constexpr int ROCK_X = 108;

// ---------------------------------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------------------------------

enum class Ph : uint8_t { Title, Ready, Run, Crash, Over, Pause };
struct Ship {
  int8_t lane, pos;  // pos 1..5 shown at P1..P5, SPAWN_POS not yet shown
};

static Ph ph = Ph::Title;
static int game = 0;  // 0 = Game A, 1 = Game B
static int score, miss, keeper, bonus_left;
static uint16_t hi[2];
static bool tick_snd = true, new_hi = false;
static Ship ships[8];
static int nships;
static bool turned[LANES], wreck[LANES];
static uint8_t wave_phase;
static int8_t last_spawn_lane;
static uint32_t next_tick, ph_t0, crash_until, last_input, pause_rem;

static Preferences prefs;
static bool prefs_open = false;

static void loadPrefs()
{
  if (!prefs_open) prefs_open = prefs.begin("play", false);
  hi[0] = prefs.getUShort("fener_hiA", 0);
  hi[1] = prefs.getUShort("fener_hiB", 0);
  tick_snd = prefs.getBool("fener_tick", true);
}

// ---------------------------------------------------------------------------------------------------------------------
// Sound: Radio_Beep plays one tone at a time, so melodies are a small queue stepped from tick()
// ---------------------------------------------------------------------------------------------------------------------

struct Note {
  uint16_t hz, ms;  // hz 0 = rest
};
static Note sq[8];
static int sq_n = 0, sq_i = 0;
static uint32_t sq_next = 0;

static void playSeq(const Note *n, int count)
{
  count = min(count, 8);
  for (int i = 0; i < count; i++) sq[i] = n[i];
  sq_n = count;
  sq_i = 0;
  sq_next = 0;
}
static bool soundBusy(uint32_t now) { return sq_i < sq_n || (int32_t)(now - sq_next) < 0; }
static void soundTick(uint32_t now)
{
  if (sq_i >= sq_n || (int32_t)(now - sq_next) < 0) return;
  const Note &n = sq[sq_i++];
  if (n.hz) Radio_Beep(n.hz, n.ms);
  sq_next = now + n.ms + 15;
}

static const Note SND_SAVE[] = { { 2000, 40 } };
static const Note SND_CRASH[] = { { 800, 90 }, { 500, 90 }, { 300, 90 } };
static const Note SND_BONUS[] = { { 1000, 70 }, { 1500, 70 }, { 2000, 90 } };
static const Note SND_OVER[] = { { 400, 600 } };
static const Note SND_HI[] = { { 400, 600 }, { 0, 100 }, { 2400, 70 }, { 0, 20 }, { 2400, 70 }, { 0, 20 }, { 2400, 70 } };

// ---------------------------------------------------------------------------------------------------------------------
// Rules
// ---------------------------------------------------------------------------------------------------------------------

static float rnd() { return (esp_random() >> 8) * (1.0f / 16777216.0f); }

static float tickMs()
{
  const Pace &m = MODES[game];
  int v = score % WRAP, h = v / 100, w = v % 100;
  if (score >= WRAP) h = max(h, MIN_HUNDRED_AFTER_WRAP);
  return max(m.t_min, m.t0 - m.per_h * h - m.per_w * w);
}

static void startGame(int g)
{
  game = g;
  score = miss = bonus_left = 0;
  keeper = 1;
  nships = 0;
  memset(turned, 0, sizeof(turned));
  memset(wreck, 0, sizeof(wreck));
  wave_phase = 0;
  last_spawn_lane = -1;
  new_hi = false;
  ph = Ph::Ready;
  ph_t0 = millis();
  last_input = ph_t0;
  Radio_Beep(1760, 50, 2, 90);
  sq_n = sq_i = 0;
  sq_next = ph_t0 + 260;
  state.dirty = true;
}

static void endGame(uint32_t now)
{
  ph = Ph::Over;
  ph_t0 = now;
  new_hi = score > hi[game];
  if (new_hi) {
    hi[game] = (uint16_t)min(score, 65535);
    if (!prefs_open) prefs_open = prefs.begin("play", false);
    prefs.putUShort(game ? "fener_hiB" : "fener_hiA", hi[game]);
    playSeq(SND_HI, sizeof(SND_HI) / sizeof(SND_HI[0]));
  } else {
    playSeq(SND_OVER, 1);
  }
  state.dirty = true;
}

// One tick. Same order as design/fener-bekcisi-mc.py: everybody one step left, the ship that reaches the rocks is
// settled, then at most one new ship is born.
static void step(uint32_t now)
{
  memset(turned, 0, sizeof(turned));
  memset(wreck, 0, sizeof(wreck));
  for (int i = 0; i < nships; i++) ships[i].pos--;
  bool crashed = false, over = false, sound = false;
  for (int i = nships - 1; i >= 0; i--) {
    if (ships[i].pos != 0) continue;
    int lane = ships[i].lane;
    ships[i] = ships[--nships];
    sound = true;
    if (lane == keeper) {
      int pts = bonus_left > 0 ? 2 : 1;
      if (bonus_left > 0) bonus_left--;
      int before = score % WRAP;
      score += pts;
      turned[lane] = true;
      playSeq(SND_SAVE, 1);
      for (int b = 0; b < 2; b++) {
        if (before < BONUS_AT[b] && score % WRAP >= BONUS_AT[b]) {
          if (miss > 0) miss = 0;
          else bonus_left = BONUS_SAVES;
          playSeq(SND_BONUS, 3);
        }
      }
    } else {
      wreck[lane] = true;
      miss++;
      crashed = true;
      playSeq(SND_CRASH, 3);
      if (miss >= MAX_MISS) over = true;
    }
  }
  wave_phase ^= 1;
  if (over) {
    endGame(now);
    return;
  }

  // spawn: one per tick; the lane needs P5 and P4 free; right after a spawn and at a fast tick only the same lane
  const Pace &m = MODES[game];
  int v = score % WRAP;
  int nmax = v >= FAST_SHIPS_FROM ? m.max_hi : m.max_lo;
  float p = m.p0 + m.per_h_p * (v / 100);
  float T = tickMs();
  bool spawned = false;
  if (nships < nmax && rnd() < p) {
    int free_lane[LANES], nf = 0;
    for (int l = 0; l < LANES; l++) {
      bool busy = false;
      for (int i = 0; i < nships; i++) busy |= ships[i].lane == l && ships[i].pos >= GAP_POS;
      if (!busy && !(last_spawn_lane >= 0 && T < FAST_TICK_MS && l != last_spawn_lane)) free_lane[nf++] = l;
    }
    if (nf) {
      int l = free_lane[esp_random() % nf];
      ships[nships++] = { (int8_t)l, (int8_t)SPAWN_POS };
      last_spawn_lane = l;
      spawned = true;
    }
  }
  if (!spawned) last_spawn_lane = -1;

  if (crashed) {
    ph = Ph::Crash;
    crash_until = now + CRASH_FREEZE_MS;
  } else {
    next_tick = now + (uint32_t)T;
    if (tick_snd && !sound && !soundBusy(now)) Radio_Beep(1000, 6);
  }
  state.dirty = true;
}

static void pauseGame(uint32_t now)
{
  if (ph != Ph::Run) return;
  int32_t rem = (int32_t)(next_tick - now);
  pause_rem = rem > 0 ? rem : 0;
  ph = Ph::Pause;
  state.dirty = true;
}

static void resumeGame(uint32_t now)
{
  ph = Ph::Run;
  next_tick = now + max(pause_rem, RESUME_DELAY_MS);
  last_input = now;
  state.dirty = true;
}

static void moveKeeper(int y)
{
  int l = y < LANE_SPLIT_Y[0] ? 0 : y < LANE_SPLIT_Y[1] ? 1 : 2;
  last_input = millis();
  if (l != keeper) {
    keeper = l;
    state.dirty = true;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------------------------------

static bool playing() { return ph == Ph::Ready || ph == Ph::Run || ph == Ph::Crash || ph == Ph::Over; }

// A finger touched down: in play the lane changes right now.
static bool fenerTouchDown(int, int y)
{
  if (!playing()) return false;
  if (ph != Ph::Over) moveKeeper(y);
  return true;
}

static bool fenerSwipesLocked() { return playing(); }

static bool fenerGesture(Gesture g, int x, int y)
{
  uint32_t now = millis();
  last_input = now;
  if (playing()) {  // a release that was not consumed at touch-down (the /cmd touch test path), hold, or a vertical swipe
    if (g == Gesture::Tap && ph != Ph::Over) moveKeeper(y);
    if (g == Gesture::LongPress && ph == Ph::Run) pauseGame(now);
    return true;
  }
  if (ph == Ph::Pause) {
    if (g == Gesture::Tap) resumeGame(now);
    return g == Gesture::Tap || g == Gesture::LongPress;
  }
  // title
  if (g == Gesture::Tap) {
    if (y < 30 && x >= 480) {  // the click-sound switch, top right
      tick_snd = !tick_snd;
      if (!prefs_open) prefs_open = prefs.begin("play", false);
      prefs.putBool("fener_tick", tick_snd);
      Radio_Beep(tick_snd ? 1046 : 523, 40);
    } else {
      startGame(x < W / 2 + 56 ? 0 : 1);
    }
    return true;
  }
  return g == Gesture::LongPress;
}

static void fenerTick()
{
  uint32_t now = millis();
  soundTick(now);
  switch (ph) {
    case Ph::Ready:
      if (now - ph_t0 >= READY_MS) {
        ph = Ph::Run;
        next_tick = now + (uint32_t)tickMs();
        last_input = now;
        state.dirty = true;
      }
      break;
    case Ph::Run:
      if (now - last_input >= AUTO_PAUSE_MS) pauseGame(now);
      else if ((int32_t)(now - next_tick) >= 0) step(now);
      break;
    case Ph::Crash:
      if ((int32_t)(now - crash_until) >= 0) {
        memset(wreck, 0, sizeof(wreck));
        ph = Ph::Run;
        next_tick = now + (uint32_t)tickMs();
        last_input = now;
        state.dirty = true;
      }
      break;
    case Ph::Over:
      if (now - ph_t0 >= OVER_TOTAL_MS) {
        ph = Ph::Title;
        state.dirty = true;
      }
      break;
    default: break;
  }
}

static void fenerEnter()
{
  loadPrefs();
  ph = Ph::Title;
  sq_n = sq_i = 0;
  sq_next = 0;
  state.dirty = true;
}

static void fenerExit()
{
  ph = Ph::Title;
  if (prefs_open) {
    prefs.end();
    prefs_open = false;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------------------------------

namespace {
struct Pal {
  uint16_t bg, ghost, lit, hot, dim, halo, hoth;
  bool ghosts, glow;
} pal;
int pass = 0;  // 0 unlit segments, 1 glow of lit ones, 2 lit ones
}  // namespace

static uint16_t rgb(int r, int g, int b) { return px::rgb(r, g, b); }

static void loadPal()
{
  theme::Id t = theme::current();
  pal = { BG, GHOST, LIT, HOT, DIM, HALO, HOTH, t != theme::ATARI, style().glow };
  if (t == theme::C64) pal.ghost = rgb(42, 31, 100);        // very dark blue on the blue screen
  else if (t == theme::AMIGA) pal.ghost = rgb(0, 40, 98);   // darker than any copper bar
}

static int sprW(const char *const *rows) { return strlen(rows[0]); }

// One segment drawn in the current pass. Sprites are placed by their bottom center.
static void segSpr(const char *const *rows, int n, int cx, int bottom, int s, bool lit, bool hot, bool flip = false)
{
  int x = cx - sprW(rows) * s / 2, y = bottom - n * s;
  if (pass == 0) {
    if (!lit && pal.ghosts) sprite(rows, n, x, y, s, pal.ghost, flip);
  } else if (pass == 1) {
    if (lit && pal.glow) {
      uint16_t h = hot ? pal.hoth : pal.halo;
      sprite(rows, n, x - 1, y, s, h, flip);
      sprite(rows, n, x + 1, y, s, h, flip);
      sprite(rows, n, x, y - 1, s, h, flip);
      sprite(rows, n, x, y + 1, s, h, flip);
    }
  } else if (lit) {
    sprite(rows, n, x, y, s, hot ? pal.hot : pal.lit, flip);
  }
}

// Place by top left instead (keeper, lamp, ring).
static void segSprTL(const char *const *rows, int n, int x, int y, int s, bool lit, bool hot)
{
  segSpr(rows, n, x + sprW(rows) * s / 2, y + n * s, s, lit, hot);
}

static void segRects(int lane, bool wave_b, bool lit)  // 8 dashes of one wave segment
{
  int y = LANE_WL[lane] + (wave_b ? 8 : 3), x0 = wave_b ? 241 : 215;
  uint16_t c = pass == 0 ? (!lit && pal.ghosts ? pal.ghost : 0) : (pass == 2 && lit ? pal.dim : 0);
  if (!c) return;
  for (int k = 0; k < 8; k++) rect(x0 + k * 52, y, 16, 2, c);
}

static void segBeam(int lane, bool lit)
{
  uint16_t c = pass == 0 ? (!lit && pal.ghosts ? pal.ghost : 0) : (pass == 2 && lit ? pal.hot : 0);
  if (!c) return;
  bool solid = theme::current() == theme::ATARI;  // the 2600 has no stripes to spare: a flat wedge
  int cy = LANE_CY[lane];
  for (int dy = -BEAM_H1; dy <= BEAM_H1; dy++) {
    int a = abs(dy);
    if (!solid && (dy & 1)) continue;
    int x0 = a <= BEAM_H0 ? BEAM_X0 : BEAM_X0 + (a - BEAM_H0) * (BEAM_X1 - BEAM_X0) / (BEAM_H1 - BEAM_H0);
    rect(x0, cy + dy, BEAM_X1 - x0, 1, c);
  }
}

static void drawFixed()
{
  bool atari = theme::current() == theme::ATARI;
  uint16_t body = pal.ghost;
  int bottom = H;
  // tower
  rect(TOWER_X - 6, TOWER_TOP - 4, TOWER_W + 12, 4, pal.dim);
  rect(TOWER_X, TOWER_TOP, TOWER_W, bottom - TOWER_TOP, body);
  frame(TOWER_X, TOWER_TOP, TOWER_W, bottom - TOWER_TOP, pal.dim);
  rect(TOWER_X, LANE_SPLIT_Y[0], TOWER_W, 1, pal.dim);
  rect(TOWER_X, LANE_SPLIT_Y[1], TOWER_W, 1, pal.dim);
  for (int l = 0; l < LANES; l++) {
    int wy = LANE_CY[l] - WIN_H / 2;
    rect(WIN_X, wy, WIN_W, WIN_H, pal.bg);
    frame(WIN_X, wy, WIN_W, WIN_H, pal.dim);
    if (atari) rect(ROCK_X - 2, LANE_WL[l] + 2, W - ROCK_X + 2, 12, body);  // the 2600 sea, a flat band
    sprite(ROCK, rowsOf(ROCK), ROCK_X, LANE_WL[l] + 5 - rowsOf(ROCK) * 3, 3, pal.dim);
  }
}

static void drawLane(int l, const bool occ[POSITIONS], bool all, uint32_t now)
{
  int s = SHIP_S[l];
  const char *const *ship = l == 0 ? SHIP_FAR : l == 1 ? SHIP_MID : SHIP_NEAR;
  int sn = l == 0 ? rowsOf(SHIP_FAR) : l == 1 ? rowsOf(SHIP_MID) : rowsOf(SHIP_NEAR);
  const char *const *tr = l == 0 ? TURN_FAR : l == 1 ? TURN_MID : TURN_NEAR;
  int tn = l == 0 ? rowsOf(TURN_FAR) : l == 1 ? rowsOf(TURN_MID) : rowsOf(TURN_NEAR);
  bool kp = all || (keeper == l && ph != Ph::Title);
  bool blink = ((now / 160) & 1) != 0;
  int wl = LANE_WL[l];

  segBeam(l, kp);
  segSprTL(KEEPER, rowsOf(KEEPER), WIN_X + 6, LANE_CY[l] - 11, 2, kp, false);
  segSprTL(LAMP, rowsOf(LAMP), WIN_X + 26, LANE_CY[l] - 6, 2, kp, true);
  segRects(l, false, !all && wave_phase == 0 && ph != Ph::Title);
  segRects(l, true, !all && wave_phase == 1 && ph != Ph::Title);
  for (int p = 0; p < POSITIONS; p++) segSpr(ship, sn, PX[p], wl, s, all || occ[p], false);
  segSpr(tr, tn, PX[0], wl, s, all || turned[l], false);
  bool w = all || (wreck[l] && (ph != Ph::Crash || blink));
  segSpr(WRECK, rowsOf(WRECK), ROCK_X + 21, LANE_WL[l] + 5, 3, w, true);
}

static void drawHud(uint32_t now, bool all)
{
  theme::Id t = theme::current();
  int y0 = t == theme::C64 ? 7 : t == theme::AMIGA ? 14 : 2;
  int h = t == theme::C64 ? 18 : t == theme::AMIGA ? 14 : 20;
  int dw = h * 2 / 3, dt = max(2, h / 7);
  Ink digit = { pal.lit, pal.glow ? pal.halo : NONE, pal.ghosts ? pal.ghost : NONE };
  Ink hotdig = { pal.hot, pal.glow ? pal.hoth : NONE, pal.ghosts ? pal.ghost : NONE };
  bool title = ph == Ph::Title;

  if (t == theme::AMIGA) {  // the window's title bar
    rect(0, 0, W, 12, pal.lit);
    rect(3, 2, 9, 8, pal.dim);
    rect(5, 4, 5, 4, pal.lit);
    rect(W - 26, 2, 9, 8, pal.dim);
    rect(W - 14, 2, 9, 8, pal.dim);
    text("FENER", 22, 2, 1, Ink{ pal.dim });
  }

  if (title) {
    text(L("LIGHTHOUSE KEEPER", "FENER BEKÇİSİ"), 14, y0 + (h - 14) / 2, 2, LIT_INK);
    const char *s = tick_snd ? L("CLICK ON", "TIK AÇIK") : L("CLICK OFF", "TIK KAPALI");
    text(s, W - 14 - textWidth(s, 2), y0 + (h - 14) / 2, 2, tick_snd ? LIT_INK : DIM_INK);
    return;
  }

  text(game ? "GAME B" : "GAME A", 14, y0 + (h - 14) / 2, 2, LIT_INK);
  // hi-score: the 3 digits blink while a new one stands
  bool hi_vis = !(new_hi && ph == Ph::Over && ((now / 250) & 1));
  text("HI", 236, y0 + (h - 14) / 2, 2, DIM_INK);
  char b[8];
  snprintf(b, sizeof(b), "%3d", (game ? hi[1] : hi[0]) % WRAP);
  if (new_hi && ph == Ph::Over) snprintf(b, sizeof(b), "%3d", score % WRAP);
  if (hi_vis || all) seg(b, 270, y0 + 2, dw * 3 / 4 + 1, h - 4, max(2, dt - 1), 4, digit);
  // score: blinks while the double points last
  bool sc_vis = !(bonus_left > 0 && ph == Ph::Run && ((now / 250) & 1));
  snprintf(b, sizeof(b), "%3d", score % WRAP);
  if (sc_vis || all) seg(b, 470, y0, dw, h, dt, 5, digit);
  // misses: life rings
  for (int i = 0; i < MAX_MISS; i++) {
    int x = 566 + i * 25, y = t == theme::AMIGA ? 12 : y0 - 1;
    bool lit = all || i < miss;
    if (pal.ghosts && !lit) sprite(RING, rowsOf(RING), x, y, 2, pal.ghost);
    if (lit) {
      if (pal.glow) {
        sprite(RING, rowsOf(RING), x - 1, y, 2, pal.hoth);
        sprite(RING, rowsOf(RING), x + 1, y, 2, pal.hoth);
      }
      sprite(RING, rowsOf(RING), x, y, 2, pal.hot);
    }
  }
}

static void drawTitle()
{
  const int by = 38, bh = 104, ax = 122, bx = 378, bw = 244;
  for (int g = 0; g < 2; g++) {
    int x = g ? bx : ax;
    rect(x, by, bw, bh, pal.bg);
    frame(x, by, bw, bh, pal.dim);
    const char *name = g ? "GAME B" : "GAME A";
    text(name, x + (bw - textWidth(name, 3)) / 2, by + 10, 3, LIT_INK);
    text("HI", x + 56, by + 48, 2, DIM_INK);
    char b[8];
    snprintf(b, sizeof(b), "%3d", hi[g] % WRAP);
    Ink digit = { pal.lit, pal.glow ? pal.halo : NONE, pal.ghosts ? pal.ghost : NONE };
    seg(b, x + 92, by + 40, 18, 30, 4, 6, digit);
    const char *d = g ? L("BUSY SEAS", "HIZLI DENİZ") : L("CALM SEAS", "SAKİN DENİZ");
    text(d, x + (bw - textWidth(d, 2)) / 2, by + 82, 2, DIM_INK);
  }
}

static void drawPause()
{
  const int bx = 138, by = 50, bw = 364, bh = 76;
  rect(bx, by, bw, bh, pal.bg);
  frame(bx, by, bw, bh, pal.hot);
  const char *a = L("PAUSE", "DURAKLADI");
  text(a, bx + (bw - textWidth(a, 4)) / 2, by + 8, 4, HOT_INK);
  const char *b = L("TAP: CONTINUE", "DOKUN: DEVAM");
  const char *c = L("SWIPE SIDEWAYS: MENU", "YANA KAYDIR: MENÜ");
  text(b, bx + (bw - textWidth(b, 2)) / 2, by + 42, 2, LIT_INK);
  text(c, bx + (bw - textWidth(c, 2)) / 2, by + 58, 2, DIM_INK);
}

static void fenerDraw(const DateTime &)
{
  uint32_t now = millis();
  loadPal();
  Style old = style(), st = old;
  st.ghosts = pal.ghosts;
  setStyle(st);

  clear(pal.bg);
  theme::Id t = theme::current();
  if (t == theme::AMIGA) {  // copper bars in the sky
    for (int y = 0; y < H; y += 6) rect(0, y, W, 6, rgb(0, 54 + y * 46 / H, 128 + y * 50 / H));
  }
  drawFixed();

  bool occ[LANES][POSITIONS] = {};
  if (ph != Ph::Title) {
    for (int i = 0; i < nships; i++) {
      if (ships[i].pos >= 1 && ships[i].pos <= POSITIONS) occ[ships[i].lane][ships[i].pos - 1] = true;
    }
  }
  bool all = ph == Ph::Over && now - ph_t0 < OVER_BLINK_MS && ((now - ph_t0) / 200) % 2 == 0;
  for (pass = 0; pass < 3; pass++) {
    for (int l = 0; l < LANES; l++) drawLane(l, occ[l], all, now);
  }
  pass = 0;
  drawHud(now, all);
  if (ph == Ph::Title) drawTitle();
  if (ph == Ph::Pause) drawPause();
  theme::decorate();  // the C64 border (full_screen apps are left alone by the page renderer)
  setStyle(old);
}

static uint32_t fenerFrameMs() { return 40; }

static void iconFener(int x, int y, uint16_t c) { sprite(ICON, rowsOf(ICON), x, y, 4, c); }

const App APP_FENER = { "fener", "KEEPER", "FENER", iconFener, true, true, fenerEnter, fenerExit, fenerDraw, fenerGesture, fenerTick,
                        fenerFrameMs, fenerTouchDown, fenerSwipesLocked };

#endif
