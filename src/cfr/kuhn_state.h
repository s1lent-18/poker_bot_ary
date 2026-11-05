#pragma once
#include <array>
#include <string>
#include <vector>

#include "cfr/kuhn.h"  // reuses terminal_payoff() so the rules aren't written twice

namespace poker::cfr {

// Kuhn poker in the generic interface. Actions: 0 = pass/fold, 1 = bet/call.
class KuhnState {
public:
    bool is_chance() const { return stage_ < 2; }
    bool is_terminal() const {
        double p;
        return stage_ == 2 && poker::kuhn::terminal_payoff(cards_, hist_, p);
    }
    int current_player() const { return static_cast<int>(hist_.size() % 2); }

    std::vector<int> actions() const {
        if (stage_ < 2) {
            std::vector<int> v;
            for (int c = 0; c < 3; ++c)
                if (c != cards_[0]) v.push_back(c);
            return v;
        }
        return {0, 1};
    }
    std::vector<double> chance_probs() const {
        const size_t n = actions().size();
        return std::vector<double>(n, 1.0 / n);
    }

    KuhnState child(int a) const {
        KuhnState s = *this;
        if (stage_ < 2) {
            s.cards_[stage_] = a;
            ++s.stage_;
        } else {
            s.hist_ += (a == 0 ? 'p' : 'b');
        }
        return s;
    }

    double utility(int player) const {
        double p0;
        poker::kuhn::terminal_payoff(cards_, hist_, p0);
        return player == 0 ? p0 : -p0;
    }

    std::string info_key() const {
        return std::string(1, "JQK"[cards_[current_player()]]) + hist_;
    }

private:
    std::array<int, 2> cards_{-1, -1};
    std::string hist_;
    int stage_ = 0;  // 0: deal player 0, 1: deal player 1, 2: betting
};

}  // namespace poker::cfr