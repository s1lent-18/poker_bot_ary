#pragma once
#include <algorithm>
#include <cmath>

#include "agents/agent.h"
#include "core/rng.h"
#include "eval/hand_evaluator.h"
#include "eval/preflop_strenght.h"

namespace poker {

// Rough made-hand strength on the flop/turn/river (no draw logic yet).
inline double postflop_strength(const std::array<Card, 2>& hole, const std::vector<Card>& board) {
    std::vector<Card> all(hole.begin(), hole.end());
    all.insert(all.end(), board.begin(), board.end());
    const HandScore sc = evaluate(all);

    // If the board alone makes the same hand, we have nothing special.
    if (board.size() == 5 && evaluate(board) == sc) return 0.25;

    int board_hi = -1;
    for (const Card& c : board) board_hi = std::max(board_hi, c.rank());

    switch (category_of(sc)) {
        case HandCategory::HighCard:
            return std::max(hole[0].rank(), hole[1].rank()) == 12 ? 0.15 : 0.10;
        case HandCategory::Pair:
            if (hole[0].rank() == hole[1].rank())
                return hole[0].rank() > board_hi ? 0.60 : 0.40;  // overpair vs underpair
            for (const Card& h : hole)
                if (h.rank() == board_hi) return 0.55;  // top pair
            for (const Card& h : hole)
                for (const Card& b : board)
                    if (h.rank() == b.rank()) return 0.40;  // weaker pair
            return 0.12;  // the pair is on the board
        case HandCategory::TwoPair: return 0.72;
        case HandCategory::ThreeOfAKind: return 0.80;
        case HandCategory::Straight: return 0.85;
        case HandCategory::Flush: return 0.88;
        case HandCategory::FullHouse: return 0.93;
        case HandCategory::FourOfAKind: return 0.98;
        case HandCategory::StraightFlush: return 1.00;
    }
    return 0.0;
}

class RuleAgent : public Agent {
public:
    explicit RuleAgent(uint64_t seed = 1) : rng_(seed) {}
    std::string name() const override { return "RuleBot"; }

    Action act(const Observation& ob, const std::vector<Action>& legal) override {
        const Action *fold = nullptr, *check = nullptr, *call = nullptr;
        std::vector<Action> raises;
        for (const Action& a : legal) {
            switch (a.type) {
                case ActionType::Fold: fold = &a; break;
                case ActionType::Check: check = &a; break;
                case ActionType::Call: call = &a; break;
                case ActionType::Raise: raises.push_back(a); break;
            }
        }
        // Raises are ascending and the last one is all-in; the rest are "sized" raises.
        const Action* allin = raises.empty() ? nullptr : &raises.back();
        std::vector<Action> sized;
        if (!raises.empty()) sized.assign(raises.begin(), raises.end() - 1);

        const bool preflop = ob.street == Street::Preflop;
        const double s = preflop ? preflop_strength(ob.hole[0], ob.hole[1])
                                 : postflop_strength(ob.hole, ob.board);

        const bool facing = (call != nullptr);
        // Preflop with no real raise yet: SB's first action, or BB's option after a limp.
        const bool unraised = preflop && (ob.history.empty() || ob.history == "c");

        const double pot_odds = facing ? double(ob.to_call) / (ob.pot + ob.to_call) : 0.0;
        const double call_threshold = std::min(0.8 * pot_odds, 0.75);

        // Strong: value-raise, sometimes shove with monsters.
        if (s >= 0.7) {
            if (allin && s >= 0.9 && chance(0.3)) return *allin;
            if (!sized.empty() && chance(0.75)) return sized.back();
            return check ? *check : *call;
        }

        // Medium: sometimes bet/raise when the pot is unraised, else play on pot odds.
        if (s >= 0.4) {
            if ((!facing || unraised) && !sized.empty() && chance(0.5)) return sized.front();
            if (!facing) return *check;
            return s > call_threshold ? *call : *fold;
        }

        // Weak: mostly give up, with a small bluff frequency.
        if ((!facing || unraised) && !sized.empty() && chance(0.07)) return sized.front();
        if (!facing) return *check;
        return s > call_threshold ? *call : *fold;
    }

private:
    bool chance(double p) { return rng_.uniform() < p; }
    Rng rng_;
};

}  // namespace poker