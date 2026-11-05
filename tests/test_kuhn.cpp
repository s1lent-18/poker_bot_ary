#include <gtest/gtest.h>

#include "cfr/kuhn.h"

using namespace poker::kuhn;

// An explicit Nash equilibrium (the alpha = 0 member of player 1's equilibrium family):
//  Player 1 (acts first): always checks first; facing a bet after checking:
//     J folds, Q calls 1/3, K calls.
//  Player 2: after a check: J bets 1/3, Q checks, K bets. Facing a bet: J folds, Q calls 1/3, K calls.
static Policy nash_p0() {
    return [](int card, const std::string& h) -> double {
        if (h.empty()) return 0.0;
        return card == 0 ? 0.0 : (card == 1 ? 1.0 / 3.0 : 1.0);  // h == "pb"
    };
}
static Policy nash_p1() {
    return [](int card, const std::string& h) -> double {
        if (h == "p") return card == 0 ? 1.0 / 3.0 : (card == 1 ? 0.0 : 1.0);
        return card == 0 ? 0.0 : (card == 1 ? 1.0 / 3.0 : 1.0);  // h == "b"
    };
}

TEST(Kuhn, TerminalPayoffs) {
    double p;
    EXPECT_TRUE(terminal_payoff({2, 0}, "pp", p));  EXPECT_EQ(p, 1);
    EXPECT_TRUE(terminal_payoff({0, 2}, "pp", p));  EXPECT_EQ(p, -1);
    EXPECT_TRUE(terminal_payoff({0, 2}, "bp", p));  EXPECT_EQ(p, 1);    // player 1 folds
    EXPECT_TRUE(terminal_payoff({2, 0}, "pbp", p)); EXPECT_EQ(p, -1);   // player 0 folds
    EXPECT_TRUE(terminal_payoff({1, 0}, "bb", p));  EXPECT_EQ(p, 2);
    EXPECT_TRUE(terminal_payoff({1, 2}, "pbb", p)); EXPECT_EQ(p, -2);
    EXPECT_FALSE(terminal_payoff({0, 1}, "", p));
    EXPECT_FALSE(terminal_payoff({0, 1}, "pb", p));
}

TEST(Kuhn, NashProfileValueIsMinusOneEighteenth) {
    EXPECT_NEAR(game_value(nash_p0(), nash_p1()), -1.0 / 18.0, 1e-12);
}

TEST(Kuhn, NashProfileHasZeroExploitability) {
    EXPECT_NEAR(exploitability(nash_p0(), nash_p1()), 0.0, 1e-12);
}

TEST(Kuhn, AlwaysBetIsExploitable) {
    const Policy always_bet = [](int, const std::string&) { return 1.0; };
    // Worked out by hand: each side's best response earns 1/3 per game.
    EXPECT_NEAR(exploitability(always_bet, always_bet), 1.0 / 3.0, 1e-9);
}

TEST(Kuhn, CfrConvergesToNash) {
    KuhnCfr cfr;
    cfr.train(100000);
    const Policy pol = cfr.average_policy();
    EXPECT_NEAR(game_value(pol, pol), -1.0 / 18.0, 5e-3);
    EXPECT_LT(exploitability(pol, pol), 5e-3);
}

TEST(Kuhn, ExploitabilityShrinksWithTraining) {
    KuhnCfr cfr;
    cfr.train(100);
    const double early = exploitability(cfr.average_policy(), cfr.average_policy());
    cfr.train(99900);
    const double late = exploitability(cfr.average_policy(), cfr.average_policy());
    EXPECT_LT(late, early);
}

TEST(Kuhn, CfrRecoversPlayerTwoStrategy) {
    // Player 2's equilibrium strategy is unique, so it can be checked directly.
    KuhnCfr cfr;
    cfr.train(200000);
    EXPECT_NEAR(cfr.avg_bet_prob(0, "p"), 1.0 / 3.0, 0.05);  // J bluffs 1/3 after a check
    EXPECT_NEAR(cfr.avg_bet_prob(1, "p"), 0.0, 0.05);        // Q checks
    EXPECT_NEAR(cfr.avg_bet_prob(2, "p"), 1.0, 0.05);        // K bets
    EXPECT_NEAR(cfr.avg_bet_prob(0, "b"), 0.0, 0.05);        // J folds to a bet
    EXPECT_NEAR(cfr.avg_bet_prob(1, "b"), 1.0 / 3.0, 0.05);  // Q calls 1/3
    EXPECT_NEAR(cfr.avg_bet_prob(2, "b"), 1.0, 0.05);        // K calls
}