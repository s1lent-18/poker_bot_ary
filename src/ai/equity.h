#pragma once
#include <array>
#include <vector>

#include "core/card.h"
#include "core/rng.h"

namespace poker {

struct EquityResult {
    double equity = 0.0;   // P(win) + 0.5 * P(tie)
    double std_err = 0.0;  // standard error of the estimate
    int samples = 0;
};

// Our equity against a uniformly random opponent hand.
// If min_opp_strength > 0, the opponent hand is restricted to hands whose
// preflop_strength() is at least that value (a crude "they bet, so they're strong" model).
// board must have 0, 3, 4 or 5 cards.
EquityResult equity_vs_random(const std::array<Card, 2>& hole, const std::vector<Card>& board,
                              int samples, Rng& rng, double min_opp_strength = 0.0);

// Our equity against one specific opponent hand.
EquityResult equity_vs_hand(const std::array<Card, 2>& hole, const std::array<Card, 2>& opp,
                            const std::vector<Card>& board, int samples, Rng& rng);

}  // namespace poker