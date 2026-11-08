#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace poker::cfr {

constexpr int kFlatMaxActions = 4;

// A game tree compiled into flat arrays. Built once from any generic State by exhaustive
// enumeration, so it only suits games whose full tree fits in memory (Kuhn, Leduc).
struct FlatGame {
    enum Kind : uint8_t { Terminal = 0, Chance = 1, Decision = 2 };

    struct Node {
        double util0 = 0.0;  // terminal: player 0's utility
        int first = 0;       // this node's first slot in child[] / prob[]
        int info = -1;       // decision: information-set id
        Kind kind = Terminal;
        int8_t player = -1;  // decision: who acts
        uint8_t n = 0;       // number of children
    };

    std::vector<Node> nodes;   // nodes[0] is the root
    std::vector<int> child;    // child node index for slot (first + i)
    std::vector<double> prob;  // chance probability for slot (first + i); chance nodes only
    std::vector<std::string> info_keys;  // id -> key
    std::vector<int> info_actions;       // id -> number of actions
    std::unordered_map<std::string, int> info_index;  // key -> id

    template <class State>
    static FlatGame build(const State& root) {
        FlatGame g;
        g.add(root);
        return g;
    }

private:
    template <class State>
    int add(const State& s) {
        const int idx = static_cast<int>(nodes.size());
        nodes.emplace_back();

        if (s.is_terminal()) {
            nodes[idx].kind = Terminal;
            nodes[idx].util0 = s.utility(0);
            return idx;
        }

        const std::vector<int> acts = s.actions();
        const int n = static_cast<int>(acts.size());
        if (n < 1 || n > 255) throw std::logic_error("FlatGame: bad child count");
        if (!s.is_chance() && n > kFlatMaxActions)
            throw std::logic_error("FlatGame: too many actions at a decision node");

        const int base = static_cast<int>(child.size());
        child.resize(base + n, -1);
        prob.resize(base + n, 0.0);

        Kind kind;
        int8_t player = -1;
        int info = -1;
        if (s.is_chance()) {
            kind = Chance;
            const std::vector<double> p = s.chance_probs();
            for (int i = 0; i < n; ++i) prob[base + i] = p[i];
        } else {
            kind = Decision;
            player = static_cast<int8_t>(s.current_player());
            const std::string key = s.info_key();
            auto it = info_index.find(key);
            if (it == info_index.end()) {
                info = static_cast<int>(info_keys.size());
                info_index.emplace(key, info);
                info_keys.push_back(key);
                info_actions.push_back(n);
            } else {
                info = it->second;
                if (info_actions[info] != n)
                    throw std::logic_error("FlatGame: inconsistent action count for " + key);
            }
        }

        for (int i = 0; i < n; ++i) {
            const int c = add(s.child(acts[i]));  // may grow the vectors: use indices only
            child[base + i] = c;
        }

        Node& nd = nodes[idx];  // fetch AFTER the recursion
        nd.kind = kind;
        nd.player = player;
        nd.n = static_cast<uint8_t>(n);
        nd.first = base;
        nd.info = info;
        return idx;
    }
};

}  // namespace poker::cfr