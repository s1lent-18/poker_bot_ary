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
#include "agents/duel.h"
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
    std::string strategy2;  // optional: the --bot cfr uses this file instead of --strategy
    uint64_t seed = std::random_device{}();
    int sim = 0;
    int watch = 0;
    bool step = false;
    bool dup = false;
    std::vector<std::string> arena;
    std::vector<std::string> info;
};

struct Loaded {
    std::shared_ptr<const HoldemStrategy> strat;
    std::string label;  // shown in output
};

// Game settings (replaced by a loaded strategy's settings), and the strategies in use.
static GameConfig g_cfg;
static Loaded g_hero, g_bot;

static void usage() {
    std::cout
        << "Usage: poker_cli [--bot rule|random|equity|cfr] [--hero rule|random|equity|cfr]\n"
           "                 [--strategy FILE] [--strategy2 FILE] [--seed N]\n"
           "                 [--sim HANDS [--dup]] [--watch HANDS [--step]]\n"
           "                 [--arena FILE FILE ...] [--info FILE ...]\n"
           "  (no flags)                    play against the bot\n"
           "  --sim N --hero X --bot Y      X vs Y for N hands, print results\n"
           "  --dup                         duplicate format: each deal twice, cards and seats swapped\n"
           "  --watch N                     X vs Y, printed hand by hand (--step pauses after each)\n"
           "  --strategy FILE               cfr strategy for --hero (and --bot if no --strategy2)\n"
           "  --strategy2 FILE              cfr strategy for --bot\n"
           "  --arena FILE FILE ...         every pair of strategy files plays (duplicate format)\n"
           "  --info FILE ...               show what is in strategy files\n";
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
        else if (k == "--strategy2") a.strategy2 = next();
        else if (k == "--seed") a.seed = std::stoull(next());
        else if (k == "--sim") a.sim = std::stoi(next());
        else if (k == "--watch") a.watch = std::stoi(next());
        else if (k == "--step") a.step = true;
        else if (k == "--dup") a.dup = true;
        else if (k == "--arena" || k == "--info") {
            std::vector<std::string>& dst = (k == "--arena") ? a.arena : a.info;
            while (i + 1 < argc && argv[i + 1][0] != '-') dst.push_back(argv[++i]);
            if (dst.empty()) { std::cerr << k << " needs at least one file\n"; std::exit(1); }
        }
        else if (k == "--help") { usage(); std::exit(0); }
        else { std::cerr << "unknown argument: " << k << "\n"; usage(); std::exit(1); }
    }
    return a;
}

static Loaded load_strategy(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "cannot open strategy file '" << path
                  << "' (train one with ./build/holdem_train)\n";
        std::exit(1);
    }
    try {
        Loaded l;
        l.strat = std::make_shared<const HoldemStrategy>(HoldemStrategy::load(f));
        const size_t slash = path.find_last_of('/');
        l.label = "cfr:" + (slash == std::string::npos ? path : path.substr(slash + 1));
        return l;
    } catch (const std::exception& e) {
        std::cerr << "bad strategy file '" << path << "': " << e.what() << "\n";
        std::exit(1);
    }
}

static bool same_game(const GameConfig& a, const GameConfig& b) {
    if (a.stack != b.stack || a.small_blind != b.small_blind || a.big_blind != b.big_blind ||
        a.num_raise_fractions != b.num_raise_fractions)
        return false;
    for (int i = 0; i < a.num_raise_fractions; ++i)
        if (a.raise_fractions[i] != b.raise_fractions[i]) return false;
    return true;
}

static std::string iterations_text(const HoldemStrategy& s) {
    return s.iterations > 0 ? std::to_string(s.iterations) + " iterations" : "iterations unknown";
}

static std::unique_ptr<Agent> make_agent(const std::string& name, uint64_t seed, const Loaded& l) {
    if (name == "random") return std::make_unique<RandomAgent>(seed);
    if (name == "rule") return std::make_unique<RuleAgent>(seed);
    if (name == "equity") return std::make_unique<EquityAgent>(seed);
    if (name == "cfr") return std::make_unique<CfrAgent>(l.strat, seed, l.label);
    std::cerr << "unknown bot: " << name << "\n";
    std::exit(1);
}

static void report_cfr(Agent* a) {
    if (auto* c = dynamic_cast<CfrAgent*>(a))
        std::cout << "  " << c->name() << " fell back to check/call on " << c->fallbacks() << " of "
                  << c->decisions() << " decisions\n";
}

static void run_info(const std::vector<std::string>& paths) {
    for (const std::string& p : paths) {
        const Loaded l = load_strategy(p);
        const HoldemStrategy& s = *l.strat;
        std::cout << p << "\n"
                  << "  trained: " << iterations_text(s) << "\n"
                  << "  info sets: " << s.table.size() << "\n"
                  << "  stack: " << s.cfg.stack / s.cfg.big_blind << "bb, blinds "
                  << s.cfg.small_blind << "/" << s.cfg.big_blind << "\n"
                  << "  raise menu:";
        for (int i = 0; i < s.cfg.num_raise_fractions; ++i)
            std::cout << " " << s.cfg.raise_fractions[i] << "x pot,";
        std::cout << " all-in\n"
                  << "  card buckets per street: " << s.bucketer.num_buckets() << "\n";
    }
}

static void run_sim(const Args& args) {
    auto hero = make_agent(args.hero, args.seed + 1, g_hero);
    auto villain = make_agent(args.bot, args.seed + 2, g_bot);
    const DuelResult r = duel(*hero, *villain, g_cfg, args.sim, args.seed, args.dup);

    std::cout << hero->name() << " vs " << villain->name() << " over " << r.hands << " hands"
              << (args.dup ? " (duplicate: each deal played twice, cards and seats swapped)" : "")
              << "\n"
              << "  " << hero->name() << ": " << r.bb_per_hand << " bb/hand  (95% CI +/- "
              << r.ci95 << ")\n"
              << "  = " << 100 * r.bb_per_hand << " bb/100\n";
    report_cfr(hero.get());
    report_cfr(villain.get());
}

// Every pair of strategy files plays in duplicate format.
static void run_arena(const Args& args) {
    if (args.arena.size() < 2) {
        std::cerr << "--arena needs at least two strategy files\n";
        std::exit(1);
    }
    std::vector<Loaded> loaded;
    for (const std::string& p : args.arena) loaded.push_back(load_strategy(p));
    for (size_t i = 1; i < loaded.size(); ++i)
        if (!same_game(loaded[0].strat->cfg, loaded[i].strat->cfg)) {
            std::cerr << "these strategies were trained for different game settings (stack or "
                         "raise menu), so they cannot play each other\n";
            std::exit(1);
        }
    const GameConfig cfg = loaded[0].strat->cfg;
    const int hands = args.sim > 0 ? args.sim : 4000;

    const size_t n = loaded.size();
    std::vector<std::unique_ptr<CfrAgent>> bots;
    std::vector<std::string> desc;
    for (size_t i = 0; i < n; ++i) {
        bots.push_back(std::make_unique<CfrAgent>(loaded[i].strat, args.seed + 10 + i,
                                                  loaded[i].label));
        desc.push_back(loaded[i].label + "  [" + iterations_text(*loaded[i].strat) + "]");
    }

    std::cout << "Arena: " << n << " versions, duplicate format, ~" << hands
              << " hands per pairing, seed " << args.seed << "\n\n"
              << std::showpos;
    std::vector<double> total(n, 0.0);
    for (size_t i = 0; i < n; ++i)
        for (size_t j = i + 1; j < n; ++j) {
            const DuelResult r = duel(*bots[i], *bots[j], cfg, hands, args.seed, true);
            total[i] += r.bb_per_hand;
            total[j] -= r.bb_per_hand;
            std::cout << desc[i] << "\n  vs " << desc[j] << "\n  -> " << r.bb_per_hand
                      << " bb/hand for the first one  (95% CI +/- " << std::noshowpos << r.ci95
                      << std::showpos << ", " << std::noshowpos << r.hands << " hands"
                      << std::showpos << ")\n\n";
        }

    std::cout << "Average bb/hand against the other versions:\n";
    for (size_t i = 0; i < n; ++i)
        std::cout << "  " << desc[i] << ":  " << total[i] / static_cast<double>(n - 1) << "\n";
    std::cout << std::noshowpos << "\nFallbacks (info sets training never reached):\n";
    for (size_t i = 0; i < n; ++i) report_cfr(bots[i].get());
}

// Bot vs bot, printed hand by hand. Seat 0 = A (--hero), seat 1 = B (--bot).
static void run_watch(const Args& args) {
    static const char* STREETS[] = {"Preflop", "Flop", "Turn", "River", "Showdown"};
    GameConfig cfg = g_cfg;
    Rng rng(args.seed);
    Deck deck;
    auto a0 = make_agent(args.hero, args.seed + 1, g_hero);
    auto a1 = make_agent(args.bot, args.seed + 2, g_bot);
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
    auto bot = make_agent(args.bot, args.seed + 1, g_bot);

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

    if (!args.info.empty()) {
        run_info(args.info);
        return 0;
    }
    if (!args.arena.empty()) {
        run_arena(args);
        return 0;
    }

    const bool hero_cfr = args.hero == "cfr", bot_cfr = args.bot == "cfr";
    if (hero_cfr) g_hero = load_strategy(args.strategy);
    if (bot_cfr) {
        if (!args.strategy2.empty()) g_bot = load_strategy(args.strategy2);
        else g_bot = hero_cfr ? g_hero : load_strategy(args.strategy);
    }
    if (hero_cfr && bot_cfr && !same_game(g_hero.strat->cfg, g_bot.strat->cfg)) {
        std::cerr << "these strategies were trained for different game settings, so they cannot "
                     "play each other\n";
        return 1;
    }
    if (hero_cfr) g_cfg = g_hero.strat->cfg;
    else if (bot_cfr) g_cfg = g_bot.strat->cfg;
    for (const Loaded* l : {&g_hero, &g_bot})
        if (l->strat)
            std::cout << "Loaded " << l->label << ": " << l->strat->table.size() << " info sets, "
                      << iterations_text(*l->strat) << "\n";

    if (args.watch > 0) run_watch(args);
    else if (args.sim > 0) run_sim(args);
    else play_interactive(args);
    return 0;
}