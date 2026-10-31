#pragma once
#include <array>

namespace poker {

struct GameConfig {
    int stack = 10000;
    int small_blind = 50;
    int big_blind = 100;

    // Raise sizes as fractions of (pot after calling). All-in is always offered too.
    // Fixed-size array so GameConfig stays cheap to copy (matters for CFR later).
    std::array<double, 4> raise_fractions{0.5, 1.0, 0.0, 0.0};
    int num_raise_fractions = 2;
};

}  // namespace poker