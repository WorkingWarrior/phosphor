#ifdef BOARD_TOUCH349
#include "TimerFaces.h"
#include "ClockFaces.h"
#include "Theme.h"
#include "ThemeFonts.h"
#include "RetroScene.h"

using namespace vfd;
using px::rgb;
void MachineTimerDigits(const String &s, bool hot);

bool TimerFaces_Draw(const TimerFace &f)
{
  auto id = theme::current();
  if (id == theme::VFD) return false;
  const char *name = id == theme::C64 ? "timer_c64" : id == theme::AMIGA ? "timer_amiga" : "timer_atari";
  if (!RetroPicture_Draw(name)) return false;
  Style saved = style();
  bool amiga = id == theme::AMIGA, c64 = id == theme::C64;
  setStyle({ c64 ? &C64_FONT : amiga ? &AMIGA_FONT : &VFD_FONT, false, false, false });
  uint16_t bright = c64 ? rgb(184,199,111) : amiga ? rgb(255,255,255) : rgb(236,236,236);
  uint16_t hot = c64 ? rgb(184,105,98) : amiga ? rgb(255,153,0) : rgb(252,188,148);
  uint16_t dim = c64 ? rgb(149,149,149) : amiga ? rgb(153,153,170) : rgb(160,160,160);
  if (f.visible) MachineTimerDigits(f.digits, f.done);
  MachineReading(f.setting,24,90,584,bright,false,true);
  if (!f.done || f.visible)
    MachineReading(f.state,24,108,584,f.done ? hot : f.running ? bright : dim,false,true);
  rect(16,124,608,4,c64 ? rgb(51,51,51) : amiga ? rgb(34,34,51) : rgb(64,64,64));
  if (f.progress) rect(16,124,f.progress,4,hot);
  rect(theme::def().border_w,132,W-2*theme::def().border_w,40,0);
  for (int i=0;i<5;i++) {
    int x=16+i*122;
    if (amiga) {
      rect(x,136,116,28,rgb(85,85,102));
      rect(x,136,116,2,bright); rect(x,136,2,28,bright);
      rect(x,162,116,2,0); rect(x+114,136,2,28,0);
    }
    int tw=MachineTextWidth(f.buttons[i]);
    MachineReading(f.buttons[i],x+(116-tw)/2,143,116,i==4 ? dim : bright,false);
  }
  setStyle(saved);
  return true;
}
#endif
