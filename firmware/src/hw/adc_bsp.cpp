#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

#include "adc_bsp.h"

static adc_cali_handle_t cali_handle;
static adc_oneshot_unit_handle_t adc1_handle;
static float battery_divider = 3.0f;

void Adc_PortInit(float divider) {
  battery_divider = divider;
  adc_cali_curve_fitting_config_t cali_config = {};
  cali_config.unit_id = ADC_UNIT_1;
  cali_config.atten = ADC_ATTEN_DB_12;
  cali_config.bitwidth = ADC_BITWIDTH_12;
  ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle));

  adc_oneshot_unit_init_cfg_t init_config1 = {};
  init_config1.unit_id = ADC_UNIT_1;
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
  adc_oneshot_chan_cfg_t config = {};
  config.bitwidth = ADC_BITWIDTH_12;
  config.atten = ADC_ATTEN_DB_12;
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_3, &config));
}

float Adc_GetBatteryVoltage(int *data) {
  int value;
  int tage = 0;
  float vol = 0;
  esp_err_t err;
  err = adc_oneshot_read(adc1_handle, ADC_CHANNEL_3, &value);
  if (err == ESP_OK) {
    adc_cali_raw_to_voltage(cali_handle, value, &tage);
    vol = 0.001 * tage * battery_divider;
  }
  if (data) {
    *data = value;
  }
  return vol;
}

// Li-ion discharge curve (open-circuit volts -> percent); the straight 3.0-4.12 V line read 50 % at 3.56 V, which is
// already nearly empty for a real cell.
static const struct { float v; uint8_t pct; } CURVE[] = {
  { 4.15f, 100 }, { 4.05f, 90 }, { 3.97f, 80 }, { 3.90f, 70 }, { 3.84f, 60 }, { 3.80f, 50 },
  { 3.77f, 40 }, { 3.74f, 30 }, { 3.70f, 20 }, { 3.64f, 10 }, { 3.30f, 0 },
};

static float curvePercent(float v) {
  const int n = sizeof(CURVE) / sizeof(CURVE[0]);
  if (v >= CURVE[0].v) return 100;
  if (v <= CURVE[n - 1].v) return 0;
  for (int i = 1; i < n; i++) {
    if (v >= CURVE[i].v) {
      float t = (v - CURVE[i].v) / (CURVE[i - 1].v - CURVE[i].v);
      return CURVE[i].pct + t * (CURVE[i - 1].pct - CURVE[i].pct);
    }
  }
  return 0;
}

// Smoothed state, advanced by Adc_Update() (every few seconds from the sensor loop).
static float smooth_v = 0;
static bool charging = false;
static float charge_ref_v = 0;
static uint32_t charge_ref_ms = 0;

#define SMOOTH_ALPHA 0.1f           // one sample per 5 s: a time constant of about 50 s, so radio bursts do not move it
#define CHARGE_CHECK_MS 120000      // compare the smoothed voltage with the one two minutes ago
#define CHARGE_STEP_V 0.02f         // a rise this big in that window = charging, a fall this big = not charging
#define CHARGE_FULL_V 4.18f         // a cell only sits this high while the charger holds it

void Adc_Update(void) {
  // Average a few raw reads: one sample jumps by tens of mV while the radio draws current.
  float sum = 0;
  int ok = 0;
  for (int i = 0; i < 8; i++) {
    float v = Adc_GetBatteryVoltage(NULL);
    if (v > 0) { sum += v; ok++; }
  }
  if (!ok) return;
  float v = sum / ok;
  smooth_v = smooth_v <= 0 ? v : smooth_v + SMOOTH_ALPHA * (v - smooth_v);

  // This board has no charge-status pin (the TCA9554 only has the battery latch and the amp pin used, and the charger
  // IC is not wired to a GPIO), so "charging" is inferred from the voltage: at the top, or climbing over two minutes.
  // It holds its state in between, so it does not flicker; on USB with a full cell it reads true (the cell sits at the top).
  uint32_t now = millis();
  if (!charge_ref_ms) {
    charge_ref_ms = now;
    charge_ref_v = smooth_v;
  } else if (now - charge_ref_ms >= CHARGE_CHECK_MS) {
    float d = smooth_v - charge_ref_v;
    if (d >= CHARGE_STEP_V) charging = true;
    else if (d <= -CHARGE_STEP_V) charging = false;
    charge_ref_v = smooth_v;
    charge_ref_ms = now;
  }
  if (smooth_v >= CHARGE_FULL_V) charging = true;
}

float Adc_GetSmoothedVoltage(void) {
  if (smooth_v <= 0) Adc_Update();
  return smooth_v;
}

uint8_t Adc_GetBatteryLevel(void) {
  return (uint8_t)(curvePercent(Adc_GetSmoothedVoltage()) + 0.5f);
}

bool Adc_IsCharging(void) {
  return charging;
}
