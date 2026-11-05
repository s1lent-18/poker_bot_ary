#pragma once
#include <algorithm>
#include <cmath>

#include "core/card.h"

namespace poker {

// Chen formula, normalised to roughly [0, 1]. AA = 1.0, 72o = 0.0.
inline double preflop_strength(Card a, Card b) {
    int r1 = a.rank(), r2 = b.rank();
    if (r1 < r2) std::swap(r1, r2);
    auto pts = [](int r) -> double {
        if (r == 12) return 10.0;  // A
        if (r == 11) return 8.0;   // K
        if (r == 10) return 7.0;   // Q
        if (r == 9) return 6.0;    // J
        return (r + 2) / 2.0;      // T=5, 9=4.5, ... 2=1
    };
    double s = pts(r1);
    if (r1 == r2) {
        s = std::max(5.0, s * 2);
    } else {
        if (a.suit() == b.suit()) s += 2;
        int gap = r1 - r2 - 1;
        if (gap == 1) s -= 1;
        else if (gap == 2) s -= 2;
        else if (gap == 3) s -= 4;
        else if (gap >= 4) s -= 5;
        if (gap <= 1 && r1 < 10) s += 1;  // connected-ish and both below a queen
    }
    s = std::ceil(s);
    return std::clamp((s + 1.0) / 21.0, 0.0, 1.0);
}

}  // namespace poker