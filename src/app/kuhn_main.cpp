#include <iomanip>
#include <iostream>

#include "cfr/kuhn.h"

using namespace poker::kuhn;

int main() {
    KuhnCfr cfr;
    int done = 0;

    std::cout << std::fixed << std::setprecision(5);
    std::cout << "iterations    game value    exploitability (antes/game)\n";
    for (int target : {1, 10, 100, 1000, 10000, 100000, 1000000}) {
        cfr.train(target - done);
        done = target;
        const Policy pol = cfr.average_policy();
        std::cout << std::setw(10) << target << "    " << std::setw(10) << game_value(pol, pol)
                  << "    " << exploitability(pol, pol) << "\n";
    }
    std::cout << "\nTheoretical value for player 1: " << -1.0 / 18.0 << "\n";

    std::cout << "\nAverage strategy (P = probability):\n";
    for (const auto& [k, node] : cfr.nodes()) {
        const int card = k[0] == 'J' ? 0 : (k[0] == 'Q' ? 1 : 2);
        const std::string hist = k.substr(1);
        const double pb = cfr.avg_bet_prob(card, hist);
        const bool facing_bet = !hist.empty() && hist.back() == 'b';
        std::cout << "  " << k[0] << "  after '" << hist << "'"
                  << std::string(hist.empty() ? 3 : (4 - hist.size() > 0 ? 4 - hist.size() : 0), ' ')
                  << (facing_bet ? "fold " : "check") << " " << std::setw(7) << 1.0 - pb << "   "
                  << (facing_bet ? "call " : "bet  ") << " " << std::setw(7) << pb << "\n";
    }
    return 0;
}