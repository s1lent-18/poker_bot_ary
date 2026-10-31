#pragma once
#include <array>
#include <stdexcept>
#include <utility>

#include "core/card.h"
#include "core/rng.h"

namespace poker {

class Deck {
public:
    Deck() { reset(); }

    // Restore the full ordered deck and put the cursor at the top.
    void reset() {
        for (int i = 0; i < NUM_CARDS; ++i) cards_[i] = Card(static_cast<uint8_t>(i));
        top_ = 0;
    }

    // Fisher-Yates shuffle of the full deck (also resets the cursor).
    void shuffle(Rng& rng) {
        reset();
        for (int i = NUM_CARDS - 1; i > 0; --i) {
            int j = static_cast<int>(rng.below(static_cast<uint64_t>(i) + 1));
            std::swap(cards_[i], cards_[j]);
        }
    }

    Card deal() {
        if (top_ >= NUM_CARDS) throw std::out_of_range("Deck is empty");
        return cards_[top_++];
    }

    int remaining() const { return NUM_CARDS - top_; }

private:
    std::array<Card, NUM_CARDS> cards_;
    int top_ = 0;
};

}  // namespace poker