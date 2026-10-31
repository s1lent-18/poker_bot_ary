#pragma once
#include <iostream>
#include <string>

#include "agents/agent.h"

namespace poker {

struct QuitGame {};  // thrown when the human types q or input ends

inline std::string pretty_card(Card c) {
    static const char* SYM[] = {"♣", "♦", "♥", "♠"};
    std::string s = std::string(1, c.to_string()[0]) + SYM[c.suit()];
    const bool red = (c.suit() == 1 || c.suit() == 2);
    return red ? "\033[31m" + s + "\033[0m" : s;
}

template <class Container>
std::string pretty_cards(const Container& cs) {
    std::string out;
    for (const Card& c : cs) {
        if (!out.empty()) out += ' ';
        out += pretty_card(c);
    }
    return out.empty() ? "(none)" : out;
}

class HumanCliAgent : public Agent {
public:
    std::string name() const override { return "You"; }

    Action act(const Observation& ob, const std::vector<Action>& legal) override {
        static const char* STREETS[] = {"Preflop", "Flop", "Turn", "River", "Showdown"};
        std::cout << "\n-------------------------------------------\n"
                  << STREETS[static_cast<int>(ob.street)] << "  |  Pot " << ob.pot
                  << "  |  You " << ob.my_stack << " behind  |  Bot " << ob.opp_stack
                  << " behind\n"
                  << "Board: " << pretty_cards(ob.board) << "\n"
                  << "Your hand: " << pretty_cards(ob.hole)
                  << (ob.button == ob.player ? "   (you're the button / small blind)"
                                             : "   (you're the big blind)")
                  << "\n";
        if (ob.to_call > 0) std::cout << "Facing a bet: " << ob.to_call << " to call\n";

        const int all_in_to = ob.my_bet + ob.my_stack;
        for (size_t i = 0; i < legal.size(); ++i)
            std::cout << "  [" << i + 1 << "] " << label(legal[i], ob, all_in_to) << "\n";
        std::cout << "(number, or f=fold k/c=check/call a=all-in q=quit)\n";

        for (;;) {
            std::cout << "> " << std::flush;
            std::string line;
            if (!std::getline(std::cin, line)) throw QuitGame{};
            const size_t b = line.find_first_not_of(" \t\r\n");
            const size_t e = line.find_last_not_of(" \t\r\n");
            line = (b == std::string::npos) ? "" : line.substr(b, e - b + 1);

            if (line == "q" || line == "quit") throw QuitGame{};

            if (!line.empty() && line.size() <= 3 &&
                line.find_first_not_of("0123456789") == std::string::npos) {
                const size_t idx = std::stoul(line);
                if (idx >= 1 && idx <= legal.size()) return legal[idx - 1];
            } else if (line == "f") {
                for (const Action& a : legal)
                    if (a.type == ActionType::Fold) return a;
            } else if (line == "k" || line == "c") {
                for (const Action& a : legal)
                    if (a.type == ActionType::Check || a.type == ActionType::Call) return a;
            } else if (line == "a") {
                const Action& last = legal.back();
                if (last.type == ActionType::Raise && last.amount == all_in_to) return last;
            }
            std::cout << "  Not a valid choice, try again.\n";
        }
    }

private:
    static std::string label(const Action& a, const Observation& ob, int all_in_to) {
        switch (a.type) {
            case ActionType::Fold: return "fold";
            case ActionType::Check: return "check";
            case ActionType::Call: return "call " + std::to_string(std::min(ob.to_call, ob.my_stack));
            case ActionType::Raise:
                return a.amount == all_in_to ? "ALL-IN (to " + std::to_string(a.amount) + ")"
                                             : "raise to " + std::to_string(a.amount);
        }
        return "?";
    }
};

}  // namespace poker