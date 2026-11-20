#ifdef BOARD_TOUCH349

#include "Theme.h"
#include <Preferences.h>

namespace theme {

using px::rgb;

// Palettes per role (Vfd.h): bg, ghost, faint, halo, lit, dim, hot, hoth, kelp. The retro ones take their colors from
// the machine's own palette; final values come from design/themes (mockups) and may still move.
static const vfd::Style VFD_STYLE = { &vfd::VFD_FONT, true, true, true };
static const vfd::Style FLAT = { &vfd::VFD_FONT, false, false, false };

static const Def DEFS[COUNT] = {
  { "vfd", "VFD",
    { 0x0041, 0x0904, 0x0082, 0x0248, 0x77BC, 0x44B1, 0xFA16, 0x6888, 0x4D72 },
    VFD_STYLE, px::Theme::Vfd, 0, 0 },
  { "cga", "CGA",
    { rgb(0, 0, 0), rgb(0, 85, 85), rgb(0, 42, 42), rgb(85, 255, 255), rgb(255, 255, 255), rgb(85, 255, 255),
      rgb(255, 85, 255), rgb(255, 85, 255), rgb(85, 255, 255) },
    FLAT, px::Theme::Cga, 0, 0 },
  { "ega", "EGA",
    { rgb(0, 0, 170), rgb(0, 0, 85), rgb(0, 0, 128), rgb(85, 85, 255), rgb(255, 255, 255), rgb(170, 170, 170),
      rgb(255, 255, 85), rgb(255, 255, 85), rgb(0, 170, 0) },
    FLAT, px::Theme::Ega, 0, 0 },
  { "c64", "C64-STYLE",
    { rgb(53, 40, 121), rgb(68, 68, 68), rgb(68, 68, 68), rgb(108, 94, 181), rgb(108, 94, 181), rgb(149, 149, 149),
      rgb(184, 199, 111), rgb(184, 199, 111), rgb(88, 141, 67) },
    FLAT, px::Theme::C64, rgb(108, 94, 181), 6 },
  { "amiga", "AMIGA-STYLE",
    { rgb(0, 85, 170), rgb(0, 68, 136), rgb(0, 68, 136), rgb(255, 255, 255), rgb(255, 255, 255), rgb(0, 0, 34),
      rgb(255, 136, 0), rgb(255, 136, 0), rgb(255, 136, 0) },
    FLAT, px::Theme::Amiga, 0, 0 },
};

static Preferences prefs;
static Id cur = VFD;
static bool natural = false;

static void apply()
{
  const Def &d = DEFS[cur];
  vfd::BG = d.pal.bg;
  vfd::GHOST = d.pal.ghost;
  vfd::FAINT = d.pal.faint;
  vfd::HALO = d.pal.halo;
  vfd::LIT = d.pal.lit;
  vfd::DIM = d.pal.dim;
  vfd::HOT = d.pal.hot;
  vfd::HOTH = d.pal.hoth;
  vfd::KELP = d.pal.kelp;
  vfd::LIT_INK = { vfd::LIT, vfd::HALO };
  vfd::HOT_INK = { vfd::HOT, vfd::HOTH };
  vfd::DIM_INK = { vfd::DIM };
  vfd::GHOST_INK = { vfd::GHOST };
  vfd::setStyle(d.style);
  px::setTheme(natural ? px::Theme::Natural : d.scenes);
}

void begin(int old_scene_pref)
{
  prefs.begin("theme", false);
  if (old_scene_pref >= 0 && !prefs.isKey("id")) {
    // before 1.1 the only theme was the pixel scenes' colors: natural, ega or vfd
    prefs.putUChar("id", old_scene_pref == 1 ? EGA : VFD);
    prefs.putBool("natural", old_scene_pref == 0);
  }
  uint8_t id = prefs.getUChar("id", VFD);
  cur = id < COUNT ? (Id)id : VFD;
  natural = prefs.getBool("natural", false);
  apply();
}

void set(Id id)
{
  if (id >= COUNT) return;
  cur = id;
  prefs.putUChar("id", id);
  apply();
}

Id current() { return cur; }
const Def &def(Id id) { return DEFS[id < COUNT ? id : VFD]; }

Id fromName(const String &name)
{
  String n = name;
  n.toLowerCase();
  for (int i = 0; i < COUNT; i++) {
    if (n == DEFS[i].key) return (Id)i;
  }
  if (n.length() && isDigit(n[0]) && n.toInt() < COUNT) return (Id)n.toInt();
  return COUNT;
}

bool naturalScenes() { return natural; }
void setNaturalScenes(bool on)
{
  natural = on;
  prefs.putBool("natural", on);
  apply();
}

void decorate()
{
  const Def &d = DEFS[cur];
  if (!d.border_w) return;
  int b = d.border_w;
  vfd::noClip();
  vfd::rect(0, 0, vfd::W, b, d.border);
  vfd::rect(0, vfd::H - b, vfd::W, b, d.border);
  vfd::rect(0, 0, b, vfd::H, d.border);
  vfd::rect(vfd::W - b, 0, b, vfd::H, d.border);
}

}  // namespace theme

#endif
