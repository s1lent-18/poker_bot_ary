#pragma once
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "agents/agent.h"
#include "ai/holdem_strategy.h"
#include "core/rng.h"

namespace poker {

// Plays a trained strategy. If it reaches an info set it never saw in training, it falls back
// to check/call and counts it, so you can see how well training covered real play.
class CfrAgent : public Agent {
public:
    explicit CfrAgent(std::shared_ptr<const HoldemStrategy> strategy, uint64_t seed = 1,
                      std::string label = "CfrBot")
        : strat_(std::move(strategy)), rng_(seed), label_(std::move(label)) {
        if (!strat_) throw std::invalid_argument("CfrAgent: no strategy");
    }

    std::string name() const override { return label_; }
    long long decisions() const { return decisions_; }
    long long fallbacks() const { return fallbacks_; }

    Action act(const Observation& ob, const std::vector<Action>& legal) override {
        ++decisions_;
        const int bucket = ob.street == Street::Preflop
                               ? preflop_class(ob.hole[0], ob.hole[1])
                               : strat_->bucketer.bucket(ob.hole, ob.board);
        const auto it = strat_->table.find(holdem_key_from_observation(ob, bucket));
        if (it == strat_->table.end() || it->second.size() != legal.size()) {
            ++fallbacks_;
            return fallback(legal);
        }
        const std::vector<float>& p = it->second;
        const double u = rng_.uniform();
        double cum = 0.0;
        for (size_t i = 0; i + 1 < p.size(); ++i) {
            cum += p[i];
            if (u < cum) return legal[i];
        }
        return legal.back();
    }

private:
    static Action fallback(const std::vector<Action>& legal) {
        for (const Action& a : legal)
            if (a.type == ActionType::Check) return a;
        for (const Action& a : legal)
            if (a.type == ActionType::Call) return a;
        return legal[0];
    }

    std::shared_ptr<const HoldemStrategy> strat_;
    Rng rng_;
    std::string label_;
    long long decisions_ = 0;
    long long fallbacks_ = 0;
};

}  // namespace poker