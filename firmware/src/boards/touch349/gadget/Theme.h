#pragma once

// The gadget's look as one setting: palette, font and VFD effects of the drawing kit (Vfd.h), and the color mode of the
// pixel scenes (Px.h), so every page looks as if it ran on that machine. Stored in NVS namespace "theme".

#include <Arduino.h>
#include "Px.h"
#include "Vfd.h"

namespace theme {

enum Id : uint8_t { VFD, CGA, EGA, C64, AMIGA, COUNT };

struct Palette {
  uint16_t bg, ghost, faint, halo, lit, dim, hot, hoth, kelp;
};

struct Def {
  const char *key;   // HTTP / settings name
  const char *name;  // shown on the device
  Palette pal;
  vfd::Style style;
  px::Theme scenes;  // scene colors when "natural scenes" is off
  uint16_t border;   // frame color drawn around every page, 0 = none
  uint8_t border_w;  // its width in px
};

// Loads the saved theme and applies it. old_scene_pref is the value of the pre-1.1 "theme" key of the "gadget"
// namespace (0 natural, 1 ega, 2 vfd), or -1 when there is none; it is migrated once.
void begin(int old_scene_pref = -1);
void set(Id id);  // apply and save
Id current();
const Def &def(Id id);
inline const Def &def() { return def(current()); }
Id fromName(const String &name);  // COUNT when unknown; accepts the key or the index

// Scenes in their natural colors instead of the theme's machine colors.
bool naturalScenes();
void setNaturalScenes(bool on);

// Draws the theme's chrome over a finished page (the C64 border). Called after every page render.
void decorate();

}  // namespace theme
