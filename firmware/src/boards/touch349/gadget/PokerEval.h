#pragma once

// Video poker hand evaluator (Jacks or Better), plain C++ with no Arduino dependency so firmware/test/poker_eval_test.cpp
// can build it on the host. A card is 0..51: rank = c % 13 (0 = two ... 8 = ten, 9 = jack, 10 = queen, 11 = king,
// 12 = ace), suit = c / 13 (0 spade, 1 heart, 2 diamond, 3 club).

namespace poker {

enum Hand { NOTHING = 0, JACKS_OR_BETTER, TWO_PAIR, THREE_KIND, STRAIGHT, FLUSH, FULL_HOUSE, FOUR_KIND, STRAIGHT_FLUSH, ROYAL_FLUSH, HAND_COUNT };

// Credits paid for one credit bet (the standard 9/6 table).
inline int payout(Hand h)
{
  static const int PAY[HAND_COUNT] = { 0, 1, 2, 3, 4, 6, 9, 25, 50, 250 };
  return PAY[h];
}

inline int rankOf(int c) { return c % 13; }
inline int suitOf(int c) { return c / 13; }

inline Hand evaluate(const int *c)
{
  int cnt[13] = {};
  for (int i = 0; i < 5; i++) cnt[rankOf(c[i])]++;
  bool flush = true;
  for (int i = 1; i < 5; i++) flush = flush && suitOf(c[i]) == suitOf(c[0]);

  int pairs = 0, threes = 0, fours = 0, high_pair = 0, distinct = 0;
  for (int r = 0; r < 13; r++) {
    if (cnt[r]) distinct++;
    if (cnt[r] == 2) pairs++, high_pair = r;
    if (cnt[r] == 3) threes++;
    if (cnt[r] == 4) fours++;
  }

  bool straight = false, royal = false;
  if (distinct == 5) {
    int lo = 12, hi = 0;
    for (int r = 0; r < 13; r++) {
      if (cnt[r]) {
        if (r < lo) lo = r;
        if (r > hi) hi = r;
      }
    }
    straight = hi - lo == 4;  // 2-3-4-5-6 ... 9-10-J-Q-K, and 10-J-Q-K-A
    if (!straight && cnt[12] && cnt[0] && cnt[1] && cnt[2] && cnt[3]) straight = true;  // the wheel: A-2-3-4-5
    royal = lo == 8 && hi == 12;                                                      // 10-J-Q-K-A
  }

  if (straight && flush) return royal ? ROYAL_FLUSH : STRAIGHT_FLUSH;
  if (fours) return FOUR_KIND;
  if (threes && pairs) return FULL_HOUSE;
  if (flush) return FLUSH;
  if (straight) return STRAIGHT;
  if (threes) return THREE_KIND;
  if (pairs == 2) return TWO_PAIR;
  if (pairs == 1 && high_pair >= 9) return JACKS_OR_BETTER;  // a pair of jacks or higher
  return NOTHING;
}

}  // namespace poker
