// Phosphor: firmware for the Waveshare ESP32-S3-Touch-LCD-3.49 desk gadget. The core (Wi-Fi portal, HTTP API, OTA) is
// shared code; the hardware is one Board subclass (src/boards/touch349). Build with BOARD_TOUCH349 defined (build.sh does).
//
// The bar handles its own touch and back buttons (Touch349Board): swipe left/right = page, swipe up/down = volume,
// PWR = screen off/on (hold 3 s: power off on battery), BOOT = brightness (hold 5 s: forget Wi-Fi), RESET = restart.

#include "src/core/state.h"
#include "src/core/Board.h"
#include "src/core/net.h"
#include "src/core/Audio.h"
#include "src/core/Watchdog.h"
#include <Preferences.h>
#ifndef BOARD_TOUCH349
#error "Phosphor only supports the Waveshare ESP32-S3-Touch-LCD-3.49: build with -DBOARD_TOUCH349 (see build.sh)"
#endif
#include "src/boards/touch349/Touch349Board.h"

Board &board()
{
  static Touch349Board instance;
  return instance;
}

#define SENSOR_INTERVAL_MS 5000
#define LONG_PRESS_MS 5000
#define VOLUME_HOLD_MS 1000
#define DOUBLE_TAP_MS 400

// Seconds between compile and the sketch running on the board (USB upload).
#define UPLOAD_DELAY_S 25

DeviceState state;

static int last_second = -1;
static uint32_t last_sensor_ms = 0;
static uint32_t last_frame_ms = 0;

static const int VOLUME_STEPS[] = { 25, 45, 65, 85, 100 };

static const char *BUILD_MONTHS[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

// Seconds since 2000-01-01, only used to compare the board clock with compile time.
static uint32_t toSeconds(const DateTime &t)
{
  static const int days_before[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
  uint32_t days = (t.year - 2000) * 365 + (t.year - 2000 + 3) / 4 + days_before[t.month - 1] + (t.day - 1);
  if (t.month > 2 && (t.year % 4) == 0) {
    days++;
  }
  return ((days * 24 + t.hour) * 60 + t.minute) * 60 + t.second;
}

// Sets the board clock from the compile timestamp when it is behind it. NTP corrects it once online.
static void syncClockFromBuild()
{
  char mon[4] = { 0 };
  DateTime build = {};
  sscanf(__DATE__, "%3s %d %d", mon, &build.day, &build.year);
  sscanf(__TIME__, "%d:%d:%d", &build.hour, &build.minute, &build.second);
  build.month = 1;
  for (int i = 0; i < 12; i++) {
    if (strcmp(mon, BUILD_MONTHS[i]) == 0) {
      build.month = i + 1;
    }
  }

  DateTime now = board().now();
  bool valid = now.year >= 2024 && now.month >= 1 && now.month <= 12 && now.day >= 1;
  if (valid && toSeconds(now) >= toSeconds(build) + UPLOAD_DELAY_S) {
    return;
  }

  build.second += UPLOAD_DELAY_S;
  build.minute += build.second / 60;
  build.second %= 60;
  build.hour += build.minute / 60;
  build.minute %= 60;
  build.hour %= 24;
  board().setTime(build);
}

static void readSensors()
{
  Sensors s = board().readSensors();
  if (!isnan(s.temp_c)) {
    state.temp_c = s.temp_c;
    state.humi = s.humidity;
  }
  state.batt_v = s.battery_v;
  state.batt_pct = s.battery_pct;
  state.charging = s.charging;
}

static void onShortPress()
{
  state.mode = state.mode == Mode::Clock ? Mode::Info : state.home;
  state.mode_until_ms = 0;
  state.dirty = true;
}

// A short message page that goes back by itself, for button feedback.
static void showBrief(const char *title, const String &text)
{
  state.message_title = title;
  state.message = text;
  state.mode = Mode::Message;
  state.mode_until_ms = millis() + 1500;
  state.dirty = true;
}

// Next volume step, wrapping round to the quietest; a beep at the new level so it can be heard.
static void stepVolume()
{
  int next = VOLUME_STEPS[0];
  for (int v : VOLUME_STEPS) {
    if (v > Audio_Volume()) {
      next = v;
      break;
    }
  }
  Audio_SetVolume(next, true);
  Audio_Beep(880, 150);
  showBrief("Volume", "%" + String(next));
}

static void applyTheme(bool inverted)
{
  state.inverted = inverted;
  board().setInverted(inverted);
}

static void toggleTheme()
{
  applyTheme(!state.inverted);
  Preferences prefs;
  prefs.begin("rlcd", false);
  prefs.putBool("inverted", state.inverted);
  prefs.end();
}

// KEY gestures are told apart on release: a tap waits DOUBLE_TAP_MS for a second one (only on boards with a theme
// to switch, so elsewhere a tap acts at once).
static void handleKey()
{
  static uint32_t down_ms = 0, tap_ms = 0;
  static bool long_done = false;
  bool down = board().keyDown();
  uint32_t now = millis();
  if (down && !down_ms) {
    down_ms = now;
    long_done = false;
  }
  if (down && !long_done && now - down_ms > LONG_PRESS_MS) {
    long_done = true;
    board().showNotice("Resetting Wi-Fi", "Setup network will open");
    delay(1500);
    Net_ResetWifi();
  }
  if (!down && down_ms) {
    uint32_t held = now - down_ms;
    down_ms = 0;
    if (long_done || held <= 30) {
      // Wi-Fi reset already handled, or contact bounce
    } else if (held >= VOLUME_HOLD_MS) {
      tap_ms = 0;
      if (Audio_Ready()) {
        stepVolume();
      }
    } else if (tap_ms) {
      tap_ms = 0;
      toggleTheme();
    } else if (board().canInvert()) {
      tap_ms = now;
    } else {
      onShortPress();
    }
  }
  if (tap_ms && !down && now - tap_ms >= DOUBLE_TAP_MS) {
    tap_ms = 0;
    onShortPress();
  }
}

static void audioInitTask(void *)
{
  Serial.println("audio: starting");
  bool ok = Audio_Begin();
  Serial.printf("audio: %s\n", ok ? "ready" : "unavailable");
  vTaskDelete(nullptr);
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  board().begin();
  Preferences prefs;
  prefs.begin("rlcd", true);
  applyTheme(prefs.getBool("inverted", false));
  prefs.end();
  syncClockFromBuild();
  readSensors();

  board().showNotice("Phosphor", "Connecting to Wi-Fi...");
  Net_Begin(false);
  // Audio starts in its own task: if a codec ever hangs, the screen, HTTP API and OTA keep working.
  xTaskCreatePinnedToCore(audioInitTask, "audio_init", 6144, nullptr, 3, nullptr, 0);
  Serial.printf("firmware %s on %s, name %s, last reset: %s\n", FW_VERSION, board().model(), state.name.c_str(), Watchdog_ResetReason());
  Watchdog_Begin();  // after Wi-Fi setup, which may wait for someone at the portal
}

void loop()
{
  Watchdog_Feed();
  Net_Loop();
  handleKey();

  if (state.mode_until_ms && (int32_t)(millis() - state.mode_until_ms) > 0) {
    state.mode = state.home;
    state.mode_until_ms = 0;
    state.dirty = true;
  }

  if (millis() - last_sensor_ms > SENSOR_INTERVAL_MS) {
    readSensors();
    last_sensor_ms = millis();
  }

  DateTime now = board().now();
  board().tick(now);
  bool ticking = state.mode == Mode::Clock || state.mode == Mode::Info;
  uint32_t frame_ms = board().frameIntervalMs();
  bool animating = frame_ms && millis() - last_frame_ms >= frame_ms;
  if (state.dirty || animating || (ticking && now.second != last_second)) {
    last_second = now.second;
    last_frame_ms = millis();
    state.dirty = false;
    board().render(now);
  }
  delay(5);
}
