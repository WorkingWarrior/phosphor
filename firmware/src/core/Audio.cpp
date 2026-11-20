#include "Audio.h"
#include <Preferences.h>
#include <freertos/ringbuf.h>
#include "Board.h"

#define QUEUE_BYTES (AUDIO_RATE * 2 * 12)  // 12 s of mono audio, in PSRAM
#define CHUNK_SAMPLES 320                   // 20 ms

static RingbufHandle_t queue = nullptr;
static volatile int level = 0;
static volatile uint32_t last_sound_ms = 0;
static volatile bool flush_requested = false;
static bool ready = false;
static int volume = 70;

// Drains the queue into the speaker, 20 ms at a time. Writes silence when idle so the DAC never
// repeats a stale DMA buffer.
static void playbackTask(void *)
{
  static int16_t stereo[CHUNK_SAMPLES * 2];
  for (;;) {
    size_t bytes = 0;
    int16_t *mono = (int16_t *)xRingbufferReceiveUpTo(queue, &bytes, pdMS_TO_TICKS(5), CHUNK_SAMPLES * 2);
    if (flush_requested) {
      if (mono) {
        vRingbufferReturnItem(queue, mono);
      }
      while ((mono = (int16_t *)xRingbufferReceiveUpTo(queue, &bytes, 0, QUEUE_BYTES)) != nullptr) {
        vRingbufferReturnItem(queue, mono);
      }
      flush_requested = false;
      continue;
    }
    size_t n = mono ? bytes / 2 : CHUNK_SAMPLES;
    float sum = 0;
    for (size_t i = 0; i < n; i++) {
      int16_t v = mono ? mono[i] : 0;
      stereo[i * 2] = stereo[i * 2 + 1] = v;
      sum += (float)v * v;
    }
    if (mono) {
      vRingbufferReturnItem(queue, mono);
      last_sound_ms = millis();
      level = constrain((int)(sqrtf(sum / n) / 95), 0, 100);  // speech peaks near RMS 9500
    } else {
      level = level * 3 / 4;
    }
    board().audioWrite(stereo, n);
  }
}

bool Audio_Begin()
{
  if (!board().hasSpeaker()) {
    return false;
  }
  Serial.println("audio: opening codecs");
  if (!board().audioStart(AUDIO_RATE)) {
    return false;
  }
  Serial.println("audio: codecs open");
  Preferences prefs;
  prefs.begin("rlcd", true);
  volume = prefs.getInt("volume", volume);
  prefs.end();
  board().setVolume(volume);
  queue = xRingbufferCreateWithCaps(QUEUE_BYTES, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM);
  if (!queue) {
    return false;
  }
  xTaskCreatePinnedToCore(playbackTask, "audio_out", 4096, nullptr, 5, nullptr, 0);
  ready = true;
  return true;
}

bool Audio_Ready()
{
  return ready;
}

bool Audio_Play(const int16_t *mono, size_t samples, uint32_t timeout_ms)
{
  if (!ready || !samples) {
    return false;
  }
  return xRingbufferSend(queue, mono, samples * 2, pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void Audio_StopPlayback()
{
  flush_requested = true;
}

bool Audio_Playing()
{
  if (!ready) {
    return false;
  }
  UBaseType_t waiting = 0;
  vRingbufferGetInfo(queue, nullptr, nullptr, nullptr, nullptr, &waiting);
  return waiting > 0 || millis() - last_sound_ms < 150;
}

int Audio_Level()
{
  return level;
}

void Audio_SetVolume(int pct, bool remember)
{
  volume = constrain(pct, 0, 100);
  board().setVolume(volume);
  if (remember) {
    Preferences prefs;
    prefs.begin("rlcd", false);
    prefs.putInt("volume", volume);
    prefs.end();
  }
}

int Audio_Volume()
{
  return volume;
}

void Audio_Beep(int freq, int ms)
{
  static int16_t chunk[CHUNK_SAMPLES];
  long total = (long)AUDIO_RATE * constrain(ms, 20, 5000) / 1000;
  for (long i = 0; i < total; i += CHUNK_SAMPLES) {
    int n = min((long)CHUNK_SAMPLES, total - i);
    for (int k = 0; k < n; k++) {
      // 10 ms fade in/out avoids clicks
      long t = i + k;
      float env = min(1.0f, min(t, total - t) / (AUDIO_RATE * 0.01f));
      chunk[k] = (int16_t)(12000 * env * sinf(2 * PI * freq * t / AUDIO_RATE));
    }
    Audio_Play(chunk, n);
  }
}
