#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "cfr/cfr.h"
#include "cfr/leduc.h"

using namespace poker::cfr;

int main(int argc, char** argv) {
    const int max_iters = argc > 1 ? std::atoi(argv[1]) : 10000;
    if (max_iters < 1) {
        std::cerr << "usage: leduc_solver [max_iterations]\n";
        return 1;
    }

    LeducState root;
    Cfr<LeducState> solver(root);
    const auto t0 = std::chrono::steady_clock::now();

    std::cout << std::fixed << std::setprecision(5);
    std::cout << "iterations   game value (P1)   exploitability (chips/game)   elapsed (s)\n";
    int done = 0;
    for (int target : {1, 10, 100, 1000, 2000, 5000, 10000, 20000, 50000, 100000}) {
        if (target > max_iters) break;
        solver.train(target - done);
        done = target;
        const StrategyFn pol = solver.average_policy();
        const double value = profile_value(root, pol);
        const double expl = exploitability(root, pol);
        const double secs =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::cout << std::setw(10) << target << "   " << std::setw(14) << value << "   "
                  << std::setw(24) << expl << "   " << std::setprecision(1) << std::setw(10)
                  << secs << std::setprecision(5) << "\n";
    }
    std::cout << "\nInformation sets: " << solver.num_info_sets()
              << "   (reference game value for Player 1: about -0.0856)\n";

    const StrategyFn pol = solver.average_policy();
    auto show = [&](const std::string& label, const std::string& key,
                    const std::vector<std::string>& names) {
        const std::vector<double> p = pol(key, static_cast<int>(names.size()));
        std::cout << "  " << label << ":";
        for (size_t i = 0; i < names.size(); ++i)
            std::cout << "  " << names[i] << " " << std::setw(7) << p[i];
        std::cout << "\n";
    };

    std::cout << "\nPlayer 1, first action in round 1:\n";
    show("holding J", "0J:", {"check", "bet"});
    show("holding Q", "0Q:", {"check", "bet"});
    show("holding K", "0K:", {"check", "bet"});
    std::cout << "\nPlayer 2, facing a bet in round 1:\n";
    show("holding J", "1J:r", {"fold", "call", "raise"});
    show("holding Q", "1Q:r", {"fold", "call", "raise"});
    show("holding K", "1K:r", {"fold", "call", "raise"});
    return 0;
}