#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <initializer_list>
#include <iostream>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

#include "agents/cfr_agent.h"
#include "agents/match.h"
#include "agents/random_agent.h"
#include "ai/bucketer.h"
#include "ai/holdem_mccfr.h"
#include "ai/holdem_strategy.h"
#include "core/deck.h"
#include "core/rng.h"

using namespace poker;

static Card C(const char* s) { return *Card::from_string(s); }
static std::vector<Card> B(std::initializer_list<const char*> l) {
    std::vector<Card> v;
    for (const char* s : l) v.push_back(C(s));
    return v;
}

static const Bucketer& shared_bucketer() {
    static const Bucketer b = [] {
        Bucketer x(8, 100);
        x.calibrate(1500, 5);
        return x;
    }();
    return b;
}

// A tiny, fast training run shared by several tests.
static const HoldemStrategy& quick_strategy() {
    static const HoldemStrategy s = [] {
        Bucketer b(6, 60);
        b.calibrate(600, 9);
        HoldemMccfr solver(holdem_config(1, 100), b, 17);
        const auto t0 = std::chrono::steady_clock::now();
        solver.train(100);
        const double secs =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::cout << "[holdem] 100 iterations in " << secs << " s ("
                  << solver.iterations() / secs << " it/s), " << solver.num_info_sets()
                  << " info sets\n";
        return solver.export_strategy();
    }();
    return s;
}

TEST(PreflopClass, Has169DistinctClasses) {
    std::set<int> ids;
    for (int i = 0; i < 52; ++i)
        for (int j = i + 1; j < 52; ++j)
            ids.insert(preflop_class(Card(static_cast<uint8_t>(i)), Card(static_cast<uint8_t>(j))));
    EXPECT_EQ(ids.size(), 169u);
    EXPECT_EQ(*ids.begin(), 0);
    EXPECT_EQ(*ids.rbegin(), 168);
    EXPECT_EQ(preflop_class(C("As"), C("Kd")), preflop_class(C("Kh"), C("Ac")));  // offsuit
    EXPECT_EQ(preflop_class(C("As"), C("Ks")), preflop_class(C("Ah"), C("Kh")));  // suited
    EXPECT_NE(preflop_class(C("As"), C("Ks")), preflop_class(C("As"), C("Kd")));
}

TEST(Bucketer, EdgesAreSortedAndComplete) {
    const Bucketer& b = shared_bucketer();
    ASSERT_TRUE(b.calibrated());
    for (const auto& e : b.edges()) {
        EXPECT_EQ(e.size(), 7u);
        EXPECT_TRUE(std::is_sorted(e.begin(), e.end()));
    }
}

TEST(Bucketer, StrongHandsLandHigh) {
    const Bucketer& b = shared_bucketer();
    // Royal flush on the flop cannot lose: top bucket.
    EXPECT_EQ(b.bucket({C("Ah"), C("Kh")}, B({"Qh", "Jh", "Th"})), b.num_buckets() - 1);
    // Complete air on a dry board sits in the lower half.
    EXPECT_LE(b.bucket({C("2c"), C("3d")}, B({"7s", "9h", "Kd"})), 3);
}

TEST(Bucketer, IsDeterministicAndOrderIndependent) {
    const Bucketer& b = shared_bucketer();
    const std::array<Card, 2> h1 = {C("Ah"), C("Kd")}, h2 = {C("Kd"), C("Ah")};
    EXPECT_DOUBLE_EQ(b.strength(h1, B({"2c", "7d", "9h"})), b.strength(h1, B({"2c", "7d", "9h"})));
    EXPECT_DOUBLE_EQ(b.strength(h1, B({"2c", "7d", "9h"})), b.strength(h2, B({"9h", "2c", "7d"})));
}

TEST(Bucketer, BucketsStayInRange) {
    const Bucketer& b = shared_bucketer();
    Rng rng(8);
    Deck deck;
    for (int i = 0; i < 300; ++i) {
        deck.shuffle(rng);
        const std::array<Card, 2> hole = {deck.deal(), deck.deal()};
        std::vector<Card> board;
        const int nb = 3 + static_cast<int>(rng.below(3));
        for (int k = 0; k < nb; ++k) board.push_back(deck.deal());
        const int x = b.bucket(hole, board);
        ASSERT_GE(x, 0);
        ASSERT_LT(x, b.num_buckets());
    }
}

TEST(Bucketer, SaveLoadRoundTrip) {
    const Bucketer& b = shared_bucketer();
    std::stringstream ss;
    b.save(ss);
    const Bucketer t = Bucketer::load(ss);
    EXPECT_EQ(t.num_buckets(), b.num_buckets());
    for (size_t s = 0; s < 3; ++s) {
        ASSERT_EQ(t.edges()[s].size(), b.edges()[s].size());
        for (size_t i = 0; i < b.edges()[s].size(); ++i)
            EXPECT_NEAR(t.edges()[s][i], b.edges()[s][i], 1e-9);
    }
    EXPECT_EQ(t.bucket({C("Ah"), C("Kd")}, B({"2c", "7d", "9h", "Js"})),
              b.bucket({C("Ah"), C("Kd")}, B({"2c", "7d", "9h", "Js"})));
}

TEST(HoldemKey, StreetHistory) {
    EXPECT_EQ(street_history(""), "");
    EXPECT_EQ(street_history("r300c"), "r300c");
    EXPECT_EQ(street_history("r300c/kk/r600"), "r600");
    EXPECT_EQ(street_history("ck/"), "");
}

TEST(HoldemKey, TrainingAndPlayKeysAgreeAcrossButtonPositions) {
    // Game A has the button on seat 0 (how we train); game B has it on seat 1 (how we may play).
    // The same action sequence must give the same key for the acting role in both.
    const GameConfig cfg = holdem_config(2, 100);
    Rng rng(3);
    Deck da, db;
    for (int hand = 0; hand < 300; ++hand) {
        da.shuffle(rng);
        db.shuffle(rng);
        GameState a(cfg, 0, da), b(cfg, 1, db);
        while (!a.is_terminal()) {
            ASSERT_FALSE(b.is_terminal());
            const std::vector<Action> legal = a.legal_actions();
            ASSERT_EQ(legal, b.legal_actions());
            ASSERT_EQ(a.current_player(), 1 - b.current_player());
            ASSERT_EQ(holdem_key_from_state(a, 7),
                      holdem_key_from_observation(b.observe(b.current_player()), 7));
            const Action act = legal[rng.below(legal.size())];
            a.apply(act);
            b.apply(act);
        }
        ASSERT_TRUE(b.is_terminal());
    }
}

TEST(HoldemMccfr, ProducesValidStrategies) {
    const HoldemStrategy& s = quick_strategy();
    ASSERT_GT(s.table.size(), 20u);
    bool root_found = false;
    for (const auto& kv : s.table) {
        EXPECT_EQ(std::count(kv.first.begin(), kv.first.end(), '|'), 4) << kv.first;
        if (kv.first.rfind("0|0|", 0) == 0) root_found = true;  // button, preflop
        ASSERT_GE(kv.second.size(), 2u);
        ASSERT_LE(kv.second.size(), 5u);
        double sum = 0;
        for (float p : kv.second) {
            EXPECT_GE(p, 0.0f);
            sum += p;
        }
        EXPECT_NEAR(sum, 1.0, 1e-4) << kv.first;
    }
    EXPECT_TRUE(root_found);
}

TEST(HoldemStrategy, SaveLoadRoundTrip) {
    const HoldemStrategy& s = quick_strategy();
    std::stringstream ss;
    s.save(ss);
    const HoldemStrategy t = HoldemStrategy::load(ss);
    EXPECT_EQ(t.cfg.stack, s.cfg.stack);
    EXPECT_EQ(t.cfg.num_raise_fractions, s.cfg.num_raise_fractions);
    ASSERT_EQ(t.table.size(), s.table.size());
    for (const auto& kv : s.table) {
        const auto it = t.table.find(kv.first);
        ASSERT_NE(it, t.table.end()) << kv.first;
        ASSERT_EQ(it->second.size(), kv.second.size());
        for (size_t i = 0; i < kv.second.size(); ++i) EXPECT_NEAR(it->second[i], kv.second[i], 1e-5);
    }
}

TEST(CfrAgent, PlaysLegallyAgainstRandom) {
    const auto strat = std::make_shared<const HoldemStrategy>(quick_strategy());
    CfrAgent bot(strat, 3);
    RandomAgent rnd(4);
    Rng rng(5);
    Deck deck;
    for (int hand = 0; hand < 400; ++hand) {
        deck.shuffle(rng);
        GameState s(strat->cfg, hand % 2, deck);
        const auto pay = play_hand(s, {&bot, &rnd});  // throws if an action is illegal
        ASSERT_EQ(pay[0] + pay[1], 0);
    }
    EXPECT_GT(bot.decisions(), 0);
    std::cout << "[holdem] untrained bot fell back on " << bot.fallbacks() << " of "
              << bot.decisions() << " decisions\n";
}

TEST(CfrAgent, UnseenInfoSetsFallBackToCheckOrCall) {
    auto empty = std::make_shared<HoldemStrategy>();
    empty->cfg = holdem_config(1, 100);
    empty->bucketer = shared_bucketer();  // calibrated, but the table has no entries
    CfrAgent bot(empty, 1);
    RandomAgent rnd(2);
    Rng rng(6);
    Deck deck;
    for (int hand = 0; hand < 200; ++hand) {
        deck.shuffle(rng);
        GameState s(empty->cfg, hand % 2, deck);
        const auto pay = play_hand(s, {&bot, &rnd});
        ASSERT_EQ(pay[0] + pay[1], 0);
    }
    EXPECT_GT(bot.decisions(), 0);
    EXPECT_EQ(bot.fallbacks(), bot.decisions());
}

#include "agents/duel.h"

TEST(HoldemStrategy, IterationCountSurvivesSaveAndOldFilesStillLoad) {
    HoldemStrategy s = quick_strategy();
    s.iterations = 12345;
    std::stringstream ss;
    s.save(ss);
    const std::string text = ss.str();
    const std::string header = "HOLDEM_CFR 2\nmeta 12345\n";
    ASSERT_EQ(text.rfind(header, 0), 0u);

    std::stringstream a(text);
    EXPECT_EQ(HoldemStrategy::load(a).iterations, 12345);

    // A version-1 file (no meta line) must still load.
    std::stringstream b("HOLDEM_CFR 1\n" + text.substr(header.size()));
    const HoldemStrategy t = HoldemStrategy::load(b);
    EXPECT_EQ(t.iterations, 0);
    EXPECT_EQ(t.table.size(), s.table.size());
}

namespace {
class CheckCallAgent : public Agent {
public:
    Action act(const Observation&, const std::vector<Action>& legal) override {
        for (const Action& a : legal)
            if (a.type == ActionType::Check) return a;
        for (const Action& a : legal)
            if (a.type == ActionType::Call) return a;
        return legal[0];
    }
    std::string name() const override { return "CheckCall"; }
};
}  // namespace

TEST(Duel, DuplicateFormatCancelsCardLuckExactly) {
    // Two identical check/call players: whoever wins a deal one way loses it the other way.
    CheckCallAgent a, b;
    const DuelResult r = duel(a, b, holdem_config(1, 100), 2000, 7, true);
    EXPECT_EQ(r.hands, 2000);
    EXPECT_DOUBLE_EQ(r.bb_per_hand, 0.0);
    EXPECT_DOUBLE_EQ(r.ci95, 0.0);
}

TEST(Duel, NonDuplicateCountsHandsAndIsZeroSum) {
    CheckCallAgent a, b;
    const DuelResult r = duel(a, b, holdem_config(1, 100), 1000, 7, false);
    EXPECT_EQ(r.hands, 1000);
    EXPECT_GT(r.ci95, 0.0);  // plain luck does not cancel
}