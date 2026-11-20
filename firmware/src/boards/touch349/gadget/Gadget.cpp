#ifdef BOARD_TOUCH349

#include "Gadget.h"
#include <Preferences.h>
#include "../../../core/net.h"
#include "../../../core/state.h"
#include "Fireplace.h"
#include "Fishtank.h"
#include "Hourglass.h"
#include "Lava.h"
#include "Motion.h"
#include "Px.h"
#include "Snowglobe.h"
#include "Radio.h"
#include "Settings.h"
#include "SettingsUi.h"
#include "Seaside.h"
#include "Study.h"
#include "Theme.h"
#include "Vfd.h"
#include "Wave.h"
#include "Weather.h"

using namespace vfd;

// ---------------------------------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------------------------------

static Preferences prefs;

static const Mode PAGES[] = { Mode::Clock, Mode::Radio, Mode::Weather, Mode::Timer, Mode::Study, Mode::Hourglass,
                              Mode::Aquarium, Mode::Snow, Mode::Fire, Mode::Sea, Mode::Wave, Mode::Lava, Mode::Settings };
static constexpr int PAGE_COUNT = sizeof(PAGES) / sizeof(PAGES[0]);

static const char *const WEEKDAYS_TR[] = { "PAZ", "PZT", "SAL", "ÇAR", "PER", "CUM", "CMT" };
static const char *const WEEKDAYS_EN[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
static const char *const MONTHS_TR[] = { "OCAK", "ŞUBAT", "MART", "NİSAN", "MAYIS", "HAZİRAN",
                                      "TEMMUZ", "AĞUSTOS", "EYLÜL", "EKİM", "KASIM", "ARALIK" };
static const char *const MONTHS_EN[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
static const char *weekdayName(int d) { return (settings::tr() ? WEEKDAYS_TR : WEEKDAYS_EN)[constrain(d, 0, 6)]; }
static const char *monthName(int m) { return (settings::tr() ? MONTHS_TR : MONTHS_EN)[constrain(m, 1, 12) - 1]; }

// Alarm
static bool alarm_on = false;
static int alarm_h = 7, alarm_m = 30;
static bool ringing = false;
static uint32_t ring_start_ms = 0, ring_beep_ms = 0;
static bool radio_by_alarm = false;
static int fired_day = -1;
#define ALARM_MAX_MS (30 * 60 * 1000UL)
#define ALARM_RADIO_GRACE_MS 12000

// Volume overlay
static uint32_t volume_shown_ms = 0;
#define VOLUME_SHOW_MS 1500

// Countdown timer: minutes set with the buttons, counts down, rings (the interval programs come back later)
enum class Phase { Idle, Running, Paused, Done };
static Phase phase = Phase::Idle;
static int32_t egg_set_s = 600;  // what it counts down from
static uint32_t egg_end_ms = 0;
static int32_t egg_left_ms = 0;  // while paused
static uint32_t egg_done_ms = 0, egg_beep_ms = 0;
#define EGG_RING_MS 60000

// Radio page spectrum (display side: smoothed, with falling peaks)
static uint8_t spec_level[RADIO_BANDS], spec_peak[RADIO_BANDS];
static uint8_t spec_hold[RADIO_BANDS];


static void savePrefs()
{
  prefs.putBool("al_on", alarm_on);
  prefs.putUChar("al_h", alarm_h);
  prefs.putUChar("al_m", alarm_m);
  prefs.putUChar("st2", Radio_Station());
  prefs.putUChar("volume", Radio_Volume());
}

static int weekday(int y, int m, int d)
{
  static const int t[] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
  if (m < 3) y--;
  return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

static String two(int v)
{
  char b[4];
  snprintf(b, sizeof(b), "%02d", v);
  return b;
}

// "14:05", or "02:05" with the 12-hour clock chosen in settings
static String hhmm(const DateTime &now)
{
  int h = now.hour;
  if (!settings::h24()) h = h % 12 == 0 ? 12 : h % 12;
  return two(h) + ":" + two(now.minute);
}

static String mss(int32_t secs)
{
  if (secs < 0) secs = 0;
  char b[12];
  snprintf(b, sizeof(b), "%d:%02d", (int)(secs / 60), (int)(secs % 60));
  return b;
}

static bool blinkOn(uint32_t period = 500) { return (millis() / period) % 2 == 0; }

// ---------------------------------------------------------------------------------------------------------------------
// Alarm and timer logic
// ---------------------------------------------------------------------------------------------------------------------

static void stopAlarm()
{
  ringing = false;
  if (radio_by_alarm) {
    Radio_Stop();
    Radio_SetVolume(prefs.getUChar("volume", 30));
  }
  radio_by_alarm = false;
  state.dirty = true;
}

static void startAlarm()
{
  ringing = true;
  ring_start_ms = millis();
  ring_beep_ms = 0;
  board().setDisplayOn(true);
  state.mode = Mode::Clock;
  state.mode_until_ms = 0;
  state.dirty = true;
  if (!Radio_On()) {
    radio_by_alarm = true;
    Radio_Play(Radio_Station());
    Radio_RampVolume(60000);
  }
}

static int32_t eggLeftMs()
{
  switch (phase) {
    case Phase::Idle: return egg_set_s * 1000;
    case Phase::Paused: return egg_left_ms;
    case Phase::Running: return max<int32_t>(0, (int32_t)(egg_end_ms - millis()));
    default: return 0;
  }
}

static void timerTick()
{
  if (phase == Phase::Running && (int32_t)(egg_end_ms - millis()) <= 0) {
    phase = Phase::Done;
    egg_done_ms = millis();
    egg_beep_ms = 0;
    board().setDisplayOn(true);
    state.mode = Mode::Timer;
    state.mode_until_ms = 0;
    state.dirty = true;
  }
  if (phase == Phase::Done) {
    if (millis() - egg_done_ms > EGG_RING_MS) {
      phase = Phase::Idle;
      state.dirty = true;
    } else if (millis() - egg_beep_ms > 1600) {
      egg_beep_ms = millis();
      Radio_Beep(1318, 110, 2, 90);
    }
  }
}

static void timerStartPause()
{
  switch (phase) {
    case Phase::Idle:
      if (egg_set_s <= 0) return;
      egg_end_ms = millis() + egg_set_s * 1000;
      phase = Phase::Running;
      Radio_Beep(880, 60);
      break;
    case Phase::Running:
      egg_left_ms = eggLeftMs();
      phase = Phase::Paused;
      break;
    case Phase::Paused:
      egg_end_ms = millis() + egg_left_ms;
      phase = Phase::Running;
      break;
    case Phase::Done: phase = Phase::Idle; break;
  }
  state.dirty = true;
}

static void timerReset()
{
  phase = Phase::Idle;
  state.dirty = true;
}

static void eggSet(int32_t secs)
{
  egg_set_s = constrain(secs, 0, 99 * 60);
  prefs.putUShort("egg", egg_set_s);
  phase = Phase::Idle;
  state.dirty = true;
}

// Minutes on or off: the running clock, the paused one, a ringing one (counts again) or the setting.
static void eggAddMinutes(int m)
{
  int32_t d = m * 60000;
  if (phase == Phase::Running) {
    int32_t left = max<int32_t>(0, (int32_t)(egg_end_ms - millis()));
    egg_end_ms = millis() + constrain(left + d, 1000, 99 * 60000);
  } else if (phase == Phase::Paused) {
    egg_left_ms = constrain(egg_left_ms + d, 1000, 99 * 60000);
  } else if (phase == Phase::Done) {
    if (m > 0) {
      egg_end_ms = millis() + d;
      phase = Phase::Running;
    }
  } else {
    eggSet(egg_set_s + m * 60);
  }
  state.dirty = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Pages
// ---------------------------------------------------------------------------------------------------------------------

// Battery level under the seconds: outline, fill by percent, the number; a bolt while charging. There is no charge
// pin, so "charging" means the voltage rose over the last minutes or sits at the top (on USB).
static void drawBatteryBadge(const DateTime &now)
{
  static float v_ref = 0;
  static uint32_t ref_ms = 0;
  static bool charging = false;
  float v = state.batt_v;
  if (state.batt_pct < 0 || v <= 0) return;
  if (!ref_ms || millis() - ref_ms > 120000) {
    if (ref_ms) charging = v > v_ref + 0.01f || v >= 4.16f;
    v_ref = v;
    ref_ms = millis();
  }
  int pct = constrain(state.batt_pct, 0, 100);
  int x = 412, y = 122;
  uint16_t ink = pct <= 15 && !charging ? HOT : DIM;
  frame(x, y, 34, 18, ink);
  rect(x + 34, y + 5, 3, 8, ink);
  rect(x + 3, y + 3, 28 * pct / 100, 12, ink);
  if (charging) {
    static const char *const BOLT[] = { "..##", ".##.", "####", ".##.", "##.." };
    sprite(BOLT, 5, x + 42, y + 4, 2, HOT);
  }
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", pct);
  text(buf, x, y + 26, 2, DIM_INK);
}

// Seconds as one magenta dot walking clockwise around the screen edge: 60 stops from the top middle, faint marks at
// the empty ones.
static void drawSecondDot(int second)
{
  const int x0 = 4, y0 = 4, x1 = W - 5, y1 = H - 5;
  const float w = x1 - x0, h = y1 - y0, per = 2 * (w + h);
  auto at = [&](int i, int &x, int &y) {
    float d = fmodf(w / 2 + i * per / 60, per);  // start at the top middle
    if (d < w) x = x0 + d, y = y0;
    else if ((d -= w) < h) x = x1, y = y0 + d;
    else if ((d -= h) < w) x = x1 - d, y = y1;
    else x = x0, y = y1 - (d - w);
  };
  for (int i = 0; i < 60; i++) {
    int x, y;
    at(i, x, y);
    rect(x - 1, y - 1, i % 5 ? 2 : 3, i % 5 ? 2 : 3, i % 15 ? GHOST : DIM);
  }
  int x, y;
  at(second, x, y);
  rect(x - 6, y - 6, 12, 12, HOTH);
  rect(x - 4, y - 4, 8, 8, HOT);
}

static void drawClock(const DateTime &now)
{
  clear();
  String hm = hhmm(now);
  seg(hm.c_str(), 20, 24, 78, 124, 12, 12, { LIT, HALO, GHOST });
  drawSecondDot(now.second);
  bool alarm_lit = alarm_on && (!ringing || blinkOn(300));
  text("AL", 412, 26, 2, alarm_on ? HOT_INK : GHOST_INK);
  if (settings::h24()) text("24H", 412, 48, 2, GHOST_INK);
  else text(now.hour < 12 ? "AM" : "PM", 412, 48, 2, DIM_INK);
  text(weekdayName(weekday(now.year, now.month, now.day)), 492, 26, 3, 2, LIT_INK);
  String date = String(now.day) + " " + monthName(now.month);
  text(date.c_str(), 492, 54, 2, DIM_INK);
  rect(492, 80, 132, 1, GHOST);

  static const char *const BELL[] = { "...#...", "..###..", ".#####.", ".#####.", ".#####.", "#######", "...#..." };
  if (alarm_lit) {
    sprite(BELL, 7, 491, 93, 2, HOTH);
    sprite(BELL, 7, 493, 95, 2, HOTH);
    sprite(BELL, 7, 492, 94, 2, HOT);
  } else {
    sprite(BELL, 7, 492, 94, 2, GHOST);
  }
  text(ringing ? L("WAKE UP!", "UYAN!") : "ALARM", 512, 96, 2, alarm_lit ? HOT_INK : GHOST_INK);
  String at = two(alarm_h) + ":" + two(alarm_m);
  seg(at.c_str(), 492, 120, 24, 40, 5, 5, alarm_on ? HOT_INK : Ink{ DIM });
  drawBatteryBadge(now);
}

static void drawVolumeBar(int x, int y)
{
  int v = Radio_Volume();
  int end = text(L("VOL", "SES"), x, y, 2, LIT_INK) + 10;
  for (int i = 0; i < 20; i++) {
    bool on = v >= (i + 1) * 5;
    if (on) rect(end + i * 12 - 1, y - 1, 10, 16, HALO);
    rect(end + i * 12, y, 8, 14, on ? LIT : GHOST);
  }
}

static void drawRadio(const DateTime &now)
{
  clear();
  int st = constrain(Radio_Station(), 0, settings::stationCount() - 1);
  const RadioStation &s = settings::station(st);
  RadioState rs = Radio_State();
  String gname = settings::group(s.group).name;
  gname.toUpperCase();

  text(("CH " + two(st + 1)).c_str(), 16, 10, 2, DIM_INK);
  text(gname.c_str(), 96, 10, 2, LIT_INK);
  text("STEREO", 440, 10, 2, rs == RadioState::Playing ? HOT_INK : GHOST_INK);
  text(hhmm(now).c_str(), 568, 10, 2, DIM_INK);

  char name[17];
  snprintf(name, sizeof(name), "%-16s", s.name.c_str());
  text(name, 16, 36, 4, 3, { LIT, HALO, GHOST });

  // line under the name: volume while it changes, else what is playing
  if (volume_shown_ms && millis() - volume_shown_ms < VOLUME_SHOW_MS) {
    drawVolumeBar(16, 78);
  } else {
    String line;
    Ink ink = DIM_INK;
    switch (rs) {
      case RadioState::Off: line = L("STOPPED · TAP TO PLAY", "DURDU · ÇALMAK İÇİN DOKUN"); break;
      case RadioState::Connecting: line = String(L("CONNECTING", "BAĞLANIYOR")) + String("...").substring(0, (millis() / 400) % 4); break;
      case RadioState::Failed:
        line = L("NO SIGNAL · TAP TO RETRY", "BAĞLANAMADI · TEKRAR DOKUN");
        ink = HOT_INK;
        break;
      case RadioState::Playing:
        line = Radio_Title();
        if (!line.length()) line = L("LIVE", "CANLI YAYIN");
        break;
    }
    int w = textWidth(line.c_str(), 2), box = 412 - 16;
    clip(16, 70, 412, 100);
    if (w <= box) {
      text(line.c_str(), 16, 80, 2, ink);
    } else {
      int span = w + 60, off = (millis() / 30) % span;
      text(line.c_str(), 16 - off, 80, 2, ink);
      text(line.c_str(), 16 - off + span, 80, 2, ink);
    }
    noClip();
  }

  // tuning dial with the needle on this station
  for (int x = 16; x <= 400; x += 8) {
    bool major = (x - 16) % 64 == 0;
    rect(x, major ? 106 : 110, major ? 2 : 1, major ? 10 : 6, major ? DIM : GHOST);
  }
  int nx = 16 + st * 384 / max(settings::stationCount() - 1, 1);
  rect(nx - 3, 100, 6, 22, HOTH);
  rect(nx - 1, 102, 2, 18, HOT);

  // spectrum analyzer
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
    int x = 426 + i * 14;
    for (int r = 0; r < 10; r++) {
      int y = 115 - r * 9;
      bool on = r < spec_level[i], peak = spec_peak[i] && r == spec_peak[i] - 1 && !on, hot = r >= 8;
      if (on || peak) rect(x - 1, y - 1, 13, 8, hot || peak ? HOTH : HALO);
      rect(x, y, 11, 6, peak || (on && hot) ? HOT : on ? LIT : GHOST);
    }
  }

  // genre presets
  for (int i = 0; i < settings::groupCount(); i++) {
    int x = 16 + i * 87;
    bool cur = i == s.group;
    frame(x, 136, 83, 28, cur ? DIM : GHOST);
    String key = settings::group(i).key;
    key.toUpperCase();
    const char *n = key.c_str();
    text(n, x + 41 - textWidth(n, 2) / 2, 143, 2, cur ? LIT_INK : GHOST_INK);
  }
}

// The sky over the desk, one 100x34 dot matrix (4 px pitch) right of the readings: the weather itself fills it, a
// striped synthwave sun, a crescent with stars, drifting clouds in two layers, rain, snow, fog bands, lightning.

static constexpr int SKY_X = 232, SKY_C = 100, SKY_R = 34;
static uint16_t sky[SKY_R][SKY_C];

static uint32_t hash32(uint32_t v)
{
  v ^= v >> 16;
  v *= 0x7feb352d;
  v ^= v >> 15;
  v *= 0x846ca68b;
  return v ^ (v >> 16);
}

static void put(int c, int r, uint16_t col)
{
  if (c >= 0 && c < SKY_C && r >= 0 && r < SKY_R) sky[r][c] = col;
}

static float wrap(float v, float span)
{
  v = fmodf(v, span);
  return v < 0 ? v + span : v;
}

// Puffy cloud shape inside a w x h box: four overlapping puffs on a flat base.
static bool inCloud(float lx, float ly, float w, float h)
{
  if (ly < 0 || ly > h - 1 || lx < 0 || lx > w) return false;
  auto in = [&](float cx, float cy, float r) { return (lx - cx) * (lx - cx) + (ly - cy) * (ly - cy) <= r * r; };
  return in(0.20f * w, 0.70f * h, 0.30f * h) || in(0.42f * w, 0.45f * h, 0.45f * h) ||
         in(0.66f * w, 0.52f * h, 0.40f * h) || in(0.84f * w, 0.72f * h, 0.28f * h) ||
         (lx >= 0.20f * w && lx <= 0.84f * w && ly >= 0.70f * h);
}

// Filled cloud with a lit rim; it hides whatever was stamped behind it.
static void stampCloud(float x, int y, int w, int h, uint16_t fill, uint16_t edge)
{
  for (int r = max(0, y); r < min(SKY_R, y + h); r++) {
    for (int c = max(0, (int)floorf(x)); c < min(SKY_C, (int)ceilf(x + w) + 1); c++) {
      float lx = c - x, ly = r - y;
      if (!inCloud(lx, ly, w, h)) continue;
      bool rim = !inCloud(lx - 1, ly, w, h) || !inCloud(lx + 1, ly, w, h) || !inCloud(lx, ly - 1, w, h) ||
                 !inCloud(lx, ly + 1, w, h);
      sky[r][c] = rim ? edge : fill;
    }
  }
}

// A row of clouds wrapping around the sky: n clouds of w x h at row y, drifting right at speed cells per second.
static void cloudRow(uint32_t t, int n, int w, int h, int y, float speed, int seed, uint16_t fill, uint16_t edge)
{
  float span = SKY_C + w + 6;
  for (int i = 0; i < n; i++) {
    float x = wrap(i * span / n + seed + t * speed / 1000.0f, span) - w - 3;
    int dy = (int)(hash32(seed * 7 + i) % 5) - 2;
    stampCloud(x, y + dy, w, h, fill, edge);
  }
}

// A low, closed cloud deck for rain and snow: two layers, the front one lit.
static void overcast(uint32_t t, bool flash)
{
  cloudRow(t, 4, 40, 16, -6, 0.5f, 11, GHOST, DIM);
  cloudRow(t, 3, 48, 18, -2, 0.9f, 40, flash ? DIM : HALO, flash ? HOT : LIT);
}

static void sun(int cx, int cy, int rad, uint32_t t)
{
  for (int k = 0; k < 12; k++) {  // rays, turning slowly
    float a = k * PI / 6 + t * 0.00012f;
    int len = k % 2 ? 2 : 4;
    for (int d = rad + 3; d < rad + 3 + len; d++) put(cx + lroundf(cosf(a) * d), cy + lroundf(sinf(a) * d), DIM);
  }
  for (int r = -rad; r <= rad; r++) {
    // synthwave stripes, wider towards the bottom
    if (r > rad / 4 && ((r - rad / 4) % 3 == 1 || (r > rad / 2 && (r - rad / 4) % 3 == 2))) continue;
    for (int c = -rad; c <= rad; c++) {
      if (c * c + r * r <= rad * rad + rad) put(cx + c, cy + r, HOT);
    }
  }
}

static void stars(uint32_t t, int n)
{
  for (int i = 0; i < n; i++) {
    uint32_t h = hash32(i + 1);
    int c = h % SKY_C, r = (h >> 8) % SKY_R;
    uint32_t tw = (t / 400 + (h >> 16)) % 9;
    if (tw < 7) put(c, r, tw == 0 ? LIT : (i % 3 ? GHOST : DIM));
  }
  uint32_t ph = t % 11000;  // a shooting star every 11 s
  if (ph < 700) {
    uint32_t h = hash32(t / 11000);
    float p = ph / 700.0f;
    int c0 = 10 + h % 50, r0 = 2 + (h >> 8) % 6;
    for (int k = 0; k < 6; k++) {
      float q = p - k * 0.04f;
      if (q < 0) break;
      put(c0 + lroundf(q * 40), r0 + lroundf(q * 14), k == 0 ? HOT : DIM);
    }
  }
}

static void moon(int cx, int cy, int rad)
{
  for (int r = -rad; r <= rad; r++) {
    for (int c = -rad; c <= rad; c++) {
      if (c * c + r * r > rad * rad + rad) continue;
      int bc = c - rad / 2 - 1, br = r + rad / 3;  // the bite that makes it a crescent
      put(cx + c, cy + r, bc * bc + br * br <= rad * rad ? GHOST : LIT);
    }
  }
}

static void rain(uint32_t t, int n, float rows_per_s, float slant, bool streak)
{
  for (int i = 0; i < n; i++) {
    uint32_t h = hash32(i * 13 + 5);
    float y = 8 + wrap((h >> 8) % 26 + t * rows_per_s / 1000.0f, 27);
    float x = (h % (SKY_C + 12)) - (y - 8) * slant;
    int c = lroundf(x), r = (int)y;
    put(c, r, LIT);
    if (streak) put(lroundf(x + slant), r - 1, DIM);
  }
}

static void snow(uint32_t t)
{
  for (int i = 0; i < 42; i++) {
    uint32_t h = hash32(i * 29 + 3);
    float y = 9 + wrap((h >> 8) % 25 + t * 3.2f / 1000.0f * (1 + (h >> 20) % 3 * 0.3f), 26);
    int c = lroundf((h % SKY_C) + sinf(t * 0.0012f + i) * 1.6f), r = (int)y;
    if (i % 3 == 0) {  // a big flake
      put(c, r, LIT);
      put(c - 1, r, DIM);
      put(c + 1, r, DIM);
      put(c, r - 1, DIM);
      put(c, r + 1, DIM);
    } else {
      put(c, r, i % 2 ? LIT : DIM);
    }
  }
}

// A forked bolt every 4.6 s; true during its first flash.
static bool bolt(uint32_t t)
{
  uint32_t ph = t % 4600;
  if (ph > 300) return false;
  uint32_t seed = t / 4600;
  int c = 14 + hash32(seed) % 76;
  for (int r = 12; r < SKY_R; r++) {
    c += (int)(hash32(seed * 31 + r) % 3) - 1;
    put(c, r, HOT);
    if (r == 20) {  // a short fork
      for (int k = 1; k < 6; k++) put(c + k, r + k, HOT);
    }
  }
  return ph < 90;
}

static void fog(uint32_t t)
{
  for (int k = 0; k < 8; k++) {
    int r = 2 + k * 4 + (k % 2);
    float off = t * (0.8f + k * 0.25f) / 1000.0f * (k % 2 ? 1 : -1);
    int len = 9 + (k * 5) % 8;
    for (int c = 0; c < SKY_C; c++) {
      if (wrap(c + off + k * 7, 24) < len) put(c, r, k % 2 ? DIM : HALO);
    }
  }
}

static void drawSky(Sky s, bool day)
{
  uint32_t t = millis();
  memset(sky, 0, sizeof(sky));
  switch (s) {
    case Sky::Clear:
      if (day) {
        sun(50, 17, 10, t);
      } else {
        stars(t, 60);
        moon(66, 13, 9);
      }
      break;
    case Sky::PartCloud:
      if (day) {
        sun(34, 15, 9, t);
      } else {
        stars(t, 40);
        moon(34, 12, 8);
      }
      cloudRow(t, 2, 40, 16, 13, 1.1f, 30, HALO, LIT);
      break;
    case Sky::Cloud:
      cloudRow(t, 4, 40, 16, 0, 0.5f, 11, GHOST, DIM);
      cloudRow(t, 3, 48, 18, 9, 0.9f, 40, HALO, LIT);
      cloudRow(t, 2, 44, 17, 18, 1.4f, 77, HALO, LIT);
      break;
    case Sky::Fog: fog(t); break;
    case Sky::Drizzle:
      rain(t, 28, 9, 0.15f, false);
      overcast(t, false);
      break;
    case Sky::Rain:
      rain(t, 55, 16, 0.3f, true);
      overcast(t, false);
      break;
    case Sky::Shower:
      rain(t, 90, 22, 0.5f, true);
      overcast(t, false);
      break;
    case Sky::Snow:
      snow(t);
      overcast(t, false);
      break;
    case Sky::Storm: {
      rain(t, 80, 22, 0.5f, true);
      bool flash = bolt(t);
      overcast(t, flash);
      break;
    }
    default: stars(t, 20); break;
  }

  // matrix: unlit dots faint, lit ones with their glow
  for (int r = 0; r < SKY_R; r++) {
    for (int c = 0; c < SKY_C; c++) {
      uint16_t col = sky[r][c];
      if (col == LIT || col == HOT) rect(SKY_X + c * 4 - 1, r * 4 - 1, 5, 5, col == HOT ? HOTH : HALO);
    }
  }
  for (int r = 0; r < SKY_R; r++) {
    for (int c = 0; c < SKY_C; c++) {
      uint16_t col = sky[r][c];
      rect(SKY_X + c * 4, r * 4, 3, 3, col ? col : FAINT);
    }
  }
}

// Small icons for the next days, 9 dots wide.
static void skyIcon(Sky s, int x, int y)
{
  static const char *const SUN[] = { ".#..#..#.", "..#####..", "#.#####.#", "..#####..", ".#..#..#." };
  static const char *const PART[] = { "..#......", "#.###....", ".####.##.", "#.##....#", "..#######" };
  static const char *const CLOUD[] = { "...###...", ".##...##.", "#.......#", "#########", "........." };
  static const char *const FOG[] = { "#########", ".........", ".#######.", ".........", "#########" };
  static const char *const RAIN[] = { "...###...", ".##...##.", "#########", ".#..#..#.", "#..#..#.." };
  static const char *const SNOW[] = { "...###...", ".##...##.", "#########", ".#..#..#.", "...#..#.." };
  static const char *const STORM[] = { "...###...", ".##...##.", "#########", "....##...", "...##...." };
  const char *const *rows = CLOUD;
  uint16_t col = LIT;
  switch (s) {
    case Sky::Clear: rows = SUN; col = HOT; break;
    case Sky::PartCloud: rows = PART; break;
    case Sky::Fog: rows = FOG; break;
    case Sky::Drizzle:
    case Sky::Rain:
    case Sky::Shower: rows = RAIN; break;
    case Sky::Snow: rows = SNOW; break;
    case Sky::Storm: rows = STORM; break;
    default: break;
  }
  sprite(rows, 5, x, y, 2, col);
}

static int sky_force = -1;   // /cmd sky: preview a sky (Sky index), -1 = the real one
static bool sky_night = false;

static void drawWeather()
{
  clear();
  WeatherNow w = Weather_Get();

  // temperature: two 7-segment cells, right aligned, the empty one ghosted
  char tbuf[6];
  if (w.valid) {
    snprintf(tbuf, sizeof(tbuf), "%2d", w.temp);
  } else {
    strcpy(tbuf, "--");
  }
  int end = seg(tbuf, 20, 22, 56, 92, 10, 10, { w.valid ? LIT : DIM, w.valid ? HALO : NONE, GHOST });
  int dx = end - 10 + 8;
  rect(dx, 20, 18, 18, HALO);
  rect(dx + 2, 22, 14, 14, LIT);
  rect(dx + 6, 26, 6, 6, BG);

  Sky s = sky_force >= 0 ? (Sky)sky_force : w.valid ? w.sky : Sky::Unknown;
  bool day = sky_force >= 0 ? !sky_night : w.day;
  const char *cond = s != Sky::Unknown ? Weather_SkyName(s) : L("WAIT", "BEKLE");
  String hum = settings::tr() ? "%" + String(w.humidity) : String(w.humidity) + "%";
  if (glyphCount(cond) <= 6) {
    // short sky name in big letters, humidity and wind beside it
    text(cond, 20, 132, 3, 2, LIT_INK);
    if (w.valid) {
      text((String(L("HUM ", "NEM ")) + hum).c_str(), 140, 126, 2, DIM_INK);
      text((String(L("WIND ", "RÜZGAR ")) + String(w.wind)).c_str(), 140, 150, 2, DIM_INK);
    }
  } else {
    // long name ("PARTLY CLOUDY") takes the line; humidity and wind go under it
    text(cond, 20, 126, 2, LIT_INK);
    if (w.valid) text((hum + "  " + String(w.wind) + (settings::fahrenheit() ? " MPH" : L(" KM/H", " KM/SA"))).c_str(), 20, 150, 2, DIM_INK);
  }

  drawSky(s, day);

  for (int i = 0; i < 3; i++) {
    const WeatherDay &d = w.next[i];
    if (d.weekday < 0) continue;
    int x = 268 + i * 122;
    skyIcon(d.sky, x, 149);
    text(weekdayName(d.weekday), x + 26, 148, 2, DIM_INK);
    text((String(d.t_max) + "°").c_str(), x + 72, 148, 2, LIT_INK);
  }
}

static const int8_t COUNT_STEPS[] = { -1, 1, 5, 10 };
static const char *const COUNT_LABELS_TR[] = { "-1 DK", "+1 DK", "+5 DK", "+10 DK", "SIFIR" };
static const char *const COUNT_LABELS_EN[] = { "-1 MIN", "+1 MIN", "+5 MIN", "+10 MIN", "RESET" };

static void drawTimer()
{
  clear();
  bool running = phase == Phase::Running, done = phase == Phase::Done;

  int32_t left_s = (eggLeftMs() + 999) / 1000;
  bool show = phase != Phase::Paused || blinkOn(400);
  Ink big = done ? Ink{ HOT, HOTH, GHOST } : Ink{ LIT, HALO, GHOST };
  if (!show || (done && !blinkOn(300))) big = Ink{ GHOST, NONE, GHOST };
  String digits = mss(left_s);
  if (digits.length() < 5) digits = " " + digits;
  seg(digits.c_str(), 20, 12, 70, 100, 12, 12, big);

  text(L("TIMER", "SAYAÇ"), 470, 16, 3, 2, LIT_INK);
  text((String(L("SET ", "AYAR ")) + mss(egg_set_s)).c_str(), 470, 50, 2, DIM_INK);
  const char *l1 = egg_set_s ? L("TAP: START", "DOKUN: BAŞLA") : L("PICK A TIME", "SÜRE SEÇ");
  Ink ink = GHOST_INK;
  if (running) {
    l1 = L("RUNNING", "SAYIYOR");
    ink = LIT_INK;
  } else if (phase == Phase::Paused) {
    l1 = L("PAUSED", "DURDU");
    ink = DIM_INK;
  } else if (done) {
    l1 = L("TIME'S UP!", "SÜRE DOLDU!");
    ink = HOT_INK;
  }
  text(l1, 470, 84, 2, ink);

  // progress
  int32_t total = max<int32_t>(1, egg_set_s * 1000);
  int w = 604, fill = phase == Phase::Idle ? 0 : done ? w : constrain((int)(w * (1 - (float)eggLeftMs() / total)), 0, w);
  rect(20, 122, w, 4, GHOST);
  if (fill > 0) {
    rect(19, 121, fill + 2, 6, HOTH);
    rect(20, 122, fill, 4, HOT);
  }

  for (int i = 0; i < 5; i++) {
    int x = 16 + i * 122;
    const char *n = (settings::tr() ? COUNT_LABELS_TR : COUNT_LABELS_EN)[i];
    frame(x, 136, 116, 28, i == 4 ? GHOST : DIM);
    text(n, x + 58 - textWidth(n, 2) / 2, 143, 2, i == 4 ? DIM_INK : LIT_INK);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Interface
// ---------------------------------------------------------------------------------------------------------------------

void Gadget_Begin(uint16_t *canvas, I2cMasterBus *bus, void (*amp)(bool))
{
  vfd::begin(canvas);
  settings::begin();
  prefs.begin("gadget", false);
  px::begin();
  int old_theme = prefs.isKey("theme") ? prefs.getUChar("theme", 2) : -1;  // pre-1.1 scene colors, migrated once
  theme::begin(old_theme);
  if (old_theme >= 0) prefs.remove("theme");
  alarm_on = prefs.getBool("al_on", false);
  alarm_h = prefs.getUChar("al_h", 7);
  alarm_m = prefs.getUChar("al_m", 30);
  egg_set_s = constrain(prefs.getUShort("egg", 600), 0, 99 * 60);
  Radio_Begin(bus, amp);
  Radio_SetVolume(prefs.getUChar("volume", 30));
  int st = prefs.getUChar("st2", 6);
  if (st < settings::stationCount()) {
    Radio_Play(st);  // selects it; stop right away, the radio starts silent
    Radio_Stop();
  }
  Weather_Begin();
  Motion_Begin(bus);
}

// Pages the auto page setting turns through: the ones that only show something.
static bool ambient(Mode m)
{
  return m == Mode::Clock || m == Mode::Weather || m == Mode::Aquarium || m == Mode::Snow || m == Mode::Fire ||
         m == Mode::Sea || m == Mode::Wave || m == Mode::Lava;
}

static uint32_t last_touch_ms = 0, last_cycle_ms = 0;

static void autoPage()
{
  int minutes = SettingsUi_CycleMin();
  if (!minutes || ringing || !ambient(state.mode)) return;
  uint32_t per = minutes * 60000UL;
  if (millis() - last_touch_ms < per || millis() - last_cycle_ms < per) return;
  last_cycle_ms = millis();
  int idx = 0;
  for (int i = 0; i < PAGE_COUNT; i++) {
    if (PAGES[i] == state.mode) idx = i;
  }
  for (int k = 1; k < PAGE_COUNT; k++) {
    Mode m = PAGES[(idx + k) % PAGE_COUNT];
    if (ambient(m)) {
      state.mode = m;
      state.dirty = true;
      return;
    }
  }
}

void Gadget_Tick(const DateTime &now)
{
  settings::tick();
  SettingsUi_Tick();
  autoPage();
  int day = now.year * 400 + now.month * 32 + now.day;
  if (alarm_on && !ringing && now.hour == alarm_h && now.minute == alarm_m && fired_day != day) {
    fired_day = day;
    startAlarm();
  }
  if (ringing) {
    uint32_t t = millis() - ring_start_ms;
    if (t > ALARM_MAX_MS) {
      stopAlarm();
    } else if ((!radio_by_alarm || Radio_State() != RadioState::Playing) && t > ALARM_RADIO_GRACE_MS &&
               millis() - ring_beep_ms > 2000) {
      ring_beep_ms = millis();  // no stream: plain beeps
      Radio_Beep(1046, 140, 3, 90);
    }
  }
  timerTick();
  Study_Tick();
  Motion_Update();
}

bool Gadget_Draws(Mode m)
{
  return m == Mode::Clock || m == Mode::Radio || m == Mode::Weather || m == Mode::Aquarium || m == Mode::Snow || m == Mode::Timer ||
         m == Mode::Fire || m == Mode::Sea || m == Mode::Study || m == Mode::Wave || m == Mode::Lava ||
         m == Mode::Hourglass || m == Mode::Settings;
}

void Gadget_Render(const DateTime &now)
{
  switch (state.mode) {
    case Mode::Clock: drawClock(now); break;
    case Mode::Radio: drawRadio(now); break;
    case Mode::Weather: drawWeather(); break;
    case Mode::Timer: drawTimer(); break;
    case Mode::Aquarium: {
      WeatherNow w = Weather_Get();
      String corner = hhmm(now);
      if (w.valid) corner += "  " + String(w.temp) + "°";
      Fishtank_Draw(corner.c_str());
      break;
    }
    case Mode::Snow: Snowglobe_Draw(hhmm(now).c_str()); break;
    case Mode::Fire: Fireplace_Draw(hhmm(now).c_str()); break;
    case Mode::Sea: Seaside_Draw(hhmm(now).c_str()); break;
    case Mode::Study: Study_Draw(hhmm(now).c_str()); break;
    case Mode::Wave: Wave_Draw(hhmm(now).c_str()); break;
    case Mode::Lava: Lava_Draw(""); break;
    case Mode::Hourglass: Hourglass_Draw(""); break;
    case Mode::Settings: SettingsUi_Draw(); break;
    default: break;
  }
  // volume bar over the other pages while it changes (the radio page has it in its title line)
  if (state.mode != Mode::Radio && volume_shown_ms && millis() - volume_shown_ms < VOLUME_SHOW_MS) {
    rect(140, 62, 360, 46, BG);
    frame(140, 62, 360, 46, DIM);
    drawVolumeBar(158, 78);
  }
  theme::decorate();
}

uint32_t Gadget_FrameMs(Mode m)
{
  if (volume_shown_ms && millis() - volume_shown_ms < VOLUME_SHOW_MS + 100) return 100;
  switch (m) {
    case Mode::Clock: return ringing ? 150 : 500;
    case Mode::Radio: return 50;
    case Mode::Weather: return 60;
    case Mode::Aquarium: return 50;
    case Mode::Snow:
    case Mode::Fire:
    case Mode::Sea:
    case Mode::Study:
    case Mode::Wave:
    case Mode::Lava:
    case Mode::Hourglass: return 50;
    case Mode::Timer: return phase == Phase::Running || phase == Phase::Done ? 60 : 400;
    case Mode::Settings: return SettingsUi_Busy() ? 150 : 1000;
    default: return 0;
  }
}

static void goPage(int dir)
{
  int idx = 0;
  for (int i = 0; i < PAGE_COUNT; i++) {
    if (PAGES[i] == state.mode) idx = i;
  }
  idx = (idx + dir + PAGE_COUNT) % PAGE_COUNT;
  state.mode = PAGES[idx];
  state.mode_until_ms = 0;
  state.dirty = true;
}

static void changeVolume(int delta)
{
  Radio_SetVolume(Radio_Volume() + delta);
  prefs.putUChar("volume", Radio_Volume());
  volume_shown_ms = millis();
  if (!Radio_On()) Radio_Beep(880, 60);  // something to hear the new level by
  state.dirty = true;
}

static void tapClock(int x, int y)
{
  if (x < 480) return;
  if (y >= 88 && y < 116) {
    alarm_on = !alarm_on;
  } else if (y >= 116 && x < 553) {
    alarm_h = (alarm_h + 1) % 24;
    alarm_on = true;
  } else if (y >= 116) {
    alarm_m = (alarm_m + 5) % 60;
    alarm_on = true;
  } else {
    return;
  }
  savePrefs();
  Radio_Beep(alarm_on ? 1046 : 523, 60);
}

static void tapRadio(int x, int y)
{
  if (y >= 130) {
    int n = settings::stationCount();
    int g = (x - 16) / 87;
    if (g >= settings::groupCount()) return;
    int cur = constrain(Radio_Station(), 0, n - 1);
    int pick = -1;
    if (settings::station(cur).group == g && Radio_On()) {  // same genre again: next station in it
      for (int i = 1; i <= n && pick < 0; i++) {
        int k = (cur + i) % n;
        if (settings::station(k).group == g) pick = k;
      }
    } else {
      for (int k = 0; k < n && pick < 0; k++) {
        if (settings::station(k).group == g) pick = k;
      }
    }
    if (pick >= 0) Radio_Play(pick);
  } else {
    Radio_Toggle();
  }
  prefs.putUChar("st2", Radio_Station());
}

static void tapTimer(int x, int y)
{
  if (y >= 130) {
    int i = constrain((x - 16) / 122, 0, 4);
    if (i == 4) {
      eggSet(0);
    } else {
      eggAddMinutes(COUNT_STEPS[i]);
    }
    Radio_Beep(1046, 30);
  } else {
    timerStartPause();
  }
}

void Gadget_Gesture(Gesture g, int x, int y)
{
  last_touch_ms = millis();
  if (ringing) {
    stopAlarm();
    return;
  }
  if (phase == Phase::Done) {  // any touch silences the egg timer
    phase = Phase::Idle;
    state.dirty = true;
    return;
  }
  switch (g) {
    case Gesture::SwipeLeft: goPage(1); return;
    case Gesture::SwipeRight: goPage(-1); return;
    case Gesture::SwipeUp:  // on the settings page: walk the list (finger up = further down it)
      if (state.mode == Mode::Settings) {
        SettingsUi_Step(1);
        break;
      }
      changeVolume(Radio_Volume() < 20 ? 5 : 10);
      return;
    case Gesture::SwipeDown:
      if (state.mode == Mode::Settings) {
        SettingsUi_Step(-1);
        break;
      }
      changeVolume(Radio_Volume() <= 20 ? -5 : -10);
      return;
    case Gesture::LongPress:
      if (state.mode == Mode::Clock) {
        alarm_on = !alarm_on;
        savePrefs();
        Radio_Beep(alarm_on ? 1046 : 523, 60);
      } else if (state.mode == Mode::Timer) {
        timerReset();
      } else if (state.mode == Mode::Study) {
        Study_Reset();
      } else if (state.mode == Mode::Hourglass) {
        Hourglass_Reset();
      }
      break;
    case Gesture::Tap:
      switch (state.mode) {
        case Mode::Clock: tapClock(x, y); break;
        case Mode::Radio: tapRadio(x, y); break;
        case Mode::Aquarium: Fishtank_Feed(x); break;
        case Mode::Snow: Snowglobe_Stir(0.6f); break;
        case Mode::Fire: Fireplace_Tap(); break;
        case Mode::Sea: Seaside_Tap(); break;
        case Mode::Study: Study_Tap(); break;
        case Mode::Wave: Wave_Tap(x); break;
        case Mode::Lava: Lava_Tap(); break;
        case Mode::Hourglass: Hourglass_Tap(); break;
        case Mode::Timer: tapTimer(x, y); break;
        case Mode::Settings: SettingsUi_Tap(x, y); break;
        default:
          if (!Gadget_Draws(state.mode)) {  // message, info, image: back home
            state.mode = Mode::Clock;
          }
          break;
      }
      break;
  }
  state.dirty = true;
}

String Gadget_Command(const String &name, const String &arg)
{
  if (name == "radiotest") {  // settings page: play a stream before adding it
    if (arg.startsWith("http")) Radio_PlayUrl(arg.c_str());
    else Radio_Stop();
    state.dirty = true;
    return "ok";
  }
  if (name == "radio") {
    if (arg == "stop") {
      Radio_Stop();
    } else if (arg == "toggle") {
      Radio_Toggle();
    } else if (arg == "next") {
      Radio_Play((Radio_Station() + 1) % settings::stationCount());
    } else if (arg.length()) {
      Radio_Play(constrain(arg.toInt(), 0, settings::stationCount() - 1));
    }
    prefs.putUChar("st2", Radio_Station());
  } else if (name == "vol") {
    if (arg.length()) {
      Radio_SetVolume(arg.toInt());
      prefs.putUChar("volume", Radio_Volume());
      volume_shown_ms = millis();
    }
  } else if (name == "alarm") {
    if (arg == "on" || arg == "off") {
      alarm_on = arg == "on";
    } else if (arg.indexOf(':') > 0) {
      alarm_h = constrain(arg.toInt(), 0, 23);
      alarm_m = constrain(arg.substring(arg.indexOf(':') + 1).toInt(), 0, 59);
      alarm_on = true;
    } else if (arg == "ring") {
      startAlarm();
    } else if (arg == "stop") {
      stopAlarm();
    }
    savePrefs();
  } else if (name == "timer") {
    if (arg == "start") {
      timerStartPause();
    } else if (arg == "reset") {
      timerReset();
    } else if (arg.startsWith("+") || arg.startsWith("-")) {
      eggAddMinutes(arg.toInt());  // minutes
    } else if (arg.length()) {
      eggSet(arg.toInt());  // seconds
    }
  } else if (name == "theme") {
    // the whole look: vfd | cga | ega | c64 | amiga (or 0..4), empty = next; "natural" / "themed" = scene colors
    if (arg == "natural" || arg == "themed") {
      theme::setNaturalScenes(arg == "natural");
    } else {
      theme::Id t = arg.length() ? theme::fromName(arg) : (theme::Id)((theme::current() + 1) % theme::COUNT);
      if (t < theme::COUNT) theme::set(t);
    }
  } else if (name == "menu") {
    // settings page at item N (help page after a dot: "8.3"), for screenshots
    SettingsUi_Select(arg.toInt(), arg.indexOf('.') > 0 ? arg.substring(arg.indexOf('.') + 1).toInt() : 0);
    state.mode = Mode::Settings;
  } else if (name == "sky") {
    // preview: Sky index (1 clear .. 9 storm), "n" after it for night; empty or -1 = the real sky
    sky_force = arg.length() ? constrain(arg.toInt(), -1, 9) : -1;
    sky_night = arg.endsWith("n");
  } else if (name != "state") {
    return String();
  }
  state.dirty = true;
  static const char *const RS[] = { "off", "connecting", "playing", "failed" };
  String title = Radio_Title();
  title.replace("\\", "");
  title.replace("\"", "'");
  float ax, ay, az;
  Motion_Raw(ax, ay, az);
  char json[560];
  snprintf(json, sizeof(json),
           "{\"theme\":\"%s\",\"natural_scenes\":%s,\"radio\":\"%s\",\"station\":%d,\"station_name\":\"%s\",\"title\":\"%s\",\"volume\":%d,"
           "\"alarm\":\"%02d:%02d\",\"alarm_on\":%s,\"ringing\":%s,\"timer\":\"%s\",\"timer_state\":%d,\"accel\":[%.2f,%.2f,%.2f]}",
           theme::def().key, theme::naturalScenes() ? "true" : "false", RS[(int)Radio_State()], Radio_Station(), settings::station(Radio_Station()).name.c_str(), title.c_str(), Radio_Volume(), alarm_h,
           alarm_m, alarm_on ? "true" : "false", ringing ? "true" : "false", mss((eggLeftMs() + 999) / 1000).c_str(), (int)phase, ax, ay, az);
  return json;
}

bool Gadget_ButtonPressed()
{
  if (phase == Phase::Done) {
    phase = Phase::Idle;
    state.dirty = true;
    return true;
  }
  if (!ringing) return false;
  stopAlarm();
  return true;
}

#endif
