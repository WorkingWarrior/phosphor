#ifdef BOARD_TOUCH349

#include "ToolApps.h"
#include <Preferences.h>
#include <esp_random.h>
#include "../../../core/Board.h"
#include "../../../core/state.h"
#include "Motion.h"
#include "Radio.h"
#include "Settings.h"
#include "Theme.h"
#include "Vfd.h"

using namespace vfd;

// The five apps of the Tools page. Each draws the whole panel in the current theme (palette and font of the settings
// page: the VFD look with glow in the VFD theme, flat colors in the machine themes), keeps its state in a few statics
// and lets go of everything in exit().

// ---------------------------------------------------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------------------------------------------------

static int textX(const char *s, int p, int x0, int x1) { return (x0 + x1 - textWidth(s, p)) / 2; }

// Filled ellipse (rx, ry) from horizontal spans.
static void ellipse(int cx, int cy, int rx, int ry, uint16_t color)
{
  if (ry < 1) {
    rect(cx - rx, cy, 2 * rx + 1, 1, color);
    return;
  }
  for (int dy = -ry; dy <= ry; dy++) {
    float f = 1.0f - (float)(dy * dy) / (float)(ry * ry);
    int dx = (int)(rx * sqrtf(f < 0 ? 0 : f) + 0.5f);
    rect(cx - dx, cy + dy, 2 * dx + 1, 1, color);
  }
}

static uint16_t rgb(int r, int g, int b) { return px::rgb(r, g, b); }

// ---------------------------------------------------------------------------------------------------------------------
// Spirit level
// ---------------------------------------------------------------------------------------------------------------------

static const char *const ICON_LEVEL[] = { "............", "............", "############", "#..........#", "#...####...#", "#..######..#",
                                          "#...####...#", "#..........#", "############", "............", "....#..#....", "............" };
static const char *const ICON_COIN[] = { "...######...", "..#......#..", ".#...##...#.", "#...####...#", "#..##..##..#", "#..#....#..#",
                                         "#..#....#..#", "#..##..##..#", "#...####...#", ".#...##...#.", "..#......#..", "...######..." };
static const char *const ICON_BREATHE[] = { "############", "#..........#", "#.########.#", "#.#......#.#", "#.#.####.#.#", "#.#.#..#.#.#",
                                            "#.#.#..#.#.#", "#.#.####.#.#", "#.#......#.#", "#.########.#", "#..........#", "############" };
static const char *const ICON_SCORE[] = { "############", "#..........#", "#.###...#..#", "#...#..##..#", "#.###...#..#",
                                          "#.#.....#..#", "#.###..###.#", "#..........#", "############" };
static const char *const ICON_LAMP[] = { "...######...", "..########..", ".##########.", ".##########.", ".##########.", ".##########.",
                                         "..########..", "...######...", "....####....", "....#..#....", "....####....", ".....##....." };

static void iconLevel(int x, int y, uint16_t c) { sprite(ICON_LEVEL, 12, x, y, 4, c); }
static void iconCoin(int x, int y, uint16_t c) { sprite(ICON_COIN, 12, x, y, 4, c); }
static void iconBreathe(int x, int y, uint16_t c) { sprite(ICON_BREATHE, 12, x, y, 4, c); }
static void iconScore(int x, int y, uint16_t c) { sprite(ICON_SCORE, 9, x, y + 6, 4, c); }
static void iconLamp(int x, int y, uint16_t c) { sprite(ICON_LAMP, 12, x, y, 4, c); }

static Preferences tools_prefs;
static bool tools_prefs_open = false;
static void openPrefs()
{
  if (tools_prefs_open) return;
  tools_prefs.begin("tools", false);
  tools_prefs_open = true;
}

static float zero_x = 0, zero_y = 0;  // degrees that count as level (NVS lvx / lvy)
static uint32_t level_note_until = 0;
static bool level_note_clear = false;

static void levelEnter()
{
  openPrefs();
  zero_x = tools_prefs.getFloat("lvx", 0);
  zero_y = tools_prefs.getFloat("lvy", 0);
  level_note_until = 0;
}

// Tilt of the long axis and of the other one, in degrees. Lying flat the second axis is the short edge; standing on
// the long edge it is the lean towards or away from you.
static void levelAngles(float &ax, float &ay, bool &flat)
{
  float gx, gy, gz;
  Motion_Gravity(gx, gy, gz);
  float m = sqrtf(gx * gx + gy * gy + gz * gz);
  if (m < 0.3f) m = 1;
  gx /= m, gy /= m, gz /= m;
  flat = fabsf(gz) > 0.7f;
  ax = asinf(constrain(gx, -1.0f, 1.0f)) * 57.2958f;
  ay = asinf(constrain(flat ? gy : gz, -1.0f, 1.0f)) * 57.2958f;
}

static bool levelGesture(Gesture g, int, int)
{
  if (g != Gesture::Tap && g != Gesture::LongPress) return false;
  openPrefs();
  if (g == Gesture::Tap) {
    float ax, ay;
    bool flat;
    // the zero is kept as the raw angle: undo the current offset first
    levelAngles(ax, ay, flat);
    zero_x = ax;
    zero_y = ay;
    level_note_clear = false;
  } else {
    zero_x = zero_y = 0;
    level_note_clear = true;
  }
  tools_prefs.putFloat("lvx", zero_x);
  tools_prefs.putFloat("lvy", zero_y);
  level_note_until = millis() + 1800;
  Radio_Beep(g == Gesture::Tap ? 1046 : 523, 50);
  return true;
}

static void levelDraw(const DateTime &)
{
  clear();
  float ax, ay;
  bool flat;
  levelAngles(ax, ay, flat);
  ax -= zero_x;
  ay -= zero_y;
  bool level = fabsf(ax) < 0.5f && fabsf(ay) < 0.5f;
  uint16_t bubble = level ? HOT : LIT;

  // the long vial
  const int TX = 24, TY = 44, TW = 380, TH = 52, TC = TX + TW / 2;
  frame(TX, TY, TW, TH, DIM);
  frame(TX + 1, TY + 1, TW - 2, TH - 2, GHOST);
  rect(TC - 30, TY, 1, TH, DIM);
  rect(TC + 30, TY, 1, TH, DIM);
  rect(TC, TY, 1, 8, DIM);
  rect(TC, TY + TH - 8, 1, 8, DIM);
  int bx = TC - (int)(ax * 15.0f);
  bx = constrain(bx, TX + 22, TX + TW - 23);
  if (style().glow) Apps_Disc(bx, TY + TH / 2, 21, HALO);
  Apps_Disc(bx, TY + TH / 2, 18, bubble);

  // the round vial
  const int RX = 528, RY = 90, RR = 60;
  Apps_Disc(RX, RY, RR, DIM);
  Apps_Disc(RX, RY, RR - 2, BG);
  rect(RX - RR + 4, RY, 2 * RR - 8, 1, GHOST);
  rect(RX, RY - RR + 4, 1, 2 * RR - 8, GHOST);
  Apps_Disc(RX, RY, 20, GHOST);
  Apps_Disc(RX, RY, 18, BG);
  float ox = -ax * 5.0f, oy = -ay * 5.0f, len = sqrtf(ox * ox + oy * oy);
  if (len > RR - 16) ox *= (RR - 16) / len, oy *= (RR - 16) / len;
  if (style().glow) Apps_Disc(RX + (int)ox, RY + (int)oy, 14, HALO);
  Apps_Disc(RX + (int)ox, RY + (int)oy, 12, bubble);

  // angles
  text(L("SPIRIT LEVEL", "SU TERAZİSİ"), TX, 14, 2, DIM_INK);
  if (millis() < level_note_until) {
    const char *n = level_note_clear ? L("ZERO CLEARED", "SIFIR SİLİNDİ") : L("ZERO SET", "SIFIR AYARLANDI");
    text(n, TX + TW - textWidth(n, 2), 14, 2, HOT_INK);
  } else if (level) {
    text(L("LEVEL", "DÜZ"), TX + TW - textWidth(L("LEVEL", "DÜZ"), 2), 14, 2, HOT_INK);
  }
  char buf[24];
  snprintf(buf, sizeof(buf), "%+.1f°", ax);
  text(buf, TX, 114, 4, LIT_INK);
  snprintf(buf, sizeof(buf), "%+.1f°", ay);
  text(buf, TX + 226, 124, 3, DIM_INK);
  text(flat ? "X / Y" : "X / Z", TX + 226, 102, 2, DIM_INK);
}

static uint32_t levelFrameMs() { return 60; }

// ---------------------------------------------------------------------------------------------------------------------
// Coin toss
// ---------------------------------------------------------------------------------------------------------------------

static constexpr uint32_t FLIP_MS = 1000;
static bool flipping = false;
static uint32_t flip_start = 0, flip_cool = 0;
static int flip_result = 0, last_face = -1;  // 0 = heads, 1 = tails
static uint16_t coin_count[2] = { 0, 0 };

static void coinFlip()
{
  if (flipping) return;
  flipping = true;
  flip_start = millis();
  flip_result = esp_random() & 1;
  state.dirty = true;
}

static void coinEnter()
{
  flipping = false;
  flip_cool = millis() + 600;
}

static void coinTick()
{
  if (flipping && millis() - flip_start >= FLIP_MS) {
    flipping = false;
    last_face = flip_result;
    coin_count[flip_result]++;
    flip_cool = millis() + 800;
    Radio_Beep(flip_result ? 660 : 880, 40);
    state.dirty = true;
  }
  if (!flipping && millis() > flip_cool && Motion_Shake() > 0.55f) coinFlip();
}

static bool coinGesture(Gesture g, int, int)
{
  if (g == Gesture::Tap) {
    coinFlip();
    return true;
  }
  if (g == Gesture::LongPress) {
    coin_count[0] = coin_count[1] = 0;
    last_face = -1;
    Radio_Beep(523, 50);
    return true;
  }
  return false;
}

static void coinDraw(const DateTime &)
{
  clear();
  const int CX = 118, CY = 94, R = 54;
  float c = 1, hop = 0;
  int face = last_face < 0 ? 0 : last_face;
  if (flipping) {
    float t = constrain((float)(millis() - flip_start) / FLIP_MS, 0.0f, 1.0f);
    float e = 1.0f - (1.0f - t) * (1.0f - t);
    float turns = 6 + flip_result;  // half turns: even ends heads up, odd tails
    c = cosf(3.14159265f * turns * e);
    face = c >= 0 ? 0 : 1;
    hop = sinf(3.14159265f * t) * 30.0f;
  }
  int ry = max(3, (int)(R * fabsf(c)));
  int cy = CY - (int)hop;
  if (style().glow) ellipse(CX, cy, R + 3, ry + 3, HALO);
  ellipse(CX, cy + 4, R, ry, DIM);  // the rim
  ellipse(CX, cy, R, ry, face ? HOT : LIT);
  if (ry > 38) {
    const char *f = face ? "T" : L("H", "Y");
    text(f, CX - 12, cy - 17, 5, Ink{ BG });
  }

  const int X0 = 226;
  if (flipping) {
    text("...", X0, 40, 6, DIM_INK);
  } else if (last_face < 0) {
    text(L("TAP TO FLIP", "DOKUN: AT"), X0, 48, 4, LIT_INK);
    text(L("OR SHAKE IT", "YA DA SALLA"), X0, 84, 3, DIM_INK);
  } else {
    const char *w = last_face ? L("TAILS", "TURA") : L("HEADS", "YAZI");
    text(w, X0, 30, 7, last_face ? HOT_INK : LIT_INK);
  }
  char buf[40];
  snprintf(buf, sizeof(buf), "%s %u", L("HEADS", "YAZI"), coin_count[0]);
  text(buf, X0, 118, 3, LIT_INK);
  snprintf(buf, sizeof(buf), "%s %u", L("TAILS", "TURA"), coin_count[1]);
  text(buf, X0 + 190, 118, 3, HOT_INK);
}

static uint32_t coinFrameMs() { return flipping ? 30 : 0; }

// ---------------------------------------------------------------------------------------------------------------------
// Breathing (box: in, hold, out, hold; four seconds each)
// ---------------------------------------------------------------------------------------------------------------------

static constexpr uint32_t PHASE_MS = 4000;
static bool br_running = false, br_beep = true;
static int br_phase = 0, br_cycles = 0;
static uint32_t br_elapsed = 0, br_last = 0;

static void breatheBeep()
{
  if (!br_beep || Radio_On()) return;
  static const int HZ[4] = { 523, 440, 392, 440 };
  Radio_Beep(HZ[br_phase], 45);
}

static void breatheEnter()
{
  br_running = false;
  br_phase = br_cycles = 0;
  br_elapsed = 0;
}

static void breatheTick()
{
  if (!br_running) return;
  uint32_t now = millis();
  br_elapsed += now - br_last;
  br_last = now;
  if (br_elapsed >= PHASE_MS) {
    br_elapsed -= PHASE_MS;
    br_phase = (br_phase + 1) % 4;
    if (br_phase == 0) br_cycles++;
    breatheBeep();
    state.dirty = true;
  }
}

static bool breatheGesture(Gesture g, int, int)
{
  if (g == Gesture::Tap) {
    br_running = !br_running;
    br_last = millis();
    if (br_running) breatheBeep();
    return true;
  }
  if (g == Gesture::LongPress) {
    br_beep = !br_beep;
    if (br_beep && !Radio_On()) Radio_Beep(660, 40);
    return true;
  }
  return false;
}

static void breatheDraw(const DateTime &)
{
  clear();
  const int CX = 94, CY = 86, BIG = 132, SMALL = 36;
  float t = constrain((float)br_elapsed / PHASE_MS, 0.0f, 1.0f);
  float e = t * t * (3 - 2 * t);
  int sz = br_phase == 0 ? SMALL + (int)((BIG - SMALL) * e) : br_phase == 1 ? BIG : br_phase == 2 ? BIG - (int)((BIG - SMALL) * e) : SMALL;
  frame(CX - BIG / 2 - 2, CY - BIG / 2 - 2, BIG + 4, BIG + 4, GHOST);
  if (style().glow) rect(CX - sz / 2 - 2, CY - sz / 2 - 2, sz + 4, sz + 4, HALO);
  rect(CX - sz / 2, CY - sz / 2, sz, sz, br_phase % 2 ? HOT : LIT);

  const int X0 = 190;
  bool fresh = !br_running && !br_phase && !br_elapsed && !br_cycles;
  static const char *const EN[4] = { "BREATHE IN", "HOLD", "BREATHE OUT", "HOLD" };
  static const char *const TR[4] = { "NEFES AL", "TUT", "NEFES VER", "TUT" };
  if (fresh) {
    text(L("TAP TO START", "BAŞLAMAK İÇİN DOKUN"), X0, 52, 3, LIT_INK);
    text(L("BOX BREATHING 4-4-4-4", "KUTU NEFESİ 4-4-4-4"), X0, 90, 2, DIM_INK);
  } else {
    text(L(EN[br_phase], TR[br_phase]), X0, 24, 5, br_phase % 2 ? HOT_INK : LIT_INK);
    char n[4];
    snprintf(n, sizeof(n), "%d", 4 - (int)(br_elapsed / 1000));
    text(n, X0, 84, 8, br_running ? LIT_INK : DIM_INK);
    if (!br_running) text(L("PAUSED", "DURAKLADI"), X0 + 70, 100, 2, DIM_INK);
  }
  if (!fresh) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %d", L("CYCLES", "TUR"), br_cycles);
    text(buf, 360, 100, 3, DIM_INK);
  }
  text(br_beep ? L("BEEP ON", "BİP AÇIK") : L("BEEP OFF", "BİP KAPALI"), 440, 140, 2, br_beep ? DIM_INK : GHOST_INK);
}

static uint32_t breatheFrameMs() { return br_running ? 40 : 0; }

// ---------------------------------------------------------------------------------------------------------------------
// Scoreboard
// ---------------------------------------------------------------------------------------------------------------------

static uint8_t score[2] = { 0, 0 };
static uint32_t reset_armed_until = 0;
static constexpr int RESET_X = 262, RESET_Y = 8, RESET_W = 116, RESET_H = 28;

static bool inReset(int x, int y) { return x >= RESET_X - 10 && x < RESET_X + RESET_W + 10 && y < RESET_Y + RESET_H + 10; }

static void scoreTick()
{
  if (reset_armed_until && (int32_t)(millis() - reset_armed_until) >= 0) {
    reset_armed_until = 0;
    state.dirty = true;
  }
}

static bool scoreGesture(Gesture g, int x, int y)
{
  if (g == Gesture::Tap) {
    if (inReset(x, y)) {
      if (reset_armed_until && (int32_t)(millis() - reset_armed_until) < 0) {  // the second tap within 2 s
        score[0] = score[1] = 0;
        reset_armed_until = 0;
        Radio_Beep(523, 60);
      } else {
        reset_armed_until = millis() + 2000;
      }
      return true;
    }
    int h = x < W / 2 ? 0 : 1;
    if (score[h] < 99) score[h]++;
    return true;
  }
  if (g == Gesture::LongPress) {
    if (inReset(x, y)) return true;
    int h = x < W / 2 ? 0 : 1;
    if (score[h] > 0) score[h]--;
    return true;
  }
  return false;
}

static void scoreDraw(const DateTime &)
{
  clear();
  rect(W / 2 - 1, 46, 2, 112, GHOST);
  bool vfdLook = theme::current() == theme::VFD;
  for (int h = 0; h < 2; h++) {
    char d[4];
    snprintf(d, sizeof(d), "%2d", score[h]);
    int cx = h ? W * 3 / 4 : W / 4;
    Ink ink = h ? Ink{ HOT, HOTH, GHOST } : Ink{ LIT, HALO, GHOST };
    if (vfdLook) {
      seg(d, cx - 76, 44, 70, 112, 14, 12, ink);
    } else {
      int p = 11;
      text(d, cx - textWidth(d, p) / 2, 62, p, h ? HOT_INK : LIT_INK);
    }
  }
  // the reset key, top middle: tap it twice within two seconds
  bool armed = reset_armed_until && (int32_t)(millis() - reset_armed_until) < 0;
  uint16_t edge = armed ? HOT : DIM;
  frame(RESET_X, RESET_Y, RESET_W, RESET_H, edge);
  const char *label = armed ? L("AGAIN?", "TEKRAR?") : L("RESET", "SIFIRLA");
  text(label, textX(label, 2, RESET_X, RESET_X + RESET_W), RESET_Y + 7, 2, armed ? HOT_INK : DIM_INK);
}

static uint32_t scoreFrameMs() { return reset_armed_until ? 200 : 0; }

// ---------------------------------------------------------------------------------------------------------------------
// Lamp
// ---------------------------------------------------------------------------------------------------------------------

struct LampColor {
  const char *en, *tr;
  uint8_t r, g, b;
};
static const LampColor LAMP[] = {
  { "WARM WHITE", "SICAK BEYAZ", 255, 190, 110 }, { "COOL WHITE", "SOĞUK BEYAZ", 205, 225, 255 }, { "AMBER", "KEHRİBAR", 255, 130, 0 },
  { "RED", "KIRMIZI", 255, 0, 0 },                { "GREEN", "YEŞİL", 0, 230, 40 },                { "BLUE", "MAVİ", 20, 60, 255 },
  { "RAINBOW", "GÖKKUŞAĞI", 0, 0, 0 },
};
static constexpr int LAMP_COUNT = sizeof(LAMP) / sizeof(LAMP[0]), LAMP_RAINBOW = LAMP_COUNT - 1;
static int lamp_idx = 0, lamp_prev_bright = -1;
static uint32_t lamp_name_until = 0;

static void lampEnter()
{
  lamp_prev_bright = board().brightness();
  lamp_name_until = millis() + 1500;
}

static void lampExit()
{
  if (lamp_prev_bright >= 0) board().setBrightness(lamp_prev_bright, false);
  lamp_prev_bright = -1;
}

static void lampTick()
{
  if (lamp_name_until && (int32_t)(millis() - lamp_name_until) >= 0) {
    lamp_name_until = 0;
    state.dirty = true;
  }
}

static bool lampGesture(Gesture g, int, int)
{
  int b = board().brightness();
  switch (g) {
    case Gesture::Tap:
      lamp_idx = (lamp_idx + 1) % LAMP_COUNT;
      lamp_name_until = millis() + 1500;
      return true;
    case Gesture::SwipeUp:
      if (b >= 0) board().setBrightness(min(100, (b / 10 + 1) * 10), true);
      return true;
    case Gesture::SwipeDown:
      if (b >= 0) board().setBrightness(max(5, (b + 9) / 10 * 10 - 10), true);
      return true;
    default: return true;
  }
}

// hue 0..359 at full saturation and value
static uint16_t hue565(int h)
{
  int s = h / 60, f = (h % 60) * 255 / 60;
  int up = f, down = 255 - f;
  int r, g, b;
  switch (s) {
    case 0: r = 255, g = up, b = 0; break;
    case 1: r = down, g = 255, b = 0; break;
    case 2: r = 0, g = 255, b = up; break;
    case 3: r = 0, g = down, b = 255; break;
    case 4: r = up, g = 0, b = 255; break;
    default: r = 255, g = 0, b = down; break;
  }
  return rgb(r, g, b);
}

static void lampDraw(const DateTime &)
{
  const LampColor &lc = LAMP[lamp_idx];
  uint16_t color = lamp_idx == LAMP_RAINBOW ? hue565((millis() / 45) % 360) : rgb(lc.r, lc.g, lc.b);
  clear(color);
  if (lamp_name_until && (int32_t)(millis() - lamp_name_until) < 0) {
    int lum = lamp_idx == LAMP_RAINBOW ? 200 : (lc.r * 3 + lc.g * 6 + lc.b) / 10;
    uint16_t ink = lum > 110 ? 0 : 0xFFFF;
    const char *n = L(lc.en, lc.tr);
    int p = 3;
    rect(textX(n, p, 0, W) - 10, 126, textWidth(n, p) + 20, 34, color);
    text(n, textX(n, p, 0, W), 132, p, Ink{ ink });
  }
}

static uint32_t lampFrameMs() { return lamp_idx == LAMP_RAINBOW ? 50 : 0; }

// ---------------------------------------------------------------------------------------------------------------------

const App APP_LEVEL = { "level", "LEVEL", "TERAZİ", iconLevel, false, false, levelEnter, nullptr, levelDraw, levelGesture, nullptr, levelFrameMs };
const App APP_COIN = { "coin", "COIN", "YAZI-TURA", iconCoin, false, false, coinEnter, nullptr, coinDraw, coinGesture, coinTick, coinFrameMs };
const App APP_BREATHE = { "breathe", "BREATHE", "NEFES", iconBreathe, false, false, breatheEnter, nullptr, breatheDraw, breatheGesture, breatheTick, breatheFrameMs };
const App APP_SCORE = { "score", "SCORE", "SKOR", iconScore, false, false, nullptr, nullptr, scoreDraw, scoreGesture, scoreTick, scoreFrameMs };
const App APP_LAMP = { "lamp", "LAMP", "LAMBA", iconLamp, false, true, lampEnter, lampExit, lampDraw, lampGesture, lampTick, lampFrameMs };

#endif
