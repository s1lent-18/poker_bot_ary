#include <gtest/gtest.h>

#include <set>

#include "core/card.h"
#include "core/deck.h"
#include "core/rng.h"

using namespace poker;

TEST(Card, RankAndSuitDecode) {
    Card as = *Card::from_string("As");
    EXPECT_EQ(as.rank(), 12);  // Ace
    EXPECT_EQ(as.suit(), 3);   // spades
    Card two_c = *Card::from_string("2c");
    EXPECT_EQ(two_c.id, 0);
}

TEST(Card, StringRoundTripForAll52) {
    for (int i = 0; i < NUM_CARDS; ++i) {
        Card c(static_cast<uint8_t>(i));
        auto parsed = Card::from_string(c.to_string());
        ASSERT_TRUE(parsed.has_value());
        EXPECT_EQ(*parsed, c);
    }
}

TEST(Card, ParseIsCaseInsensitiveAndRejectsGarbage) {
    EXPECT_EQ(*Card::from_string("tD"), *Card::from_string("Td"));
    EXPECT_FALSE(Card::from_string("").has_value());
    EXPECT_FALSE(Card::from_string("Xx").has_value());
    EXPECT_FALSE(Card::from_string("Asd").has_value());
    EXPECT_FALSE(Card::from_string("1s").has_value());
}

TEST(Deck, DealsAll52UniqueCards) {
    Rng rng(123);
    Deck d;
    d.shuffle(rng);
    std::set<int> seen;
    for (int i = 0; i < NUM_CARDS; ++i) seen.insert(d.deal().id);
    EXPECT_EQ(seen.size(), 52u);
    EXPECT_EQ(d.remaining(), 0);
}

TEST(Deck, DealingFromEmptyDeckThrows) {
    Deck d;
    for (int i = 0; i < NUM_CARDS; ++i) d.deal();
    EXPECT_THROW(d.deal(), std::out_of_range);
}

TEST(Deck, SameSeedGivesSameShuffle) {
    Rng a(42), b(42);
    Deck d1, d2;
    d1.shuffle(a);
    d2.shuffle(b);
    for (int i = 0; i < NUM_CARDS; ++i) EXPECT_EQ(d1.deal(), d2.deal());
}

TEST(Deck, DifferentSeedsGiveDifferentShuffles) {
    Rng a(1), b(2);
    Deck d1, d2;
    d1.shuffle(a);
    d2.shuffle(b);
    bool differs = false;
    for (int i = 0; i < NUM_CARDS; ++i) differs |= (d1.deal() != d2.deal());
    EXPECT_TRUE(differs);
}

TEST(Deck, ShuffleIsRoughlyUniform) {
    // Top card should be each of the 52 cards ~1/52 of the time.
    Rng rng(7);
    Deck d;
    const int trials = 52000;
    std::array<int, NUM_CARDS> counts{};
    for (int t = 0; t < trials; ++t) {
        d.shuffle(rng);
        counts[d.deal().id]++;
    }
    for (int c : counts) {
        EXPECT_GT(c, 800);   // expected 1000, generous bounds
        EXPECT_LT(c, 1200);
    }
}

TEST(Rng, BelowStaysInRange) {
    Rng rng(99);
    for (int i = 0; i < 100000; ++i) EXPECT_LT(rng.below(7), 7u);
}