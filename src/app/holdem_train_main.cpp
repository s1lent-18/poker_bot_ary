#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

#include "ai/bucketer.h"
#include "ai/holdem_mccfr.h"
#include "ai/holdem_strategy.h"

using namespace poker;
using Clock = std::chrono::steady_clock;

struct Opts {
    long long iters = 100000;
    double seconds = 0;  // 0 = no time limit
    int buckets = 12;
    int sizes = 1;
    int stack_bb = 100;
    uint64_t seed = 1;
    std::string out = "holdem.cfr";
    double report_sec = 30;
    double save_sec = 600;
    double snapshot_sec = 0;  // 0 = no numbered snapshots
};

static void usage() {
    std::cout << "Usage: holdem_train [--iters N] [--seconds S] [--buckets B] [--sizes 1|2]\n"
                 "                    [--stack-bb N] [--seed N] [--out FILE] [--snapshot-sec S]\n"
                 "  trains until N iterations or S seconds (whichever comes first)\n"
                 "  --snapshot-sec S  also keep a numbered copy (FILE_<iterations>.cfr) every S seconds\n";
}

static Opts parse(int argc, char** argv) {
    Opts o;
    for (int i = 1; i < argc; ++i) {
        const std::string k = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "missing value for " << k << "\n";
                std::exit(1);
            }
            return argv[++i];
        };
        if (k == "--iters") o.iters = std::stoll(next());
        else if (k == "--seconds") o.seconds = std::stod(next());
        else if (k == "--buckets") o.buckets = std::stoi(next());
        else if (k == "--sizes") o.sizes = std::stoi(next());
        else if (k == "--stack-bb") o.stack_bb = std::stoi(next());
        else if (k == "--seed") o.seed = std::stoull(next());
        else if (k == "--out") o.out = next();
        else if (k == "--snapshot-sec") o.snapshot_sec = std::stod(next());
        else if (k == "--help") { usage(); std::exit(0); }
        else { std::cerr << "unknown argument: " << k << "\n"; usage(); std::exit(1); }
    }
    return o;
}

static double since(Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

static bool save_to(const HoldemMccfr& solver, const std::string& path) {
    std::ofstream f(path);
    if (!f) {
        std::cerr << "cannot write " << path << " (does the folder exist?)\n";
        return false;
    }
    solver.export_strategy().save(f);
    return f.good();
}

// "versions/run1.cfr" + 123400 iterations -> "versions/run1_000123400.cfr" (sorts chronologically)
static std::string snapshot_path(const std::string& out, long long iters) {
    std::string stem = out;
    const std::string ext = ".cfr";
    if (stem.size() > ext.size() && stem.compare(stem.size() - ext.size(), ext.size(), ext) == 0)
        stem.resize(stem.size() - ext.size());
    char buf[32];
    std::snprintf(buf, sizeof buf, "_%09lld", iters);
    return stem + buf + ".cfr";
}

int main(int argc, char** argv) {
    const Opts o = parse(argc, argv);
    const GameConfig cfg = holdem_config(o.sizes, o.stack_bb);

    std::cout << "Hold'em CFR: stack " << o.stack_bb << "bb, raise menu "
              << (o.sizes == 1 ? "pot + all-in" : "1/2 pot, pot + all-in") << ", " << o.buckets
              << " buckets/street, seed " << o.seed << "\n";

    Bucketer bucketer(o.buckets, 300);
    auto t0 = Clock::now();
    std::cout << "Calibrating card buckets..." << std::flush;
    bucketer.calibrate(20000, o.seed);
    std::cout << " done (" << std::fixed << std::setprecision(1) << since(t0) << " s)\n";

    HoldemMccfr solver(cfg, bucketer, o.seed);
    const auto start = Clock::now();
    auto last_report = start, last_save = start, last_snap = start;

    while (solver.iterations() < o.iters && (o.seconds <= 0 || since(start) < o.seconds)) {
        solver.train(20);
        if (since(last_report) >= o.report_sec) {
            last_report = Clock::now();
            std::cout << std::setw(10) << solver.iterations() << " iterations   "
                      << std::setprecision(1) << std::setw(8) << solver.iterations() / since(start)
                      << " it/s   " << std::setw(9) << solver.num_info_sets() << " info sets   "
                      << std::setw(7) << since(start) << " s\n"
                      << std::flush;
        }
        if (since(last_save) >= o.save_sec) {
            last_save = Clock::now();
            if (save_to(solver, o.out)) std::cout << "  (autosaved " << o.out << ")\n" << std::flush;
        }
        if (o.snapshot_sec > 0 && since(last_snap) >= o.snapshot_sec) {
            last_snap = Clock::now();
            const std::string p = snapshot_path(o.out, solver.iterations());
            if (save_to(solver, p)) std::cout << "  (snapshot " << p << ")\n" << std::flush;
        }
    }

    std::cout << "\nFinished: " << solver.iterations() << " iterations, " << solver.num_info_sets()
              << " info sets, " << since(start) << " s ("
              << solver.iterations() / std::max(1e-9, since(start)) << " it/s)\n";
    if (!save_to(solver, o.out)) return 1;
    std::cout << "Saved " << o.out << "\n";
    if (o.snapshot_sec > 0) {
        const std::string p = snapshot_path(o.out, solver.iterations());
        if (save_to(solver, p)) std::cout << "Saved final snapshot " << p << "\n";
    }
    std::cout << "Next: ./build/poker_cli --sim 5000 --hero cfr --bot rule --strategy " << o.out
              << "\n";
    return 0;
}