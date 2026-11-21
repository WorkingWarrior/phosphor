#pragma once
#include <Arduino.h>

// Strings come from the clock's existing locale and 12/24-hour helpers.
struct ClockFace {
  String time, date, long_date, alarm;
  bool alarm_lit, ringing;
  bool cursor;
  int battery;
};
void ClockFaces_Draw(const ClockFace &face);

#include "Weather.h"
struct RadioFaceData {
  String channel, genre, station, line, time, volume_label;
  String genres[7];
  int group, groups, volume;
  bool playing, show_volume, cursor;
  int state;
};
struct WeatherFace {
  WeatherNow weather;
  Sky sky;
  String condition, humidity, wind;
  String days[3];
};
void RadioFaces_Draw(const RadioFaceData &face);
void RadioFaces_Border();
void WeatherFaces_Draw(const WeatherFace &face);
void MachineLabel(const String &s, int x, int y, uint16_t color, bool shadow = false);
void MachinePanel(int x, int y, int w, int h);
void MachineTemperature(const String &s, int x, int y, bool amiga, bool atari);

void MachineReading(const String &s, int x, int y, int w, uint16_t color, bool scroll = true, bool outline = false);

String MachineFit(const String &s, int cells);
int MachineTextWidth(const String &s);
int MachineBlockText(const String &s, int x, int y, bool draw = true);
