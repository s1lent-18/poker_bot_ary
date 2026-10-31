#include <gtest/gtest.h>

#include <array>
#include <sstream>
#include <stdexcept>

#include "core/deck.h"
#include "core/rng.h"
#include "eval/hand_evaluator.h"

using namespace poker;

// "As Kd 7c ..." -> score
static HandScore ev(const std::string& s) {
    std::vector<Card> cs;
    std::istringstream is(s);
    std::string tok;
    while (is >> tok) {
        auto c = Card::from_string(tok);
        if (!c) throw std::runtime_error("bad card: " + tok);
        cs.push_back(*c);
    }
    return evaluate(cs);
}

static HandCategory cat(const std::string& s) { return category_of(ev(s)); }

TEST(Evaluator, DetectsEveryCategory) {
    EXPECT_EQ(cat("As Kd 9c 7h 4s 3d 2c"), HandCategory::HighCard);
    EXPECT_EQ(cat("As Ad 9c 7h 4s 3d 2c"), HandCategory::Pair);
    EXPECT_EQ(cat("As Ad 9c 9h 4s 3d 2c"), HandCategory::TwoPair);
    EXPECT_EQ(cat("As Ad Ac 9h 4s 3d 2c"), HandCategory::ThreeOfAKind);
    EXPECT_EQ(cat("5s 6d 7c 8h 9s Kd 2c"), HandCategory::Straight);
    EXPECT_EQ(cat("As Ks 9s 7s 4s 3d 2c"), HandCategory::Flush);
    EXPECT_EQ(cat("As Ad Ac 9h 9s 3d 2c"), HandCategory::FullHouse);
    EXPECT_EQ(cat("As Ad Ac Ah 9s 3d 2c"), HandCategory::FourOfAKind);
    EXPECT_EQ(cat("5s 6s 7s 8s 9s Kd 2c"), HandCategory::StraightFlush);
}

TEST(Evaluator, FiveCardHandsWork) {
    EXPECT_EQ(cat("As Ks Qs Js Ts"), HandCategory::StraightFlush);
    EXPECT_EQ(cat("2c 2d 5h 9s Kd"), HandCategory::Pair);
}

TEST(Evaluator, RejectsBadCardCounts) {
    EXPECT_THROW(ev("As Kd 9c 7h"), std::invalid_argument);
    EXPECT_THROW(ev("As Kd 9c 7h 4s 3d 2c 5c"), std::invalid_argument);
}

TEST(Evaluator, CategoryOrdering) {
    // Each hand strictly beats the one before it
    const char* hands[] = {
        "As Kd 9c 7h 4s 3d 2c",  // high card
        "As Ad 9c 7h 4s 3d 2c",  // pair
        "As Ad 9c 9h 4s 3d 2c",  // two pair
        "As Ad Ac 9h 4s 3d 2c",  // trips
        "5s 6d 7c 8h 9s Kd 2c",  // straight
        "As Ks 9s 7s 4s 3d 2c",  // flush
        "As Ad Ac 9h 9s 3d 2c",  // full house
        "As Ad Ac Ah 9s 3d 2c",  // quads
        "5s 6s 7s 8s 9s Kd 2c",  // straight flush
    };
    for (int i = 1; i < 9; ++i) EXPECT_GT(ev(hands[i]), ev(hands[i - 1])) << i;
}

TEST(Evaluator, WheelIsFiveHighStraight) {
    EXPECT_EQ(cat("As 2d 3c 4h 5s Kd Qc"), HandCategory::Straight);
    // wheel loses to a six-high straight, beats trips
    EXPECT_LT(ev("As 2d 3c 4h 5s Kd Qc"), ev("2s 3d 4c 5h 6s Kd Qc"));
    EXPECT_GT(ev("As 2d 3c 4h 5s Kd Qc"), ev("9s 9d 9c 4h 5s Kd Qc"));
}

TEST(Evaluator, AceHighStraightBeatsKingHigh) {
    EXPECT_GT(ev("Ts Jd Qc Kh As 2d 3c"), ev("9s Td Jc Qh Ks 2d 3c"));
}

TEST(Evaluator, NoWrapAroundStraight) {
    // Q K A 2 3 is NOT a straight
    EXPECT_EQ(cat("Qs Kd Ac 2h 3s 7d 9c"), HandCategory::HighCard);
}

TEST(Evaluator, StraightFlushWheelAndRoyal) {
    EXPECT_EQ(cat("As 2s 3s 4s 5s Kd Qc"), HandCategory::StraightFlush);
    EXPECT_GT(ev("Ts Js Qs Ks As 2d 3c"), ev("As 2s 3s 4s 5s Kd Qc"));
}

TEST(Evaluator, StraightFlushNeedsSameSuit) {
    // 5-6-7-8-9 exists but only four are spades -> plain straight, not SF
    EXPECT_EQ(cat("5s 6s 7s 8s 9d 2h Kc"), HandCategory::Straight);
    // flush AND a straight exist in different cards -> flush wins
    EXPECT_EQ(cat("2s 4s 6s 8s Ts 9d 7c"), HandCategory::Flush);
    // five spades plus a straight that mixes suits -> flush, not straight flush
    EXPECT_EQ(cat("5s 6s 7s 8s 2s 9d Kc"), HandCategory::Flush);

}

TEST(Evaluator, FullHouseWithTwoTrips) {
    // AAA KKK 2 -> aces full of kings
    EXPECT_EQ(cat("As Ad Ac Ks Kd Kc 2h"), HandCategory::FullHouse);
    EXPECT_EQ(ev("As Ad Ac Ks Kd Kc 2h"), ev("As Ad Ac Ks Kd 2c 2h"));  // AAA KK either way
    EXPECT_GT(ev("As Ad Ac Ks Kd Kc 2h"), ev("Ks Kd Kc As Ad 2c 3h"));  // aces full > kings full
}

TEST(Evaluator, FullHouseUsesBestPair) {
    // AAA + pairs of 9 and 5 -> aces full of nines
    EXPECT_EQ(ev("As Ad Ac 9s 9d 5c 5h"), ev("As Ad Ac 9s 9d 2c 3h"));
}

TEST(Evaluator, TwoPairKickerFromThirdPair) {
    // AA KK QQ 2 -> two pair A,K with Q kicker (not the 2)
    EXPECT_GT(ev("As Ad Ks Kd Qs Qd 2c"), ev("As Ad Ks Kd Js 2d 3c"));
    EXPECT_EQ(ev("As Ad Ks Kd Qs Qd 2c"), ev("As Ad Ks Kd Qs 2d 3c"));
}

TEST(Evaluator, KickersDecideEqualCategories) {
    EXPECT_GT(ev("As Ad Kc 7h 4s 3d 2c"), ev("As Ad Qc 7h 4s 3d 2c"));  // pair, K vs Q kicker
    EXPECT_GT(ev("As Ad Ac Kh 4s 3d 2c"), ev("As Ad Ac Qh 4s 3d 2c"));  // trips kicker
    EXPECT_GT(ev("As Ad Ac Ah Ks 3d 2c"), ev("Ks Kd Kc Kh As 3d 2c"));  // quads rank beats kicker
    EXPECT_GT(ev("As Ad Ac Ah Ks 3d 2c"), ev("As Ad Ac Ah Qs 3d 2c"));  // quads kicker
}

TEST(Evaluator, QuadsKickerCanBeAPairOrTrips) {
    // KKKK + QQQ -> kicker is Q
    EXPECT_EQ(ev("Ks Kd Kc Kh Qs Qd Qc"), ev("Ks Kd Kc Kh Qs 2d 3c"));
}

TEST(Evaluator, FlushComparedByAllFiveCards) {
    EXPECT_GT(ev("As Ks Qs Js 9s 2d 3c"), ev("As Ks Qs Js 8s 2d 3c"));
    // 6 suited cards: only the best five count
    EXPECT_EQ(ev("As Ks Qs Js 9s 2s 3c"), ev("As Ks Qs Js 9s 4d 3c"));
}

TEST(Evaluator, BoardPlaysIsATie) {
    // Board is a royal flush; hole cards irrelevant
    EXPECT_EQ(ev("Ts Js Qs Ks As 2d 3c"), ev("Ts Js Qs Ks As 7h 8h"));
}

TEST(Evaluator, SameRanksDifferentSuitsTie) {
    EXPECT_EQ(ev("As Kd 9c 7h 4s 3d 2c"), ev("Ad Ks 9h 7c 4d 3s 2h"));
}

// ---- Cross-checks ----

// Best 5-of-7 by brute force: drop each pair of cards, evaluate the remaining 5.
static HandScore brute_best5(const std::array<Card, 7>& c) {
    HandScore best = 0;
    for (int i = 0; i < 7; ++i)
        for (int j = i + 1; j < 7; ++j) {
            Card five[5];
            int k = 0;
            for (int x = 0; x < 7; ++x)
                if (x != i && x != j) five[k++] = c[x];
            best = std::max(best, evaluate(five, 5));
        }
    return best;
}

TEST(Evaluator, SevenCardMatchesBruteForceBest5) {
    Rng rng(2024);
    Deck d;
    for (int t = 0; t < 100000; ++t) {
        d.shuffle(rng);
        std::array<Card, 7> c;
        for (auto& x : c) x = d.deal();
        ASSERT_EQ(evaluate(c.data(), 7), brute_best5(c)) << "trial " << t;
    }
}

TEST(Evaluator, CategoryFrequenciesMatchTheory) {
    // Known 7-card probabilities: high 17.41%, pair 43.82%, two pair 23.50%,
    // trips 4.83%, straight 4.62%, flush 3.03%, full house 2.60%,
    // quads 0.168%, straight flush 0.031%
    Rng rng(555);
    Deck d;
    const int N = 1000000;
    std::array<int, 9> freq{};
    for (int t = 0; t < N; ++t) {
        d.shuffle(rng);
        std::array<Card, 7> c;
        for (auto& x : c) x = d.deal();
        freq[static_cast<int>(category_of(evaluate(c.data(), 7)))]++;
    }
    auto pct = [&](int i) { return 100.0 * freq[i] / N; };
    EXPECT_NEAR(pct(0), 17.41, 0.3);
    EXPECT_NEAR(pct(1), 43.82, 0.3);
    EXPECT_NEAR(pct(2), 23.50, 0.3);
    EXPECT_NEAR(pct(3), 4.83, 0.15);
    EXPECT_NEAR(pct(4), 4.62, 0.15);
    EXPECT_NEAR(pct(5), 3.03, 0.15);
    EXPECT_NEAR(pct(6), 2.60, 0.15);
    EXPECT_NEAR(pct(7), 0.168, 0.05);
    EXPECT_NEAR(pct(8), 0.031, 0.02);
}