#pragma once
#include <Arduino.h>

// Fixed cells keep weather and timer aligned with the VFD layout.
void MachineDigits_Draw(const String &s, int x, int y, int cellWidth, int height, int gap, bool hot = false);
