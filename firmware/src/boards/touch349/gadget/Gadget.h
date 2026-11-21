#pragma once

// The retro desk gadget on the 3.49 bar (design/retro, direction A): its own pages, swiped left/right, each drawn in
// the VFD look: clock + alarm, radio, weather, aquarium, snow globe, countdown and a few ambient scenes.
// Vertical swipes change the volume everywhere. The board feeds it gestures and button presses.

#include <Arduino.h>
#include "../../../core/Board.h"
#include "../../../hw/i2c_bsp.h"

enum class Gesture { Tap, LongPress, SwipeLeft, SwipeRight, SwipeUp, SwipeDown };

void Gadget_Begin(uint16_t *canvas, I2cMasterBus *bus, void (*amp)(bool on));
// Every loop: alarm, timer phases, beeps.
void Gadget_Tick(const DateTime &now);
// Pages the gadget draws itself (into the canvas given to Gadget_Begin).
bool Gadget_Draws(Mode m);
void Gadget_Render(const DateTime &now);
// Redraw period of the current page, 0 = only when something changes.
uint32_t Gadget_FrameMs(Mode m);
void Gadget_Gesture(Gesture g, int x, int y);
// The pages of the swipe ring, for the settings: index 0..Gadget_PageCount()-1 in ring order (a page without content,
// like Play before it has an app, is not listed). Clock and Settings can never be hidden.
int Gadget_PageCount();
const char *Gadget_PageName(int i);  // in the language of the settings
bool Gadget_PageCanHide(int i);
bool Gadget_PageShown(int i);
void Gadget_PageSetShown(int i, bool shown);  // saved in the settings file ("hide=")
// A back button was pressed; true if the gadget used it (stopping a ringing alarm), then the board ignores it.
bool Gadget_ButtonPressed();
// HTTP /cmd: radio (station index | next | stop | toggle), vol (0..100), alarm (HH:MM | on | off), timer (start |
// reset | +N / -N minutes | seconds), sky (preview a Sky index, "n" suffix = night, -1 = the real one). Answers with
// the gadget state as JSON; empty for an unknown command.
String Gadget_Command(const String &name, const String &arg);
