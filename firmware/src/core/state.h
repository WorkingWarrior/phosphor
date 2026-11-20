#pragma once

#include <Arduino.h>

#define FW_VERSION "0.10.0"

enum class Mode { Clock, Info, Message, Image, Aquarium, AquariumNight, Radio, Weather, Timer, Snow, Fire, Sea, Study, Wave, Lava, Hourglass, Settings };

struct DeviceState {
  Mode mode = Mode::Clock;
  Mode home = Mode::Clock;        // where taps on a message or image and timeouts return
  String name;                    // hostname and mDNS name, e.g. phosphor-1a2b
  String message;
  String message_title;
  uint32_t mode_until_ms = 0;     // 0 = message/image stays until KEY
  uint8_t *image = nullptr;       // pushed image, board().frameBytes() in PSRAM
  bool dirty = true;              // full redraw requested
  bool inverted = false;          // light-on-dark theme (KEY double tap), kept in NVS
  float temp_c = NAN;
  float humi = NAN;
  float batt_v = 0;
  int batt_pct = -1;
};

extern DeviceState state;
