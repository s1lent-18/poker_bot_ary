#pragma once
#include <array>
#include <string>
#include <vector>

#include "core/card.h"
#include "core/deck.h"
#include "engine/action.h"
#include "engine/game_config.h"

namespace poker {

enum class Street : uint8_t { Preflop = 0, Flop, Turn, River, Showdown };

// Everything one player is legitimately allowed to know.
struct Observation {
    int player = 0;
    std::array<Card, 2> hole;
    std::vector<Card> board;  // only the revealed cards
    Street street = Street::Preflop;
    int pot = 0;
    int my_stack = 0, opp_stack = 0;
    int my_bet = 0, opp_bet = 0;  // chips in during the current street
    int to_call = 0;
    int button = 0;
    std::string history;  // f=fold k=check c=call r<N>=raise-to N, '/' between streets
};

class GameState {
public:
    // Rigged deal, for tests: cards = p0 hole(2), p1 hole(2), board(5).
    GameState(const GameConfig& cfg, int button, const std::array<Card, 9>& cards);
    GameState(const GameConfig& cfg, int button, const std::array<Card, 9>& cards,
              const std::array<int, 2>& starting_stacks);
    // Normal deal from a shuffled deck.
    GameState(const GameConfig& cfg, int button, Deck& shuffled_deck);
    GameState(const GameConfig& cfg, int button, Deck& shuffled_deck,
              const std::array<int, 2>& starting_stacks);

    bool is_terminal() const { return terminal_; }
    int current_player() const { return to_act_; }

    // Order: Fold (if facing a bet), Check/Call, then raises ascending (last = all-in).
    std::vector<Action> legal_actions() const;
    void apply(const Action& a);  // throws std::invalid_argument if illegal

    // Net chips won/lost per player. Zero-sum. Valid only when terminal.
    std::array<int, 2> payoffs() const;

    Observation observe(int player) const;

    // Accessors (engine/test use; agents should only use observe())
    Street street() const { return street_; }
    int pot() const { return total_[0] + total_[1]; }
    int stack(int p) const { return stack_[p]; }
    int bet(int p) const { return bet_[p]; }
    int button() const { return button_; }
    const std::string& history() const { return history_; }
    const std::array<Card, 2>& hole(int p) const { return hole_[p]; }
    int board_visible() const;

private:
    void post_blind(int p, int amount);
    void end_street();

    GameConfig cfg_;
    int button_;
    std::array<std::array<Card, 2>, 2> hole_;
    std::array<Card, 5> board_;

    std::array<int, 2> stack_{};  // chips behind
    std::array<int, 2> bet_{};    // committed this street
    std::array<int, 2> total_{};  // committed this hand
    std::array<bool, 2> need_action_{};
    Street street_ = Street::Preflop;
    int to_act_ = 0;
    int last_raise_ = 0;  // size of the last raise this street (sets the min-raise)
    bool terminal_ = false;
    int folded_ = -1;
    std::string history_;
};

}  // namespace poker