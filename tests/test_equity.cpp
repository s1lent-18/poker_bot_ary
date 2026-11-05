#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <vector>

#include "agents/equity_agent.h"
#include "agents/match.h"
#include "agents/random_agent.h"
#include "agents/rule_agent.h"
#include "ai/equity.h"
#include "core/deck.h"
#include "eval/hand_evaluator.h"

using namespace poker;

static Card C(const char* s) { return *Card::from_string(s); }
static std::vector<Card> B(std::initializer_list<const char*> l) {
    std::vector<Card> v;
    for (const char* s : l) v.push_back(C(s));
    return v;
}

TEST(Equity, RejectsBadInput) {
    Rng rng(1);
    EXPECT_THROW(equity_vs_random({C("As"), C("Ad")}, B({"As", "2c", "7d"}), 10, rng),
                 std::invalid_argument);  // duplicate card
    EXPECT_THROW(equity_vs_random({C("As"), C("Ad")}, B({"2c", "7d"}), 10, rng),
                 std::invalid_argument);  // 2-card board
    EXPECT_THROW(equity_vs_random({C("As"), C("Ad")}, {}, 0, rng), std::invalid_argument);
    EXPECT_THROW(equity_vs_hand({C("As"), C("Ad")}, {C("Ad"), C("Kc")}, {}, 10, rng),
                 std::invalid_argument);  // shared card with opponent
}

TEST(Equity, CompleteBoardIsDeterministic) {
    Rng rng(2);
    // Hero has a royal flush; opponent cannot beat it.
    auto r = equity_vs_hand({C("As"), C("Ks")}, {C("9h"), C("8h")},
                            B({"Qs", "Js", "Ts", "2c", "3d"}), 1000, rng);
    EXPECT_DOUBLE_EQ(r.equity, 1.0);
    // Royal flush on the board: every opponent hand ties.
    auto t = equity_vs_random({C("2d"), C("3c")}, B({"Ts", "Js", "Qs", "Ks", "As"}), 1000, rng);
    EXPECT_DOUBLE_EQ(t.equity, 0.5);
}

TEST(Equity, KnownPreflopMatchups) {
    Rng rng(3);
    const int N = 200000;
    // AA vs random ~ 85.2%
    EXPECT_NEAR(equity_vs_random({C("Ah"), C("Ad")}, {}, N, rng).equity, 0.852, 0.01);
    // 72 offsuit vs random ~ 34.6%
    EXPECT_NEAR(equity_vs_random({C("7d"), C("2c")}, {}, N, rng).equity, 0.346, 0.015);
    // AA vs KK ~ 81.9%
    EXPECT_NEAR(equity_vs_hand({C("Ah"), C("Ad")}, {C("Ks"), C("Kc")}, {}, N, rng).equity, 0.819,
                0.012);
    // AKs vs QQ is the classic coin flip, ~ 46%
    EXPECT_NEAR(equity_vs_hand({C("As"), C("Ks")}, {C("Qh"), C("Qd")}, {}, N, rng).equity, 0.46,
                0.015);
}

// Exact equity on the turn by enumerating every opponent hand and every river card.
static double exact_turn_equity(const std::array<Card, 2>& hole, const std::vector<Card>& board4) {
    bool used[52] = {};
    used[hole[0].id] = used[hole[1].id] = true;
    for (const Card& c : board4) used[c.id] = true;
    std::vector<Card> rem;
    for (int i = 0; i < 52; ++i)
        if (!used[i]) rem.push_back(Card(static_cast<uint8_t>(i)));

    double wins = 0;
    long n = 0;
    for (size_t i = 0; i < rem.size(); ++i)
        for (size_t j = i + 1; j < rem.size(); ++j)
            for (size_t r = 0; r < rem.size(); ++r) {
                if (r == i || r == j) continue;
                Card h[7] = {hole[0], hole[1], board4[0], board4[1], board4[2], board4[3], rem[r]};
                Card o[7] = {rem[i], rem[j], board4[0], board4[1], board4[2], board4[3], rem[r]};
                const HandScore hs = evaluate(h, 7), os = evaluate(o, 7);
                wins += hs > os ? 1.0 : (hs == os ? 0.5 : 0.0);
                ++n;
            }
    return wins / n;
}

TEST(Equity, MatchesExactEnumerationWithFlushDraw) {
    // Nut flush draw with overcards on the turn: the simulation must price the draw correctly.
    const std::array<Card, 2> hole = {C("Ah"), C("Kh")};
    const auto board = B({"2h", "7h", "Jc", "9d"});
    const double exact = exact_turn_equity(hole, board);
    Rng rng(4);
    EXPECT_NEAR(equity_vs_random(hole, board, 200000, rng).equity, exact, 0.01);
}

TEST(Equity, NarrowerOpponentRangeLowersEquity) {
    Rng rng(5);
    const double vs_random = equity_vs_random({C("9s"), C("9d")}, {}, 50000, rng).equity;
    const double vs_strong = equity_vs_random({C("9s"), C("9d")}, {}, 50000, rng, 0.5).equity;
    EXPECT_LT(vs_strong, vs_random - 0.05);
}

TEST(Equity, SameSeedGivesSameResult) {
    Rng a(77), b(77);
    const auto ra = equity_vs_random({C("Qh"), C("Jh")}, B({"2c", "9h", "Th"}), 5000, a);
    const auto rb = equity_vs_random({C("Qh"), C("Jh")}, B({"2c", "9h", "Th"}), 5000, b);
    EXPECT_DOUBLE_EQ(ra.equity, rb.equity);
}

TEST(EquityAgent, FoldsTrashToShoveAndCallsWithAces) {
    // Preflop, opponent has shoved 10000 into our big blind: fold or call only.
    Observation ob;
    ob.street = Street::Preflop;
    ob.pot = 10100;
    ob.to_call = 9900;
    ob.history = "r10000";
    const std::vector<Action> legal = {Action::fold(), Action::call()};

    EquityAgent bot(1, 3000);
    ob.hole = {C("7d"), C("2c")};
    EXPECT_EQ(bot.act(ob, legal), Action::fold());
    ob.hole = {C("Ah"), C("Ad")};
    EXPECT_EQ(bot.act(ob, legal), Action::call());
}

static double bb_per_hand(Agent& a, Agent& b, int hands, uint64_t seed) {
    Rng rng(seed);
    Deck deck;
    GameConfig cfg;
    long long total = 0;
    for (int h = 0; h < hands; ++h) {
        deck.shuffle(rng);
        GameState s(cfg, h % 2, deck);
        const auto pay = play_hand(s, {&a, &b});  // throws on an illegal action
        EXPECT_EQ(pay[0] + pay[1], 0);
        total += pay[0];
    }
    return double(total) / hands / cfg.big_blind;
}

TEST(EquityAgent, IsLegalAndBeatsRandom) {
    EquityAgent a(8, 400);
    RandomAgent b(9);
    const double bb = bb_per_hand(a, b, 10000, 55);
    std::cout << "EquityBot vs Random: " << bb << " bb/hand\n";
    EXPECT_GT(bb, 0.0);
}

TEST(EquityAgent, HeadToHeadVsRuleBotIsLegal) {
    EquityAgent a(10, 300);
    RuleAgent b(11);
    double bb = 0;
    EXPECT_NO_THROW(bb = bb_per_hand(a, b, 10000, 66));
    std::cout << "EquityBot vs RuleBot: " << bb << " bb/hand (informational)\n";
}