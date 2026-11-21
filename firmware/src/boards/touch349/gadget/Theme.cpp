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
  { "c64", "C64-STYLE",
    { rgb(53, 40, 121), rgb(68, 68, 68), rgb(68, 68, 68), rgb(108, 94, 181), rgb(255, 255, 255), rgb(149, 149, 149),
      rgb(184, 199, 111), rgb(184, 199, 111), rgb(88, 141, 67) },
    FLAT, px::Theme::C64, rgb(108, 94, 181), 6 },
  { "amiga", "AMIGA-STYLE",
    { rgb(0, 85, 170), rgb(0, 68, 136), rgb(0, 68, 136), rgb(255, 255, 255), rgb(255, 255, 255), rgb(0, 0, 34),
      rgb(255, 136, 0), rgb(255, 136, 0), rgb(255, 136, 0) },
    FLAT, px::Theme::Amiga, 0, 0 },
  // prototypes (design/retro-proto): only the snow cabin has its own art so far
  { "atari", "2600-STYLE",
    { rgb(0, 0, 0), rgb(24, 26, 80), rgb(24, 26, 80), rgb(236, 236, 236), rgb(236, 236, 236), rgb(160, 160, 200),
      rgb(252, 188, 116), rgb(252, 188, 116), rgb(92, 186, 92) },
    FLAT, px::Theme::Ega, 0, 0 },
  { "lcd", "LCD-STYLE",
    { rgb(178, 186, 168), rgb(166, 174, 156), rgb(166, 174, 156), rgb(178, 186, 168), rgb(34, 38, 36), rgb(80, 86, 80),
      rgb(150, 56, 40), rgb(150, 56, 40), rgb(60, 90, 60) },
    { &vfd::VFD_FONT, true, false, false }, px::Theme::Natural, 0, 0 },
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
  if (old_scene_pref >= 0 && !prefs.isKey("key") && !prefs.isKey("id")) {
    // before 1.1 the only theme was the pixel scenes' colors: natural, ega or vfd (EGA is now part of VFD)
    prefs.putString("key", "vfd");
    prefs.putBool("natural", old_scene_pref == 0);
  }
  if (!prefs.isKey("key") && prefs.isKey("id")) {
    // 1.1 betas kept the index of { vfd, cga, ega, c64, amiga }; CGA and EGA were folded into VFD
    static const char *const OLD[] = { "vfd", "vfd", "vfd", "c64", "amiga" };
    uint8_t id = prefs.getUChar("id", 0);
    prefs.putString("key", id < 5 ? OLD[id] : "vfd");
    prefs.remove("id");
  }
  Id id = fromName(prefs.getString("key", "vfd"));
  cur = id < COUNT ? id : VFD;
  natural = prefs.getBool("natural", false);
  apply();
}

void set(Id id)
{
  if (id >= COUNT) return;
  cur = id;
  prefs.putString("key", DEFS[id].key);  // by name, so the list can change without moving anyone's choice
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
