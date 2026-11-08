#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <unordered_map>

#include "ai/bucketer.h"
#include "ai/holdem_strategy.h"
#include "core/deck.h"
#include "core/rng.h"
#include "engine/game_state.h"

namespace poker {

// External-sampling MCCFR on the real game engine. Each iteration deals two random hands
// (button fixed at seat 0) and lets each seat take a turn as the "traverser": the traverser
// tries every legal action, the opponent and the cards are sampled.
class HoldemMccfr {
public:
    HoldemMccfr(const GameConfig& cfg, Bucketer bucketer, uint64_t seed);

    void iterate();
    void train(long long n) {
        for (long long i = 0; i < n; ++i) iterate();
    }
    long long iterations() const { return iterations_; }
    size_t num_info_sets() const { return info_.size(); }

    // Normalised average strategy, ready to save or to play with.
    HoldemStrategy export_strategy() const;

private:
    static constexpr int kMax = 5;  // fold, call, up to 2 sized raises, all-in

    struct Info {
        std::array<double, kMax> regret{};
        std::array<double, kMax> strat_sum{};
        int n = 0;
    };
    struct Cache {  // bucket of each (seat, street) for the current deal
        int bucket[2][4];
        Cache() {
            for (auto& row : bucket)
                for (int& x : row) x = -1;
        }
    };

    double walk(const GameState& s, int trav, Cache& cache);
    int bucket_of(const GameState& s, int p) const;
    Info& info_for(const std::string& key, int n);

    GameConfig cfg_;
    Bucketer bucketer_;
    Rng rng_;
    double bb_;
    Deck deck_;
    std::unordered_map<std::string, Info> info_;
    long long iterations_ = 0;
};

}  // namespace poker