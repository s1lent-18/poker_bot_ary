#include "ai/equity.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include "eval/hand_evaluator.h"
#include "eval/preflop_strenght.h"

namespace poker {
namespace {

EquityResult simulate(const std::array<Card, 2>& hole, const std::vector<Card>& board,
                      const std::array<Card, 2>* known_opp, int samples, Rng& rng,
                      double min_opp_strength) {
    if (samples <= 0) throw std::invalid_argument("equity: samples must be > 0");
    const int b = static_cast<int>(board.size());
    if (b != 0 && b != 3 && b != 4 && b != 5)
        throw std::invalid_argument("equity: board must have 0, 3, 4 or 5 cards");

    bool used[NUM_CARDS] = {};
    auto mark = [&](Card c) {
        if (used[c.id]) throw std::invalid_argument("equity: duplicate card " + c.to_string());
        used[c.id] = true;
    };
    mark(hole[0]);
    mark(hole[1]);
    for (const Card& c : board) mark(c);
    if (known_opp) {
        mark((*known_opp)[0]);
        mark((*known_opp)[1]);
    }

    // Pool of cards still in the deck. We re-permute it in place every sample.
    Card pool[NUM_CARDS];
    int m = 0;
    for (int i = 0; i < NUM_CARDS; ++i)
        if (!used[i]) pool[m++] = Card(static_cast<uint8_t>(i));

    const int need = 5 - b;                    // board cards still to come
    const int opp_draw = known_opp ? 0 : 2;    // opponent cards to draw from the pool

    // Hero and opponent 7-card arrays: [hole0, hole1, board0..4]
    Card h[7], o[7];
    h[0] = hole[0];
    h[1] = hole[1];
    for (int i = 0; i < b; ++i) h[2 + i] = o[2 + i] = board[i];
    if (known_opp) {
        o[0] = (*known_opp)[0];
        o[1] = (*known_opp)[1];
    }

    // Partial Fisher-Yates step: put a uniformly random remaining card at position k.
    auto pick = [&](int k) {
        const int j = k + static_cast<int>(rng.below(static_cast<uint64_t>(m - k)));
        std::swap(pool[k], pool[j]);
    };

    double sum = 0.0, sumsq = 0.0;
    for (int s = 0; s < samples; ++s) {
        if (!known_opp) {
            // Rejection-sample the opponent hand if a range restriction is set.
            for (int attempt = 0; attempt < 100; ++attempt) {
                pick(0);
                pick(1);
                if (min_opp_strength <= 0.0 ||
                    preflop_strength(pool[0], pool[1]) >= min_opp_strength)
                    break;
            }
            o[0] = pool[0];
            o[1] = pool[1];
        }
        for (int i = 0; i < need; ++i) {
            pick(opp_draw + i);
            h[2 + b + i] = o[2 + b + i] = pool[opp_draw + i];
        }
        const HandScore hs = evaluate(h, 7), os = evaluate(o, 7);
        const double x = hs > os ? 1.0 : (hs == os ? 0.5 : 0.0);
        sum += x;
        sumsq += x * x;
    }

    EquityResult r;
    r.samples = samples;
    r.equity = sum / samples;
    const double var = std::max(0.0, sumsq / samples - r.equity * r.equity);
    r.std_err = std::sqrt(var / samples);
    return r;
}

}  // namespace

EquityResult equity_vs_random(const std::array<Card, 2>& hole, const std::vector<Card>& board,
                              int samples, Rng& rng, double min_opp_strength) {
    return simulate(hole, board, nullptr, samples, rng, min_opp_strength);
}

EquityResult equity_vs_hand(const std::array<Card, 2>& hole, const std::array<Card, 2>& opp,
                            const std::vector<Card>& board, int samples, Rng& rng) {
    return simulate(hole, board, &opp, samples, rng, 0.0);
}

}  // namespace poker