#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/card.h"

namespace poker {

enum class HandCategory : uint8_t {
    HighCard = 0,
    Pair,
    TwoPair,
    ThreeOfAKind,
    Straight,
    Flush,
    FullHouse,
    FourOfAKind,
    StraightFlush,  // includes royal flush
};

using HandScore = uint32_t;  // higher = better, equal = tie

// Evaluates the best 5-card hand from 5..7 cards.
HandScore evaluate(const Card* cards, int n);

inline HandScore evaluate(const std::vector<Card>& cards) {
    return evaluate(cards.data(), static_cast<int>(cards.size()));
}

inline HandCategory category_of(HandScore s) { return static_cast<HandCategory>(s >> 20); }

std::string category_name(HandCategory c);

}  // namespace poker