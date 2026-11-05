#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace poker::cfr {

// Leduc Hold'em. Cards 0..5 (J,J,Q,Q,K,K), rank = id / 2.
// Decision actions: 0 = fold, 1 = check/call, 2 = bet/raise.
class LeducState {
    enum class Stage : uint8_t { DealP0, DealP1, Bet1, DealBoard, Bet2, Done };

public:
    bool is_terminal() const { return stage_ == Stage::Done; }
    bool is_chance() const {
        return stage_ == Stage::DealP0 || stage_ == Stage::DealP1 || stage_ == Stage::DealBoard;
    }
    int current_player() const { return to_act_; }

    std::vector<int> actions() const {
        std::vector<int> out;
        if (is_chance()) {
            for (int c = 0; c < 6; ++c)
                if (c != hole_[0] && c != hole_[1] && c != board_) out.push_back(c);
            return out;
        }
        if (facing_bet()) out.push_back(0);
        out.push_back(1);
        if (raises_ < 2) out.push_back(2);
        return out;
    }

    std::vector<double> chance_probs() const {
        const size_t n = actions().size();
        return std::vector<double>(n, 1.0 / n);
    }

    LeducState child(int a) const {
        LeducState s = *this;
        switch (stage_) {
            case Stage::DealP0:
                s.hole_[0] = a;
                s.stage_ = Stage::DealP1;
                break;
            case Stage::DealP1:
                s.hole_[1] = a;
                s.start_round(Stage::Bet1);
                break;
            case Stage::DealBoard:
                s.board_ = a;
                s.hist_ += '/';
                s.start_round(Stage::Bet2);
                break;
            case Stage::Bet1:
            case Stage::Bet2:
                s.bet(a);
                break;
            case Stage::Done:
                throw std::logic_error("child() on a terminal state");
        }
        return s;
    }

    // Zero-sum payoff for `player` (chips won or lost, antes included).
    double utility(int player) const {
        if (folded_ >= 0) {
            const double won = in_[folded_];
            return player == folded_ ? -won : won;
        }
        const int r0 = hole_[0] / 2, r1 = hole_[1] / 2, rb = board_ / 2;
        int winner = -1;  // -1 = split
        if ((r0 == rb) != (r1 == rb)) winner = (r0 == rb) ? 0 : 1;  // exactly one pair
        else if (r0 != r1) winner = r0 > r1 ? 0 : 1;               // high card
        if (winner < 0) return 0.0;
        const double won = in_[1 - winner];
        return player == winner ? won : -won;
    }

    // What the acting player knows: who they are, own card, public card (round 2), betting.
    std::string info_key() const {
        std::string k;
        k += static_cast<char>('0' + to_act_);
        k += "JQK"[hole_[to_act_] / 2];
        if (stage_ == Stage::Bet2) {
            k += '|';
            k += "JQK"[board_ / 2];
        }
        k += ':';
        k += hist_;
        return k;
    }

private:
    bool facing_bet() const { return in_[1 - to_act_] > in_[to_act_]; }

    void start_round(Stage st) {
        stage_ = st;
        to_act_ = 0;
        raises_ = 0;
        acts_ = 0;
    }

    void bet(int a) {
        if (a < 0 || a > 2 || (a == 0 && !facing_bet()) || (a == 2 && raises_ >= 2))
            throw std::invalid_argument("illegal Leduc action");
        const int me = to_act_, opp = 1 - me;
        hist_ += "fcr"[a];
        if (a == 0) {
            folded_ = me;
            stage_ = Stage::Done;
            return;
        }
        bool round_over = false;
        if (a == 1) {
            if (in_[opp] > in_[me]) {  // call
                in_[me] = in_[opp];
                round_over = true;
            } else if (acts_ >= 1) {   // second check
                round_over = true;
            }
        } else {                       // bet / raise
            in_[me] = std::max(in_[0], in_[1]) + (stage_ == Stage::Bet1 ? 2 : 4);
            ++raises_;
        }
        ++acts_;
        if (round_over)
            stage_ = (stage_ == Stage::Bet1) ? Stage::DealBoard : Stage::Done;
        else
            to_act_ = opp;
    }

    std::array<int, 2> hole_{-1, -1};
    int board_ = -1;
    Stage stage_ = Stage::DealP0;
    std::array<int, 2> in_{1, 1};  // chips each player has put in (antes included)
    int to_act_ = 0;
    int raises_ = 0;   // raises so far this round
    int acts_ = 0;     // actions so far this round
    int folded_ = -1;
    std::string hist_;
};

}  // namespace poker::cfr