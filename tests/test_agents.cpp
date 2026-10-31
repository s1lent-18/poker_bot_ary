#include <gtest/gtest.h>

#include "agents/match.h"
#include "agents/random_agent.h"
#include "agents/rule_agent.h"
#include "core/deck.h"

using namespace poker;

static Card C(const char* s) { return *Card::from_string(s); }

// Average big blinds won per hand by `a` (seat 0) against `b`, alternating the button.
static double bb_per_hand(Agent& a, Agent& b, int hands, uint64_t seed) {
    Rng rng(seed);
    Deck deck;
    GameConfig cfg;
    long long total = 0;
    for (int h = 0; h < hands; ++h) {
        deck.shuffle(rng);
        GameState s(cfg, h % 2, deck);
        const auto pay = play_hand(s, {&a, &b});
        EXPECT_EQ(pay[0] + pay[1], 0);
        total += pay[0];
    }
    return double(total) / hands / cfg.big_blind;
}

TEST(Strength, PreflopOrdering) {
    EXPECT_DOUBLE_EQ(preflop_strength(C("As"), C("Ad")), 1.0);
    EXPECT_LT(preflop_strength(C("7s"), C("2d")), 0.1);
    EXPECT_GT(preflop_strength(C("As"), C("Ks")), preflop_strength(C("As"), C("Kd")));  // suited
    EXPECT_GT(preflop_strength(C("Ks"), C("Kd")), preflop_strength(C("Ts"), C("Td")));  // pairs
    EXPECT_GT(preflop_strength(C("Ts"), C("Td")), preflop_strength(C("2s"), C("2d")));
    // order of the two cards doesn't matter
    EXPECT_DOUBLE_EQ(preflop_strength(C("Js"), C("9h")), preflop_strength(C("9h"), C("Js")));
}

TEST(Strength, PostflopOrdering) {
    const std::array<Card, 2> trips_hole = {C("As"), C("Ad")};
    const std::vector<Card> trips_board = {C("Ah"), C("2c"), C("7d")};
    const std::array<Card, 2> top_pair_hole = {C("As"), C("Kd")};
    const std::vector<Card> top_pair_board = {C("Ac"), C("7d"), C("2h")};
    const std::array<Card, 2> board_pair_hole = {C("Ks"), C("Qd")};
    const std::vector<Card> board_pair_board = {C("7c"), C("7d"), C("2h")};
    const std::array<Card, 2> air_hole = {C("Ks"), C("Qd")};
    const std::vector<Card> air_board = {C("2c"), C("7d"), C("9h")};

    const double trips = postflop_strength(trips_hole, trips_board);
    const double top_pair = postflop_strength(top_pair_hole, top_pair_board);
    const double board_pair = postflop_strength(board_pair_hole, board_pair_board);
    const double air = postflop_strength(air_hole, air_board);
    EXPECT_GT(trips, top_pair);
    EXPECT_GT(top_pair, board_pair);
    EXPECT_GE(board_pair, air);

    // Royal flush on the board: our hole cards add nothing
    EXPECT_DOUBLE_EQ(postflop_strength({C("2d"), C("3c")},
                                       {C("Ts"), C("Js"), C("Qs"), C("Ks"), C("As")}),
                     0.25);
}

TEST(Agents, RandomVsRandomIsLegalAndZeroSum) {
    RandomAgent a(1), b(2);
    EXPECT_NO_THROW(bb_per_hand(a, b, 5000, 11));  // an illegal action would throw
}

TEST(Agents, RuleVsRuleTerminatesAndIsLegal) {
    RuleAgent a(1), b(2);
    EXPECT_NO_THROW(bb_per_hand(a, b, 20000, 22));
}

TEST(Agents, RuleAgentIsLegalAgainstRandom) {
    RuleAgent a(3);
    RandomAgent b(4);
    EXPECT_NO_THROW(bb_per_hand(a, b, 20000, 33));
}

TEST(Agents, RuleAgentBeatsRandom) {
    RuleAgent a(5);
    RandomAgent b(6);
    const double bb = bb_per_hand(a, b, 30000, 44);
    std::cout << "RuleBot vs Random: " << bb << " bb/hand\n";
    EXPECT_GT(bb, 0.0);
}