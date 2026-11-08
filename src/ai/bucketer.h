#pragma once
#include <array>
#include <cstdint>
#include <iosfwd>
#include <vector>

#include "core/card.h"

namespace poker {

// 0..168: one id per distinct starting hand (13 pairs + 78 suited + 78 offsuit).
int preflop_class(Card a, Card b);

// Groups flop/turn/river hands into equity buckets. Strength = equity vs a random hand,
// estimated by Monte Carlo with a seed derived from the cards, so the same hand on the
// same board always lands in the same bucket (training and play agree).
class Bucketer {
public:
    explicit Bucketer(int buckets = 12, int samples = 300);

    int num_buckets() const { return buckets_; }
    int samples() const { return samples_; }
    bool calibrated() const { return calibrated_; }
    const std::array<std::vector<double>, 3>& edges() const { return edges_; }

    // Sets the bucket boundaries to equal-population quantiles of random hands.
    void calibrate(int hands_per_street, uint64_t seed);

    double strength(const std::array<Card, 2>& hole, const std::vector<Card>& board) const;
    int bucket(const std::array<Card, 2>& hole, const std::vector<Card>& board) const;

    void save(std::ostream& os) const;
    static Bucketer load(std::istream& is);

private:
    int buckets_;
    int samples_;
    bool calibrated_ = false;
    std::array<std::vector<double>, 3> edges_;  // flop, turn, river; buckets_-1 edges each
};

}  // namespace poker