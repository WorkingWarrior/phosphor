#ifdef BOARD_TOUCH349
#include "ClockFaces.h"
#include "Theme.h"
#include "ThemeFonts.h"
#include "Settings.h"

using namespace vfd;
using px::rgb;

namespace {
constexpr uint16_t WHITE = rgb(236, 236, 236);
constexpr uint16_t BLUE = rgb(136, 204, 255);
constexpr uint16_t GOLD = rgb(255, 153, 0);
constexpr uint16_t PEACH = rgb(252, 188, 148);

// Tiny forecast pictograms share the machine's rectangular pixel grid.
void miniSky(Sky sky, int x, int y, int grid, uint16_t ink, uint16_t accent)
{
  if (sky == Sky::Unknown) return;
  if (sky == Sky::Clear || sky == Sky::PartCloud) {
    rect(x + grid, y, grid * 3, 6, accent);
    rect(x + grid * 2, y - 2, grid, 10, accent);
    if (sky == Sky::Clear) return;
  }
  if (sky == Sky::Fog) {
    for (int row = 0; row < 3; row++) rect(x, y + row * 4, grid * (row == 1 ? 4 : 6), 2, ink);
    return;
  }
  rect(x + grid, y + 2, grid * 3, 4, ink);
  rect(x, y + 6, grid * 6, 4, ink);
  if (sky == Sky::Rain || sky == Sky::Drizzle || sky == Sky::Shower || sky == Sky::Snow) {
    for (int i = 0; i < 3; i++) rect(x + i * grid * 2, y + 12, grid, 2, sky == Sky::Snow ? ink : accent);
  } else if (sky == Sky::Storm) {
    rect(x + 3 * grid, y + 10, grid * 2, 2, accent);
    rect(x + 2 * grid, y + 12, grid * 2, 2, accent);
  }
}

void weatherLayer(Sky sky, bool amiga)
{
  if (sky == Sky::Unknown || sky == Sky::Clear) return;
  int grid = amiga ? 2 : 4;
  clip(0, 0, W, 132);
  uint32_t tick = millis() / 100;
  bool rain = sky == Sky::Rain || sky == Sky::Drizzle || sky == Sky::Shower || sky == Sky::Storm;
  if (rain || sky == Sky::Snow) {
    for (int i = 0; i < 28; i++) {
      int x = ((i * 83 + tick * (rain ? 2 : 1)) % (W / grid)) * grid;
      int y = ((i * 37 + tick * (rain ? 4 : 1)) % 66) * 2;
      if (rain) {
        rect(x, y, grid, 6, BLUE);
        rect(x - grid, y + 6, grid, 2, BLUE);
      } else {
        rect(x, y, grid, 6, WHITE);
        rect(x - grid, y + 2, grid * 3, 2, WHITE);
      }
    }
  } else {
    for (int i = 0; i < 6; i++) {
      int x = ((i * 51 + tick / 3) % (W / grid + 24)) * grid - 24 * grid;
      int y = 10 + (i * 17 % 42) * 2;
      rect(x + 4 * grid, y, 12 * grid, 4, DIM);
      rect(x, y + 4, 24 * grid, 4, DIM);
    }
  }
  noClip();
}
}  // namespace

void WeatherFaces_Draw(const WeatherFace &f)
{
  Style saved = style();
  bool amiga = theme::current() == theme::AMIGA;
  bool atari = theme::current() == theme::ATARI;
  setStyle({ atari ? &VFD_FONT : amiga ? &AMIGA_FONT : &C64_FONT, false, false, false });
  if (!amiga && !atari) {
    constexpr uint16_t ink = rgb(108, 94, 181);
    MachineLabel("READY.", 30, 16, ink);
    if (f.weather.valid) MachineBlockText(String(f.weather.temp) + "°", 40, 44);
    else MachineLabel(f.condition, 40, 44, ink);
    MachineLabel(MachineFit(f.condition, 17), 330, 44, ink);
    if (f.weather.valid) {
      MachineLabel(MachineFit(f.humidity, 17), 330, 64, ink);
      MachineLabel(MachineFit(f.wind, 17), 330, 84, ink);
      String forecast;
      for (int i = 0; i < 3; i++) {
        const WeatherDay &d = f.weather.next[i];
        if (d.weekday < 0) continue;
        if (forecast.length()) forecast += "   ";
        forecast += f.days[i] + " " + String(d.t_max) + "°";
      }
      forecast = MachineFit(forecast, (W - 2 * (theme::def().border_w + 24)) / 16);
      MachineLabel(forecast, (W - MachineTextWidth(forecast)) / 2, 128, ink);
    }
    setStyle(saved);
    return;
  }
  weatherLayer(f.sky, amiga);
  int tx = amiga ? 200 : atari ? 24 : 32;
  MachineTemperature(f.weather.valid ? String(f.weather.temp) : String("--"), tx, amiga ? 14 : 16, amiga, atari);
  if (amiga) {
    MachineReading(f.condition, 200, 90, 426, WHITE, true, true);
    if (f.weather.valid) {
      MachineReading(f.humidity, 200, 112, 152, BLUE);
      MachineReading(f.wind, 360, 112, 266, BLUE);
    }
    MachinePanel(200, 132, 432, 34);
  } else if (atari) {
    MachineReading(f.condition, 24, 106, 360, WHITE);
    if (f.weather.valid) {
      MachineReading(f.humidity, 392, 30, 232, PEACH);
      MachineReading(f.wind, 392, 60, 232, PEACH);
    }
    rect(0, 132, W, 40, 0);
  }
  for (int i = 0; i < 3; i++) {
    const WeatherDay &d = f.weather.next[i];
    if (!f.weather.valid || d.weekday < 0) continue;
    int x = amiga ? 208 + i * 142 : 24 + i * 204;
    int y = amiga ? 142 : atari ? 144 : 144;
    int grid = amiga ? 2 : 4;
    miniSky(d.sky, x, y, grid, WHITE, amiga ? GOLD : atari ? PEACH : BLUE);
    MachineReading(f.days[i], x + 28, y, 48, WHITE);
    MachineReading(String(d.t_max) + "°", x + 80, y, amiga ? 58 : 100, amiga ? GOLD : atari ? PEACH : rgb(184, 199, 111));
  }
  setStyle(saved);
}
#endif
