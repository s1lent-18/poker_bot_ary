#pragma once
#include <string>
#include <vector>

#include "engine/action.h"
#include "engine/game_state.h"

namespace poker {

// Anything that can play poker. It only ever sees an Observation, never the full state.
class Agent {
public:
    virtual ~Agent() = default;
    virtual Action act(const Observation& obs, const std::vector<Action>& legal) = 0;
    virtual std::string name() const = 0;
};

}  // namespace poker