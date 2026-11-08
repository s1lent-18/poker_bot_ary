#include "ai/bucketer.h"

#include <algorithm>
#include <iomanip>
#include <istream>
#include <ostream>
#include <stdexcept>
#include <string>

#include "ai/equity.h"
#include "core/deck.h"
#include "core/rng.h"

namespace poker {

int preflop_class(Card a, Card b) {
    const int hi = std::max(a.rank(), b.rank());
    const int lo = std::min(a.rank(), b.rank());
    return a.suit() == b.suit() ? lo * NUM_RANKS + hi : hi * NUM_RANKS + lo;
}

Bucketer::Bucketer(int buckets, int samples) : buckets_(buckets), samples_(samples) {
    if (buckets < 2 || samples < 1) throw std::invalid_argument("Bucketer: bad parameters");
}

double Bucketer::strength(const std::array<Card, 2>& hole, const std::vector<Card>& board) const {
    std::array<int, 2> h = {hole[0].id, hole[1].id};
    std::sort(h.begin(), h.end());
    std::vector<int> b;
    for (const Card& c : board) b.push_back(c.id);
    std::sort(b.begin(), b.end());

    uint64_t seed = 1469598103934665603ULL;  // FNV-1a style mix of the sorted cards
    auto mix = [&](uint64_t v) {
        seed ^= v + 1;
        seed *= 1099511628211ULL;
    };
    mix(static_cast<uint64_t>(h[0]));
    mix(static_cast<uint64_t>(h[1]));
    mix(100);
    for (int x : b) mix(static_cast<uint64_t>(x));

    Rng rng(seed);
    return equity_vs_random(hole, board, samples_, rng).equity;
}

int Bucketer::bucket(const std::array<Card, 2>& hole, const std::vector<Card>& board) const {
    if (!calibrated_) throw std::logic_error("Bucketer: calibrate() or load() first");
    if (board.size() < 3 || board.size() > 5)
        throw std::invalid_argument("Bucketer: board must have 3, 4 or 5 cards");
    const std::vector<double>& e = edges_[board.size() - 3];
    const double s = strength(hole, board);
    return static_cast<int>(std::upper_bound(e.begin(), e.end(), s) - e.begin());
}

void Bucketer::calibrate(int n, uint64_t seed) {
    if (n < buckets_) throw std::invalid_argument("Bucketer::calibrate: too few hands");
    Deck deck;
    for (int street = 0; street < 3; ++street) {
        Rng rng(seed + 1000003ULL * static_cast<uint64_t>(street + 1));
        std::vector<double> vals;
        vals.reserve(n);
        for (int i = 0; i < n; ++i) {
            deck.shuffle(rng);
            std::array<Card, 2> hole = {deck.deal(), deck.deal()};
            std::vector<Card> board;
            for (int k = 0; k < 3 + street; ++k) board.push_back(deck.deal());
            vals.push_back(strength(hole, board));
        }
        std::sort(vals.begin(), vals.end());
        std::vector<double>& e = edges_[street];
        e.clear();
        for (int k = 1; k < buckets_; ++k)
            e.push_back(vals[std::min(vals.size() - 1, static_cast<size_t>(k) * vals.size() /
                                                           static_cast<size_t>(buckets_))]);
    }
    calibrated_ = true;
}

void Bucketer::save(std::ostream& os) const {
    if (!calibrated_) throw std::logic_error("Bucketer::save: not calibrated");
    os << "bucketer " << buckets_ << ' ' << samples_ << '\n';
    os << std::setprecision(12);
    for (const auto& e : edges_) {
        for (size_t i = 0; i < e.size(); ++i) os << (i ? " " : "") << e[i];
        os << '\n';
    }
}

Bucketer Bucketer::load(std::istream& is) {
    std::string tag;
    int b = 0, s = 0;
    if (!(is >> tag >> b >> s) || tag != "bucketer") throw std::runtime_error("bad bucketer header");
    Bucketer out(b, s);
    for (auto& e : out.edges_) {
        e.resize(static_cast<size_t>(b - 1));
        for (double& x : e)
            if (!(is >> x)) throw std::runtime_error("bad bucketer edges");
    }
    out.calibrated_ = true;
    return out;
}

}  // namespace poker