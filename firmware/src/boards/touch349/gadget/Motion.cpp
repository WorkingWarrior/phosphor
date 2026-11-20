#ifdef BOARD_TOUCH349

#include "Motion.h"

#define QMI_ADDR 0x6b
#define REG_WHO_AM_I 0x00  // 0x05
#define REG_CTRL1 0x02
#define REG_CTRL2 0x03  // accel: full scale + output rate
#define REG_CTRL7 0x08  // sensor enable
#define REG_AX_L 0x35
#define REG_RESET 0x60

static I2cMasterBus *bus = nullptr;
static i2c_master_dev_handle_t dev = nullptr;
static bool ok = false;
static float raw[3] = { 0, 0, 1 }, grav[3] = { 0, 0, 1 };
static float shake = 0, prev_mag = 1;
static uint32_t last_ms = 0, knock_quiet_until = 0;
static bool knock = false;

static bool writeReg(uint8_t reg, uint8_t val) { return bus->i2c_write_buff(dev, reg, &val, 1) == ESP_OK; }

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
       && writeReg(REG_CTRL2, 0x05)  // +-2 g, ~235 Hz
       && writeReg(REG_CTRL7, 0x01); // accelerometer on
  Serial.printf("motion: QMI8658 %s\n", ok ? "on" : "failed");
  return ok;
}

bool Motion_Ok() { return ok; }

// Sensor reading (it points up, against gravity) into the down direction in screen axes (x right, y down, z out of
// the glass). Standing on the desk the board reads (0, -0.75, 0.62): sensor y runs down the screen, z out of the glass,
// so for a right-handed sensor x runs to the left.
static void toScreen(const float *s, float *o)
{
  o[0] = s[0];
  o[1] = -s[1];
  o[2] = -s[2];
}

void Motion_Update()
{
  if (!ok) return;
  uint32_t now = millis();
  if (now - last_ms < 15) return;
  float dt = min(0.2f, (now - last_ms) / 1000.0f);
  last_ms = now;
  uint8_t b[6];
  if (bus->i2c_read_buff(dev, REG_AX_L, b, 6) != ESP_OK) return;
  float s[3];
  for (int i = 0; i < 3; i++) s[i] = (int16_t)(b[2 * i] | (b[2 * i + 1] << 8)) / 16384.0f;
  memcpy(raw, s, sizeof(raw));
  float a[3];
  toScreen(s, a);

  float k = 1 - expf(-dt / 0.12f);  // gravity: low pass, ~120 ms
  for (int i = 0; i < 3; i++) grav[i] += (a[i] - grav[i]) * k;

  float mag = sqrtf(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
  float jerk = fabsf(mag - prev_mag);
  prev_mag = mag;
  float dev_g = fabsf(mag - 1);
  shake = max(shake * expf(-dt / 0.35f), constrain((dev_g - 0.35f) / 0.9f, 0.0f, 1.0f));
  // a knock: one sharp spike while the board is otherwise still
  if (jerk > 0.28f && shake < 0.3f && now > knock_quiet_until) {
    knock = true;
    knock_quiet_until = now + 250;
  }
}

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

#endif
