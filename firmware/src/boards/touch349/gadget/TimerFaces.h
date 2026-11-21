#pragma once
#include <Arduino.h>

struct TimerFace {
  String digits, setting, state;
  const char *buttons[5];
  bool visible, running, paused, done;
  int progress;  // 0..608
};
// false leaves the timer's existing plain layout in charge.
bool TimerFaces_Draw(const TimerFace &f);
