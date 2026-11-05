#pragma once
#include <algorithm>

#include "agents/agent.h"
#include "ai/equity.h"
#include "core/rng.h"

namespace poker {

// Decides by Monte Carlo equity vs pot odds.
//  - Facing a bet: raise with very strong equity, call if equity beats pot odds + margin, else fold.
//  - Not facing a bet: bet for value with strong equity, sometimes thin-bet, occasionally bluff.
// When the opponent has voluntarily bet or raised, their range is narrowed in proportion to the
// bet size, since "equity vs a random hand" badly overestimates our chances against a big bet.
class EquityAgent : public Agent {
public:
    explicit EquityAgent(uint64_t seed = 1, int samples = 2000) : rng_(seed), samples_(samples) {}
    std::string name() const override { return "EquityBot"; }

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
        // Raises ascend; the last one is all-in, the rest are sized raises.
        const Action* allin = raises.empty() ? nullptr : &raises.back();
        std::vector<Action> sized;
        if (!raises.empty()) sized.assign(raises.begin(), raises.end() - 1);

        const bool facing = (call != nullptr);
        // The small blind's first action is facing a forced blind, not a real bet.
        const bool opp_acted = facing && !(ob.street == Street::Preflop && ob.history.empty());

        double min_s = 0.0;
        if (opp_acted) {
            const double before = std::max(1, ob.pot - ob.to_call);  // pot before their bet
            min_s = std::clamp(0.35 * ob.to_call / before, 0.0, 0.55);
        }
        const double eq = equity_vs_random(ob.hole, ob.board, samples_, rng_, min_s).equity;

        if (facing) {
            if (allin && eq >= 0.75) {
                if (eq >= 0.92 && chance(0.3)) return *allin;
                if (!sized.empty() && chance(0.7)) return eq >= 0.85 ? sized.back() : sized.front();
            }
            const double pot_odds = double(ob.to_call) / (ob.pot + ob.to_call);
            const double margin = ob.street == Street::Preflop ? 0.08 : 0.04;
            return eq > pot_odds + margin ? *call : *fold;
        }

        if (!sized.empty()) {
            if (eq >= 0.75 && chance(0.85))
                return (allin && eq >= 0.92 && chance(0.2)) ? *allin : sized.back();
            if (eq >= 0.58 && chance(0.6)) return sized.front();
            if (eq < 0.40 && chance(0.06)) return sized.front();  // bluff
        }
        return *check;
    }

private:
    bool chance(double p) { return rng_.uniform() < p; }
    Rng rng_;
    int samples_;
};

}  // namespace poker