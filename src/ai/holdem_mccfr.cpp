#include "ai/holdem_mccfr.h"

#include <algorithm>
#include <stdexcept>
#include <utility>
#include <vector>

namespace poker {

HoldemMccfr::HoldemMccfr(const GameConfig& cfg, Bucketer bucketer, uint64_t seed)
    : cfg_(cfg), bucketer_(std::move(bucketer)), rng_(seed), bb_(cfg.big_blind) {
    if (!bucketer_.calibrated())
        throw std::invalid_argument("HoldemMccfr: the bucketer must be calibrated");
}

HoldemMccfr::Info& HoldemMccfr::info_for(const std::string& key, int n) {
    Info& info = info_[key];  // references into unordered_map stay valid when it grows
    if (info.n == 0)
        info.n = n;
    else if (info.n != n)
        throw std::logic_error("HoldemMccfr: legal action count changed for info set " + key);
    return info;
}

int HoldemMccfr::bucket_of(const GameState& s, int p) const {
    if (s.street() == Street::Preflop) return preflop_class(s.hole(p)[0], s.hole(p)[1]);
    return bucketer_.bucket(s.hole(p), s.observe(p).board);
}

double HoldemMccfr::walk(const GameState& s, int trav, Cache& cache) {
    if (s.is_terminal()) return static_cast<double>(s.payoffs()[trav]) / bb_;  // in big blinds

    const int p = s.current_player();
    const std::vector<Action> legal = s.legal_actions();
    const int n = static_cast<int>(legal.size());
    if (n > kMax) throw std::logic_error("HoldemMccfr: too many legal actions");
    if (n == 1) {  // forced move, nothing to learn
        GameState c = s;
        c.apply(legal[0]);
        return walk(c, trav, cache);
    }

    int& b = cache.bucket[p][static_cast<int>(s.street())];
    if (b < 0) b = bucket_of(s, p);
    Info& info = info_for(holdem_key_from_state(s, b), n);

    // Regret matching
    double strat[kMax];
    double pos = 0.0;
    for (int a = 0; a < n; ++a) pos += std::max(0.0, info.regret[a]);
    for (int a = 0; a < n; ++a)
        strat[a] = pos > 0.0 ? std::max(0.0, info.regret[a]) / pos : 1.0 / n;

    if (p == trav) {
        double vals[kMax];
        double node = 0.0;
        for (int a = 0; a < n; ++a) {
            GameState c = s;
            c.apply(legal[a]);
            vals[a] = walk(c, trav, cache);
            node += strat[a] * vals[a];
        }
        for (int a = 0; a < n; ++a) info.regret[a] += vals[a] - node;
        return node;
    }

    // Opponent node: accumulate its average strategy, then sample one action.
    for (int a = 0; a < n; ++a) info.strat_sum[a] += strat[a];
    int pick = n - 1;
    const double u = rng_.uniform();
    double cum = 0.0;
    for (int a = 0; a + 1 < n; ++a) {
        cum += strat[a];
        if (u < cum) {
            pick = a;
            break;
        }
    }
    GameState c = s;
    c.apply(legal[pick]);
    return walk(c, trav, cache);
}

void HoldemMccfr::iterate() {
    for (int trav = 0; trav < 2; ++trav) {
        deck_.shuffle(rng_);
        const GameState s(cfg_, 0, deck_);
        Cache cache;
        walk(s, trav, cache);
    }
    ++iterations_;
}

HoldemStrategy HoldemMccfr::export_strategy() const {
    HoldemStrategy out;
    out.cfg = cfg_;
    out.bucketer = bucketer_;
    out.iterations = iterations_;
    out.table.reserve(info_.size());
    for (const auto& kv : info_) {
        const Info& info = kv.second;
        double sum = 0.0;
        for (int a = 0; a < info.n; ++a) sum += info.strat_sum[a];
        std::vector<float> p(static_cast<size_t>(info.n));
        for (int a = 0; a < info.n; ++a)
            p[a] = sum > 0.0 ? static_cast<float>(info.strat_sum[a] / sum) : 1.0f / info.n;
        out.table.emplace(kv.first, std::move(p));
    }
    return out;
}

}  // namespace poker