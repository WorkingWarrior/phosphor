// Host test for the video poker evaluator:
//   g++ -std=c++11 -Wall -o /tmp/poker_eval_test firmware/test/poker_eval_test.cpp && /tmp/poker_eval_test
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "../src/boards/touch349/gadget/PokerEval.h"

using namespace poker;

static int fails = 0, checks = 0;

// "AS", "10H", "KD", "2C": rank then suit letter (S H D C).
static int card(const char *s)
{
  int r;
  const char *p = s;
  if (p[0] == '1' && p[1] == '0') {
    r = 8, p += 2;
  } else {
    switch (*p) {
      case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': r = *p - '2'; break;
      case 'J': r = 9; break;
      case 'Q': r = 10; break;
      case 'K': r = 11; break;
      default: r = 12; break;  // A
    }
    p++;
  }
  int su = *p == 'S' ? 0 : *p == 'H' ? 1 : *p == 'D' ? 2 : 3;
  return su * 13 + r;
}

static void expect(const char *hand, Hand want)
{
  int c[5];
  char buf[64];
  snprintf(buf, sizeof(buf), "%s", hand);
  int n = 0;
  for (char *t = strtok(buf, " "); t && n < 5; t = strtok(nullptr, " ")) c[n++] = card(t);
  Hand got = evaluate(c);
  checks++;
  if (got != want) {
    fails++;
    printf("FAIL %-22s got %d want %d\n", hand, got, want);
  }
}

int main()
{
  expect("10H JH QH KH AH", ROYAL_FLUSH);
  expect("AS KS QS JS 10S", ROYAL_FLUSH);     // order does not matter
  expect("9H 10H JH QH KH", STRAIGHT_FLUSH);  // king-high straight flush is not royal
  expect("AH 2H 3H 4H 5H", STRAIGHT_FLUSH);   // ace-low (wheel) straight flush
  expect("5C 6C 7C 8C 9C", STRAIGHT_FLUSH);
  expect("9S 9H 9D 9C 2S", FOUR_KIND);
  expect("2S AH AD AC AS", FOUR_KIND);
  expect("3S 3H 3D KC KS", FULL_HOUSE);
  expect("KS KH 3D 3C 3S", FULL_HOUSE);
  expect("2H 7H 9H JH KH", FLUSH);
  expect("AS 3S 4S 5S 9S", FLUSH);            // ace-low-looking but not a straight
  expect("AS 2H 3D 4C 5S", STRAIGHT);         // ace-low (wheel)
  expect("10S JH QD KC AS", STRAIGHT);        // ace-high
  expect("5S 6H 7D 8C 9S", STRAIGHT);
  expect("QS KH AD 2C 3S", NOTHING);          // no wrap-around straights
  expect("JS QH KD AC 2S", NOTHING);
  expect("KS AH 2D 3C 4S", NOTHING);
  expect("8S 8H 8D KC 2S", THREE_KIND);
  expect("2S 2H 2D 9C AS", THREE_KIND);
  expect("4S 4H 9D 9C AS", TWO_PAIR);
  expect("2S 2H AD AC 5S", TWO_PAIR);
  expect("JS JH 2D 5C 9S", JACKS_OR_BETTER);  // the threshold: jacks pay
  expect("QS QH 2D 5C 9S", JACKS_OR_BETTER);
  expect("KS KH 2D 5C 9S", JACKS_OR_BETTER);
  expect("AS AH 2D 5C 9S", JACKS_OR_BETTER);
  expect("10S 10H 2D 5C 9S", NOTHING);        // tens do not pay
  expect("2S 2H 3D 5C 9S", NOTHING);
  expect("9S 9H JD 5C AS", NOTHING);
  expect("2S 4H 6D 8C 10S", NOTHING);
  expect("JS QH KD AC 9S", NOTHING);          // four to a straight

  static const int want_pay[HAND_COUNT] = { 0, 1, 2, 3, 4, 6, 9, 25, 50, 250 };
  for (int h = 0; h < HAND_COUNT; h++) {
    checks++;
    if (payout((Hand)h) != want_pay[h]) fails++, printf("FAIL payout %d\n", h);
  }

  // Every 5-card hand of the deck: the counts per hand type must be the known ones (2,598,960 hands in all).
  static long count[HAND_COUNT];
  long total = 0;
  for (int a = 0; a < 52; a++)
    for (int b = a + 1; b < 52; b++)
      for (int c = b + 1; c < 52; c++)
        for (int d = c + 1; d < 52; d++)
          for (int e = d + 1; e < 52; e++) {
            int h[5] = { a, b, c, d, e };
            count[evaluate(h)]++;
            total++;
          }
  static const long want_count[HAND_COUNT] = { 2062860, 337920, 123552, 54912, 10200, 5108, 3744, 624, 36, 4 };
  for (int h = 0; h < HAND_COUNT; h++) {
    checks++;
    if (count[h] != want_count[h]) fails++, printf("FAIL count of hand %d: %ld want %ld\n", h, count[h], want_count[h]);
  }
  printf("%d checks, %ld hands, %d failures\n", checks, total, fails);
  return fails ? 1 : 0;
}
