#ifdef BOARD_TOUCH349
#include "ClockFaces.h"
#include "Theme.h"
#include "ThemeFonts.h"
#include "RadioFace.h"
#include "Radio.h"
#include "Settings.h"

using namespace vfd;
using px::rgb;

namespace {
constexpr uint16_t WHITE = rgb(255, 255, 255);
constexpr uint16_t GREEN = rgb(102, 204, 68);
constexpr uint16_t ORANGE = rgb(255, 153, 0);
constexpr uint16_t YELLOW = rgb(184, 199, 111);
constexpr uint16_t BASIC = rgb(108, 94, 181);
constexpr uint16_t BASIC_DIM = rgb(112, 164, 178);
constexpr uint16_t BASIC_OFF = rgb(80, 70, 150);
static const uint8_t DIGITS[10][5] = {
    { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 },
    { 5, 5, 7, 1, 1 }, { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 },
    { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 }
};
const char *const BELL[] = { "..#..", ".###.", ".###.", "#####", "..#.." };
const uint16_t CHROME[] = {
  rgb(255, 255, 255), rgb(255, 255, 255), rgb(255, 255, 255), rgb(255, 255, 255),
  rgb(255, 255, 238), rgb(255, 255, 238), rgb(255, 255, 238), rgb(255, 255, 238),
  rgb(255, 238, 221), rgb(255, 238, 204), rgb(255, 238, 187), rgb(255, 221, 187),
  rgb(255, 221, 170), rgb(255, 221, 153), rgb(255, 204, 136), rgb(255, 204, 119),
  rgb(255, 187, 102), rgb(187, 102, 68), rgb(170, 68, 68), rgb(238, 119, 102),
  rgb(255, 136, 102), rgb(238, 119, 119), rgb(238, 119, 119), rgb(238, 102, 119),
  rgb(221, 102, 136), rgb(221, 85, 136), rgb(221, 85, 136), rgb(204, 85, 136),
  rgb(204, 85, 153), rgb(187, 68, 153), rgb(170, 68, 153), rgb(170, 68, 153),
  rgb(153, 68, 153), rgb(136, 51, 136), rgb(136, 51, 136), rgb(119, 51, 136)
};

// Keep the mockup's own lowercase and Turkish glyphs, rather than VFD's accent synthesis.
uint32_t codepoint(const char *&s)
{
  uint8_t c = *s++;
  if (c < 128) return c;
  int n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : 1;
  uint32_t cp = c & (0x3f >> n);
  while (n-- && *s) cp = (cp << 6) | (*s++ & 0x3f);
  return cp;
}

void label(const String &s, int x, int y, uint16_t color, bool shadow = false)
{
  const Font &font = *style().font;
  if (&font == &VFD_FONT) {
    text(s.c_str(), x, y, 2, { color });
    return;
  }
  auto draw = [&](int dx, int dy, uint16_t c) {
    const char *p = s.c_str();
    int cx = x;
    while (*p) {
      const uint8_t *rows = font.rows(codepoint(p));
      if (!rows) rows = font.rows('?');
      if (rows) for (int r = 0; r < 8; r++) for (int col = 0; col < 8; col++)
        if (rows[r] & (0x80 >> col)) rect(cx + col * 2 + dx, y + r * 2 + dy, 2, 2, c);
      cx += 16;
    }
  };
  if (shadow) draw(2, 2, 0);
  draw(0, 0, color);
}

void battery(int x, int y, int pct, uint16_t color, uint16_t fill)
{
  if (pct < 0) return;
  pct = constrain(pct, 0, 100);
  rect(x, y, 20, 2, color);
  rect(x, y + 8, 20, 2, color);
  rect(x, y, 2, 10, color);
  rect(x + 18, y, 2, 10, color);
  rect(x + 20, y + 2, 2, 6, color);
  rect(x + 2, y + 2, ((8 * pct + 50) / 100) * 2, 6, fill);
  label(String(pct) + "%", x + 32, y - 2, color);
}

void bigTime(const String &s)
{
  const Font &font = AMIGA_FONT;
  int sx = 10, sy = 10, y = 14;
  // Three passes over the whole mask: shadows must not cover adjacent glyphs.
  auto draw = [&](int pass) {
    int x = 192;
    for (unsigned i = 0; i < s.length(); i++) {
      char ch = s[i];
      const uint8_t *rows = font.rows(ch);
      int gx = ch == ':' ? x - 2 * sx : x;
      if (rows) for (int r = 0; r < 8; r++) {
        uint8_t bits = ch == '0' && r >= 2 && r <= 5 ? rows[1] : rows[r];
        for (int c = 0; c < 8; c++) {
          if (!(bits & (0x80 >> c))) continue;
          int xx = gx + c * sx, yy = y + r * sy;
          if (pass == 0) rect(xx + 6, yy + 6, sx, sy, rgb(17, 0, 34));
          else if (pass == 1) rect(xx - 2, yy - 2, sx + 4, sy + 4, 0);
          else for (int j = 0; j < sy; j += 2) {
            int row = (yy + j - y) / 2;
            rect(xx, yy + j, sx, 2, CHROME[min(row, 35)]);
          }
        }
      }
      x += (ch == ':' ? 2 : 7) * sx + 8;
    }
  };
  draw(0);
  draw(1);
  draw(2);
}

void panel(int x0, int y0, int w, int h)
{
  for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) {
    uint16_t &c = canvas()[y * W + x];
    c = (c & 0xf7de) >> 1;
  }
  rect(x0, y0, w, 2, rgb(153, 153, 170));
  rect(x0, y0, 2, h, rgb(153, 153, 170));
  rect(x0, y0 + h - 2, w, 2, 0);
  rect(x0 + w - 2, y0, 2, h, 0);
}

void atariTime(const String &s)
{
  // Cool rows over the warm sunset bands, one color per block row like the 2600 changed colors per line.
  static const uint16_t COLORS[] = { rgb(236, 236, 236), rgb(164, 224, 252), rgb(144, 204, 232),
                                      rgb(124, 180, 212), rgb(104, 156, 192) };
  const uint16_t SHADOW = rgb(0, 24, 124);
  for (int pass = 0; pass < 2; pass++) {
    int x = 24, d = pass ? 0 : 4;
    for (unsigned i = 0; i < s.length(); i++) {
      char ch = s[i];
      if (ch == ':') {
        for (int y : { 28, 56 }) rect(x + d, y + d, 10, 10, pass ? COLORS[(y - 14) / 14] : SHADOW);
        x += 24;
      } else if (ch >= '0' && ch <= '9') {
        for (int r = 0; r < 5; r++) for (int c = 0; c < 3; c++)
          if (DIGITS[ch - '0'][r] & (4 >> c))
            rect(x + c * 20 + d, 14 + r * 14 + d, 20, 14, pass ? COLORS[r] : SHADOW);
        x += 72;
      }
    }
  }
}
}  // namespace

void ClockFaces_Draw(const ClockFace &f)
{
  Style saved = style();
  bool amiga = theme::current() == theme::AMIGA, atari = theme::current() == theme::ATARI;
  setStyle({ atari ? &VFD_FONT : amiga ? &AMIGA_FONT : &C64_FONT, false, false, false });
  uint16_t alarm = f.alarm_lit ? (amiga ? ORANGE : atari ? rgb(252, 188, 148) : YELLOW)
                              : amiga ? rgb(119, 68, 34) : atari ? rgb(112, 72, 48) : rgb(68, 68, 68);
  if (amiga) {
    bigTime(f.time);
    panel(340, 110, 292, 56);
    const String &date = textWidth(f.long_date.c_str(), 2) <= 272 ? f.long_date : f.date;
    label(date, 352, 118, WHITE, true);
    sprite(BELL, 5, 352, 144, 2, alarm);
    label(f.alarm, 370, 142, alarm, true);
    // A ringing English message needs more room than the alarm time.
    battery(f.ringing ? 508 : 476, 144, f.battery, WHITE, GREEN);
  } else if (atari) {
    atariTime(f.time);
    rect(0, 150, W, 22, 0);
    label(f.date, 24, 154, rgb(236, 236, 236));
    sprite(BELL, 5, 256, 156, 2, alarm);
    label(f.alarm, 276, 154, alarm);
    battery(508, 156, f.battery, rgb(184, 252, 184), rgb(184, 252, 184));
  } else {
    label("READY.", 30, 16, BASIC);
    if (f.cursor) rect(30, 32, 16, 16, BASIC);
    MachineBlockText(f.time, (W - MachineBlockText(f.time, 0, 0, false)) / 2, 56);
    String date = f.date, alarm = f.ringing ? f.alarm : "ALARM " + f.alarm;
    String batt = f.battery >= 0 ? "   " + String(L("BATT ", "PİL ")) + String(f.battery) + "%" : "";
    int available = W - 2 * (theme::def().border_w + 24);
    if (MachineTextWidth(date + "   " + alarm + batt) > available) batt = "";
    if (MachineTextWidth(date + "   " + alarm) > available)
      date = MachineFit(date, max(0, available / 16 - MachineTextWidth(alarm) / 16 - 3));
    int x = (W - MachineTextWidth(date + "   " + alarm + batt)) / 2;
    label(date, x, 128, BASIC); x += MachineTextWidth(date + "   ");
    if (!f.ringing || f.alarm_lit) label(alarm, x, 128, f.alarm_lit ? BASIC_DIM : BASIC_OFF);
    x += MachineTextWidth(alarm);
    label(batt, x, 128, BASIC);
  }
  setStyle(saved);
}
void MachineLabel(const String &s, int x, int y, uint16_t color, bool shadow) { label(s, x, y, color, shadow); }
void MachinePanel(int x, int y, int w, int h) { panel(x, y, w, h); }

String MachineFit(const String &s, int cells)
{
  const char *p = s.c_str(), *begin = p;
  while (*p && cells-- > 0) codepoint(p);
  return s.substring(0, p - begin);
}

int MachineTextWidth(const String &s)
{
  const char *p = s.c_str(); int n = 0;
  while (*p) { codepoint(p); n++; }
  return n * 16;
}

int MachineBlockText(const String &s, int x, int y, bool draw)
{
  const char *p = s.c_str(); int advance = 0, right = 0;
  while (*p) {
    uint32_t ch = codepoint(p);
    const uint8_t *rows = C64_FONT.rows(ch);
    int shift = ch == ':' ? -16 : 0;
    if (rows) for (int r = 0; r < 7; r++) {
      uint8_t bits = ch == '0' && r >= 2 && r <= 5 ? rows[1] : rows[r];
      for (int c = 0; c < 8; c++) if (bits & (0x80 >> c)) {
        right = max(right, advance + shift + (c + 1) * 8);
        if (draw) rect(x + advance + shift + c * 8, y + r * 8, 8, 8, BASIC);
      }
    }
    advance += ch == ':' ? 32 : 64;
  }
  return right;
}

void MachineTemperature(const String &s, int x, int y, bool amiga, bool atari)
{
  int sx = atari ? 16 : 8, sy = atari ? 14 : 8;
  const Font &font = amiga ? AMIGA_FONT : C64_FONT;
  for (int pass = 0; pass < 3; pass++) {
    int cx = x;
    for (unsigned i = 0; i < s.length(); i++) {
      char ch = s[i];
      const uint8_t *rows = atari ? nullptr : font.rows(ch);
      for (int r = 0; r < (atari ? 5 : 8); r++) for (int c = 0; c < (atari ? 3 : 8); c++) {
        bool on = atari ? (ch == '-' ? r == 2 : ch >= '0' && ch <= '9' && (DIGITS[ch-'0'][r] & (4 >> c)))
                        : rows && (rows[r] & (0x80 >> c));
        if (!on) continue;
        int xx = cx + c*sx, yy = y + r*sy;
        if (pass == 0) rect(xx+4, yy+4, sx, sy, 0);
        else if (pass == 1 && !atari) rect(xx-2, yy-2, sx+4, sy+4, 0);
        else if (pass == 2) for (int j=0;j<sy;j+=2)
          rect(xx, yy+j, sx, 2, atari ? rgb(164-r*15,224-r*17,252-r*15) : CHROME[min((r*sy+j)/2,35)]);
      }
      cx += 64;
    }
    int dx = cx + 4;
    uint16_t col = atari ? rgb(236,236,236) : WHITE;
    if (pass == 2) {
      rect(dx, y, 16, 4, col); rect(dx, y+12, 16, 4, col);
      rect(dx, y, 4, 16, col); rect(dx+12, y, 4, 16, col);
    }
  }
}

// One bounded text path for all machine HUDs, including UTF-8 titles and weather readings.
void MachineReading(const String &s, int x, int y, int w, uint16_t col, bool scroll, bool outline)
{
  int width = textWidth(s.c_str(), 2);
  int span = width + 48, off = scroll && width > w ? (millis()/35)%span : 0;
  clip(x,y-4,x+w,y+20);
  auto draw = [&](int xx) {
    if (outline) {
      label(s,xx-2,y,0); label(s,xx+2,y,0);
      label(s,xx,y-2,0); label(s,xx,y+2,0);
    }
    label(s,xx,y,col);
  };
  draw(x-off);
  if (scroll && width>w) draw(x-off+span);
  noClip();
}

void RadioFaces_Draw(const RadioFaceData &f)
{
  Style saved=style();
  bool amiga=theme::current()==theme::AMIGA, atari=theme::current()==theme::ATARI;
  setStyle({atari ? &VFD_FONT : amiga ? &AMIGA_FONT : &C64_FONT,false,false,false});
  if (!amiga && !atari) {
    String name = MachineFit(f.station, 19 - 10);  // 19 cells reach x 334, just short of the companion at 343
    label("LOAD\"" + name + "\",8,1", 30, 12, BASIC);
    String top = f.channel + " " + f.time;
    label(top, 610 - MachineTextWidth(top), 12, BASIC_DIM);
    int last = 44;
    switch (RadioState(f.state)) {
      case RadioState::Off:
        if (f.cursor) rect(30, 28, 16, 16, BASIC);
        MachineReading(f.line, 30, last, 300, BASIC_DIM);
        break;
      case RadioState::Connecting:
        MachineReading("SEARCHING FOR " + f.station, 30, 28, 300, BASIC);
        label("LOADING" + String("...").substring(0, (millis() / 400) % 4), 30, last, BASIC);
        break;
      case RadioState::Playing:
        label("LOADING", 30, 28, BASIC);
        MachineReading(f.line, 30, last, 300, BASIC);
        break;
      case RadioState::Failed:
        MachineReading("?FILE NOT FOUND  ERROR", 30, 28, 300, BASIC);
        MachineReading(f.line, 30, last, 300, BASIC_DIM);
        break;
    }
    if (f.show_volume) {
      rect(30, last, 300, 16, rgb(53, 40, 121));
      label("VOL", 30, last, BASIC);
      for (int i = 0; i < 20; i++)
        rect(94 + i * 11, last, 8, 14, f.volume >= (i + 1) * 5 ? BASIC : BASIC_OFF);
    }
    RadioFace_DrawSlot(); RadioFace_DrawEq();
    for (int i = 0; i < f.groups; i++) {
      int x = 16 + i * 87; bool cur = i == f.group;
      if (cur) rect(x, 140, 83, 22, BASIC);
      String key = MachineFit(f.genres[i], 5);
      label(key, x + (83 - MachineTextWidth(key)) / 2, 143, cur ? rgb(53, 40, 121) : BASIC);
    }
    setStyle(saved);
    return;
  }
  int b=theme::def().border_w;
  uint16_t peach=rgb(252,188,148);
  String genre = f.genre + (f.playing ? "  STEREO" : "");
  if (amiga) {
    panel(8,8,328,118); panel(422,28,208,98);
    MachineReading(f.station,18,14,208,WHITE);
    label(f.channel,234,14,ORANGE);
    MachineReading(genre,18,36,220,ORANGE); label(f.time,248,36,WHITE);
    // Tracker is decoration, but stays at the same readable pitch as the labels.
    static uint32_t tick=0,last=0;
    uint32_t now=millis();
    if (f.playing && last) tick+=min(uint32_t(200),uint32_t(now-last));
    last=now;
    for(int k=0;k<3;k++) {
      int row=(tick/150+k)%64, y=70+k*18;
      bool cur=k==1;
      if(cur) for(int j=0;j<16;j+=2) rect(16,y+j,312,2,CHROME[12+j]);
      char line[24];
      snprintf(line,sizeof(line),"%02X  %s %02d  %s",row,(row%3)?"C-4":"---",row%16,(row%2)?"A#3":"F-2");
      MachineReading(line,18,y,308,cur ? 0 : WHITE,false);
    }
  } else {
    rect(b,atari ? 0 : b,W-2*b,atari ? 34 : 48,0);
    if(atari) {
      text(f.channel.c_str(),16,6,3,{rgb(236,236,236)});
      clip(128,0,536,33); text(f.station.c_str(),128,6,3,{rgb(236,236,236)}); noClip();
    }
    label(f.time,atari ? 560 : 548,atari ? 8 : 10,WHITE);
    rect(422,30,208,96,0);
    if(atari) rect(16,34,320,22,0);
    MachineReading(atari ? genre : f.channel+" "+genre,16,106,316,peach);
  }
  int ly=amiga ? 54 : atari ? 38 : 28;
  if(f.show_volume) {
    rect(16,ly-2,320,20,0);
    label(f.volume_label,18,ly,WHITE);
    for(int i=0;i<20;i++) rect(76+i*12,ly,8,14,f.volume >= (i+1)*5 ? (amiga ? ORANGE : peach) : rgb(68,68,68));
  } else {
    MachineReading(f.line,18,ly,310,atari ? peach : WHITE);
  }
  RadioFace_DrawSlot(); RadioFace_DrawEq();
  if(!amiga) rect(b,132,W-2*b,atari ? 40 : 34,0);
  for(int i=0;i<f.groups;i++) {
    int x=16+i*87; bool cur=i==f.group;
    uint16_t accent=amiga ? ORANGE : peach;
    if(amiga) {
      if(cur) rect(x,136,83,28,accent); else panel(x,136,83,28);
      uint16_t hi=cur ? 0 : WHITE, lo=cur ? WHITE : 0;
      rect(x,136,83,2,hi); rect(x,136,2,28,hi);
      rect(x,162,83,2,lo); rect(x+81,136,2,28,lo);
    } else if(cur) rect(x,136,83,28,accent);
    int tw=textWidth(f.genres[i].c_str(),2);
    int inset=max(1,(83-tw)/2);  // a 5-letter genre fills the key in the 16 px theme fonts; never scroll a key
    MachineReading(f.genres[i],x+inset,143,83-inset,cur ? 0 : WHITE,false);
  }
  setStyle(saved);
}

void RadioFaces_Border()
{
  if(theme::current()!=theme::C64 || Radio_State()!=RadioState::Playing) return;
  uint8_t lv[RADIO_BANDS]; Radio_Levels(lv);
  int sum=0; for(int i=0;i<RADIO_BANDS;i++) sum+=lv[i];
  static const uint16_t LOAD[] = {
    rgb(53,40,121), rgb(108,94,181), rgb(112,164,178), rgb(149,149,149),
    rgb(154,210,132), rgb(184,199,111), rgb(111,79,37), rgb(184,105,98)
  };
  int b=theme::def().border_w, n=constrain(2+sum/20,2,8);
  for(int y=0;y<H;y+=4) {
    uint16_t c=LOAD[(y/4+millis()/80)%n];
    rect(0,y,b,min(4,H-y),c); rect(W-b,y,b,min(4,H-y),c);
    if(y<b) rect(b,y,W-2*b,min(4,b-y),c);
    if(y+4>H-b) rect(b,max(y,H-b),W-2*b,min(4,H-y)-(max(y,H-b)-y),c);
  }
}
#endif
