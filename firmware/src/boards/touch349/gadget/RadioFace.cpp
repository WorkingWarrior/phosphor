#ifdef BOARD_TOUCH349

#include "RadioFace.h"
#include <Preferences.h>
#include <math.h>
#include "Radio.h"
#include "Vfd.h"

using namespace vfd;

// ---------------------------------------------------------------------------------------------------------------------
// Choices
// ---------------------------------------------------------------------------------------------------------------------

enum Slot : uint8_t { DANCERS, CASSETTE, NO_SLOT, SLOT_COUNT };
enum Eq : uint8_t { BARS, PEAKS, MIRROR, VU, SCOPE, EQ_COUNT };
static const char *const SLOT_KEYS[SLOT_COUNT] = { "dancers", "cassette", "none" };
static const char *const EQ_KEYS[EQ_COUNT] = { "bars", "peaks", "mirror", "vu", "scope" };

static Preferences prefs;
static int slot = -1, eq = -1;  // -1 = not read yet

static void load()
{
  if (slot >= 0) return;
  prefs.begin("radio", true);
  slot = prefs.getUChar("slot", DANCERS) % SLOT_COUNT;
  eq = prefs.getUChar("eq", BARS) % EQ_COUNT;
  prefs.end();
}

static void save()
{
  prefs.begin("radio", false);
  prefs.putUChar("slot", slot);
  prefs.putUChar("eq", eq);
  prefs.end();
}

bool RadioFace_Slot()
{
  load();
  return slot != NO_SLOT;
}

// ---------------------------------------------------------------------------------------------------------------------
// Companion slot: a 26 x 32 dot matrix, 3 px pitch
// ---------------------------------------------------------------------------------------------------------------------

static constexpr int CW = 26, CH = 32, PITCH = 3;

// Paint grid with the shapes the figures are made of (same geometry as the design mockups).
struct Cells {
  uint8_t v[CH][CW];
  void clear() { memset(v, 0, sizeof(v)); }
  void set(float x, float y, uint8_t k = 1)
  {
    int i = (int)lroundf(x), j = (int)lroundf(y);
    if (i >= 0 && j >= 0 && i < CW && j < CH) v[j][i] = k;
  }
  void rect(int x, int y, int w, int h, uint8_t k = 1)
  {
    for (int j = y; j < y + h; j++)
      for (int i = x; i < x + w; i++) set(i, j, k);
  }
  void disc(float cx, float cy, float r, uint8_t k = 1)
  {
    for (int j = 0; j < CH; j++)
      for (int i = 0; i < CW; i++) {
        float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
        if (dx * dx + dy * dy <= r * r) v[j][i] = k;
      }
  }
  void line(float x0, float y0, float x1, float y1, int t = 1, uint8_t k = 1)
  {
    int n = (int)(fmaxf(fabsf(x1 - x0), fabsf(y1 - y0)) * 2) + 1;
    for (int s = 0; s <= n; s++) {
      float x = x0 + (x1 - x0) * s / n, y = y0 + (y1 - y0) * s / n;
      rect((int)lroundf(x - (t - 1) / 2.0f), (int)lroundf(y - (t - 1) / 2.0f), t, t, k);
    }
  }
  void poly(const float (*p)[2], int n, uint8_t k = 1)
  {
    for (int j = 0; j < CH; j++)
      for (int i = 0; i < CW; i++) {
        float x = i + 0.5f, y = j + 0.5f;
        bool in = false;
        for (int a = 0, b = n - 1; a < n; b = a++) {
          if ((p[a][1] > y) != (p[b][1] > y) && x < (p[b][0] - p[a][0]) * (y - p[a][1]) / (p[b][1] - p[a][1]) + p[a][0])
            in = !in;
        }
        if (in) v[j][i] = k;
      }
  }
};

// The couple: he (left) leads, she (right) wears a flared skirt. Poses by tempo:
//   still       standing hand in hand (nothing playing, or no steady beat)
//   sway L/C/R  slow dance, close together, leaning with the beat (under 95 BPM)
//   step A/B    he leads her into a turn under their raised hands, one pose a beat (95-130 BPM)
//   spin        her turn with the skirt flying, mixed into the steps (over 130 BPM)
enum Pose : uint8_t { STILL, SWAY_L, SWAY_C, SWAY_R, STEP_A, STEP_B, SPIN, POSE_COUNT };

static void him(Cells &g, float b, bool wide_arm)
{
  g.disc(7.5f, 6.5f + b, 2.6f);
  const float torso[][2] = { { 4.5f, 10 + b }, { 10.5f, 10 + b }, { 9.8f, 19 + b }, { 5.2f, 19 + b } };
  g.poly(torso, 4);
  g.line(10, 11 + b, 13, 1.5f, 2);  // raised arm to the joined hands
  g.line(5, 11 + b, wide_arm ? 1.5f : (b ? 3 : 2), wide_arm ? 14 : 17, 2);
}

static void buildPose(Cells &g, Pose p)
{
  g.clear();
  switch (p) {
    case STILL: {
      g.disc(7.5f, 6.5f, 2.6f);
      const float t1[][2] = { { 4.5f, 10 }, { 10.5f, 10 }, { 9.8f, 19 }, { 5.2f, 19 } };
      g.poly(t1, 4);
      g.line(6, 19, 6, 31, 2);
      g.line(9, 19, 9, 31, 2);
      g.line(5, 11, 4, 18, 2);
      g.line(10, 11, 12.5f, 18, 2);
      g.disc(18.5f, 8, 2.4f);
      g.disc(20.6f, 6.5f, 1.2f);
      const float t2[][2] = { { 16.4f, 11.5f }, { 20.6f, 11.5f }, { 20, 18 }, { 17, 18 } };
      g.poly(t2, 4);
      const float sk[][2] = { { 17, 17 }, { 20, 17 }, { 22, 25 }, { 15, 25 } };
      g.poly(sk, 4);
      g.line(17.5f, 26, 17.5f, 31, 1);
      g.line(19.5f, 26, 19.5f, 31, 1);
      g.line(16.5f, 12.5f, 13.5f, 18, 1);
      g.line(20.5f, 13, 21.5f, 18, 1);
      g.rect(12, 17, 2, 2);
      break;
    }
    case SWAY_L:
    case SWAY_C:
    case SWAY_R: {
      float s = p == SWAY_L ? -1 : p == SWAY_R ? 1 : 0, u = s * 0.6f;
      g.disc(9 + s, 6.5f, 2.6f);
      const float t1[][2] = { { 6 + u, 10 }, { 12 + u, 10 }, { 11.3f, 19 }, { 6.7f, 19 } };
      g.poly(t1, 4);
      g.line(7.5f, 19, 7 + s * 0.5f, 31, 2);
      g.line(10.5f, 19, 11 + s * 0.5f, 31, 2);
      g.line(6.5f + u, 11, 3.5f + s, 7, 2);  // joined hands held out
      g.rect((int)lroundf(2.5f + s), 5, 2, 2);
      g.disc(14.8f + s, 8.2f, 2.4f);  // her head on his shoulder
      g.disc(16.9f + s, 6.7f, 1.2f);
      const float t2[][2] = { { 13 + u, 11.5f }, { 17.4f + u, 11.5f }, { 16.8f, 18 }, { 13.6f, 18 } };
      g.poly(t2, 4);
      const float sk[][2] = { { 13.6f, 17 }, { 16.8f, 17 }, { 19 + s, 25 }, { 11.8f + s, 25 } };
      g.poly(sk, 4);
      g.line(14, 26, 14, 31, 1);
      g.line(16.5f, 26, 16.5f, 31, 1);
      g.line(11.5f + u, 12, 17.5f + u, 14.5f, 2);  // his arm around her
      g.line(13.5f + u, 12, 6.5f + u, 11, 1);       // hers across to his hand
      break;
    }
    case STEP_A:
    case STEP_B: {
      float b = p == STEP_B ? 1 : 0;
      him(g, b, false);
      g.line(6, 19 + b, b ? 4 : 6, 31, 2);
      g.line(9, 19 + b, b ? 10 : 11, 31, 2);
      g.disc(18.5f, 8 - b, 2.4f);
      g.disc(20.6f, 6.5f - b, 1.2f);
      const float t2[][2] = { { 16.4f, 11.5f - b }, { 20.6f, 11.5f - b }, { 20, 18 }, { 17, 18 } };
      g.poly(t2, 4);
      if (b) {
        const float sk[][2] = { { 17, 17 }, { 20, 17 }, { 25, 26 }, { 13.5f, 25 } };
        g.poly(sk, 4);
      } else {
        const float sk[][2] = { { 17, 17 }, { 20, 17 }, { 23.5f, 25 }, { 12, 26 } };
        g.poly(sk, 4);
      }
      g.line(17.5f, 26, b ? 17 : 16, 31, 1);
      g.line(19.5f, 26, b ? 21 : 20, 31, 1);
      g.line(16.5f, 12.5f - b, 13, 1.5f, 1);  // her raised arm
      g.line(20.5f, 13 - b, 22.5f, 18, 1);
      g.rect(12, 0, 3, 3);  // hands
      break;
    }
    case SPIN: {
      him(g, 0, true);
      g.line(6, 19, 5, 31, 2);
      g.line(9, 19, 11, 31, 2);
      g.disc(18.5f, 7, 2.4f);  // seen turning: the bun on the other side
      g.disc(16.4f, 5.5f, 1.2f);
      const float t2[][2] = { { 17, 10.5f }, { 20, 10.5f }, { 19.6f, 16.5f }, { 17.4f, 16.5f } };
      g.poly(t2, 4);
      const float sk[][2] = { { 16.8f, 16 }, { 20.2f, 16 }, { 26, 21 }, { 24.5f, 23 }, { 13, 23 }, { 11, 21 } };
      g.poly(sk, 6);
      g.line(17.5f, 23, 16.5f, 31, 1);
      g.line(19.5f, 23, 21, 30, 1);
      g.line(18, 11.5f, 13, 1.5f, 1);
      g.line(20, 12, 24, 14.5f, 1);
      g.rect(12, 0, 3, 3);
      break;
    }
    default: break;
  }
}

// Poses are built once, 26 bits a row.
static uint32_t poses[POSE_COUNT][CH];
static bool poses_built = false;

static void buildPoses()
{
  static Cells g;
  for (int p = 0; p < POSE_COUNT; p++) {
    buildPose(g, (Pose)p);
    for (int j = 0; j < CH; j++) {
      uint32_t row = 0;
      for (int i = 0; i < CW; i++)
        if (g.v[j][i]) row |= 1u << i;
      poses[p][j] = row;
    }
  }
  poses_built = true;
}

static Cells cells;

static void drawCells()
{
  int x0 = RADIO_SLOT_X, y0 = RADIO_SLOT_Y;
  for (int j = 0; j < CH; j++)
    for (int i = 0; i < CW; i++)
      if (!cells.v[j][i]) rect(x0 + i * PITCH, y0 + j * PITCH, 2, 2, FAINT);
  for (int j = 0; j < CH; j++)
    for (int i = 0; i < CW; i++) {
      uint8_t k = cells.v[j][i];
      if (k == 1 || k == 3) rect(x0 + i * PITCH - 1, y0 + j * PITCH - 1, 4, 4, k == 3 ? HOTH : HALO);
    }
  for (int j = 0; j < CH; j++)
    for (int i = 0; i < CW; i++) {
      uint8_t k = cells.v[j][i];
      if (k) rect(x0 + i * PITCH, y0 + j * PITCH, 2, 2, k == 1 ? LIT : k == 2 ? DIM : HOT);
    }
}

static int tempo_set = 1;  // 0 slow, 1 steps, 2 fast; with a little hysteresis

static void dancers()
{
  if (!poses_built) buildPoses();
  RadioBeat b = Radio_Beat();
  bool playing = Radio_State() == RadioState::Playing;
  static uint32_t on_ms = 0;  // a beat lost for a moment (a break in the song) does not stop the dance at once
  if (b.on) on_ms = millis();
  bool dancing = playing && on_ms && millis() - on_ms < 2000;
  Pose p = STILL;
  int hop = 0;
  if (dancing) {
    if (b.bpm < (tempo_set == 0 ? 100 : 92)) tempo_set = 0;
    else if (b.bpm > (tempo_set == 2 ? 126 : 134)) tempo_set = 2;
    else tempo_set = 1;
    if (tempo_set == 0) {
      static const Pose SWAY[4] = { SWAY_L, SWAY_C, SWAY_R, SWAY_C };
      p = SWAY[(b.count % 2) * 2 + (b.phase >= 0.5f)];
    } else if (tempo_set == 1) {
      p = b.count % 2 ? STEP_B : STEP_A;
      hop = b.phase < 0.18f;
    } else {
      static const Pose FAST[4] = { STEP_A, STEP_B, SPIN, STEP_B };
      p = FAST[b.count % 4];
      hop = b.phase < 0.3f;
    }
  }
  uint8_t ink = playing ? 1 : 2;  // waiting for music: dim
  cells.clear();
  for (int j = 0; j < CH; j++) {
    int y = j - hop;
    if (y < 0) continue;
    for (int i = 0; i < CW; i++)
      if (poses[p][j] >> i & 1) cells.v[y][i] = ink;
  }
  drawCells();
}

// Tape counter digits, 3 x 5 cells.
static const uint16_t MINI[10] = { 0x7B6F, 0x2C97, 0x73E7, 0x73CF, 0x5BC9, 0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF };

static void miniDigit(int d, int x, int y, uint8_t k)
{
  for (int j = 0; j < 5; j++)
    for (int i = 0; i < 3; i++)
      if (MINI[d] >> (14 - (j * 3 + i)) & 1) cells.set(x + i, y + j, k);
}

static uint32_t tape_ms = 0, tape_last = 0;  // how long the tape has been playing

static void cassette()
{
  bool playing = Radio_State() == RadioState::Playing;
  uint32_t now = millis();
  if (playing && tape_last) tape_ms += now - tape_last;
  tape_last = now;

  cells.clear();
  // shell, label stripe, window, the slanted bottom with its two holes
  cells.rect(0, 8, CW, 1, 2);
  cells.rect(0, 25, CW, 1, 2);
  cells.rect(0, 8, 1, 18, 2);
  cells.rect(CW - 1, 8, 1, 18, 2);
  cells.rect(2, 10, 22, 2, 1);
  cells.rect(4, 13, 18, 1, 2);
  cells.rect(4, 21, 18, 1, 2);
  cells.rect(4, 13, 1, 9, 2);
  cells.rect(21, 13, 1, 9, 2);
  cells.rect(7, 22, 12, 1, 2);
  cells.set(6, 23, 2);
  cells.set(19, 23, 2);
  cells.set(9, 24, 1);
  cells.set(16, 24, 1);

  // tape moves from the left reel to the right one over 30 minutes, then starts again
  float p = (tape_ms % 1800000) / 1800000.0f;
  const float cx[2] = { 8, 17 }, pack[2] = { 1.8f + 1.7f * (1 - p), 1.8f + 1.7f * p };
  int step = playing ? (tape_ms / 140) % 8 : 0;
  for (int r = 0; r < 2; r++) {
    cells.disc(cx[r] + 0.5f, 17.5f, pack[r], 2);
    for (int k = 0; k < 8; k++) {  // the hub: alternate dots chase round while it turns
      float a = k * PI / 4;
      cells.set(cx[r] + cosf(a) * 1.4f, 17 + sinf(a) * 1.4f, (k + step) % 2 ? 1 : 0);
    }
    cells.set(cx[r], 17, 1);
  }

  // counter: minutes of tape played
  int n = (tape_ms / 60000) % 1000;
  miniDigit(n / 100, 7, 27, 2);
  miniDigit(n / 10 % 10, 11, 27, 2);
  miniDigit(n % 10, 15, 27, 2);
  drawCells();
}

void RadioFace_DrawSlot()
{
  load();
  if (slot == DANCERS) dancers();
  else if (slot == CASSETTE) cassette();
}

// ---------------------------------------------------------------------------------------------------------------------
// Equalizer: 14 bands, 196 x 88 px from (426, 34)
// ---------------------------------------------------------------------------------------------------------------------

static constexpr int EQ_X = RADIO_EQ_X, EQ_TOP = 34, EQ_W = 196, EQ_BOT = 121;
static uint8_t spec_level[RADIO_BANDS], spec_peak[RADIO_BANDS], spec_hold[RADIO_BANDS];

static void specStep()
{
  uint8_t lv[RADIO_BANDS];
  Radio_Levels(lv);
  for (int i = 0; i < RADIO_BANDS; i++) {
    spec_level[i] = lv[i] >= spec_level[i] ? lv[i] : spec_level[i] - 1;
    if (spec_level[i] >= spec_peak[i]) {
      spec_peak[i] = spec_level[i];
      spec_hold[i] = 12;
    } else if (spec_hold[i]) {
      spec_hold[i]--;
    } else if (spec_peak[i]) {
      spec_peak[i]--;
      spec_hold[i] = 2;
    }
  }
}

static void eqBars()
{
  for (int i = 0; i < RADIO_BANDS; i++) {
    int x = EQ_X + i * 14;
    for (int r = 0; r < 10; r++) {
      int y = 115 - r * 9;
      bool on = r < spec_level[i], peak = spec_peak[i] && r == spec_peak[i] - 1 && !on, hot = r >= 8;
      if (on || peak) rect(x - 1, y - 1, 13, 8, hot || peak ? HOTH : HALO);
      rect(x, y, 11, 6, peak || (on && hot) ? HOT : on ? LIT : GHOST);
    }
  }
}

// Only the tops: the level as one lit cell, the falling peak above it, the rest of the column a thin guide.
static void eqPeaks()
{
  for (int i = 0; i < RADIO_BANDS; i++) {
    int x = EQ_X + i * 14;
    rect(x + 5, 34, 1, 87, GHOST);
    int l = spec_level[i], pk = spec_peak[i];
    if (l) {
      int y = 115 - (l - 1) * 9;
      rect(x - 1, y - 1, 13, 8, HALO);
      rect(x, y, 11, 6, LIT);
    }
    if (pk && pk > l) {
      int y = 115 - (pk - 1) * 9 + 2;
      rect(x - 1, y - 1, 13, 4, HOTH);
      rect(x, y, 11, 2, HOT);
    }
  }
}

// Bars growing both ways from the middle line.
static void eqMirror()
{
  rect(EQ_X, 77, EQ_W - 1, 1, GHOST);
  for (int i = 0; i < RADIO_BANDS; i++) {
    int x = EQ_X + i * 14, n = (spec_level[i] + 1) / 2;
    for (int r = 0; r < 5; r++) {
      bool on = r < n, hot = r >= 4;
      int yu = 70 - r * 9, yd = 80 + r * 9;
      for (int y : { yu, yd }) {
        if (on) rect(x - 1, y - 1, 13, 8, hot ? HOTH : HALO);
        rect(x, y, 11, 6, on && hot ? HOT : on ? LIT : GHOST);
      }
    }
  }
}

// A dotted line, 2 px dots, clipped by the caller.
static void dots(float x0, float y0, float x1, float y1, uint16_t c, int sz = 2)
{
  int n = (int)fmaxf(fabsf(x1 - x0), fabsf(y1 - y0));
  for (int s = 0; s <= n; s++) {
    float t = n ? (float)s / n : 0;
    rect((int)lroundf(x0 + (x1 - x0) * t) - sz / 2, (int)lroundf(y0 + (y1 - y0) * t) - sz / 2, sz, sz, c);
  }
}

// Two analog VU meters, left and right channel, with needle ballistics.
static float needle[2];

static void eqVu()
{
  float lv[2];
  Radio_Vu(lv[0], lv[1]);
  for (int c = 0; c < 2; c++) {
    float db = 20 * log10f(fmaxf(lv[c], 0.001f));
    float target = constrain((db + 20) / 23, 0.0f, 1.0f);
    needle[c] += (target - needle[c]) * (target > needle[c] ? 0.35f : 0.12f);

    int x = EQ_X + c * 101, w = 94, cx = x + w / 2, py = 136, R = 80;
    frame(x, EQ_TOP - 4, w, EQ_BOT - EQ_TOP + 6, GHOST);
    clip(x + 1, EQ_TOP - 3, x + w - 1, EQ_BOT + 1);
    const float A = 0.56f;  // half the swing, radians
    for (int k = 0; k <= 10; k++) {  // scale: the last three marks are the red zone
      float a = -A + 2 * A * k / 10, s = sinf(a), co = cosf(a);
      bool hot = k >= 8, major = k % 5 == 0 || hot;
      float r0 = R + (major ? 1 : 3), r1 = R + 7;
      dots(cx + s * r0, py - co * r0, cx + s * r1, py - co * r1, hot ? HOT : DIM, major ? 2 : 1);
    }
    for (int k = 0; k <= 40; k++) {  // the arc under the marks
      float a = -A + 2 * A * k / 40;
      rect((int)lroundf(cx + sinf(a) * R), (int)lroundf(py - cosf(a) * R), 1, 1, k >= 32 ? HOT : DIM);
    }
    float a = -A + 2 * A * needle[c], s = sinf(a), co = cosf(a);
    dots(cx, py, cx + s * (R + 5), py - co * (R + 5), HALO, 4);
    dots(cx, py, cx + s * (R + 5), py - co * (R + 5), needle[c] > 0.8f ? HOT : LIT, 2);
    noClip();
    text("VU", x + 6, EQ_BOT - 14, 2, GHOST_INK);  // corners: the needle sweeps through the middle
    text(c ? "R" : "L", x + w - 16, EQ_BOT - 14, 2, DIM_INK);
  }
}

// Oscilloscope: the wave itself over a dotted graticule.
static void eqScope()
{
  int8_t tr[RADIO_TRACE];
  Radio_Trace(tr);
  int mid = (EQ_TOP + EQ_BOT) / 2;
  frame(EQ_X, EQ_TOP - 4, EQ_W - 1, EQ_BOT - EQ_TOP + 6, GHOST);
  for (int x = EQ_X + 4; x < EQ_X + EQ_W - 4; x += 4) {
    rect(x, mid, 1, 1, DIM);
    rect(x, mid - 22, 1, 1, GHOST);
    rect(x, mid + 22, 1, 1, GHOST);
  }
  for (int k = 1; k < 4; k++)
    for (int y = EQ_TOP; y < EQ_BOT; y += 4) rect(EQ_X + k * EQ_W / 4, y, 1, 1, GHOST);

  clip(EQ_X + 1, EQ_TOP - 3, EQ_X + EQ_W - 2, EQ_BOT + 1);
  for (int pass = 0; pass < 2; pass++) {
    int prev_y = mid - tr[0] * 40 / 100;
    for (int i = 1; i < RADIO_TRACE; i++) {
      int x0 = EQ_X + 2 + (i - 1) * (EQ_W - 5) / (RADIO_TRACE - 1), x1 = EQ_X + 2 + i * (EQ_W - 5) / (RADIO_TRACE - 1);
      int y = mid - tr[i] * 40 / 100;
      int ya = min(prev_y, y), yb = max(prev_y, y);
      if (pass == 0) rect(x0 - 1, ya - 1, x1 - x0 + 3, yb - ya + 4, HALO);
      else rect(x0, ya, x1 - x0 + 1, yb - ya + 2, LIT);
      prev_y = y;
    }
  }
  noClip();
}

void RadioFace_DrawEq()
{
  load();
  specStep();
  switch (eq) {
    case BARS: eqBars(); break;
    case PEAKS: eqPeaks(); break;
    case MIRROR: eqMirror(); break;
    case VU: eqVu(); break;
    case SCOPE: eqScope(); break;
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Changing them
// ---------------------------------------------------------------------------------------------------------------------

bool RadioFace_Hold(int x, int y)
{
  load();
  if (y >= 130) return false;
  if (x >= RADIO_EQ_X - 4) {
    eq = (eq + 1) % EQ_COUNT;
  } else if (x >= RADIO_SLOT_X - 4 && slot != NO_SLOT) {
    slot = (slot + 1) % SLOT_COUNT;
  } else if (slot == NO_SLOT && x >= RADIO_SLOT_X - 4) {  // the empty slot area brings the dancers back
    slot = DANCERS;
  } else {
    return false;
  }
  save();
  return true;
}

void RadioFace_Command(const String &arg)
{
  load();
  if (arg == "eq") eq = (eq + 1) % EQ_COUNT;
  else if (arg == "comp") slot = (slot + 1) % SLOT_COUNT;
  else if (arg.startsWith("eq")) eq = constrain(arg.substring(2).toInt(), 0, EQ_COUNT - 1);
  else if (arg.startsWith("comp")) slot = constrain(arg.substring(4).toInt(), 0, SLOT_COUNT - 1);
  else return;
  save();
}

String RadioFace_Status()
{
  load();
  RadioBeat b = Radio_Beat();
  char s[120];
  snprintf(s, sizeof(s), "\"companion\":\"%s\",\"eq\":\"%s\",\"beat\":%s,\"bpm\":%.0f,\"beat_sure\":%.2f", SLOT_KEYS[slot],
           EQ_KEYS[eq], b.on ? "true" : "false", b.bpm, b.strength);
  return s;
}

#endif
