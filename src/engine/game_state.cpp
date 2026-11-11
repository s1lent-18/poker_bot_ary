#include "engine/game_state.h"

#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>

#include "eval/hand_evaluator.h"

namespace poker {

static std::array<Card, 9> deal9(Deck& d) {
    std::array<Card, 9> c;
    for (auto& x : c) x = d.deal();
    return c;
}

GameState::GameState(const GameConfig& cfg, int button, Deck& d)
    : GameState(cfg, button, deal9(d), {cfg.stack, cfg.stack}) {}

GameState::GameState(const GameConfig& cfg, int button, Deck& d, const std::array<int, 2>& starting_stacks)
    : GameState(cfg, button, deal9(d), starting_stacks) {}

GameState::GameState(const GameConfig& cfg, int button, const std::array<Card, 9>& cards)
    : GameState(cfg, button, cards, {cfg.stack, cfg.stack}) {}

GameState::GameState(const GameConfig& cfg, int button, const std::array<Card, 9>& cards,
                     const std::array<int, 2>& starting_stacks)
    : cfg_(cfg), button_(button) {
    if (button < 0 || button > 1) throw std::invalid_argument("button must be 0 or 1");
    if (cfg.small_blind <= 0 || cfg.small_blind > cfg.big_blind)
        throw std::invalid_argument("bad GameConfig");

    std::set<int> seen;
    for (const Card& c : cards) seen.insert(c.id);
    if (seen.size() != 9) throw std::invalid_argument("duplicate cards in deal");

    hole_[0] = {cards[0], cards[1]};
    hole_[1] = {cards[2], cards[3]};
    for (int i = 0; i < 5; ++i) board_[i] = cards[4 + i];

    stack_ = starting_stacks;
    post_blind(button_, cfg.small_blind);      // heads-up: button = small blind
    post_blind(1 - button_, cfg.big_blind);

    street_ = Street::Preflop;
    last_raise_ = cfg.big_blind;
    need_action_ = {true, true};  // the big blind gets an option even if SB limps
    to_act_ = button_;            // SB acts first preflop

    // If a player went all-in posting blinds and bet is covered/matched:
    if (stack_[button_] == 0) {
        street_ = Street::Showdown;
        terminal_ = true;
    } else if (stack_[1 - button_] == 0 && bet_[button_] >= bet_[1 - button_]) {
        street_ = Street::Showdown;
        terminal_ = true;
    }
}

void GameState::post_blind(int p, int amount) {
    int a = std::min(amount, stack_[p]);
    stack_[p] -= a;
    bet_[p] += a;
    total_[p] += a;
}

int GameState::board_visible() const {
    switch (street_) {
        case Street::Preflop: return 0;
        case Street::Flop: return 3;
        case Street::Turn: return 4;
        default: return 5;  // River or Showdown
    }
}

std::vector<Action> GameState::legal_actions() const {
    std::vector<Action> out;
    if (terminal_) return out;

    const int p = to_act_, o = 1 - p;
    const int to_call = bet_[o] - bet_[p];

    if (to_call > 0) {
        out.push_back(Action::fold());
        out.push_back(Action::call());
    } else {
        out.push_back(Action::check());
    }

    // No raising an opponent who is already all-in, or if we can't cover the call.
    if (stack_[o] == 0) return out;
    const int max_to = bet_[p] + stack_[p];
    if (max_to <= bet_[o]) return out;

    const int min_to = bet_[o] + last_raise_;
    const int pot_after_call = pot() + to_call;

    int prev = -1;
    for (int i = 0; i < cfg_.num_raise_fractions; ++i) {
        int t = bet_[o] + static_cast<int>(std::lround(cfg_.raise_fractions[i] * pot_after_call));
        t = std::max(t, min_to);
        if (t >= max_to) continue;  // all-in covers this
        if (t == prev) continue;    // dedupe (fractions assumed ascending)
        out.push_back(Action::raise(t));
        prev = t;
    }
    out.push_back(Action::raise(max_to));  // all-in (may be a short raise)
    return out;
}

void GameState::apply(const Action& a) {
    if (terminal_) throw std::logic_error("apply() on a terminal state");
    {
        auto legal = legal_actions();
        if (std::find(legal.begin(), legal.end(), a) == legal.end())
            throw std::invalid_argument("illegal action: " + a.to_string());
    }

    const int p = to_act_, o = 1 - p;

    switch (a.type) {
        case ActionType::Fold:
            history_ += 'f';
            folded_ = p;
            terminal_ = true;
            return;
        case ActionType::Check:
            history_ += 'k';
            break;
        case ActionType::Call: {
            int amt = std::min(bet_[o] - bet_[p], stack_[p]);
            stack_[p] -= amt;
            bet_[p] += amt;
            total_[p] += amt;
            history_ += 'c';
            break;
        }
        case ActionType::Raise: {
            int add = a.amount - bet_[p];
            stack_[p] -= add;
            bet_[p] = a.amount;
            total_[p] += add;
            last_raise_ = std::max(last_raise_, a.amount - bet_[o]);
            need_action_[o] = true;  // opponent must respond
            history_ += 'r' + std::to_string(a.amount);
            break;
        }
    }

    need_action_[p] = false;
    if (!need_action_[0] && !need_action_[1])
        end_street();
    else
        to_act_ = o;
}

void GameState::end_street() {
    bet_ = {0, 0};

    // Someone is all-in and bets are matched: no more betting, run out the board.
    if (stack_[0] == 0 || stack_[1] == 0) {
        street_ = Street::Showdown;
        terminal_ = true;
        return;
    }

    street_ = static_cast<Street>(static_cast<int>(street_) + 1);
    if (street_ == Street::Showdown) {
        terminal_ = true;
        return;
    }

    need_action_ = {true, true};
    last_raise_ = cfg_.big_blind;
    to_act_ = 1 - button_;  // big blind acts first postflop
    history_ += '/';
}

std::array<int, 2> GameState::payoffs() const {
    if (!terminal_) throw std::logic_error("payoffs() before the hand is over");

    if (folded_ >= 0) {
        int w = 1 - folded_;
        std::array<int, 2> r{};
        int amt = std::min(total_[folded_], total_[w]);
        r[w] = amt;
        r[folded_] = -amt;
        return r;
    }

    // Showdown: winner nets contested chips up to the smaller total commitment.
    HandScore score[2];
    for (int p = 0; p < 2; ++p) {
        Card seven[7] = {hole_[p][0], hole_[p][1], board_[0], board_[1],
                         board_[2],   board_[3],   board_[4]};
        score[p] = evaluate(seven, 7);
    }
    int contested = std::min(total_[0], total_[1]);
    if (score[0] > score[1]) return {contested, -contested};
    if (score[1] > score[0]) return {-contested, contested};
    return {0, 0};
}

Observation GameState::observe(int player) const {
    if (player < 0 || player > 1) throw std::invalid_argument("bad player");
    Observation ob;
    ob.player = player;
    ob.hole = hole_[player];
    ob.board.assign(board_.begin(), board_.begin() + board_visible());
    ob.street = street_;
    ob.pot = pot();
    ob.my_stack = stack_[player];
    ob.opp_stack = stack_[1 - player];
    ob.my_bet = bet_[player];
    ob.opp_bet = bet_[1 - player];
    ob.to_call = std::max(0, bet_[1 - player] - bet_[player]);
    ob.button = button_;
    ob.history = history_;
    return ob;
}

}  // namespace poker