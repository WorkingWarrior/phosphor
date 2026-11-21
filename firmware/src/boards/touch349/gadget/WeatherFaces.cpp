#ifdef BOARD_TOUCH349
#include "ClockFaces.h"
#include "Theme.h"
#include "ThemeFonts.h"
#include "Settings.h"
#include "RetroScene.h"

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

const char *pictureSky(Sky sky, bool day)
{
  switch (sky) {
    case Sky::PartCloud: return day ? "part" : "night";
    case Sky::Cloud: case Sky::Fog: return "cloud";
    case Sky::Drizzle: case Sky::Rain: case Sky::Shower: return "rain";
    case Sky::Snow: return "snow";
    case Sky::Storm: return "storm";
    default: return day ? "clear" : "night";
  }
}

void weatherLayer(Sky sky, bool amiga)
{
  int grid = amiga ? 2 : 4;
  clip(0, 0, W, 132);
  uint32_t now = millis(), tick = now / 100;
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
  } else if (sky == Sky::Fog) {
    uint16_t grey = amiga ? rgb(187, 187, 204) : rgb(149, 149, 149);
    for (int i = 0; i < 5; i++) {
      int x = ((i * 53 + tick / 8) % (W / grid + 40)) * grid - 40 * grid;
      rect(x, 26 + i * 18, grid * (30 + i * 3), 2, grey);
    }
  }
  // A frame counter keeps the flash to two redraws, independent of the refresh interval.
  static uint32_t nextFlash = 0;
  static uint8_t flashFrames = 0;
  if (sky == Sky::Storm) {
    if (!nextFlash) nextFlash = now + 4000;
    if ((int32_t)(now - nextFlash) >= 0) {
      flashFrames = 2;
      nextFlash = now + 4000 + ((now / 17) % 5001);
    }
    if (flashFrames) {
      flashFrames--;
      for (int y = 8; y < 80; y += 18) rect(8, y, W - 16, 2, WHITE);
      for (int i = 0; i < 10; i++) {
        int x = (520 / grid + (i < 5 ? -i : i - 10)) * grid;
        rect(x, 10 + i * 6, grid * 3, 8, WHITE);
      }
    }
  } else { nextFlash = 0; flashFrames = 0; }
  noClip();
}
}  // namespace

void MachineWashText(const String &s, int x, int y, uint16_t colorOverride)
{
  const uint16_t wash[] = { rgb(255,255,255), rgb(154,210,132), rgb(184,199,111), rgb(112,164,178) };
  for (int pass = 0; pass < 3; pass++) {
    int cx = x;
    const char *p = s.c_str();
    while (*p) {
      uint8_t first = *p++;
      uint32_t cp = first;
      if (first >= 128) {
        int count = first >= 0xf0 ? 3 : first >= 0xe0 ? 2 : 1;
        cp = first & (0x3f >> count);
        while (count-- && *p) cp = (cp << 6) | (*p++ & 0x3f);
      }
      const uint8_t *rows = C64_FONT.rows(cp);
      int gx = cp == ':' ? cx - 16 : cx;
      if (rows) for (int r = 0; r < 8; r++) for (int c = 0; c < 8; c++) {
        if (!(MachineGlyphRow(rows, cp, r) & (0x80 >> c))) continue;
        int xx = gx + c * 8, yy = y + r * 8;
        if (pass == 0) rect(xx + 4, yy + 4, 8, 8, 0);
        else if (pass == 1) rect(xx - 2, yy - 2, 12, 12, colorOverride ? rgb(104,55,43) : 0);
        else rect(xx, yy, 8, 8, colorOverride ? colorOverride : wash[r / 2]);
      }
      cx += cp == ':' ? 32 : 64;
    }
  }
}

void WeatherFaces_Draw(const WeatherFace &f)
{
  Style saved = style();
  bool amiga = theme::current() == theme::AMIGA;
  bool atari = theme::current() == theme::ATARI;
  setStyle({ atari ? &VFD_FONT : amiga ? &AMIGA_FONT : &C64_FONT, false, false, false });
  char picture[32];
  snprintf(picture, sizeof(picture), "wx_%s_%s", pictureSky(f.sky, f.day), amiga ? "amiga" : atari ? "atari" : "c64");
  if (!RetroPicture_Draw(picture) && !RetroWorld_Draw()) clear();
  weatherLayer(f.sky, amiga);
  if (!amiga && !atari) {
    MachineWashText(f.weather.valid ? String(f.weather.temp) + "°" : String("--"), 28, 12);
    MachineReading(f.condition, 28, 84, 584, WHITE, true, true);
    if (f.weather.valid) MachineReading(f.humidity + "   " + f.wind, 28, 104, 584, rgb(184, 199, 111), true, true);
    int border = theme::def().border_w;
    rect(border, 140, W - 2 * border, 26, 0);
    for (int i = 0; i < 3; i++) {
      const WeatherDay &d = f.weather.next[i];
      if (!f.weather.valid || d.weekday < 0) continue;
      int x = 28 + i * 204;
      MachineReading(f.days[i], x, 144, 48, WHITE, false);
      miniSky(d.sky, x + 56, 146, 4, WHITE, rgb(184, 199, 111));
      MachineReading(String(d.t_max) + "°", x + 88, 144, 100, rgb(184, 199, 111), false);
    }
    setStyle(saved);
    return;
  }
  int tx = amiga ? 200 : atari ? 24 : 32;
  MachineTemperature(f.weather.valid ? String(f.weather.temp) : String("--"), tx, amiga ? 14 : 16, amiga, atari);
  if (amiga) {
    MachineReading(f.condition, 200, 90, 426, WHITE, true, true);
    if (f.weather.valid) {
      MachineReading(f.humidity, 200, 112, 152, BLUE, true, true);
      MachineReading(f.wind, 360, 112, 266, BLUE, true, true);
    }
    MachinePanel(200, 132, 432, 34);
  } else if (atari) {
    MachineReading(f.condition, 24, 106, 360, WHITE, true, true);
    if (f.weather.valid) {
      MachineReading(f.humidity, 392, 30, 232, PEACH, true, true);
      MachineReading(f.wind, 392, 60, 232, PEACH, true, true);
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
