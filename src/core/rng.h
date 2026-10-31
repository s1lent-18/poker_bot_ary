#pragma once
#include <cstdint>
#include <random>

namespace poker 
{

    // Seeded RNG wrapper. Same seed => same sequence, so bugs and training runs
    // are reproducible. We implement bounded sampling ourselves because
    // std::uniform_int_distribution gives different results on different compilers.
    class Rng 
    {
        public:
            explicit Rng(uint64_t seed = std::random_device{}()) : 
              seed_(seed), eng_(seed) {}
            uint64_t seed() const 
            { 
                return seed_; 
            }

            // Uniform integer in [0, n). Unbiased (rejection sampling). n must be > 0.
            uint64_t below(uint64_t n) 
            {
                const uint64_t threshold = (-n) % n;  // discard the biased low region
                for (;;) 
                {
                    uint64_t x = eng_();
                    if (x >= threshold) return x % n;
                }
            }

            // Uniform double in [0, 1)
            double uniform() { return (eng_() >> 11) * (1.0 / 9007199254740992.0); }

        private:
            uint64_t seed_;
            std::mt19937_64 eng_;
    };

}  // namespace poker