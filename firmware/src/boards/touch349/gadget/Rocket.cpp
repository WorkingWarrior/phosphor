#ifdef BOARD_TOUCH349
#include "Rocket.h"
#include "Theme.h"
using namespace vfd;
using px::rgb;

void Rocket_Draw(bool done, uint32_t elapsed)
{
  auto id=theme::current();
  bool vfd=id==theme::VFD, amiga=id==theme::AMIGA, atari=id==theme::ATARI;
  int gx=atari ? 8 : amiga ? 2 : 4, gy=amiga || id==theme::C64 ? 2 : 4;
  clip(420,0,466,122);
  rect(422,118,44,4,vfd ? DIM : rgb(149,149,149));
  // Smoke is deterministic: a few expanding stippled puffs, no particle allocations.
  if (done && elapsed<3500) {
    for (int i=0;i<5;i++) {
      int age=(int)elapsed-i*140;
      if (age<0 || age>2500) continue;
      int radius=2+age/190, cy=116-age/120;
      uint16_t grey=vfd ? (age<900 ? DIM : HALO) : rgb(max(28,150-age/20),max(28,150-age/20),max(28,150-age/20));
      for (int yy=-radius;yy<=radius;yy+=gy) for (int xx=-radius;xx<=radius;xx+=gx)
        if (xx*xx+yy*yy<radius*radius && ((xx/gx+yy/gy+i)&1)==0)
          rect(444+xx+(i%2 ? 8 : -8),cy+yy,vfd ? 3 : gx,vfd ? 3 : gy,grey);
    }
  }
  int flight=done && elapsed>500 ? min<uint32_t>(elapsed-500,2000) : 0;
  int rise=flight*flight/14000;
  if (rise>140) { noClip(); return; }
  int top=22-rise;
  // A narrow silhouette leaves both the countdown and the information column clear.
  for (int pass=0;pass<2;pass++) for (int yy=0;yy<96;yy+=gy) for (int xx=-16;xx<16;xx+=gx) {
    int mid=xx+gx/2;
    bool nose=yy<20 && abs(mid)<(yy/4+2);
    bool body=yy>=20 && yy<84 && abs(mid)<10;
    bool fins=yy>=70 && yy<92 && abs(mid)<16 && abs(mid)>=8;
    bool nozzle=yy>=84 && yy<96 && abs(mid)<6;
    if (!nose && !body && !fins && !nozzle) continue;
    uint16_t col;
    if (nose || fins) col=vfd ? HOT : rgb(104,55,43);
    else if (nozzle) col=vfd ? LIT : rgb(100,100,100);
    else col=vfd ? LIT : amiga ? (mid< -2 ? rgb(255,255,255) : mid<4 ? rgb(210,210,210) : rgb(149,149,149)) : atari ? rgb(236,236,236) : mid<0 ? rgb(236,236,236) : rgb(149,149,149);
    if (body && yy>=32 && yy<44 && abs(mid)<6) col=vfd ? HOT : rgb(136,204,255);
    if (amiga && (nose || fins) && mid<0) col=rgb(184,105,98);
    if (amiga && body && yy==32 && mid<0 && abs(mid)<6) col=rgb(255,255,255);
    int px=444+xx, py=top+yy;
    if (pass==0) {
      if (vfd) rect(px-1,py-1,5,5,col==HOT ? HOTH : HALO);
      else if (!atari) rect(px-1,py-1,gx+2,gy+2,0);
    } else rect(px,py,vfd ? 3 : gx,vfd ? 3 : gy,col);
  }
  if (done && elapsed<2200) {
    int length=(elapsed/90)%2 ? 20 : 12;
    for (int yy=0;yy<length;yy+=gy) {
      int width= max(gx,12-yy/2);
      uint16_t col=vfd ? HOT : yy<6 ? rgb(255,240,80) : yy<12 ? rgb(255,153,0) : rgb(184,60,32);
      for (int xx=-width/2;xx<width/2;xx+=gx) rect(444+xx,top+96+yy,vfd ? 3 : gx,vfd ? 3 : gy,col);
    }
  }
  noClip();
}
#endif
