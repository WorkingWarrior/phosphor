#pragma once

// The weather page's forecast view (tap on the weather page): tomorrow and the two days after, side by side, with
// the sky, the high and low and the chance of rain, from the daily fields of the Open-Meteo request (Weather.cpp).

#include <Arduino.h>
#include "Weather.h"

void Forecast_Draw(const WeatherNow &w, const char *const days[3]);  // days: names of w.next[0..2]
