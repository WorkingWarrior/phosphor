#pragma once
#include <Arduino.h>
#include "Rocket.h"

struct TimerFace {
  String digits, title, setting, state;
  const char *buttons[5];
  bool visible, running, paused, done;
  uint32_t launch_ms;  // elapsed since Done began
  int progress;  // 0..604
};
// false is reserved for the original VFD layout.
bool TimerFaces_Draw(const TimerFace &f);
