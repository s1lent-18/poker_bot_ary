#pragma once
#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

#include "cfr/cfr.h"  // StrategyFn
#include "cfr/flat_game.h"
#include "core/rng.h"

namespace poker::cfr {

// Shared storage: regrets and strategy sums in flat arrays indexed by info * kFlatMaxActions.
// The FlatGame passed in MUST outlive the solver (we keep a reference).
class FlatSolverBase {
public:
    size_t num_info_sets() const { return g_.info_keys.size(); }
    long long iterations() const { return iterations_; }

    std::vector<double> average_strategy(int info) const {
        const int n = g_.info_actions[info];
        const double* ss = &strat_sum_[static_cast<size_t>(info) * kFlatMaxActions];
        double sum = 0.0;
        for (int a = 0; a < n; ++a) sum += ss[a];
        std::vector<double> out(n, 1.0 / n);
        if (sum > 0.0)
            for (int a = 0; a < n; ++a) out[a] = ss[a] / sum;
        return out;
    }

    // Plugs into the generic exploitability() / profile_value() from cfr.h.
    StrategyFn average_policy() const {
        return [this](const std::string& key, int n) -> std::vector<double> {
            auto it = g_.info_index.find(key);
            if (it == g_.info_index.end()) return std::vector<double>(n, 1.0 / n);
            if (g_.info_actions[it->second] != n)
                throw std::logic_error("action count mismatch for " + key);
            return average_strategy(it->second);
        };
    }

protected:
    explicit FlatSolverBase(const FlatGame& g)
        : g_(g),
          regret_(g.info_keys.size() * kFlatMaxActions, 0.0),
          strat_sum_(g.info_keys.size() * kFlatMaxActions, 0.0) {}

    // Regret matching: play actions in proportion to positive regret.
    static void regret_matching(const double* reg, int n, double* out) {
        double pos = 0.0;
        for (int a = 0; a < n; ++a) pos += std::max(0.0, reg[a]);
        for (int a = 0; a < n; ++a) out[a] = pos > 0.0 ? std::max(0.0, reg[a]) / pos : 1.0 / n;
    }

    const FlatGame& g_;
    std::vector<double> regret_;
    std::vector<double> strat_sum_;
    long long iterations_ = 0;
};

// ---------------------------------------------------------------------------------------
// Full-traversal CFR.
//  Vanilla: both players updated every iteration, uniform averaging (same algorithm as M6b).
//  Plus (CFR+): regrets floored at 0, linearly weighted averaging, alternating player updates.
//  NOTE: for Plus, one "iteration" is one full traversal that updates ONE player, so two
//  iterations = one update of each player.
enum class CfrVariant { Vanilla, Plus };

class FlatCfr : public FlatSolverBase {
public:
    explicit FlatCfr(const FlatGame& g, CfrVariant v = CfrVariant::Vanilla)
        : FlatSolverBase(g), plus_(v == CfrVariant::Plus) {
        if (plus_) {
            info_player_.assign(g.info_keys.size(), -1);
            for (const auto& nd : g.nodes) {
                if (nd.kind == FlatGame::Decision) info_player_[nd.info] = nd.player;
            }
        }
    }
    FlatCfr(FlatGame&&, CfrVariant = CfrVariant::Vanilla) = delete;  // would dangle

    void iterate() {
        if (plus_) {
            const int upd = static_cast<int>(iterations_ % 2);
            const double w = static_cast<double>(iterations_ / 2 + 1);
            walk(0, 1.0, 1.0, 1.0, upd, 0.0);
            for (size_t i = 0; i < g_.info_keys.size(); ++i) {
                if (info_player_[i] == upd) {
                    double* reg = &regret_[i * kFlatMaxActions];
                    double* ss = &strat_sum_[i * kFlatMaxActions];
                    const int n = g_.info_actions[i];
                    double st[kFlatMaxActions];
                    regret_matching(reg, n, st);
                    for (int a = 0; a < n; ++a) {
                        ss[a] += w * st[a];
                        reg[a] = std::max(0.0, reg[a]);
                    }
                }
            }
        } else {
            walk(0, 1.0, 1.0, 1.0, -1, 1.0);
        }
        ++iterations_;
    }
    void train(long long n) {
        for (long long i = 0; i < n; ++i) iterate();
    }

private:
    // Returns player 0's expected utility. upd = player being updated (-1 = both).
    double walk(int idx, double p0, double p1, double pc, int upd, double w) {
        const FlatGame::Node& nd = g_.nodes[idx];
        if (nd.kind == FlatGame::Terminal) return nd.util0;

        const int n = nd.n;
        if (nd.kind == FlatGame::Chance) {
            double v = 0.0;
            for (int i = 0; i < n; ++i) {
                const double pr = g_.prob[nd.first + i];
                v += pr * walk(g_.child[nd.first + i], p0, p1, pc * pr, upd, w);
            }
            return v;
        }

        const int pl = nd.player;
        double* reg = &regret_[static_cast<size_t>(nd.info) * kFlatMaxActions];
        double* ss = &strat_sum_[static_cast<size_t>(nd.info) * kFlatMaxActions];

        double strat[kFlatMaxActions];
        regret_matching(reg, n, strat);

        const double reach_self = pl == 0 ? p0 : p1;
        const double reach_cf = (pl == 0 ? p1 : p0) * pc;  // opponent AND chance reach
        const bool learn = (upd < 0 || upd == pl);
        if (!plus_ && learn)
            for (int a = 0; a < n; ++a) ss[a] += reach_self * strat[a];

        const double sign = pl == 0 ? 1.0 : -1.0;
        double util[kFlatMaxActions];  // utilities for the acting player
        double node_util = 0.0;
        for (int a = 0; a < n; ++a) {
            const int c = g_.child[nd.first + a];
            const double u0 = pl == 0 ? walk(c, p0 * strat[a], p1, pc, upd, w)
                                      : walk(c, p0, p1 * strat[a], pc, upd, w);
            util[a] = sign * u0;
            node_util += strat[a] * util[a];
        }
        if (learn) {
            for (int a = 0; a < n; ++a) {
                reg[a] += reach_cf * (util[a] - node_util);
            }
        }
        return sign * node_util;
    }

    bool plus_;
    std::vector<int8_t> info_player_;
};

// ---------------------------------------------------------------------------------------
// External-sampling MCCFR (Lanctot et al. 2009). Per iteration, each player takes a turn as
// "traverser": at the traverser's nodes we try every action; at chance nodes and at the
// opponent's nodes we sample ONE outcome. Cheap per iteration, noisier per iteration.
class FlatMccfr : public FlatSolverBase {
public:
    FlatMccfr(const FlatGame& g, uint64_t seed) : FlatSolverBase(g), rng_(seed) {}
    FlatMccfr(FlatGame&&, uint64_t) = delete;

    void iterate() {
        for (int trav = 0; trav < 2; ++trav) es_walk(0, trav);
        ++iterations_;
    }
    void train(long long n) {
        for (long long i = 0; i < n; ++i) iterate();
    }

private:
    int sample(const double* p, int n) {
        const double u = rng_.uniform();
        double c = 0.0;
        for (int i = 0; i < n - 1; ++i) {
            c += p[i];
            if (u < c) return i;
        }
        return n - 1;
    }

    // Returns the traverser's expected (sampled) utility.
    double es_walk(int idx, int trav) {
        const FlatGame::Node& nd = g_.nodes[idx];
        if (nd.kind == FlatGame::Terminal) return trav == 0 ? nd.util0 : -nd.util0;

        const int n = nd.n;
        if (nd.kind == FlatGame::Chance) {
            const int i = sample(&g_.prob[nd.first], n);
            return es_walk(g_.child[nd.first + i], trav);
        }

        double* reg = &regret_[static_cast<size_t>(nd.info) * kFlatMaxActions];
        double* ss = &strat_sum_[static_cast<size_t>(nd.info) * kFlatMaxActions];
        double strat[kFlatMaxActions];
        regret_matching(reg, n, strat);

        if (nd.player == trav) {
            double vals[kFlatMaxActions];
            double node = 0.0;
            for (int a = 0; a < n; ++a) {
                vals[a] = es_walk(g_.child[nd.first + a], trav);
                node += strat[a] * vals[a];
            }
            for (int a = 0; a < n; ++a) reg[a] += vals[a] - node;
            return node;
        }

        // Opponent node: its average strategy is accumulated here, then one action is sampled.
        for (int a = 0; a < n; ++a) ss[a] += strat[a];
        const int a = sample(strat, n);
        return es_walk(g_.child[nd.first + a], trav);
    }

    Rng rng_;
};

}  // namespace poker::cfr