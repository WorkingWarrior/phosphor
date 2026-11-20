#pragma once

// QMI8658 motion sensor (accelerometer only) on the main I2C bus: where "down" is on the screen, shakes and knocks on
// the desk. Read from the main loop (Motion_Update every frame or so).

#include <Arduino.h>
#include "../../../hw/i2c_bsp.h"

bool Motion_Begin(I2cMasterBus *bus);
bool Motion_Ok();
void Motion_Update();
// Smoothed gravity (where things fall) in screen axes (x right, y down, z out of the glass), in g.
void Motion_Gravity(float &x, float &y, float &z);
// Raw acceleration in sensor axes, in g (for /cmd state while mapping the axes).
void Motion_Raw(float &x, float &y, float &z);
// Shaking right now, 0 (still) .. 1 (hard), decays within a second after it stops.
float Motion_Shake();
// True once per knock (a short sharp tap on the desk or the case).
bool Motion_TakeKnock();
