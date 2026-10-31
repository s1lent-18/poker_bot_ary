#pragma once
#include <array>
#include <functional>

#include "agents/agent.h"
#include "engine/game_state.h"

namespace poker {

// Called after each action is applied (used by the CLI to print what the bot did).
using ActionObserver = std::function<void(int player, const Action&, const GameState&)>;

// Plays one hand to completion and returns the payoffs.
inline std::array<int, 2> play_hand(GameState& s, const std::array<Agent*, 2>& agents,
                                    const ActionObserver& observer = nullptr) {
    while (!s.is_terminal()) {
        const int p = s.current_player();
        const Action a = agents[p]->act(s.observe(p), s.legal_actions());
        s.apply(a);  // throws if the agent returned an illegal action
        if (observer) observer(p, a, s);
    }
    return s.payoffs();
}

}  // namespace poker