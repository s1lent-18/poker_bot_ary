#include <gtest/gtest.h>

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

#include "cfr/cfr.h"
#include "cfr/flat_cfr.h"
#include "cfr/flat_game.h"
#include "cfr/kuhn_state.h"
#include "cfr/leduc.h"

using namespace poker::cfr;

static void check_structure(const FlatGame& g) {
    ASSERT_FALSE(g.nodes.empty());
    for (const FlatGame::Node& nd : g.nodes) {
        if (nd.kind == FlatGame::Terminal) continue;
        const int n = nd.n;
        ASSERT_GE(n, 1);
        double sum = 0.0;
        for (int i = 0; i < n; ++i) {
            const int c = g.child[nd.first + i];
            ASSERT_GE(c, 0);
            ASSERT_LT(c, static_cast<int>(g.nodes.size()));
            sum += g.prob[nd.first + i];
        }
        if (nd.kind == FlatGame::Chance) {
            EXPECT_NEAR(sum, 1.0, 1e-12);
        } else {
            ASSERT_GE(nd.info, 0);
            ASSERT_EQ(g.info_actions[nd.info], n);
        }
    }
}

TEST(FlatGame, KuhnStructure) {
    const FlatGame g = FlatGame::build(KuhnState{});
    check_structure(g);
    EXPECT_EQ(g.info_keys.size(), 12u);  // 3 cards x 2 spots, for each of 2 players
}

TEST(FlatGame, LeducStructure) {
    const FlatGame g = FlatGame::build(LeducState{});
    check_structure(g);
    EXPECT_EQ(g.info_keys.size(), 288u);  // same count the generic solver found in M6b
}

TEST(FlatCfr, VanillaConvergesOnKuhn) {
    const KuhnState root;
    const FlatGame g = FlatGame::build(root);
    FlatCfr s(g);
    s.train(20000);
    const StrategyFn pol = s.average_policy();
    EXPECT_NEAR(profile_value(root, pol), -1.0 / 18.0, 5e-3);
    EXPECT_LT(exploitability(root, pol), 5e-3);
}

TEST(FlatCfr, ReproducesTheGenericSolverOnLeduc) {
    // Same algorithm as M6b, so it should land on the same numbers M6b printed at 2000
    // iterations (value -0.08750, exploitability 0.00995).
    const LeducState root;
    const FlatGame g = FlatGame::build(root);
    FlatCfr s(g);
    s.train(2000);
    const StrategyFn pol = s.average_policy();
    const double e = exploitability(root, pol);
    const double v = profile_value(root, pol);
    std::cout << "flat vanilla CFR, Leduc, 2000 it: value " << v << ", exploitability " << e << "\n";
    EXPECT_NEAR(v, -0.0875, 0.002);
    EXPECT_NEAR(e, 0.00995, 0.002);
}

TEST(FlatCfr, PlusBeatsVanillaOnLeduc) {
    const LeducState root;
    const FlatGame g = FlatGame::build(root);
    FlatCfr vanilla(g, CfrVariant::Vanilla), plus(g, CfrVariant::Plus);
    vanilla.train(1000);
    plus.train(1000);
    const double ev = exploitability(root, vanilla.average_policy());
    const double ep = exploitability(root, plus.average_policy());
    std::cout << "Leduc, 1000 it: vanilla " << ev << ", CFR+ " << ep << "\n";
    EXPECT_LT(ep, ev);
}

TEST(FlatCfr, StrategiesAreProbabilityDistributions) {
    const FlatGame g = FlatGame::build(LeducState{});
    FlatCfr s(g, CfrVariant::Plus);
    s.train(300);
    const StrategyFn pol = s.average_policy();
    for (size_t i = 0; i < g.info_keys.size(); ++i) {
        const std::vector<double> p = pol(g.info_keys[i], g.info_actions[i]);
        double sum = 0.0;
        for (double x : p) {
            EXPECT_GE(x, 0.0);
            sum += x;
        }
        EXPECT_NEAR(sum, 1.0, 1e-9);
    }
}

TEST(FlatMccfr, ConvergesOnKuhn) {
    const KuhnState root;
    const FlatGame g = FlatGame::build(root);
    FlatMccfr s(g, 7);
    s.train(200000);
    const double e = exploitability(root, s.average_policy());
    std::cout << "ES-MCCFR Kuhn, 200k it: exploitability " << e << "\n";
    EXPECT_LT(e, 0.02);
}

TEST(FlatMccfr, ImprovesOnLeduc) {
    const LeducState root;
    const FlatGame g = FlatGame::build(root);
    FlatMccfr s(g, 11);
    s.train(5000);
    const double e_early = exploitability(root, s.average_policy());
    s.train(495000);
    const double e_late = exploitability(root, s.average_policy());
    std::cout << "ES-MCCFR Leduc: 5k it " << e_early << ", 500k it " << e_late << "\n";
    EXPECT_LT(e_late, e_early);
    EXPECT_LT(e_late, 0.15);
}

TEST(FlatMccfr, SameSeedIsReproducible) {
    const FlatGame g = FlatGame::build(LeducState{});
    FlatMccfr a(g, 99), b(g, 99);
    a.train(1000);
    b.train(1000);
    EXPECT_EQ(a.average_policy()("0J:", 2), b.average_policy()("0J:", 2));
    EXPECT_EQ(a.average_policy()("1K:r", 3), b.average_policy()("1K:r", 3));
}