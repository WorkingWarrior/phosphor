#pragma once

// Internet radio and beeps on the 3.49 bar: ES8311 codec + amplifier, ESP32-audioI2S for the streams (MP3/AAC),
// a small spectrum analyzer on the decoded samples, and beeps mixed into whatever plays (or played alone).
// All calls are safe from the UI loop; the stream work happens in the radio task on core 0.

#include <Arduino.h>
#include "../../../hw/i2c_bsp.h"
#include "Settings.h"

enum class RadioState { Off, Connecting, Playing, Failed };

// amp(true/false) switches the speaker amplifier (TCA9554 EXIO7 on this board).
void Radio_Begin(I2cMasterBus *bus, void (*amp)(bool on));
bool Radio_Ready();

void Radio_Play(int station);  // index into settings::station()
void Radio_PlayUrl(const char *url);  // try a stream URL (settings page "test"), the station index stays
void Radio_Stop();
void Radio_Toggle();
RadioState Radio_State();
bool Radio_On();               // playing or trying to
int Radio_Station();           // the station playing or last played
String Radio_Title();          // "ARTIST - SONG" from the stream, empty if none yet

int Radio_Volume();            // 0..100
void Radio_SetVolume(int pct);
// Volume ramps from low to the set level over ms (alarm); 0 cancels a ramp.
void Radio_RampVolume(uint32_t ms);

// Spectrum, 14 bands, each 0..10 (log scale, auto gain).
static constexpr int RADIO_BANDS = 14;
void Radio_Levels(uint8_t out[RADIO_BANDS]);

// Beat of what plays, from the bass onsets: on = a steady beat is heard, bpm = its tempo, phase 0..1 within the
// current beat (0 = on the beat), count = beats so far, strength 0..1 = how sure the tracker is.
struct RadioBeat {
  bool on;
  float bpm;
  float phase;
  uint32_t count;
  float strength;
};
RadioBeat Radio_Beat();

// Loudness per channel 0..1 (VU needles) and ~10 ms of the wave, -100..100 (oscilloscope).
static constexpr int RADIO_TRACE = 128;
void Radio_Vu(float &left, float &right);
void Radio_Trace(int8_t out[RADIO_TRACE]);

// count beeps of hz for ms each, gap_ms apart; mixed into the radio, or played alone.
void Radio_Beep(int hz, int ms, int count = 1, int gap_ms = 120);
