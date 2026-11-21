#ifdef BOARD_TOUCH349
#include "TimerFaces.h"
#include "ClockFaces.h"
#include "MachineDigits.h"
#include "Theme.h"
#include "ThemeFonts.h"
#include "RetroScene.h"

using namespace vfd;
using px::rgb;

bool TimerFaces_Draw(const TimerFace &f)
{
  auto id = theme::current();
  if (id == theme::VFD) return false;
  const char *name = id == theme::C64 ? "timer_c64" : id == theme::AMIGA ? "timer_amiga" : "timer_atari";
  if (!RetroPicture_Draw(name)) clear();
  Style saved = style();
  bool amiga = id == theme::AMIGA, c64 = id == theme::C64, atari = id == theme::ATARI;
  setStyle({ c64 ? &C64_FONT : amiga ? &AMIGA_FONT : &VFD_FONT, false, false, false });
  uint16_t bright = c64 ? rgb(184,199,111) : rgb(236,236,236);
  uint16_t hot = c64 ? rgb(184,105,98) : amiga ? rgb(255,153,0) : rgb(252,188,148);
  uint16_t dim = c64 ? rgb(149,149,149) : amiga ? rgb(153,153,170) : rgb(160,160,160);
  if (f.visible) MachineDigits_Draw(f.digits,20,12,70,100,12,f.done);
  Rocket_Draw(f.done,f.launch_ms);
  if (atari) MachineShade(466,0,174,118);
  // The title uses p3; longer state lines retain p2 and scroll inside the column.
  clip(468,12,640,46);
  if (!atari) {
    for (int dx=-2;dx<=2;dx+=2) for (int dy=-2;dy<=2;dy+=2)
      text(f.title.c_str(),470+dx,16+dy,3,{0});
  }
  text(f.title.c_str(),470,16,3,{bright});
  noClip();
  MachineReading(f.setting,470,50,170,dim,true,!atari);
  if (!f.done || f.visible)
    MachineReading(f.state,470,84,170,f.done ? hot : f.running ? bright : dim,true,!atari);
  rect(20,122,604,4,c64 ? rgb(51,51,51) : amiga ? rgb(34,34,51) : rgb(64,64,64));
  if (f.progress) rect(20,122,constrain(f.progress,0,604),4,bright);
  rect(theme::def().border_w,132,W-2*theme::def().border_w,40,0);
  for (int i=0;i<5;i++) {
    int x=16+i*122;
    if (amiga) {
      rect(x,136,116,28,rgb(85,85,102));
      rect(x,136,116,2,bright); rect(x,136,2,28,bright);
      rect(x,162,116,2,0); rect(x+114,136,2,28,0);
    } else frame(x,136,116,28,i==4 ? dim : bright);
    int tw=MachineTextWidth(f.buttons[i]);
    MachineReading(f.buttons[i],x+(116-tw)/2,143,116,i==4 ? dim : bright,false,!atari);
  }
  setStyle(saved);
  return true;
}
#endif
