#pragma once
#include <cstdint>
#include <string>

namespace poker {

enum class ActionType : uint8_t { Fold, Check, Call, Raise };

// For Raise, `amount` is "raise to": the player's total chips in this street
// after the action. All-in is just a Raise to (current bet + remaining stack).
// For other types amount is always 0.
struct Action {
    ActionType type = ActionType::Fold;
    int amount = 0;

    static Action fold() { return {ActionType::Fold, 0}; }
    static Action check() { return {ActionType::Check, 0}; }
    static Action call() { return {ActionType::Call, 0}; }
    static Action raise(int to) { return {ActionType::Raise, to}; }

    bool operator==(const Action& o) const { return type == o.type && amount == o.amount; }
    bool operator!=(const Action& o) const { return !(*this == o); }

    std::string to_string() const {
        switch (type) {
            case ActionType::Fold: return "fold";
            case ActionType::Check: return "check";
            case ActionType::Call: return "call";
            case ActionType::Raise: return "raise to " + std::to_string(amount);
        }
        return "?";
    }
};

}  // namespace poker