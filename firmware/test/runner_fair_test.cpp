// Host test for the endless runner: is every obstacle the generator makes solvable with the action it asks for?
//   g++ -std=c++11 -O2 -Wall -o /tmp/runner_fair_test firmware/test/runner_fair_test.cpp && /tmp/runner_fair_test [-v]
//
// It runs the real game rules (RunnerSim.h, the same file the device runs) with a bot as the player:
//   * "sees" a bot notices an obstacle only 0.25 s after it is on screen (a moving enemy 0.25 s after it wakes, an archer's
//     arrow 0.25 s after the bow starts to draw), then picks the action the obstacle asks for: a short hop, a held jump or a
//     duck, centres it on the obstacle, and needs 0.06 s to move its finger between the two halves of the screen;
//   * "sloppy" the same, but every action is 0..50 ms early or late (random per obstacle);
//   * "idle" never touches the screen: it must die early;
//   * "jump-only" hops over everything and never ducks: it must die at the first overhead obstacle.
// The first two must survive 5000 m on all 200 seeds, on the narrow (C64) and the wide screen.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <map>
#include <vector>
#include "../src/boards/touch349/gadget/RunnerSim.h"

using namespace runner;

static const float DT = 0.008f;           // the device steps in slices of at most 8 ms
static const float GOAL_M = 5000.0f;      // metres of score
static const int SEEDS = 200;

enum Act { SHORT_HOP, FULL_HOP, DUCK, LEDGE_HOP };

struct Haz {
  uint32_t id;
  uint8_t type;
  double tc, d;  // seconds until the runner's centre meets its centre; time the hit boxes overlap
  Act act;
};

struct Bot {
  int mode;           // 0 idle, 1 right action, 2 jump only
  double react, jitter, latency;
  float vis_ahead;    // how far ahead it can see, cells
  uint32_t jit_rng = 12345;
  std::map<uint32_t, bool> acted;
  std::map<uint32_t, double> jit;
  bool holding = false;
  double rel_at = 0, ready_at = 0;

  double jr(uint32_t id)
  {
    auto it = jit.find(id);
    if (it != jit.end()) return it->second;
    jit_rng ^= jit_rng << 13, jit_rng ^= jit_rng >> 17, jit_rng ^= jit_rng << 5;
    double v = ((jit_rng & 0xFFFF) / 65536.0 * 2 - 1) * jitter;
    jit[id] = v;
    return v;
  }

  static Act actFor(const Ob &o)
  {
    switch (o.type) {
      case BEAM: case SWARM: case WASP: return DUCK;
      case ARROW: case ARCHER: return o.var == 0 ? DUCK : SHORT_HOP;
      case TALL: case HOPPER: return FULL_HOP;
      case LEDGE: return LEDGE_HOP;
      default: return SHORT_HOP;
    }
  }

  void collect(const Sim &s, std::vector<Haz> &out) const
  {
    double S = s.speed, rc = s.dist + RUN_CX, rx0 = s.dist + RUN_X0;
    for (const Ob &o : s.obs) {
      if (!o.alive) continue;
      Haz h;
      h.id = o.id, h.type = o.type, h.act = actFor(o);
      const Dim &dm = DIMS[o.type];
      double hitw = o.w - 2 * dm.inset, c = o.x + o.w * 0.5, ahead = c - rc;
      switch (o.type) {
        case PUDDLE: case CRATE: case TALL: case BIRD: case BEAM: case LEDGE:
          if (o.x - rx0 > vis_ahead - react * S) continue;
          h.tc = ahead / S, h.d = (hitw + 6) / S;
          break;
        case PIT:
          if (o.x - rx0 > vis_ahead - react * S) continue;
          h.tc = ahead / S, h.d = (o.w + 3.5) / S;
          break;
        case BUG: case SWARM: case HOPPER: case WASP:
          if (!o.active || o.t < react) continue;
          h.tc = ahead / (S + dm.v), h.d = (hitw + 6) / (S + dm.v);
          break;
        case ARCHER: {
          if (!o.active || o.t < react || o.fired) continue;
          double rem = DRAW_S - o.t, ac = o.x - 2 + 5;
          h.tc = rem + (ac - (rc + S * rem)) / (S + ARROW_V), h.d = 14.0 / (S + ARROW_V);
          break;
        }
        case ARROW:
          h.tc = (o.x + 5 - rc) / (S + ARROW_V), h.d = 14.0 / (S + ARROW_V);
          break;
        default: continue;
      }
      out.push_back(h);
    }
  }

  double startAt(const Haz &h) const
  {
    switch (h.act) {
      case SHORT_HOP: return h.tc - 0.28;
      case FULL_HOP: return h.tc - 0.37;
      case LEDGE_HOP: return h.tc - h.d / 2 - 0.13;
      default: return h.tc - h.d / 2 - 0.10;
    }
  }

  void act(Sim &s)
  {
    if (mode == 0) return;
    double now = s.clk;
    if (holding && now >= rel_at) {
      s.release();
      holding = false, ready_at = now + latency;
    }
    std::vector<Haz> hz;
    collect(s, hz);
    const Haz *best = nullptr;
    double best_ts = 1e9;
    for (const Haz &h : hz) {
      if (acted[h.id] || h.tc < -h.d) continue;
      double ts = startAt(h);
      if (ts < best_ts) best_ts = ts, best = &h;
    }
    if (!best) return;
    Act a = best->act;
    if (mode == 2 && a == DUCK) a = SHORT_HOP;
    double ts = best_ts;  // seconds from now until the action should start (negative = late)
    if (mode == 2 && best->act == DUCK) ts = best->tc - 0.28;
    if (ts > -jr(best->id)) return;  // jitter: > 0 acts late, < 0 early
    if (holding) {
      s.release();
      holding = false, ready_at = now + latency;
    }
    if (now < ready_at) return;
    if (a != DUCK && !s.on_ground) return;
    acted[best->id] = true;
    if (a == DUCK) {
      s.touchBottom();
      holding = true, rel_at = now + rmax(0.01f, (float)(best->tc + best->d / 2 + 0.03));
    } else {
      s.touchTop();
      holding = true, rel_at = now + (a == FULL_HOP ? 0.30 : 0.07);
    }
  }
};

struct Result {
  bool survived;
  double m;           // metres reached
  int killer;         // type that killed, -1 none (pit falls = PIT)
  int counts[TYPES];  // spawned in this run
  double min_flight;  // shortest arrow flight seen
  int zones;
};

static Result play(uint32_t seed, int mode, double jitter, float view, float vis)
{
  Sim s;
  s.reset(seed * 2654435761u + 7, view);
  Bot b;
  b.mode = mode, b.react = 0.25, b.jitter = jitter, b.latency = 0.06, b.vis_ahead = vis, b.jit_rng = seed * 40503u + 99;
  Result r;
  memset(&r, 0, sizeof(r));
  r.killer = -1, r.min_flight = 99;
  std::map<uint32_t, bool> seen;
  float limit_s = 3000;
  while (!s.dead && s.score() < GOAL_M && s.clk < limit_s) {
    b.act(s);
    s.step(DT);
    for (const Ob &o : s.obs) {
      if (o.alive && !seen[o.id] && o.type != ARROW) seen[o.id] = true, r.counts[o.type]++;
      if (o.alive && o.type == ARROW && !seen[o.id + 1000000]) {
        seen[o.id + 1000000] = true;
        double fl = (o.x + 5 - (s.dist + RUN_CX)) / (s.speed + ARROW_V);
        if (fl < r.min_flight) r.min_flight = fl;
      }
    }
  }
  r.survived = !s.dead;
  r.m = s.score();
  r.zones = Sim::zoneOf(s.score()) + (int)(s.score() / 1600);
  if (s.dead) {
    if (s.fell) {
      r.killer = PIT;
    } else {
      float rb = s.y + RUN_BOT, rt = s.y + (s.ducking ? R_DUCK_TOP : R_TOP), rx0 = s.dist + RUN_X0, rx1 = s.dist + RUN_X1;
      for (const Ob &o : s.obs) {
        float x0, x1, bb, t;
        if (Sim::box(o, x0, x1, bb, t) && rx1 > x0 && rx0 < x1 && rb < t && rt > bb) r.killer = o.type;
      }
    }
  }
  return r;
}

static const char *NAMES[TYPES] = { "puddle", "crate", "tall crate", "low bird", "ground bug", "archer", "beam", "bug swarm", "pit", "hopping bug", "diving wasp", "ledge", "arrow" };

static int fails = 0;
static void check(bool ok, const char *what)
{
  printf("  %-64s %s\n", what, ok ? "ok" : "FAIL");
  if (!ok) fails++;
}

static void runBatch(const char *label, int mode, double jitter, bool must_survive_all, bool verbose)
{
  printf("%s\n", label);
  for (int cfg = 0; cfg < 2; cfg++) {
    float view = cfg == 0 ? 128.0f : 160.0f, vis = cfg == 0 ? 119.2f : 149.0f;
    int ok = 0;
    std::vector<double> ms;
    std::map<int, int> killers;
    long counts[TYPES] = { 0 };
    double min_flight = 99;
    for (int sd = 1; sd <= SEEDS; sd++) {
      Result r = play(sd, mode, jitter, view, vis);
      if (r.survived) ok++;
      ms.push_back(r.m);
      if (!r.survived) killers[r.killer]++;
      for (int t = 0; t < TYPES; t++) counts[t] += r.counts[t];
      if (r.min_flight < min_flight) min_flight = r.min_flight;
      if (verbose && !r.survived) printf("    seed %d died at %.0f m by %s\n", sd, r.m, r.killer >= 0 ? NAMES[r.killer] : "?");
    }
    std::sort(ms.begin(), ms.end());
    printf("  %s screen (%d cells): %d/%d reached %.0f m, deaths at min %.0f / median %.0f m", cfg == 0 ? "narrow" : "wide", (int)view, ok, SEEDS, GOAL_M, ms[0], ms[ms.size() / 2]);
    for (auto &k : killers) printf("  [%s x%d]", k.first >= 0 ? NAMES[k.first] : "?", k.second);
    printf("\n");
    if (mode == 1) {
      printf("    obstacles seen per run (avg):");
      for (int t = 0; t < TYPES; t++) {
        if (t != ARROW) printf(" %s %.0f,", NAMES[t], counts[t] / (double)SEEDS);
      }
      printf("\n    shortest arrow flight %.2f s (the bow is drawn for %.1f s before it)\n", min_flight, DRAW_S);
    }
    char buf[96];
    if (must_survive_all) {
      snprintf(buf, sizeof(buf), "%s: all %d seeds survive %.0f m (%s screen)", label, SEEDS, GOAL_M, cfg == 0 ? "narrow" : "wide");
      check(ok == SEEDS, buf);
    } else if (mode == 0) {
      snprintf(buf, sizeof(buf), "idle bot always dies, median under 100 m (%s screen)", cfg == 0 ? "narrow" : "wide");
      check(ok == 0 && ms[ms.size() / 2] < 100, buf);
    } else {
      snprintf(buf, sizeof(buf), "jump-only bot never gets past 1000 m (%s screen)", cfg == 0 ? "narrow" : "wide");
      check(ok == 0 && ms.back() < 1000, buf);
    }
  }
}

int main(int argc, char **argv)
{
  bool verbose = argc > 1 && !strcmp(argv[1], "-v");
  if (argc > 2 && !strcmp(argv[1], "-d")) {  // trace one seed: -d <seed> [jitter]
    Sim s;
    uint32_t seed = atoi(argv[2]);
    s.reset(seed * 2654435761u + 7, 128);
    Bot b;
    b.mode = 1, b.react = 0.25, b.jitter = argc > 3 ? atof(argv[3]) : 0, b.latency = 0.06, b.vis_ahead = 119.2f, b.jit_rng = seed * 40503u + 99;
    for (int i = 0; i < 400000 && !s.dead && s.score() < GOAL_M; i++) {
      bool h = b.holding;
      b.act(s);
      if (b.holding != h) printf("t=%.3f m=%.0f %s y=%.2f\n", s.clk, s.score(), b.holding ? "press" : "release", s.y);
      s.step(DT);
    }
    printf("end m=%.1f dead=%d\n", s.score(), s.dead);
    return 0;
  }
  if (argc > 2 && !strcmp(argv[1], "-j")) {  // stress: how sloppy can the bot be? -j <seconds>
    runBatch("stress bot", 1, atof(argv[2]), true, false);
    return 0;
  }
  runBatch("bot 250 ms reaction, right action", 1, 0.0, true, verbose);
  runBatch("bot 250 ms reaction, sloppy timing +-50 ms", 1, 0.05, true, verbose);
  runBatch("bot that never acts", 0, 0.0, false, verbose);
  runBatch("bot that only jumps", 2, 0.0, false, verbose);

  // the unlock list, in metres of score
  printf("unlock check\n");
  {
    Sim s;
    s.reset(5, 128);
    int first[TYPES];
    for (int i = 0; i < TYPES; i++) first[i] = 99999;
    std::map<uint32_t, bool> seen;
    Bot b;
    b.mode = 1, b.react = 0.25, b.jitter = 0, b.latency = 0.06, b.vis_ahead = 119.2f;
    for (int run = 1; run <= 40; run++) {
      s.reset(run * 7919u, 128);
      seen.clear();
      b = Bot();
      b.mode = 1, b.react = 0.25, b.jitter = 0, b.latency = 0.06, b.vis_ahead = 119.2f;
      while (!s.dead && s.score() < 1200) {
        b.act(s);
        s.step(DT);
        for (const Ob &o : s.obs) {
          if (o.alive && o.type != ARROW && !seen[o.id]) {
            seen[o.id] = true;
            // the metre at which it was placed ahead of the runner
            int m = (int)(s.score() + (o.x - s.dist) * M_PER_CELL);
            if (m < first[o.type]) first[o.type] = m;
          }
        }
      }
    }
    for (int i = 0; i < TYPES; i++) {
      if (i == ARROW) continue;
      int unlock = 0;
      for (const Spec &sp : SPECS) {
        if (sp.type == i) unlock = sp.unlock;
      }
      char buf[96];
      snprintf(buf, sizeof(buf), "%s first appears at %d m (unlock %d m)", NAMES[i], first[i], unlock);
      check(first[i] >= unlock - 5 && first[i] < unlock + 400, buf);
    }
  }
  printf("%s\n", fails ? "FAILED" : "all passed");
  return fails ? 1 : 0;
}
