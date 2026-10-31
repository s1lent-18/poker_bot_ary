#include <cstdlib>
#include <iostream>
#include <memory>
#include <random>
#include <string>

#include "agents/human_cli_agent.h"
#include "agents/match.h"
#include "agents/random_agent.h"
#include "agents/rule_agent.h"
#include "core/deck.h"
#include "eval/hand_evaluator.h"

using namespace poker;

struct Args {
    std::string bot = "rule";
    uint64_t seed = std::random_device{}();
    int sim = 0;
};

static void usage() {
    std::cout << "Usage: poker_cli [--bot rule|random] [--seed N] [--sim HANDS]\n"
                 "  (no flags)        play against the bot\n"
                 "  --sim N --bot X   RuleBot vs bot X for N hands, print results\n";
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
        else if (k == "--seed") a.seed = std::stoull(next());
        else if (k == "--sim") a.sim = std::stoi(next());
        else if (k == "--help") { usage(); std::exit(0); }
        else { std::cerr << "unknown argument: " << k << "\n"; usage(); std::exit(1); }
    }
    return a;
}

static std::unique_ptr<Agent> make_agent(const std::string& name, uint64_t seed) {
    if (name == "random") return std::make_unique<RandomAgent>(seed);
    if (name == "rule") return std::make_unique<RuleAgent>(seed);
    std::cerr << "unknown bot: " << name << "\n";
    std::exit(1);
}

static void run_sim(const Args& args) {
    GameConfig cfg;
    Rng rng(args.seed);
    Deck deck;
    RuleAgent hero(args.seed + 1);
    auto villain = make_agent(args.bot, args.seed + 2);

    long long total = 0;
    for (int h = 0; h < args.sim; ++h) {
        deck.shuffle(rng);
        GameState s(cfg, h % 2, deck);  // alternate the button
        total += play_hand(s, {&hero, villain.get()})[0];
    }
    std::cout << "RuleBot vs " << villain->name() << " over " << args.sim << " hands\n"
              << "  RuleBot net: " << total << " chips\n"
              << "  = " << double(total) / args.sim / cfg.big_blind << " big blinds / hand\n";
}

static void play_interactive(const Args& args) {
    GameConfig cfg;
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
    if (args.sim > 0) run_sim(args);
    else play_interactive(args);
    return 0;
}