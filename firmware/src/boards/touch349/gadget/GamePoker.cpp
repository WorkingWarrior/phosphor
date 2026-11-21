#ifdef BOARD_TOUCH349

#include "GamePoker.h"
#include <Preferences.h>
#include <esp_random.h>
#include "../../../core/Board.h"
#include "../../../core/state.h"
#include "Motion.h"
#include "PokerEval.h"
#include "Radio.h"
#include "Settings.h"
#include "Theme.h"
#include "Vfd.h"

using namespace vfd;

// Video poker, one player, no money: bet 1 credit, five cards, hold the ones you like, draw once. Jacks or Better,
// 9/6 pay table. 100 credits a day (reset at local midnight), best of the day and best ever are kept in NVS ("games").
// DEAL / DRAW: tap the button column on the right, or knock on the table (the motion sensor; a knock that is just the
// finger landing on the glass is ignored).

namespace {

using poker::Hand;

// ---------------------------------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------------------------------

enum class Ph : uint8_t { Idle, Hold, Drawing, Result, Broke };

constexpr uint32_t START_CREDITS = 100;
constexpr int CX0 = 8, CY = 8, CW = 96, CH = 132, CG = 4;  // cards, left aligned
constexpr int SIDE_X = 512, SIDE_W = 120;                  // the button column
constexpr int STRIP_Y = 143;                               // under the cards: HOLD labels, result line

Preferences prefs;
bool prefs_open = false;
Ph ph = Ph::Idle;
int deck[52], hand[5];
int deck_pos = 0;
bool held[5];
uint32_t reveal_at[5];  // millis() at which card i turns face up
Hand result = poker::NOTHING;
int won = 0;
uint32_t credits = START_CREDITS, best_today = START_CREDITS, best_ever = START_CREDITS, day_key = 0;
uint32_t result_ms = 0, lock_until = 0, day_check_ms = 0;
int last_win = -1;  // pay table row of the last win, lit on the start screen

// input
bool touching = false;
uint32_t last_down_ms = 0, last_up_ms = 0, knock_ms = 0;
bool knock_pending = false;

// win jingle: Radio_Beep plays one tone at a time, so the notes are fed from tick
int jingle_hz[8], jingle_ms[8], jingle_n = 0, jingle_i = 0;
uint32_t jingle_next = 0;

uint32_t today()
{
  DateTime n = board().now();
  return (uint32_t)n.year * 10000 + n.month * 100 + n.day;
}

void openPrefs()
{
  if (prefs_open) return;
  prefs.begin("games", false);
  prefs_open = true;
}

void saveAll()
{
  openPrefs();
  prefs.putUInt("pk_cr", credits);
  prefs.putUInt("pk_day", day_key);
  prefs.putUInt("pk_bt", best_today);
  prefs.putUInt("pk_be", best_ever);
}

void newDay()
{
  credits = START_CREDITS;
  best_today = START_CREDITS;
  day_key = today();
  if (best_ever < START_CREDITS) best_ever = START_CREDITS;
  saveAll();
}

void loadAll()
{
  openPrefs();
  credits = prefs.getUInt("pk_cr", START_CREDITS);
  day_key = prefs.getUInt("pk_day", 0);
  best_today = prefs.getUInt("pk_bt", START_CREDITS);
  best_ever = prefs.getUInt("pk_be", START_CREDITS);
  if (day_key != today()) newDay();
}

// ---------------------------------------------------------------------------------------------------------------------
// Game
// ---------------------------------------------------------------------------------------------------------------------

void shuffle()
{
  for (int i = 0; i < 52; i++) deck[i] = i;
  for (int i = 51; i > 0; i--) {  // Fisher-Yates
    int j = (int)(esp_random() % (uint32_t)(i + 1));
    int t = deck[i];
    deck[i] = deck[j];
    deck[j] = t;
  }
}

bool allRevealed(uint32_t now)
{
  for (int i = 0; i < 5; i++) {
    if ((int32_t)(now - reveal_at[i]) < 0) return false;
  }
  return true;
}

void startJingle(Hand h)
{
  static const int NOTES[8] = { 523, 587, 659, 784, 880, 1046, 1175, 1318 };
  jingle_n = min(8, 2 + (int)h);
  for (int i = 0; i < jingle_n; i++) {
    jingle_hz[i] = NOTES[i];
    jingle_ms[i] = h >= poker::FLUSH ? 90 : 110;
  }
  jingle_i = 0;
  jingle_next = millis();
}

void deal()
{
  if (credits == 0) return;
  credits--;
  shuffle();
  for (int i = 0; i < 5; i++) {
    hand[i] = deck[i];
    held[i] = false;
    reveal_at[i] = millis() + 80 + i * 95;
  }
  deck_pos = 5;
  result = poker::NOTHING;
  won = 0;
  ph = Ph::Hold;
  lock_until = reveal_at[4] + 150;
  saveAll();
  Radio_Beep(520, 18);
  jingle_n = 0;
}

// The draw is over: pay, remember the bests.
void finish()
{
  result = poker::evaluate(hand);
  won = poker::payout(result);
  credits += won;
  if (credits > best_today) best_today = credits;
  if (credits > best_ever) best_ever = credits;
  last_win = won ? (int)result : -1;
  saveAll();
  ph = Ph::Result;
  result_ms = millis();
  lock_until = millis() + 400;
  if (won) {
    startJingle(result);
  } else {
    Radio_Beep(196, 140);
  }
}

void draw()
{
  uint32_t now = millis();
  int k = 0;
  for (int i = 0; i < 5; i++) {
    if (!held[i]) {
      hand[i] = deck[deck_pos++];
      reveal_at[i] = now + 120 + k * 110;
      k++;
    }
  }
  ph = Ph::Drawing;
  lock_until = now + 120 + k * 110;
  if (!k) finish();  // all held: nothing to turn
  else Radio_Beep(440, 18);
}

void action()
{
  uint32_t now = millis();
  if ((int32_t)(now - lock_until) < 0) return;
  if (ph == Ph::Idle || ph == Ph::Result) {
    deal();
  } else if (ph == Ph::Hold && allRevealed(now)) {
    draw();
  }
  state.dirty = true;
}

void toggleHold(int i)
{
  if (ph != Ph::Hold || (int32_t)(millis() - reveal_at[i]) < 0) return;
  held[i] = !held[i];
  Radio_Beep(held[i] ? 880 : 660, 25);
  state.dirty = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Names and pay table
// ---------------------------------------------------------------------------------------------------------------------

const char *handName(Hand h)
{
  switch (h) {
    case poker::ROYAL_FLUSH: return "ROYAL FLUSH";
    case poker::STRAIGHT_FLUSH: return L("STRAIGHT FLUSH", "SIRALI RENK");
    case poker::FOUR_KIND: return L("FOUR OF A KIND", "KARE");
    case poker::FULL_HOUSE: return L("FULL HOUSE", "FULL");
    case poker::FLUSH: return L("FLUSH", "RENK");
    case poker::STRAIGHT: return L("STRAIGHT", "SIRA");
    case poker::THREE_KIND: return L("THREE OF A KIND", "ÜÇLÜ");
    case poker::TWO_PAIR: return L("TWO PAIR", "İKİ ÇİFT");
    case poker::JACKS_OR_BETTER: return L("JACKS OR BETTER", "VALE VEYA ÜSTÜ");
    default: return L("NO WIN", "KAZANÇ YOK");
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Drawing
// ---------------------------------------------------------------------------------------------------------------------

// Suits as strings, 9 wide, each '#' a block.
const char *const SPADE[] = { "....#....", "...###...", "..#####..", ".#######.", "#########", "#########", "#########", ".##.#.##.", "....#....", "...###..." };
const char *const HEART[] = { ".##...##.", "####.####", "#########", "#########", ".#######.", "..#####..", "...###...", "....#...." };
const char *const DIAMOND[] = { "....#....", "...###...", "..#####..", ".#######.", "#########", ".#######.", "..#####..", "...###...", "....#...." };
const char *const CLUB[] = { "...###...", "..#####..", "..#####..", "...###...", ".###.###.", "#########", "#########", ".##.#.##.", "....#....", "...###..." };
const int SUIT_ROWS[4] = { 10, 8, 9, 10 };
const char *const *const SUITS[4] = { SPADE, HEART, DIAMOND, CLUB };
const char *const RANKS[13] = { "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K", "A" };

const char *const ICON_POKER[] = { "..########..", ".#........#.", ".#...##...#.", ".#..####..#.", ".#.######.#.", ".#.######.#.",
                                   ".#.######.#.", ".#..#.#...#.", ".#...##...#.", ".#..####..#.", ".#........#.", "..########.." };

void iconPoker(int x, int y, uint16_t c) { sprite(ICON_POKER, 12, x, y, 4, c); }

struct Pal {
  uint16_t face, edge, inkb, inkr, hold, back1, back2;
};

Pal pal()
{
  using px::rgb;
  switch (theme::current()) {
    case theme::C64: return { 0xFFFF, rgb(149, 149, 149), rgb(0, 0, 0), rgb(136, 57, 57), HOT, HALO, BG };
    case theme::AMIGA: return { 0xFFFF, rgb(0, 0, 34), rgb(0, 0, 34), rgb(221, 68, 0), HOT, rgb(0, 68, 136), rgb(255, 136, 0) };
    case theme::ATARI: return { rgb(236, 236, 236), rgb(160, 160, 200), rgb(24, 26, 80), rgb(200, 72, 48), HOT, rgb(24, 26, 80), rgb(160, 160, 200) };
    default: return { GHOST, DIM, LIT, HOT, HOT, GHOST, DIM };
  }
}

void drawSuit(int s, int x, int y, int scale, uint16_t color)
{
  if (style().glow) {  // VFD: a thin halo around it
    sprite(SUITS[s], SUIT_ROWS[s], x - 1, y, scale, HALO);
    sprite(SUITS[s], SUIT_ROWS[s], x + 1, y, scale, HALO);
    sprite(SUITS[s], SUIT_ROWS[s], x, y - 1, scale, HALO);
    sprite(SUITS[s], SUIT_ROWS[s], x, y + 1, scale, HALO);
  }
  sprite(SUITS[s], SUIT_ROWS[s], x, y, scale, color);
}

void drawBack(int x, int y, const Pal &p)
{
  rect(x, y, CW, CH, p.back1);
  frame(x, y, CW, CH, p.edge);
  for (int j = 0; j < (CH - 12) / 8; j++) {
    for (int i = 0; i < (CW - 12) / 8; i++) {
      if ((i + j) & 1) rect(x + 6 + i * 8, y + 6 + j * 8, 8, 8, p.back2);
    }
  }
}

void drawFace(int c, int x, int y, const Pal &p)
{
  int s = poker::suitOf(c), r = poker::rankOf(c);
  bool red = s == 1 || s == 2;
  uint16_t ink = red ? p.inkr : p.inkb;
  rect(x, y, CW, CH, p.face);
  Ink t = style().glow ? (red ? HOT_INK : LIT_INK) : Ink{ ink };
  text(RANKS[r], x + 7, y + 7, 4, t);
  drawSuit(s, x + 8, y + 42, 2, ink);
  drawSuit(s, x + (CW - 45) / 2, y + CH - 55, 5, ink);
}

void drawCard(int i, const Pal &p, uint32_t now)
{
  int x = CX0 + i * (CW + CG), y = CY;
  bool up = (int32_t)(now - reveal_at[i]) >= 0;
  if (style().glow) frame(x - 1, y - 1, CW + 2, CH + 2, HALO);
  if (!up) {
    drawBack(x, y, p);
    return;
  }
  drawFace(hand[i], x, y, p);
  if (held[i]) {
    uint16_t hc = p.hold;
    frame(x, y, CW, CH, hc);
    frame(x + 1, y + 1, CW - 2, CH - 2, hc);
    frame(x + 2, y + 2, CW - 4, CH - 4, hc);
    const char *h = L("HOLD", "TUT");
    text(h, x + (CW - textWidth(h, 2)) / 2, STRIP_Y + 2, 2, HOT_INK);
  } else {
    frame(x, y, CW, CH, p.edge);
  }
}

// The pay table, with the row of the last win lit.
void drawPayTable()
{
  text(L("JACKS OR BETTER  BET 1", "VALE VEYA ÜSTÜ  BAHİS 1"), CX0, 10, 2, DIM_INK);
  static const Hand ORDER[9] = { poker::ROYAL_FLUSH, poker::STRAIGHT_FLUSH, poker::FOUR_KIND, poker::FULL_HOUSE, poker::FLUSH,
                                 poker::STRAIGHT, poker::THREE_KIND, poker::TWO_PAIR, poker::JACKS_OR_BETTER };
  for (int i = 0; i < 9; i++) {
    int col = i / 5, row = i % 5;
    int x = CX0 + col * 252, y = 34 + row * 22, w = 240;
    bool lit = ORDER[i] == last_win;
    if (lit) rect(x - 4, y - 3, w + 8, 20, style().glow ? GHOST : HALO);
    const Ink &ink = lit ? HOT_INK : LIT_INK;
    text(handName(ORDER[i]), x, y, 2, ink);
    char b[8];
    snprintf(b, sizeof(b), "%d", poker::payout(ORDER[i]));
    text(b, x + w - textWidth(b, 2), y, 2, ink);
  }
  // the tenth cell: how to start
  text(L("KNOCK ON THE TABLE", "MASAYA VUR"), CX0 + 252, 34 + 4 * 22, 2, DIM_INK);
}

void button(int x, int y, int w, int h, const char *label, bool on)
{
  int tw = textWidth(label, 3);
  switch (theme::current()) {
    case theme::C64:
      rect(x, y, w, h, on ? HALO : BG);
      frame(x, y, w, h, HALO);
      text(label, x + (w - tw) / 2, y + (h - 21) / 2, 3, Ink{ on ? (uint16_t)0xFFFF : HALO });
      break;
    case theme::AMIGA: {
      uint16_t grey = px::rgb(153, 153, 170);
      rect(x, y, w, h, on ? grey : px::rgb(0, 68, 136));
      rect(x, y, w, 2, 0xFFFF);
      rect(x, y, 2, h, 0xFFFF);
      rect(x, y + h - 2, w, 2, px::rgb(0, 0, 34));
      rect(x + w - 2, y, 2, h, px::rgb(0, 0, 34));
      text(label, x + (w - tw) / 2, y + (h - 21) / 2, 3, Ink{ on ? px::rgb(0, 0, 34) : px::rgb(100, 140, 190) });
      break;
    }
    case theme::ATARI:
      rect(x, y, w, h, on ? px::rgb(252, 188, 116) : px::rgb(24, 26, 80));
      rect(x, y + h - 4, w, 4, on ? px::rgb(208, 128, 92) : px::rgb(0, 0, 0));
      text(label, x + (w - tw) / 2, y + (h - 21) / 2 - 2, 3, Ink{ on ? (uint16_t)0 : px::rgb(160, 160, 200) });
      break;
    default:
      if (on) {
        frame(x - 1, y - 1, w + 2, h + 2, HALO);
        rect(x, y, w, h, GHOST);
      }
      frame(x, y, w, h, on ? LIT : DIM);
      if (on) frame(x + 2, y + 2, w - 4, h - 4, HALO);
      text(label, x + (w - tw) / 2, y + (h - 21) / 2, 3, on ? LIT_INK : DIM_INK);
      break;
  }
}

void drawSide(uint32_t now)
{
  char b[24];
  text(L("CREDITS", "KREDİ"), SIDE_X, 8, 2, DIM_INK);
  snprintf(b, sizeof(b), "%lu", (unsigned long)credits);
  int p = textWidth(b, 4) <= SIDE_W ? 4 : 3;
  text(b, SIDE_X, 26, p, credits == 0 ? HOT_INK : LIT_INK);
  snprintf(b, sizeof(b), "%s %lu", L("TODAY", "BUGÜN"), (unsigned long)best_today);
  text(b, SIDE_X, 64, 2, DIM_INK);
  snprintf(b, sizeof(b), "%s %lu", L("BEST", "REKOR"), (unsigned long)best_ever);
  text(b, SIDE_X, 80, 2, DIM_INK);
  const char *label;
  bool on = true;
  if (ph == Ph::Hold && allRevealed(now)) {
    label = L("DRAW", "ÇEK");
  } else if (ph == Ph::Hold || ph == Ph::Drawing) {
    label = "...";
    on = false;
  } else if (credits == 0) {
    label = L("OUT", "BİTTİ");
    on = false;
  } else {
    label = L("DEAL", "DAĞIT");
  }
  button(SIDE_X, 100, SIDE_W, 40, label, on);
}

void pokerDraw(const DateTime &)
{
  uint32_t now = millis();
  Pal p = pal();
  clear();
  if (ph == Ph::Idle) {
    drawPayTable();
    text(L("TAP DEAL, OR KNOCK ON THE TABLE", "DOKUN: DAĞIT, YA DA MASAYA VUR"), CX0, STRIP_Y + 2, 2, LIT_INK);
  } else if (ph == Ph::Broke) {
    const char *a = L("OUT OF CREDITS", "KREDİ BİTTİ"), *c = L("BACK TOMORROW", "YARIN TEKRAR");
    int w = 496;
    text(a, CX0 + (w - textWidth(a, 5)) / 2, 30, 5, HOT_INK);
    text(c, CX0 + (w - textWidth(c, 4)) / 2, 84, 4, LIT_INK);
  } else {
    for (int i = 0; i < 5; i++) drawCard(i, p, now);
    if (ph == Ph::Hold) {
      bool any = false;
      for (int i = 0; i < 5; i++) any = any || held[i];
      if (!any && allRevealed(now)) {
        const char *h = L("TAP CARDS TO HOLD, THEN DRAW", "KARTA DOKUN: TUT, SONRA ÇEK");
        text(h, CX0 + (496 - textWidth(h, 2)) / 2, STRIP_Y + 2, 2, DIM_INK);
      }
    } else if (ph == Ph::Result) {
      const char *n = handName(result);
      bool flash = won && now - result_ms < 1600 && ((now - result_ms) / 200) % 2;
      Ink ink = won ? (flash ? LIT_INK : HOT_INK) : DIM_INK;
      text(n, CX0, STRIP_Y, 3, ink);
      if (won) {
        char b[12];
        snprintf(b, sizeof(b), "+%d", won);
        text(b, CX0 + 496 - textWidth(b, 3), STRIP_Y, 3, ink);
      }
    }
  }
  drawSide(now);
  if (theme::current() == theme::C64) {  // the machine's border (the app draws the whole panel)
    const theme::Def &d = theme::def();
    rect(0, 0, W, d.border_w, d.border);
    rect(0, H - d.border_w, W, d.border_w, d.border);
    rect(0, 0, d.border_w, H, d.border);
    rect(W - d.border_w, 0, d.border_w, H, d.border);
  }
}

// ---------------------------------------------------------------------------------------------------------------------
// Input and loop
// ---------------------------------------------------------------------------------------------------------------------

void pokerEnter()
{
  loadAll();
  if (ph == Ph::Drawing) finish();
  if (ph == Ph::Hold) {
    // back in the middle of a hand: its cards stay face up
    for (int i = 0; i < 5; i++) reveal_at[i] = millis();
  } else {
    ph = credits ? Ph::Idle : Ph::Broke;
  }
  touching = false;
  knock_pending = false;
  Motion_TakeKnock();  // forget old knocks
  lock_until = millis() + 300;
  jingle_n = 0;
  day_check_ms = millis();
}

void pokerExit()
{
  if (ph == Ph::Drawing) finish();
  jingle_n = 0;
}

bool pokerGesture(Gesture g, int x, int y)
{
  if (g == Gesture::LongPress) return true;
  // Tap, and an up/down swipe too: a sloppy tap. (Left/right swipes are the framework's: back to the Play menu.)
  if (g != Gesture::Tap && g != Gesture::SwipeUp && g != Gesture::SwipeDown) return false;
  if (x >= SIDE_X - 6) {
    action();
    return true;
  }
  if (ph == Ph::Hold) {
    int i = constrain((x - CX0 + CG / 2) / (CW + CG), 0, 4);
    toggleHold(i);
  } else {
    action();  // no cards to hold: a tap anywhere deals
  }
  return true;
}

void pokerTouch(bool down, int, int)
{
  touching = down;
  if (down) last_down_ms = millis();
  else last_up_ms = millis();
}

// Is the touch driver's last edge within 250 ms of t?
bool touchNear(uint32_t t)
{
  int32_t d = (int32_t)(last_down_ms - t), u = (int32_t)(last_up_ms - t);
  return touching || (d > -250 && d < 250) || (u > -250 && u < 250);
}

void pokerTick()
{
  uint32_t now = millis();
  // a knock lands slightly before the touch driver reports the finger (30 ms polling): decide 140 ms later
  if (Motion_TakeKnock()) {
    knock_ms = now;
    knock_pending = true;
  }
  if (knock_pending && now - knock_ms >= 140) {
    knock_pending = false;
    if (!touchNear(knock_ms)) action();
  }
  if (ph == Ph::Drawing && allRevealed(now)) {
    finish();
    state.dirty = true;
  }
  if (ph == Ph::Result && credits == 0 && now - result_ms > 2800) {
    ph = Ph::Broke;
    state.dirty = true;
  }
  if (jingle_n && jingle_i < jingle_n && (int32_t)(now - jingle_next) >= 0) {
    Radio_Beep(jingle_hz[jingle_i], jingle_ms[jingle_i]);
    jingle_next = now + jingle_ms[jingle_i] + 35;
    jingle_i++;
  }
  if (now - day_check_ms > 5000) {  // midnight passed while it was open
    day_check_ms = now;
    if (day_key != today() && ph != Ph::Hold && ph != Ph::Drawing) {
      newDay();
      ph = Ph::Idle;
      last_win = -1;
      state.dirty = true;
    }
  }
}

uint32_t pokerFrameMs()
{
  uint32_t now = millis();
  bool anim = (ph == Ph::Result && won && now - result_ms < 1700) || ph == Ph::Drawing;
  if (ph == Ph::Hold && !allRevealed(now)) anim = true;
  return anim ? 40 : 250;
}

}  // namespace

const App APP_POKER = { "poker", "POKER", "POKER", iconPoker, true, true, pokerEnter, pokerExit, pokerDraw, pokerGesture, pokerTick, pokerFrameMs, pokerTouch };

#endif
