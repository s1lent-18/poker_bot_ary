#include "eval/hand_evaluator.h"

#include <initializer_list>
#include <stdexcept>

namespace poker {
namespace {

constexpr uint32_t WHEEL_MASK = (1u << 12) | 0xFu;  // A,2,3,4,5

// Packs category + up to 5 tiebreak ranks (most significant first).
HandScore make(HandCategory c, std::initializer_list<int> ranks) {
    uint32_t v = 0;
    int k = 0;
    for (int r : ranks) {
        v = (v << 4) | static_cast<uint32_t>(r);
        ++k;
    }
    for (; k < 5; ++k) v <<= 4;
    return (static_cast<uint32_t>(c) << 20) | v;
}

// Highest card of a straight in a 13-bit rank mask, or -1. Wheel returns 3 (five-high).
int straight_high(uint32_t mask) {
    for (int high = 12; high >= 4; --high) {
        uint32_t need = 0x1Fu << (high - 4);
        if ((mask & need) == need) return high;
    }
    if ((mask & WHEEL_MASK) == WHEEL_MASK) return 3;
    return -1;
}

// Writes the k highest set bits of mask (descending) into out. Returns how many.
int take_top(uint32_t mask, int k, int* out) {
    int n = 0;
    for (int r = 12; r >= 0 && n < k; --r)
        if (mask & (1u << r)) out[n++] = r;
    return n;
}

int top_bit(uint32_t mask) {
    for (int r = 12; r >= 0; --r)
        if (mask & (1u << r)) return r;
    return -1;
}

}  // namespace

HandScore evaluate(const Card* cards, int n) {
    if (n < 5 || n > 7) throw std::invalid_argument("evaluate: need 5..7 cards");

    int cnt[NUM_RANKS] = {};
    uint32_t suit_mask[NUM_SUITS] = {};
    int suit_cnt[NUM_SUITS] = {};
    uint32_t all = 0;

    for (int i = 0; i < n; ++i) {
        int r = cards[i].rank(), s = cards[i].suit();
        cnt[r]++;
        suit_mask[s] |= (1u << r);
        suit_cnt[s]++;
        all |= (1u << r);
    }

    // Flush suit (with <= 7 cards at most one suit can have 5+)
    int fs = -1;
    for (int s = 0; s < NUM_SUITS; ++s)
        if (suit_cnt[s] >= 5) fs = s;

    // 1. Straight flush
    if (fs >= 0) {
        int sh = straight_high(suit_mask[fs]);
        if (sh >= 0) return make(HandCategory::StraightFlush, {sh});
    }

    // Group ranks by multiplicity, each list in descending rank order
    int quads = -1;
    int trips[2], nt = 0;
    int pairs[3], np = 0;
    for (int r = 12; r >= 0; --r) {
        if (cnt[r] == 4) quads = r;
        else if (cnt[r] == 3 && nt < 2) trips[nt++] = r;
        else if (cnt[r] == 2 && np < 3) pairs[np++] = r;
    }

    // 2. Four of a kind
    if (quads >= 0) {
        int kicker = top_bit(all & ~(1u << quads));
        return make(HandCategory::FourOfAKind, {quads, kicker});
    }

    // 3. Full house (a second set of trips can serve as the pair)
    if (nt >= 1 && (nt >= 2 || np >= 1)) {
        int pair_rank = -1;
        if (nt >= 2) pair_rank = trips[1];
        if (np >= 1 && pairs[0] > pair_rank) pair_rank = pairs[0];
        return make(HandCategory::FullHouse, {trips[0], pair_rank});
    }

    // 4. Flush
    if (fs >= 0) {
        int t[5];
        take_top(suit_mask[fs], 5, t);
        return make(HandCategory::Flush, {t[0], t[1], t[2], t[3], t[4]});
    }

    // 5. Straight
    int sh = straight_high(all);
    if (sh >= 0) return make(HandCategory::Straight, {sh});

    // 6. Three of a kind
    if (nt >= 1) {
        int k[2];
        take_top(all & ~(1u << trips[0]), 2, k);
        return make(HandCategory::ThreeOfAKind, {trips[0], k[0], k[1]});
    }

    // 7. Two pair (with 7 cards there can be three pairs; the third supplies a kicker)
    if (np >= 2) {
        uint32_t used = (1u << pairs[0]) | (1u << pairs[1]);
        int kicker = top_bit(all & ~used);
        return make(HandCategory::TwoPair, {pairs[0], pairs[1], kicker});
    }

    // 8. One pair
    if (np == 1) {
        int k[3];
        take_top(all & ~(1u << pairs[0]), 3, k);
        return make(HandCategory::Pair, {pairs[0], k[0], k[1], k[2]});
    }

    // 9. High card
    int t[5];
    take_top(all, 5, t);
    return make(HandCategory::HighCard, {t[0], t[1], t[2], t[3], t[4]});
}

std::string category_name(HandCategory c) {
    switch (c) {
        case HandCategory::HighCard: return "High Card";
        case HandCategory::Pair: return "Pair";
        case HandCategory::TwoPair: return "Two Pair";
        case HandCategory::ThreeOfAKind: return "Three of a Kind";
        case HandCategory::Straight: return "Straight";
        case HandCategory::Flush: return "Flush";
        case HandCategory::FullHouse: return "Full House";
        case HandCategory::FourOfAKind: return "Four of a Kind";
        case HandCategory::StraightFlush: return "Straight Flush";
    }
    return "?";
}

}  // namespace poker