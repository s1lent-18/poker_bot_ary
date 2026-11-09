#pragma once
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "ai/bucketer.h"
#include "engine/game_config.h"
#include "engine/game_state.h"

namespace poker {

// sizes = 1: raise menu {pot} + all-in.   sizes = 2: {1/2 pot, pot} + all-in.
inline GameConfig holdem_config(int sizes = 1, int stack_bb = 100) {
    if (sizes != 1 && sizes != 2) throw std::invalid_argument("holdem_config: sizes must be 1 or 2");
    GameConfig c;
    c.small_blind = 50;
    c.big_blind = 100;
    c.stack = stack_bb * c.big_blind;
    if (sizes == 1) {
        c.raise_fractions = {1.0, 0.0, 0.0, 0.0};
        c.num_raise_fractions = 1;
    } else {
        c.raise_fractions = {0.5, 1.0, 0.0, 0.0};
        c.num_raise_fractions = 2;
    }
    return c;
}

// Everything after the last '/' of an engine history string = betting on the current street.
inline std::string street_history(const std::string& full) {
    const size_t p = full.rfind('/');
    return p == std::string::npos ? full : full.substr(p + 1);
}

// Info set key. role: 0 = button (small blind), 1 = big blind.
// pot_start = pot at the start of the street (equal stacks => this fixes the stacks behind).
inline std::string holdem_key(int role, int street, int bucket, int pot_start,
                              const std::string& street_hist) {
    std::string k;
    k.reserve(24 + street_hist.size());
    k += std::to_string(role);
    k += '|';
    k += std::to_string(street);
    k += '|';
    k += std::to_string(bucket);
    k += '|';
    k += std::to_string(pot_start);
    k += '|';
    k += street_hist;
    return k;
}

// The same key from the two views of a game: what a player sees, and the full state.
inline std::string holdem_key_from_observation(const Observation& ob, int bucket) {
    const int role = ob.player == ob.button ? 0 : 1;
    return holdem_key(role, static_cast<int>(ob.street), bucket, ob.pot - ob.my_bet - ob.opp_bet,
                      street_history(ob.history));
}

inline std::string holdem_key_from_state(const GameState& s, int bucket) {
    const int p = s.current_player();
    const int role = p == s.button() ? 0 : 1;
    return holdem_key(role, static_cast<int>(s.street()), bucket, s.pot() - s.bet(0) - s.bet(1),
                      street_history(s.history()));
}

// A trained bot: the game settings, the card bucketing, and the average strategy.
struct HoldemStrategy {
    GameConfig cfg;
    Bucketer bucketer;
    std::unordered_map<std::string, std::vector<float>> table;  // key -> P(action), legal order
    long long iterations = 0;  // training iterations behind this strategy (0 = unknown)

    void save(std::ostream& os) const;
    static HoldemStrategy load(std::istream& is);
};

}  // namespace poker