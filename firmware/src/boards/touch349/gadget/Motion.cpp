#ifdef BOARD_TOUCH349

#include "Motion.h"

#define QMI_ADDR 0x6b
#define REG_WHO_AM_I 0x00  // 0x05
#define REG_CTRL1 0x02
#define REG_CTRL2 0x03  // accel: full scale + output rate
#define REG_CTRL5 0x06  // low pass filter
#define REG_CTRL7 0x08  // sensor enable
#define REG_AX_L 0x35
#define REG_RESET 0x60

// Sampling: a small task (core 0) reads the accelerometer every 2 ms while the chip makes a new sample every 1 ms (1 kHz,
// no low pass: CTRL5 stays 0). The old way read it from the main loop every 15 ms or more at ~235 Hz and compared two
// consecutive samples, so a desk knock (a burst a few ms long) was usually missed between two reads. Now the task tracks
// how far the magnitude jumps off a slow baseline and keeps the peak, so it sees the spike however short it is. The
// hardware tap engine was not used: it needs a CTRL9 command sequence and tuned thresholds, reports on an interrupt line
// this board does not wire, and is made for the device being tapped, not for a table.
static I2cMasterBus *bus = nullptr;
static i2c_master_dev_handle_t dev = nullptr;
static bool ok = false;
static volatile float raw[3] = { 0, 0, 1 }, grav[3] = { 0, 0, 1 };
static volatile float shake = 0;
static volatile bool knock = false;
static volatile uint32_t knock_count = 0;
static volatile uint32_t knock_quiet_until = 0;
// peak of the last ~4 s in 1 s buckets (for /status)
static volatile float peak_bucket[4] = { 0, 0, 0, 0 };
static volatile uint32_t peak_sec[4] = { 0, 0, 0, 0 };

// A knock: the magnitude jumps this far off its baseline (g). Noise at 1 kHz is about 0.005 g, a speaker beep about 0.03 g.
static const float KNOCK_G = 0.12f;

static bool writeReg(uint8_t reg, uint8_t val) { return bus->i2c_write_buff(dev, reg, &val, 1) == ESP_OK; }

// Sensor reading (it points up, against gravity) into the down direction in screen axes (x right, y down, z out of
// the glass). Standing on the desk the board reads (0, -0.75, 0.62): sensor y runs down the screen, z out of the glass,
// so for a right-handed sensor x runs to the left.
static void toScreen(const float *s, float *o)
{
  o[0] = s[0];
  o[1] = -s[1];
  o[2] = -s[2];
}

static void sampleTask(void *)
{
  float g[3] = { 0, 0, 1 }, base = 1, sh = 0;
  uint32_t last = millis();
  TickType_t wake = xTaskGetTickCount();
  for (;;) {
    vTaskDelayUntil(&wake, 2);
    uint8_t b[6];
    if (bus->i2c_read_buff(dev, REG_AX_L, b, 6) != ESP_OK) continue;
    uint32_t now = millis();
    float dt = min(0.05f, max(0.001f, (now - last) / 1000.0f));
    last = now;
    float s[3];
    for (int i = 0; i < 3; i++) s[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) / 16384.0f;
    float a[3];
    toScreen(s, a);
    for (int i = 0; i < 3; i++) raw[i] = s[i];

    float k = 1 - expf(-dt / 0.12f);  // gravity: low pass, ~120 ms
    for (int i = 0; i < 3; i++) {
      g[i] += (a[i] - g[i]) * k;
      grav[i] = g[i];
    }
    float mag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    float kb = 1 - expf(-dt / 0.04f);  // baseline of the magnitude: follows tilt and slow moves, not a spike
    float dev = fabsf(mag - base);
    base += (mag - base) * kb;
    float dev_g = fabsf(mag - 1);
    sh = max(sh * expf(-dt / 0.35f), constrain((dev_g - 0.35f) / 0.9f, 0.0f, 1.0f));
    shake = sh;

    uint32_t sec = now / 1000;
    int bi = sec & 3;
    if (peak_sec[bi] != sec) {
      peak_sec[bi] = sec;
      peak_bucket[bi] = 0;
    }
    if (dev > peak_bucket[bi]) peak_bucket[bi] = dev;

    if (dev > KNOCK_G && sh < 0.3f && (int32_t)(now - knock_quiet_until) > 0) {
      knock = true;
      knock_count = knock_count + 1;
      knock_quiet_until = now + 250;
    }
  }
}

bool Motion_Begin(I2cMasterBus *b)
{
  bus = b;
  i2c_device_config_t cfg = {};
  cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
  cfg.device_address = QMI_ADDR;
  cfg.scl_speed_hz = 300000;
  if (i2c_master_bus_add_device(bus->Get_I2cBusHandle(), &cfg, &dev) != ESP_OK) return false;
  uint8_t id = 0;
  if (bus->i2c_read_buff(dev, REG_WHO_AM_I, &id, 1) != ESP_OK || id != 0x05) {
    Serial.printf("motion: no QMI8658 (id %02x)\n", id);
    return false;
  }
  writeReg(REG_RESET, 0xB0);
  delay(15);
  ok = writeReg(REG_CTRL1, 0x40)     // register address auto increment
       && writeReg(REG_CTRL2, 0x03)  // +-2 g, 1 kHz
       && writeReg(REG_CTRL5, 0x00)  // no low pass filter
       && writeReg(REG_CTRL7, 0x01); // accelerometer on
  Serial.printf("motion: QMI8658 %s\n", ok ? "on" : "failed");
  if (ok) xTaskCreatePinnedToCore(sampleTask, "motion", 4096, nullptr, 2, nullptr, 0);
  return ok;
}

bool Motion_Ok() { return ok; }

// Sampling runs in its own task (sampleTask); nothing to do from the main loop.
void Motion_Update() {}

void Motion_Gravity(float &x, float &y, float &z)
{
  x = grav[0];
  y = grav[1];
  z = grav[2];
}

void Motion_Raw(float &x, float &y, float &z)
{
  x = raw[0];
  y = raw[1];
  z = raw[2];
}

float Motion_Shake() { return shake; }

bool Motion_TakeKnock()
{
  bool k = knock;
  knock = false;
  return k;
}

uint32_t Motion_KnockCount() { return knock_count; }

float Motion_KnockPeak()
{
  uint32_t sec = millis() / 1000;
  float m = 0;
  for (int i = 0; i < 4; i++)
    if (sec - peak_sec[i] < 4 && peak_bucket[i] > m) m = peak_bucket[i];
  return m;
}

#endif
