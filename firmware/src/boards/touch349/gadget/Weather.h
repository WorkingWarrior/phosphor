#pragma once

// Istanbul weather from Open-Meteo (no key), fetched in a background task every 20 minutes.

#include <Arduino.h>

enum class Sky { Unknown, Clear, PartCloud, Cloud, Fog, Drizzle, Rain, Shower, Snow, Storm };

struct WeatherDay {
  int weekday = -1;  // 0 = Sunday
  int t_max = 0;
  int t_min = 0;
  int rain = -1;  // chance of precipitation in %, -1 = not known
  Sky sky = Sky::Unknown;
};

struct WeatherNow {
  bool valid = false;
  uint32_t fetched_ms = 0;
  int temp = 0;
  int humidity = 0;
  int wind = 0;  // km/h
  bool day = true;
  Sky sky = Sky::Unknown;
  WeatherDay next[3];
};

void Weather_Begin();
WeatherNow Weather_Get();
const char *Weather_SkyName(Sky s);  // Turkish, upper case
