#pragma once

// The moving parts of the radio page: the companion slot left of the equalizer (a dancing couple that follows the
// beat, a cassette with turning reels, or nothing) and the equalizer itself in five looks (bars, peaks, mirror, VU
// needles, oscilloscope). Hold the slot or the equalizer to change it; the choice is kept.

#include <Arduino.h>

constexpr int RADIO_EQ_X = 426;       // the equalizer starts here
constexpr int RADIO_SLOT_X = 343;     // the companion slot, 78 x 96 px
constexpr int RADIO_SLOT_Y = 28;

bool RadioFace_Slot();             // false = no companion: the page uses the room (longer name, wider dial)
void RadioFace_DrawSlot();
void RadioFace_DrawEq();
bool RadioFace_Hold(int x, int y);  // long press on the radio page; true = it was on the slot or the equalizer
void RadioFace_Command(const String &arg);  // HTTP /cmd name=radioface: eq, comp, eq<n>, comp<n>
String RadioFace_Status();         // for /status: companion, equalizer, tempo
