---
register: product
---

# Phosphor themes: product context

## Product
Phosphor is open-source (GPLv3) firmware for the Waveshare ESP32-S3-Touch-LCD-3.49, a 640x172 touch bar that sits on a
desk. Pages: clock (with alarm), weather, internet radio, timers, and slow pixel scenes (aquarium, seaside lighthouse,
fireplace, snow cabin, lava lamp). Shared on r/esp32, where the audience is makers and retro-computing fans.

## Users and setting
Makers who flashed it from the browser installer. The bar stands next to a monitor or on a nightstand: read at a glance
from 50 to 80 cm, often in a dim room at night, touched with one finger. Text under 14 px tall is decoration, not
information.

## This piece
v1.1 adds one runtime theme setting that changes everything (palette, 8x8 font, page chrome, scene colors): VFD (the
current look), CGA, EGA, C64-style and Amiga-style. These mockups decide each theme's palette, font and layouts before
the firmware work.

## Principles
- Each theme should feel like the page was loaded on that machine, not a modern UI wearing a retro color.
- Hardware palettes win over modern color rules: CGA is 4 colors, EGA 16, C64 its 16, Amiga 12-bit. Dithering instead
  of extra colors.
- Everything must be drawable by the firmware kit: rects, lines, 8x8 bitmap text, sprites, ordered dither. No
  anti-aliasing, no gradients outside what the machine could do (Amiga copper bands are allowed).
- Touch targets at least 48 px.

## Anti-references
- No copied ROM fonts (C64 character ROM, Topaz) and no logos or trademarks. Names stay "C64-style", "Amiga-style".
- No modern flat UI with retro colors pasted on.
