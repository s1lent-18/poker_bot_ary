#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include "agents/cfr_agent.h"
#include "agents/equity_agent.h"
#include "agents/human_cli_agent.h"
#include "agents/match.h"
#include "agents/random_agent.h"
#include "agents/rule_agent.h"
#include "core/deck.h"
#include "eval/hand_evaluator.h"

using namespace poker;

struct Args {
    std::string bot = "rule";
    std::string hero = "rule";
    std::string strategy = "holdem.cfr";
    uint64_t seed = std::random_device{}();
    int sim = 0;
    int watch = 0;
    bool step = false;
};

// Game settings; replaced by the strategy file's settings when a cfr bot is used.
static GameConfig g_cfg;
static std::shared_ptr<const HoldemStrategy> g_strategy;

static void usage() {
    std::cout << "Usage: poker_cli [--bot rule|random|equity|cfr] [--hero rule|random|equity|cfr]\n"
                 "                 [--strategy FILE] [--seed N] [--sim HANDS] [--watch HANDS] [--step]\n"
                 "  (no flags)                      play against the bot\n"
                 "  --sim N --hero X --bot Y        X vs Y for N hands, print results\n"
                 "  --watch N --hero X --bot Y      X vs Y, print every hand action by action\n"
                 "  --step                          with --watch: pause after each hand\n"
                 "  --strategy FILE                 strategy file for cfr (default holdem.cfr)\n";
}

static Args parse(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string k = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) { std::cerr << "missing value for " << k << "\n"; std::exit(1); }
            return argv[++i];
        };
        if (k == "--bot") a.bot = next();
        else if (k == "--hero") a.hero = next();
        else if (k == "--strategy") a.strategy = next();
        else if (k == "--seed") a.seed = std::stoull(next());
        else if (k == "--sim") a.sim = std::stoi(next());
        else if (k == "--watch") a.watch = std::stoi(next());
        else if (k == "--step") a.step = true;
        else if (k == "--help") { usage(); std::exit(0); }
        else { std::cerr << "unknown argument: " << k << "\n"; usage(); std::exit(1); }
    }
    return a;
}

static std::unique_ptr<Agent> make_agent(const std::string& name, uint64_t seed) {
    if (name == "random") return std::make_unique<RandomAgent>(seed);
    if (name == "rule") return std::make_unique<RuleAgent>(seed);
    if (name == "equity") return std::make_unique<EquityAgent>(seed);
    if (name == "cfr") return std::make_unique<CfrAgent>(g_strategy, seed);
    std::cerr << "unknown bot: " << name << "\n";
    std::exit(1);
}

static void report_cfr(Agent* a) {
    if (auto* c = dynamic_cast<CfrAgent*>(a))
        std::cout << "  " << c->name() << " fell back to check/call on " << c->fallbacks() << " of "
                  << c->decisions() << " decisions\n";
}

static void run_sim(const Args& args) {
    GameConfig cfg = g_cfg;
    Rng rng(args.seed);
    Deck deck;
    auto hero = make_agent(args.hero, args.seed + 1);
    auto villain = make_agent(args.bot, args.seed + 2);

    double sum = 0, sumsq = 0;
    for (int h = 0; h < args.sim; ++h) {
        deck.shuffle(rng);
        GameState s(cfg, h % 2, deck);  // alternate the button
        const double bb = double(play_hand(s, {hero.get(), villain.get()})[0]) / cfg.big_blind;
        sum += bb;
        sumsq += bb * bb;
    }
    const double n = args.sim, mean = sum / n;
    const double var = std::max(0.0, (sumsq - n * mean * mean) / std::max(1.0, n - 1));
    const double se = std::sqrt(var / n);
    std::cout << hero->name() << " vs " << villain->name() << " over " << args.sim << " hands\n"
              << "  " << hero->name() << ": " << mean << " bb/hand  (95% CI +/- " << 1.96 * se
              << ")\n"
              << "  = " << 100 * mean << " bb/100\n";
    report_cfr(hero.get());
    report_cfr(villain.get());
}

// Bot vs bot, printed hand by hand. Seat 0 = A (--hero), seat 1 = B (--bot).
static void run_watch(const Args& args) {
    static const char* STREETS[] = {"Preflop", "Flop", "Turn", "River", "Showdown"};
    GameConfig cfg = g_cfg;
    Rng rng(args.seed);
    Deck deck;
    auto a0 = make_agent(args.hero, args.seed + 1);
    auto a1 = make_agent(args.bot, args.seed + 2);
    Agent* agents[2] = {a0.get(), a1.get()};
    const std::string label[2] = {"A(" + a0->name() + ")", "B(" + a1->name() + ")"};

    std::cout << "Watching " << label[0] << " vs " << label[1] << "  (seed " << args.seed
              << ", stacks " << cfg.stack << ", blinds " << cfg.small_blind << "/" << cfg.big_blind
              << ")\n";

    long long total = 0;
    int played = 0;
    for (int hand = 0; hand < args.watch; ++hand) {
        deck.shuffle(rng);
        GameState s(cfg, hand % 2, deck);
        const int button = hand % 2;
        std::cout << "\n=========== Hand #" << hand + 1 << " ===========\n"
                  << "Button/SB: " << label[button] << "   BB: " << label[1 - button] << "\n"
                  << label[0] << ": " << pretty_cards(s.hole(0)) << "\n"
                  << label[1] << ": " << pretty_cards(s.hole(1)) << "\n";

        int shown = 0;
        while (!s.is_terminal()) {
            const int p = s.current_player();
            const Observation ob = s.observe(p);
            const Action act = agents[p]->act(ob, s.legal_actions());
            const bool allin =
                (act.type == ActionType::Raise && act.amount == ob.my_bet + ob.my_stack) ||
                (act.type == ActionType::Call && ob.to_call >= ob.my_stack);
            std::cout << "  [" << STREETS[static_cast<int>(ob.street)] << ", pot " << ob.pot << "] "
                      << label[p] << ": " << act.to_string() << (allin ? " (ALL-IN)" : "") << "\n";
            s.apply(act);
            const int n = s.board_visible();
            if (n > shown) {
                shown = n;
                std::cout << "  ** Board: " << pretty_cards(s.observe(0).board) << "\n";
            }
        }

        const bool folded = !s.history().empty() && s.history().back() == 'f';
        if (!folded) {
            const auto board = s.observe(0).board;
            std::cout << "--- Showdown ---\nBoard: " << pretty_cards(board) << "\n";
            for (int p = 0; p < 2; ++p) {
                std::vector<Card> seven(s.hole(p).begin(), s.hole(p).end());
                seven.insert(seven.end(), board.begin(), board.end());
                std::cout << label[p] << ": " << pretty_cards(s.hole(p)) << "  ("
                          << category_name(category_of(evaluate(seven))) << ")\n";
            }
        }

        const auto pay = s.payoffs();
        if (pay[0] > 0) std::cout << "=> " << label[0] << " wins " << pay[0] << " chips\n";
        else if (pay[0] < 0) std::cout << "=> " << label[1] << " wins " << -pay[0] << " chips\n";
        else std::cout << "=> split pot\n";

        total += pay[0];
        ++played;
        std::cout << "Running total for " << label[0] << ": " << total << " chips ("
                  << double(total) / played / cfg.big_blind << " bb/hand over " << played
                  << " hands)\n";

        if (args.step && hand + 1 < args.watch) {
            std::cout << "Enter = next hand, q = stop: " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line) || line == "q") break;
        }
    }

    std::cout << "\nFinal: " << label[0] << " " << (total >= 0 ? "+" : "") << total << " chips over "
              << played << " hands (" << double(total) / std::max(1, played) / cfg.big_blind
              << " bb/hand)\n";
    report_cfr(a0.get());
    report_cfr(a1.get());
}

static void play_interactive(const Args& args) {
    GameConfig cfg = g_cfg;
    Rng rng(args.seed);
    Deck deck;
    HumanCliAgent human;
    auto bot = make_agent(args.bot, args.seed + 1);

    std::cout << "Heads-up No-Limit Hold'em vs " << bot->name() << "  (seed " << args.seed
              << ")\nStacks reset to " << cfg.stack << " every hand. Blinds " << cfg.small_blind
              << "/" << cfg.big_blind << ".\n";

    long long total = 0;
    try {
        for (int hand = 0;; ++hand) {
            deck.shuffle(rng);
            GameState s(cfg, hand % 2, deck);  // seat 0 = you, seat 1 = bot
            std::cout << "\n=========== Hand #" << hand + 1 << " ===========\n";

            int shown = 0;
            auto observer = [&](int p, const Action& a, const GameState& st) {
                if (p == 1) {
                    std::cout << "  >> Bot: " << a.to_string()
                              << (a.type != ActionType::Fold && st.stack(1) == 0 ? " (ALL-IN)" : "")
                              << "\n";
                }
                const int n = st.board_visible();
                if (n > shown) {
                    shown = n;
                    std::cout << "  ** Board: " << pretty_cards(st.observe(0).board) << "\n";
                }
            };

            const auto pay = play_hand(s, {&human, bot.get()}, observer);
            const bool folded = !s.history().empty() && s.history().back() == 'f';

            std::cout << "\n--- Result ---\n";
            if (!folded) {
                const auto board = s.observe(0).board;
                std::cout << "Board: " << pretty_cards(board) << "\n";
                for (int p = 0; p < 2; ++p) {
                    std::vector<Card> seven(s.hole(p).begin(), s.hole(p).end());
                    seven.insert(seven.end(), board.begin(), board.end());
                    std::cout << (p == 0 ? "You: " : "Bot: ") << pretty_cards(s.hole(p)) << "  ("
                              << category_name(category_of(evaluate(seven))) << ")\n";
                }
            }
            if (pay[0] > 0) std::cout << "You win " << pay[0] << " chips.\n";
            else if (pay[0] < 0) std::cout << "You lose " << -pay[0] << " chips.\n";
            else std::cout << "Split pot.\n";

            total += pay[0];
            std::cout << "Running total: " << total << " chips ("
                      << double(total) / (hand + 1) / cfg.big_blind << " bb/hand)\n"
                      << "Enter = next hand, q = quit: " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line) || line == "q") break;
        }
    } catch (const QuitGame&) {
    }
    std::cout << "\nThanks for playing! Final: " << total << " chips.\n";
}

int main(int argc, char** argv) {
    Args args = parse(argc, argv);

    if (args.hero == "cfr" || args.bot == "cfr") {
        std::ifstream f(args.strategy);
        if (!f) {
            std::cerr << "cannot open strategy file '" << args.strategy
                      << "' (train one with ./build/holdem_train)\n";
            return 1;
        }
        try {
            g_strategy = std::make_shared<const HoldemStrategy>(HoldemStrategy::load(f));
        } catch (const std::exception& e) {
            std::cerr << "bad strategy file: " << e.what() << "\n";
            return 1;
        }
        g_cfg = g_strategy->cfg;
        std::cout << "Loaded " << g_strategy->table.size() << " info sets from " << args.strategy
                  << "\n";
    }

    if (args.watch > 0) run_watch(args);
    else if (args.sim > 0) run_sim(args);
    else play_interactive(args);
    return 0;
}