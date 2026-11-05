#pragma once
#include <algorithm>
#include <array>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

// Generic game interface (State is a cheap-to-copy value type):
//   bool is_terminal() const;            bool is_chance() const;
//   int current_player() const;          // decision nodes only
//   std::vector<int> actions() const;    // chance nodes: the possible outcomes
//   std::vector<double> chance_probs() const;   // chance nodes, parallel to actions()
//   State child(int action) const;
//   double utility(int player) const;    // terminal nodes, zero-sum
//   std::string info_key() const;        // decision nodes: what the acting player knows
namespace poker::cfr {

// Maps (info-set key, number of actions) -> action probabilities, in actions() order.
using StrategyFn = std::function<std::vector<double>(const std::string& key, int num_actions)>;

inline StrategyFn uniform_strategy() {
    return [](const std::string&, int n) { return std::vector<double>(n, 1.0 / n); };
}

// ---------------------------------------------------------------- vanilla CFR
template <class State>
class Cfr {
public:
    static constexpr int kMaxActions = 4;

    explicit Cfr(State root) : root_(std::move(root)) {}

    void iterate() {
        traverse(root_, 1.0, 1.0, 1.0);
        ++iterations_;
    }
    void train(int n) {
        for (int i = 0; i < n; ++i) iterate();
    }
    int iterations() const { return iterations_; }
    size_t num_info_sets() const { return nodes_.size(); }

    // The AVERAGE strategy is the one that converges to equilibrium.
    std::vector<double> average_strategy(const std::string& key, int n) const {
        std::vector<double> out(n, 1.0 / n);
        auto it = nodes_.find(key);
        if (it == nodes_.end()) return out;
        double sum = 0.0;
        for (int a = 0; a < n; ++a) sum += it->second.strategy_sum[a];
        if (sum > 0.0)
            for (int a = 0; a < n; ++a) out[a] = it->second.strategy_sum[a] / sum;
        return out;
    }

    // Valid only while this solver object is alive.
    StrategyFn average_policy() const {
        return [this](const std::string& key, int n) { return average_strategy(key, n); };
    }

private:
    struct Node {
        std::array<double, kMaxActions> regret{};
        std::array<double, kMaxActions> strategy_sum{};
    };

    // Returns player 0's expected utility from s. p0/p1 = each player's own reach
    // probability, pc = chance reach. (Players don't strictly alternate in Leduc, so we
    // always return player 0's value and flip the sign for player 1.)
    double traverse(const State& s, double p0, double p1, double pc) {
        if (s.is_terminal()) return s.utility(0);
        const std::vector<int> acts = s.actions();
        const int n = static_cast<int>(acts.size());

        if (s.is_chance()) {
            const std::vector<double> probs = s.chance_probs();
            double v = 0.0;
            for (int i = 0; i < n; ++i)
                v += probs[i] * traverse(s.child(acts[i]), p0, p1, pc * probs[i]);
            return v;
        }

        if (n > kMaxActions) throw std::logic_error("too many actions for Cfr::kMaxActions");
        const int pl = s.current_player();
        Node& node = nodes_[s.info_key()];  // unordered_map references stay valid on insert

        // Regret matching: play actions in proportion to positive regret.
        std::array<double, kMaxActions> strat{};
        double pos = 0.0;
        for (int a = 0; a < n; ++a) pos += std::max(0.0, node.regret[a]);
        for (int a = 0; a < n; ++a)
            strat[a] = pos > 0.0 ? std::max(0.0, node.regret[a]) / pos : 1.0 / n;

        const double reach_self = pl == 0 ? p0 : p1;
        const double reach_cf = (pl == 0 ? p1 : p0) * pc;  // opponent AND chance reach
        for (int a = 0; a < n; ++a) node.strategy_sum[a] += reach_self * strat[a];

        const double sign = pl == 0 ? 1.0 : -1.0;
        std::array<double, kMaxActions> util{};  // utilities for the acting player
        double node_util = 0.0;
        for (int a = 0; a < n; ++a) {
            const State next = s.child(acts[a]);
            const double u0 = pl == 0 ? traverse(next, p0 * strat[a], p1, pc)
                                      : traverse(next, p0, p1 * strat[a], pc);
            util[a] = sign * u0;
            node_util += strat[a] * util[a];
        }
        for (int a = 0; a < n; ++a) node.regret[a] += reach_cf * (util[a] - node_util);
        return sign * node_util;
    }

    State root_;
    std::unordered_map<std::string, Node> nodes_;
    int iterations_ = 0;
};

// -------------------------------------------------------- evaluation utilities

// Player 0's expected utility when both players follow `strategy`.
template <class State>
double profile_value(const State& s, const StrategyFn& strategy) {
    if (s.is_terminal()) return s.utility(0);
    const std::vector<int> acts = s.actions();
    const int n = static_cast<int>(acts.size());
    double v = 0.0;
    if (s.is_chance()) {
        const std::vector<double> probs = s.chance_probs();
        for (int i = 0; i < n; ++i) v += probs[i] * profile_value(s.child(acts[i]), strategy);
        return v;
    }
    const std::vector<double> st = strategy(s.info_key(), n);
    for (int i = 0; i < n; ++i)
        if (st[i] > 0.0) v += st[i] * profile_value(s.child(acts[i]), strategy);
    return v;
}

// Exact best-response value for `player` against the opponent's fixed strategy.
// Perfect recall lets us decide each of the player's info sets once, from the
// opponent-and-chance-weighted values of every history in that info set.
template <class State>
double best_response_value(const State& root, int player, const StrategyFn& opp_strategy) {
    struct Member {
        State state;
        double weight;  // opponent + chance reach probability of this history
    };
    std::unordered_map<std::string, std::vector<Member>> infosets;

    // Pass 1: collect the player's info sets, following only positive-probability branches.
    std::function<void(const State&, double)> collect = [&](const State& s, double w) {
        if (s.is_terminal()) return;
        const std::vector<int> acts = s.actions();
        const int n = static_cast<int>(acts.size());
        if (s.is_chance()) {
            const std::vector<double> probs = s.chance_probs();
            for (int i = 0; i < n; ++i) collect(s.child(acts[i]), w * probs[i]);
        } else if (s.current_player() == player) {
            infosets[s.info_key()].push_back(Member{s, w});
            for (int i = 0; i < n; ++i) collect(s.child(acts[i]), w);
        } else {
            const std::vector<double> st = opp_strategy(s.info_key(), n);
            for (int i = 0; i < n; ++i)
                if (st[i] > 0.0) collect(s.child(acts[i]), w * st[i]);
        }
    };
    collect(root, 1.0);

    // Pass 2: value under best-response play, deciding info sets lazily (bottom-up).
    std::unordered_map<std::string, int> best_action;
    std::function<double(const State&)> value;
    std::function<int(const std::string&, int)> choose;

    value = [&](const State& s) -> double {
        if (s.is_terminal()) return s.utility(player);
        const std::vector<int> acts = s.actions();
        const int n = static_cast<int>(acts.size());
        double v = 0.0;
        if (s.is_chance()) {
            const std::vector<double> probs = s.chance_probs();
            for (int i = 0; i < n; ++i) v += probs[i] * value(s.child(acts[i]));
            return v;
        }
        if (s.current_player() == player) {
            const int a = choose(s.info_key(), n);
            return value(s.child(acts[a]));
        }
        const std::vector<double> st = opp_strategy(s.info_key(), n);
        for (int i = 0; i < n; ++i)
            if (st[i] > 0.0) v += st[i] * value(s.child(acts[i]));
        return v;
    };

    choose = [&](const std::string& key, int n) -> int {
        auto it = best_action.find(key);
        if (it != best_action.end()) return it->second;
        std::vector<double> cfv(n, 0.0);  // counterfactual value of each action
        for (const Member& m : infosets.at(key)) {
            const std::vector<int> acts = m.state.actions();
            for (int a = 0; a < n; ++a) cfv[a] += m.weight * value(m.state.child(acts[a]));
        }
        const int arg = static_cast<int>(std::max_element(cfv.begin(), cfv.end()) - cfv.begin());
        best_action[key] = arg;
        return arg;
    };

    return value(root);
}

// Average amount a perfect opponent could win per game; 0 exactly at a Nash equilibrium.
template <class State>
double exploitability(const State& root, const StrategyFn& strategy) {
    return 0.5 * (best_response_value(root, 0, strategy) + best_response_value(root, 1, strategy));
}

}  // namespace poker::cfr