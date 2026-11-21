#pragma once
#include <Arduino.h>

// Strings come from the clock's existing locale and 12/24-hour helpers.
struct ClockFace {
  String time, date, long_date, alarm;
  bool alarm_lit, ringing;
  int battery;
};
void ClockFaces_Draw(const ClockFace &face);
