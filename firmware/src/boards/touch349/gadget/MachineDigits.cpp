#ifdef BOARD_TOUCH349
#include "MachineDigits.h"
#include "ClockFaces.h"
#include "Theme.h"
#include "ThemeFonts.h"

using namespace vfd;
using px::rgb;
namespace {
const uint8_t DIGITS[10][5] = {
  {7,5,5,5,7}, {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7}, {5,5,7,1,1},
  {7,4,7,1,7}, {7,4,7,5,7}, {7,1,1,1,1}, {7,5,7,5,7}, {7,5,7,1,7}
};
uint16_t chrome(int row)
{
  static const uint16_t bands[] = {
    rgb(255,255,255), rgb(255,255,238), rgb(255,238,204), rgb(255,221,153),
    rgb(255,187,102), rgb(170,68,68), rgb(238,119,119), rgb(221,85,136),
    rgb(204,85,153), rgb(170,68,153), rgb(136,51,136), rgb(119,51,136)
  };
  return bands[constrain(row,0,11)];
}
}
void MachineDigits_Draw(const String &s, int x, int y, int cellWidth, int height, int gap, bool hot)
{
  bool atari = theme::current() == theme::ATARI, c64 = theme::current() == theme::C64;
  const Font &font = c64 ? C64_FONT : AMIGA_FONT;
  const uint16_t wash[] = {rgb(255,255,255),rgb(154,210,132),rgb(184,199,111),rgb(112,164,178)};
  for (int pass=0; pass<3; pass++) {
    int cx=x;
    for (unsigned i=0; i<s.length(); i++,cx+=cellWidth+gap) {
      char ch=s[i];
      const uint8_t *rows=font.rows(ch);
      int nr=atari ? 5 : 7, nc=atari ? 3 : 8;
      for (int r=0;r<nr;r++) for (int c=0;c<nc;c++) {
        bool on=atari ? (ch==':' ? c==1 && (r==1 || r==3) : ch=='-' ? r==2 :
          ch>='0' && ch<='9' && (DIGITS[ch-'0'][r] & (4>>c))) :
          rows && (MachineGlyphRow(rows,ch,r)&(0x80>>c));
        if (!on) continue;
        int xx=cx+c*cellWidth/nc, yy=y+r*height/nr;
        int ww=(c+1)*cellWidth/nc-c*cellWidth/nc, hh=(r+1)*height/nr-r*height/nr;
        if (pass==0) rect(xx+4,yy+4,ww,hh,0);
        else if (pass==1) {
          rect(xx-2,yy-2,ww+4,hh+4,hot && c64 ? rgb(104,55,43) : 0);  // also on the 2600: its light digits sit on snow skies
        } else if (c64 || atari) {
          uint16_t col=c64 ? (hot ? rgb(184,105,98) : wash[min(r/2,3)]) :
            hot ? (r<3 ? rgb(252,188,148) : rgb(208,128,92)) : rgb(236-r*24,236-r*16,236-r*9);
          rect(xx,yy,ww,hh,col);
        } else for (int j=0;j<hh;j+=2)
          rect(xx,yy+j,ww,min(2,hh-j),hot ? rgb(255,153,0) : chrome((yy+j-y)*12/height));
      }
    }
  }
}
#endif
