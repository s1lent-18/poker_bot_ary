#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <iostream>
#include <vector>

#include "cfr/cfr.h"
#include "cfr/kuhn.h"
#include "cfr/kuhn_state.h"
#include "cfr/leduc.h"
#include "core/rng.h"

using namespace poker;
using namespace poker::cfr;

// Card ids: J = 0,1   Q = 2,3   K = 4,5.  The first two actions deal the hole cards.
static LeducState play(std::initializer_list<int> acts) {
    LeducState s;
    for (int a : acts) s = s.child(a);
    return s;
}

// ------------------------------------------------------------ Leduc rules

TEST(Leduc, ChanceOutcomes) {
    LeducState s;
    EXPECT_TRUE(s.is_chance());
    EXPECT_EQ(s.actions().size(), 6u);
    double sum = 0;
    for (double p : s.chance_probs()) sum += p;
    EXPECT_NEAR(sum, 1.0, 1e-12);
    const LeducState s1 = s.child(4);
    EXPECT_EQ(s1.actions().size(), 5u);
    EXPECT_FALSE(s1.child(0).is_chance());
}

TEST(Leduc, BettingActionsAndRaiseCap) {
    const LeducState s = play({4, 0});  // P0 holds K, P1 holds J
    EXPECT_EQ(s.current_player(), 0);
    EXPECT_EQ(s.actions(), (std::vector<int>{1, 2}));  // check / bet
    const LeducState r = s.child(2);                    // P0 bets
    EXPECT_EQ(r.current_player(), 1);
    EXPECT_EQ(r.actions(), (std::vector<int>{0, 1, 2}));
    const LeducState rr = r.child(2);                   // P1 raises
    EXPECT_EQ(rr.current_player(), 0);
    EXPECT_EQ(rr.actions(), (std::vector<int>{0, 1}));  // two raises is the cap
}

TEST(Leduc, ShowdownByHighCard) {
    const LeducState s = play({4, 0, 1, 1, 2, 1, 1});  // K vs J, board Q, checks throughout
    ASSERT_TRUE(s.is_terminal());
    EXPECT_EQ(s.utility(0), 1.0);
    EXPECT_EQ(s.utility(1), -1.0);
}

TEST(Leduc, PairBeatsHighCard) {
    const LeducState s = play({4, 0, 1, 1, 1, 1, 1});  // board is the other J: P1 has a pair
    ASSERT_TRUE(s.is_terminal());
    EXPECT_EQ(s.utility(0), -1.0);
}

TEST(Leduc, TieSplitsThePot) {
    const LeducState s = play({4, 5, 1, 1, 2, 1, 1});  // K vs K
    ASSERT_TRUE(s.is_terminal());
    EXPECT_EQ(s.utility(0), 0.0);
}

TEST(Leduc, FoldPaysTheRaiser) {
    const LeducState s = play({4, 0, 2, 0});  // P0 bets, P1 folds
    ASSERT_TRUE(s.is_terminal());
    EXPECT_EQ(s.utility(0), 1.0);
    EXPECT_EQ(s.utility(1), -1.0);
}

TEST(Leduc, RaiseWarThenCall) {
    // P0 bets (3 in), P1 raises (5 in), P0 calls (5 in); round 2 checks; K beats J.
    const LeducState s = play({4, 0, 2, 2, 1, 2, 1, 1});
    ASSERT_TRUE(s.is_terminal());
    EXPECT_EQ(s.utility(0), 5.0);
}

TEST(Leduc, SecondRoundBetsAreBigger) {
    const LeducState called = play({4, 0, 1, 1, 2, 2, 1});  // bet 4 and call: 1 + 4 = 5 each
    ASSERT_TRUE(called.is_terminal());
    EXPECT_EQ(called.utility(0), 5.0);
    const LeducState folded = play({4, 0, 1, 1, 2, 2, 0});
    EXPECT_EQ(folded.utility(0), 1.0);
}

TEST(Leduc, InfoKeys) {
    EXPECT_EQ(play({4, 0}).info_key(), "0K:");
    EXPECT_EQ(play({4, 0, 2}).info_key(), "1J:r");
    EXPECT_EQ(play({4, 0, 1, 1, 2}).info_key(), "0K|Q:cc/");
}

TEST(Leduc, RandomPlayoutsAreZeroSumAndBounded) {
    Rng rng(5);
    for (int i = 0; i < 20000; ++i) {
        LeducState s;
        while (!s.is_terminal()) {
            const auto a = s.actions();
            ASSERT_FALSE(a.empty());
            s = s.child(a[rng.below(a.size())]);
        }
        ASSERT_EQ(s.utility(0) + s.utility(1), 0.0);
        ASSERT_LE(std::abs(s.utility(0)), 13.0);
    }
}

TEST(Leduc, HasExpectedNumberOfInfoSets) {
    Cfr<LeducState> solver{LeducState{}};
    solver.iterate();  // one full-tree iteration visits every information set
    EXPECT_EQ(solver.num_info_sets(), 288u);
}

// ------------------------------------- generic code validated against known Kuhn results

static StrategyFn always_bet() {
    return [](const std::string&, int n) {
        std::vector<double> v(n, 0.0);
        v[n - 1] = 1.0;
        return v;
    };
}

TEST(GenericCfr, ExploitabilityOfAlwaysBetMatchesTheory) {
    EXPECT_NEAR(exploitability(KuhnState{}, always_bet()), 1.0 / 3.0, 1e-9);
}

TEST(GenericCfr, ConvergesToKuhnEquilibrium) {
    Cfr<KuhnState> solver{KuhnState{}};
    solver.train(20000);
    const StrategyFn pol = solver.average_policy();
    EXPECT_NEAR(profile_value(KuhnState{}, pol), -1.0 / 18.0, 5e-3);
    EXPECT_LT(exploitability(KuhnState{}, pol), 5e-3);
}

TEST(GenericCfr, BestResponseMatchesBruteForceOnKuhn) {
    Cfr<KuhnState> solver{KuhnState{}};
    solver.train(2000);
    const StrategyFn fn = solver.average_policy();
    const poker::kuhn::Policy pol = [&](int card, const std::string& h) {
        return fn(std::string(1, "JQK"[card]) + h, 2)[1];  // P(bet or call)
    };
    EXPECT_NEAR(exploitability(KuhnState{}, fn), poker::kuhn::exploitability(pol, pol), 1e-9);
}

// ---------------------------------------------------------------------- Leduc CFR

TEST(LeducCfr, ExploitabilityFallsAndBeatsUniform) {
    const LeducState root;
    Cfr<LeducState> solver(root);

    solver.train(10);
    const double e10 = exploitability(root, solver.average_policy());
    solver.train(190);
    const double e200 = exploitability(root, solver.average_policy());
    solver.train(1800);
    const StrategyFn pol = solver.average_policy();
    const double e2000 = exploitability(root, pol);
    const double value = profile_value(root, pol);
    const double uniform = exploitability(root, uniform_strategy());

    std::cout << "Leduc exploitability: 10 it = " << e10 << ", 200 it = " << e200
              << ", 2000 it = " << e2000 << ", uniform = " << uniform
              << "; game value = " << value << "\n";

    EXPECT_GE(e2000, -1e-9);  // exploitability is never negative
    EXPECT_LT(e200, e10);
    EXPECT_LT(e2000, e200);
    EXPECT_LT(e2000, uniform);
    EXPECT_LT(e2000, 0.25);
    EXPECT_NEAR(value, -0.0856, 0.1);  // loose check against the reference value
}