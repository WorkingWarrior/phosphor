#pragma once

// Waveshare ESP32-S3-Touch-LCD-3.49: 3.49" 172x640 IPS bar, AXS15231B over QSPI with capacitive touch on the same chip,
// PWM backlight, PCF85063 RTC, battery ADC on GPIO4, TCA9554 expander (EXIO6 = battery power latch), BOOT button on GPIO0.
// Mounted as a landscape strip: the firmware works in 640x172 and the driver rotates into the native portrait
// panel. Frames are RGB565 little endian, row major.
//
// The bar runs as the retro desk gadget (gadget/Gadget.h): swipe left/right = page, swipe up/down = volume, taps act on
// the page. The two buttons on the back are the board's own: PWR (GPIO16) tap = screen off/on, hold 3 s = power off on
// battery; BOOT (GPIO0) tap = next brightness step, hold 5 s = Wi-Fi reset; RESET restarts. Either button first stops a
// ringing alarm. Touch never resets Wi-Fi: a palm resting on the glass must not wipe the setup.

#include "gadget/Settings.h"
#include <esp_lcd_panel_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include "../../core/Board.h"
#include "../../hw/i2c_bsp.h"
#include "U8g2Canvas.h"
#include "gadget/Gadget.h"

// Blends two RGB565 colors: w = 0 gives a, 256 gives b.
static inline uint16_t mix565(uint16_t a, uint16_t b, int w)
{
  int r = (a >> 11) + ((((b >> 11) - (a >> 11)) * w) >> 8);
  int g = ((a >> 5) & 63) + (((((b >> 5) & 63) - ((a >> 5) & 63)) * w) >> 8);
  int bl = (a & 31) + ((((b & 31) - (a & 31)) * w) >> 8);
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

class Touch349Board : public Board {
 public:
  static constexpr int WIDTH = 640;
  static constexpr int HEIGHT = 172;
  // BOOT steps through these; the overlay lights one segment per step.
  static constexpr int LEVELS[] = { 10, 25, 45, 70, 100 };
  static constexpr int LEVEL_COUNT = sizeof(LEVELS) / sizeof(LEVELS[0]);
  // Bar palette from DESIGN.md (OKLCH converted), RGB565.
  static constexpr uint16_t BG = 0x1081;
  static constexpr uint16_t INK = 0xF77C;
  static constexpr uint16_t DIM = 0xAD12;
  static constexpr uint16_t RULE = 0x4A07;
  static constexpr uint16_t AMBER = 0xFDA9;

  Touch349Board();

  const char *model() const override { return "waveshare-touch-lcd-3.49"; }
  void begin() override;

  int width() const override { return WIDTH; }
  int height() const override { return HEIGHT; }
  const char *frameFormat() const override { return "rgb565"; }
  size_t frameBytes() const override { return (size_t)WIDTH * HEIGHT * 2; }
  bool acceptFrame(const uint8_t *buf, size_t len, uint8_t *out) const override;

  void render(const DateTime &now) override;
  void showSetup(const char *ap_name) override;
  void showNotice(const char *title, const char *line) override;

  DateTime now() override;
  void setTime(const DateTime &t) override;
  Sensors readSensors() override;
  bool keyDown() override;
  void setBrightness(int pct, bool show_level = true) override;
  int brightness() const override { return brightness_pct_; }
  void setDisplayOn(bool on) override;
  int displayOn() const override { return display_on_ ? 1 : 0; }
  bool snapshot(uint8_t *out) override;
  bool ownsPages() const override { return true; }
  void tick(const DateTime &now) override { Gadget_Tick(now); }
  uint32_t frameIntervalMs() const override { return display_on_ ? Gadget_FrameMs(state.mode) : 0; }
  String command(const String &name, const String &arg) override { return Gadget_Command(name, arg); }
  const char *settingsPage() const override { return SETTINGS_PAGE; }
  String settingsText() override { return settings::text(); }
  bool applySettings(const String &body, String &err) override { return settings::apply(body, err); }
  // Speaker amplifier (TCA9554 EXIO7); called from the radio task.
  void setAmp(bool on);

 private:
  void initPanel();
  void initBacklight();
  void initExpander();
  void setPowerLatch(bool closed);
  void setExpanderBit(int bit, bool on);
  void initTouch();
  // True while touched; x, y in landscape screen coordinates.
  bool readTouch(int &x, int &y);
  void trackGesture(bool down, int x, int y, uint32_t now_ms);
  void checkButtons(uint32_t now_ms);
  void stepBrightness();
  void powerOff();
  void setDuty(int pct);
  void fadeBacklight(int from_pct, int to_pct, int ms);
  // Shows a finished picture: adds the brightness overlay while it is up, unfolds it after power-on, stays dark while
  // the screen is off.
  void show(const uint16_t *landscape);
  // Power off/on transitions (an old CRT: the picture squeezes into a line, the line into a dot).
  void collapse(const uint16_t *from);
  void unfold(const uint16_t *to);
  // The level overlay over a dimmed copy of the picture, into scratch_ (Touch349Ui.cpp).
  void drawLevelOverlay(const uint16_t *under);
  // Rotates a WIDTH x HEIGHT RGB565 image into the native panel and sends it.
  void push(const uint16_t *landscape);
  void pushAnim(const uint16_t *frame);  // push at the squeeze animation's fixed pace
  // Waits for the panel to finish the chunk in flight; false on timeout.
  bool waitFlush();
  // Sends what the U8g2 pages drew.
  void pushCanvas() { show(canvas_); }

  I2cMasterBus i2c_;        // RTC, TCA9554
  I2cMasterBus touch_i2c_;  // AXS15231B touch
  i2c_master_dev_handle_t expander_ = nullptr;
  i2c_master_dev_handle_t touch_ = nullptr;
  esp_lcd_panel_handle_t panel_ = nullptr;
  SemaphoreHandle_t flush_done_ = nullptr;
  uint16_t *dma_[2] = {};       // two chunks of native rows, internal RAM for DMA, used in turn
  uint16_t *canvas_ = nullptr;  // pages drawn on the board, PSRAM
  uint16_t *scratch_ = nullptr; // picture with the overlay on it, PSRAM
  uint16_t *anim_ = nullptr;    // transition frames, PSRAM
  const uint16_t *shown_ = nullptr;  // what the panel shows now (for /screen and the power-off transition)
  U8g2Canvas page_;

  static constexpr int FLUSH_TIMEOUT_MS = 500;
  static constexpr int FLUSH_MAX_FAILS = 2;
  int flush_fails_ = 0;
  int brightness_pct_ = 78;  // the factory duty (200 of 255) until someone sets it; kept across restarts

  bool display_on_ = true;
  bool unfold_next_ = true;      // the next picture comes in with the power-on transition (also at boot)
  uint32_t level_until_ms_ = 0;  // brightness overlay on screen until then
  int level_dir_ = -1;           // BOOT walks the levels down to the dimmest, then back up

  uint32_t pwr_down_ms_ = 0;
  uint32_t boot_down_ms_ = 0;
  bool pwr_held_ = false;   // long press already acted, ignore the release
  bool boot_held_ = false;
  bool pwr_armed_ = false;  // PWR still down from switching the board on: wait for its release first

  SemaphoreHandle_t expander_mx_ = nullptr;

  uint32_t last_poll_ms_ = 0;
  bool touching_ = false;
  int touch_x0_ = 0, touch_y0_ = 0, touch_x_ = 0, touch_y_ = 0;
  uint32_t touch_down_ms_ = 0;
  bool long_sent_ = false;
};
