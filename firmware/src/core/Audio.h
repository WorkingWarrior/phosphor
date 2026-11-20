#pragma once

// Board-independent audio: one playback queue fed by anyone (beeps, PCM pushed over HTTP).
// Everything is 16-bit mono at AUDIO_RATE; the playback task turns it into the board's stereo frames.

#include <Arduino.h>

#define AUDIO_RATE 16000

bool Audio_Begin();
bool Audio_Ready();
// Queues mono samples; blocks while the queue is full (up to timeout_ms). Returns false if dropped.
bool Audio_Play(const int16_t *mono, size_t samples, uint32_t timeout_ms = 2000);
void Audio_StopPlayback();
// True while queued audio is still coming out of the speaker.
bool Audio_Playing();
// Loudness of what is playing now (0-100), for animations.
int Audio_Level();
// Speaker volume 0-100. remember = keep it across restarts (kept in NVS); /beep and /play only override it.
void Audio_SetVolume(int pct, bool remember = false);
int Audio_Volume();
// A short sine tone with soft edges, queued like any other sound.
void Audio_Beep(int freq, int ms);
