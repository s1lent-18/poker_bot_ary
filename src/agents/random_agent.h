#pragma once
#include "agents/agent.h"
#include "core/rng.h"

namespace poker {

class RandomAgent : public Agent {
public:
    explicit RandomAgent(uint64_t seed = 1) : rng_(seed) {}

    Action act(const Observation&, const std::vector<Action>& legal) override {
        return legal[rng_.below(legal.size())];
    }
    std::string name() const override { return "Random"; }

private:
    Rng rng_;
};

}  // namespace poker