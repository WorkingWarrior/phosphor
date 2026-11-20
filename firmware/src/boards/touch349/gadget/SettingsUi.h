#pragma once

// The settings page on the device itself, last in the ring: the items on the left, the chosen one's value on the
// right between two big tap zones (left = back, right = next, the value itself = next, or "find me" for the city).
// Everything takes effect at once; a new theme repaints the page you are looking at. Swipe up/down walks the list.
// Help holds the gesture tutorial; Wi-Fi and About say where the phone settings live.

#include <Arduino.h>

void SettingsUi_Draw();
void SettingsUi_Tap(int x, int y);
void SettingsUi_Step(int dir);  // +1 = next item, -1 = previous
void SettingsUi_Select(int item, int help_page = 0);  // HTTP /cmd menu: for screenshots
bool SettingsUi_Busy();         // something changes on its own (a lookup, a note): redraw often
void SettingsUi_Tick();         // main loop: saves a city picked from the list once the stepping stops

// Auto page: minutes between the ambient pages, 0 = off.
int SettingsUi_CycleMin();

// Holiday lights on the snow page: by the date (Dec 1 - Jan 6), always or never.
bool SettingsUi_Holidays();
void SettingsUi_SetHolidays(int mode);  // 0 = by date, 1 = always, 2 = never
