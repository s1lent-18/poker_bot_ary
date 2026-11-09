#pragma once
#include <algorithm>
#include <array>
#include <cmath>

#include "agents/agent.h"
#include "agents/match.h"
#include "core/deck.h"
#include "core/rng.h"
#include "engine/game_state.h"

namespace poker {

struct DuelResult {
    double bb_per_hand = 0.0;  // agent a's average result, in big blinds per hand
    double ci95 = 0.0;         // half-width of the 95% confidence interval
    long long hands = 0;
};

// Plays a against b (a = seat 0, b = seat 1).
//  duplicate = false: `hands` independent hands, the button alternating.
//  duplicate = true:  `hands/2` deals, each played twice with the two players' hole cards
//                     swapped and the button moved, so card luck mostly cancels.
inline DuelResult duel(Agent& a, Agent& b, const GameConfig& cfg, int hands, uint64_t seed,
                       bool duplicate) {
    Rng rng(seed);
    Deck deck;
    const int units = duplicate ? std::max(1, hands / 2) : hands;
    double sum = 0.0, sumsq = 0.0;

    for (int u = 0; u < units; ++u) {
        deck.shuffle(rng);
        double v;
        if (!duplicate) {
            GameState s(cfg, u % 2, deck);
            v = static_cast<double>(play_hand(s, {&a, &b})[0]) / cfg.big_blind;
        } else {
            std::array<Card, 9> c;
            for (Card& x : c) x = deck.deal();  // p0 hole, p1 hole, 5 board cards
            const std::array<Card, 9> d = {c[2], c[3], c[0], c[1], c[4], c[5], c[6], c[7], c[8]};
            GameState s1(cfg, 0, c), s2(cfg, 1, d);
            const int p1 = play_hand(s1, {&a, &b})[0];
            const int p2 = play_hand(s2, {&a, &b})[0];
            v = static_cast<double>(p1 + p2) / cfg.big_blind;
        }
        sum += v;
        sumsq += v * v;
    }

    const double n = units, mean = sum / n;
    const double var = std::max(0.0, (sumsq - n * mean * mean) / std::max(1.0, n - 1.0));
    const double per = duplicate ? 2.0 : 1.0;  // hands per unit
    DuelResult r;
    r.hands = static_cast<long long>(units * per);
    r.bb_per_hand = mean / per;
    r.ci95 = 1.96 * std::sqrt(var / n) / per;
    return r;
}

}  // namespace poker