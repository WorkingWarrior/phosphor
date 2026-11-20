#pragma once

// Everything the core (Wi-Fi, HTTP API, app loop) needs from a piece of hardware.
// One subclass per board model lives in src/boards/<model>/; firmware.ino picks which one is built.

#include <Arduino.h>
#include "state.h"

struct DateTime {
  int year, month, day, hour, minute, second;
};

struct Sensors {
  float temp_c = NAN;      // NAN when the board has no sensor
  float humidity = NAN;
  float battery_v = 0;
  int battery_pct = -1;    // -1 when unknown
};

class Board {
 public:
  virtual ~Board() = default;

  // Model id of the board, reported in /status.
  virtual const char *model() const = 0;
  virtual void begin() = 0;

  virtual int width() const = 0;
  virtual int height() const = 0;

  // Pushed images (POST /image) use the model's byte format.
  virtual const char *frameFormat() const = 0;  // "rgb565" on the 3.49
  virtual size_t frameBytes() const = 0;
  // Validates a frame as received over HTTP and copies its pixels into out (frameBytes() long).
  virtual bool acceptFrame(const uint8_t *buf, size_t len, uint8_t *out) const = 0;

  // Draws the whole screen for the current state: a frame/image, or one of the local pages.
  virtual void render(const DateTime &now) = 0;
  // Copies what the panel shows now into out (frameFormat(), frameBytes() long), for GET /screen on local pages.
  virtual bool snapshot(uint8_t *out) { return false; }
  // Light-on-dark theme for every page. Boards that cannot invert ignore it (KEY double tap is then off).
  virtual bool canInvert() const { return false; }
  virtual void setInverted(bool on) {}
  virtual void showSetup(const char *ap_name) = 0;
  virtual void showNotice(const char *title, const char *line) = 0;

  // Audio output, optional: 16-bit stereo frames at the rate given to audioStart(). The core runs the playback
  // loop (src/core/Audio.cpp); boards only move samples. There is no microphone support.
  virtual bool hasSpeaker() const { return false; }
  virtual bool audioStart(int sample_rate) { return false; }
  virtual void audioWrite(const int16_t *stereo, size_t frames) {}     // blocks at playback speed
  virtual void setVolume(int pct) {}

  // Backlight, optional (backlit panels only). brightness() is -1 without a control.
  virtual void setBrightness(int pct) {}
  virtual int brightness() const { return -1; }
  // Screen off/on without cutting power (backlight dark, nothing drawn). displayOn() is -1 without a control.
  virtual void setDisplayOn(bool on) {}
  virtual int displayOn() const { return -1; }

  // A board that runs its own pages with touch navigation (the 3.49 bar as a desk gadget): home is the clock, the
  // board turns touches into page changes itself (keyDown() stays false), tick() runs every loop, and animated pages
  // ask for a redraw every frameIntervalMs().
  virtual bool ownsPages() const { return false; }
  virtual void tick(const DateTime &now) {}
  virtual uint32_t frameIntervalMs() const { return 0; }
  // POST /cmd name=..&arg=..: board-specific commands; empty reply = unknown.
  virtual String command(const String &name, const String &arg) { return String(); }
  // GET /settings page, GET/POST /api/settings in the board's text format; nullptr / empty = no settings page.
  virtual const char *settingsPage() const { return nullptr; }
  virtual String settingsText() { return String(); }
  virtual bool applySettings(const String &body, String &err) { return false; }

  virtual DateTime now() = 0;
  virtual void setTime(const DateTime &t) = 0;
  virtual Sensors readSensors() = 0;
  virtual bool keyDown() = 0;
};

// The board this firmware is built for (defined in firmware.ino).
Board &board();
