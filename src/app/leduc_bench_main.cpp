#include <chrono>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "cfr/cfr.h"
#include "cfr/flat_cfr.h"
#include "cfr/flat_game.h"
#include "cfr/leduc.h"

using namespace poker::cfr;
using Clock = std::chrono::steady_clock;

static double since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

// Trains in chunks; training time excludes the (exact) exploitability evaluations.
template <class Solver>
static void run(const std::string& name, Solver& solver, const LeducState& root,
                const std::vector<long long>& checkpoints) {
    double train_secs = 0.0;
    long long done = 0;
    for (long long target : checkpoints) {
        const auto t0 = Clock::now();
        solver.train(target - done);
        train_secs += since(t0);
        done = target;
        const double e = exploitability(root, solver.average_policy());
        std::cout << std::left << std::setw(14) << name << std::right << std::setw(11) << target
                  << std::setw(15) << train_secs << std::setw(17) << e << "\n";
    }
}

int main(int argc, char** argv) {
    const bool quick = argc > 1 && std::string(argv[1]) == "quick";
    const LeducState root;

    auto t0 = Clock::now();
    const FlatGame game = FlatGame::build(root);
    std::cout << std::fixed << std::setprecision(5);
    std::cout << "Flattened Leduc tree: " << game.nodes.size() << " nodes, "
              << game.info_keys.size() << " info sets (built in " << since(t0) * 1000 << " ms)\n\n";

    {  // speed of the M6b solver vs the flat solver (same algorithm)
        Cfr<LeducState> old_solver(root);
        t0 = Clock::now();
        old_solver.train(300);
        const double old_ms = since(t0) * 1000.0 / 300;
        FlatCfr flat(game);
        t0 = Clock::now();
        flat.train(3000);
        const double flat_ms = since(t0) * 1000.0 / 3000;
        std::cout << "Per-iteration cost, vanilla CFR:  M6b solver " << old_ms << " ms,  flat solver "
                  << flat_ms << " ms  (" << std::setprecision(1) << old_ms / flat_ms
                  << "x faster)\n\n"
                  << std::setprecision(5);
    }

    std::cout << std::left << std::setw(14) << "algorithm" << std::right << std::setw(11)
              << "iterations" << std::setw(15) << "train time (s)" << std::setw(17)
              << "exploitability" << "\n";

    const std::vector<long long> full_cps =
        quick ? std::vector<long long>{100, 1000, 5000} : std::vector<long long>{100, 1000, 10000, 50000};
    const std::vector<long long> mc_cps =
        quick ? std::vector<long long>{10000, 100000, 500000}
              : std::vector<long long>{10000, 100000, 1000000, 5000000};

    FlatCfr vanilla(game, CfrVariant::Vanilla);
    run("vanilla CFR", vanilla, root, full_cps);
    FlatCfr plus(game, CfrVariant::Plus);
    run("CFR+", plus, root, full_cps);
    FlatMccfr mc(game, 12345);
    run("ES-MCCFR", mc, root, mc_cps);

    std::cout << "\nexploitability = chips/game a perfect opponent could win (0 = Nash equilibrium)\n"
                 "CFR+ 'iteration' = one traversal updating one player; ES-MCCFR 'iteration' = one\n"
                 "sampled traversal per player.\n";
    return 0;
}