#pragma once
#include <algorithm>
#include <array>
#include <functional>
#include <map>
#include <string>

// Kuhn poker: cards 0,1,2 = J,Q,K.  Actions: 'p' (pass/check/fold) and 'b' (bet/call).
// History strings: "", "p", "b", "pp", "pb", "bp", "bb", "pbp", "pbb".
namespace poker::kuhn {

// P(bet) for a given (own card, history). Used to describe any strategy.
using Policy = std::function<double(int card, const std::string& history)>;

// If h is terminal, sets payoff to PLAYER 0's payoff and returns true.
inline bool terminal_payoff(const std::array<int, 2>& c, const std::string& h, double& payoff0) {
    if (h == "pp") { payoff0 = c[0] > c[1] ? 1 : -1; return true; }
    if (h == "bp") { payoff0 = 1; return true; }    // player 1 folded to our bet
    if (h == "pbp") { payoff0 = -1; return true; }  // we folded to player 1's bet
    if (h == "bb" || h == "pbb") { payoff0 = c[0] > c[1] ? 2 : -2; return true; }
    return false;
}

// Expected payoff for player 0 for one fixed deal, given both players' policies.
inline double ev_node(const std::array<int, 2>& c, const std::string& h, const Policy& p0,
                      const Policy& p1) {
    double pay;
    if (terminal_payoff(c, h, pay)) return pay;
    const int player = static_cast<int>(h.size() % 2);
    const double pb = (player == 0 ? p0 : p1)(c[player], h);
    double v = 0.0;
    if (pb > 0.0) v += pb * ev_node(c, h + 'b', p0, p1);
    if (pb < 1.0) v += (1.0 - pb) * ev_node(c, h + 'p', p0, p1);
    return v;
}

// Player 0's expected payoff averaged over all 6 deals.
inline double game_value(const Policy& p0, const Policy& p1) {
    double total = 0.0;
    int n = 0;
    for (int c0 = 0; c0 < 3; ++c0)
        for (int c1 = 0; c1 < 3; ++c1) {
            if (c0 == c1) continue;
            total += ev_node({c0, c1}, "", p0, p1);
            ++n;
        }
    return total / n;
}

// Best-response value for `player` against `opp`'s fixed policy.
// Kuhn is tiny (each player has 6 info sets), so we simply try all 2^6 = 64 pure strategies.
inline double best_response_value(int player, const Policy& opp) {
    double best = -1e18;
    for (int mask = 0; mask < 64; ++mask) {
        // Info set index = card*2 + (are we in the "second decision" spot?)
        // Player 0's second spot is history "pb"; player 1's is "b".
        Policy pure = [mask, player](int card, const std::string& h) {
            const int second = (player == 0) ? (h == "pb") : (h == "b");
            return static_cast<double>((mask >> (card * 2 + second)) & 1);
        };
        const double v0 = (player == 0) ? game_value(pure, opp) : game_value(opp, pure);
        best = std::max(best, player == 0 ? v0 : -v0);
    }
    return best;
}

// How much a perfect opponent could win per game, on average over both seats.
// 0 exactly at a Nash equilibrium. Units: antes per game.
inline double exploitability(const Policy& p0, const Policy& p1) {
    return 0.5 * (best_response_value(0, p1) + best_response_value(1, p0));
}

class KuhnCfr {
public:
    struct Node {
        std::array<double, 2> regret_sum{};     // index 0 = pass, 1 = bet
        std::array<double, 2> strategy_sum{};   // reach-weighted sum of strategies played
    };

    // One full-tree iteration over all 6 deals. Returns player 0's expected value
    // under the CURRENT (not average) strategy.
    double iterate() {
        double total = 0.0;
        for (int c0 = 0; c0 < 3; ++c0)
            for (int c1 = 0; c1 < 3; ++c1) {
                if (c0 == c1) continue;
                total += cfr({c0, c1}, "", 1.0, 1.0);
            }
        ++iterations_;
        return total / 6.0;
    }

    void train(int n) {
        for (int i = 0; i < n; ++i) iterate();
    }

    int iterations() const { return iterations_; }

    // Average strategy: probability of betting (or calling) at an info set.
    double avg_bet_prob(int card, const std::string& history) const {
        auto it = nodes_.find(key(card, history));
        if (it == nodes_.end()) return 0.5;
        const double sum = it->second.strategy_sum[0] + it->second.strategy_sum[1];
        return sum > 0.0 ? it->second.strategy_sum[1] / sum : 0.5;
    }

    // Valid only while this KuhnCfr object is alive.
    Policy average_policy() const {
        return [this](int card, const std::string& h) { return avg_bet_prob(card, h); };
    }

    const std::map<std::string, Node>& nodes() const { return nodes_; }

private:
    static std::string key(int card, const std::string& h) {
        return std::string(1, "JQK"[card]) + h;
    }

    static std::array<double, 2> regret_matching(const Node& n) {
        const double a = std::max(0.0, n.regret_sum[0]);
        const double b = std::max(0.0, n.regret_sum[1]);
        const double s = a + b;
        if (s > 0.0) return {a / s, b / s};
        return {0.5, 0.5};
    }

    // Returns the expected utility for the player to act at history h.
    // p0, p1 = probability each player's own actions reach this history.
    double cfr(const std::array<int, 2>& c, const std::string& h, double p0, double p1) {
        const int player = static_cast<int>(h.size() % 2);

        double pay0;
        if (terminal_payoff(c, h, pay0)) return player == 0 ? pay0 : -pay0;

        Node& node = nodes_[key(c[player], h)];  // std::map references stay valid
        const double reach_self = player == 0 ? p0 : p1;
        const double reach_opp = player == 0 ? p1 : p0;

        const std::array<double, 2> strat = regret_matching(node);
        for (int a = 0; a < 2; ++a) node.strategy_sum[a] += reach_self * strat[a];

        std::array<double, 2> util{};
        double node_util = 0.0;
        for (int a = 0; a < 2; ++a) {
            const std::string next = h + (a == 0 ? 'p' : 'b');
            // Zero-sum: the child's utility is for the OTHER player, so negate it.
            util[a] = player == 0 ? -cfr(c, next, p0 * strat[a], p1)
                                  : -cfr(c, next, p0, p1 * strat[a]);
            node_util += strat[a] * util[a];
        }

        // Counterfactual regret: weighted by how likely the OPPONENT made us get here.
        for (int a = 0; a < 2; ++a) node.regret_sum[a] += reach_opp * (util[a] - node_util);
        return node_util;
    }

    std::map<std::string, Node> nodes_;
    int iterations_ = 0;
};

}  // namespace poker::kuhn