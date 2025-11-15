#pragma once
#include <cstdint>
#include <array>

class Xoshiro256StarStar {
public:
    using result_type = uint64_t;

    // State: must be non-zero
    std::array<uint64_t, 4> s;

    explicit Xoshiro256StarStar(uint64_t seed = 1) {
        seed_state(seed);
    }

    // Seeding with splitmix64
    static uint64_t splitmix64(uint64_t& x) {
        uint64_t z = (x += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31);
    }

    void seed_state(uint64_t seed) {
        uint64_t x = seed;
        for (int i = 0; i < 4; ++i)
            s[i] = splitmix64(x);
    }

    // The PRNG's main step function
    uint64_t operator()() {
        return next();
    }

    // Generate next number
    uint64_t next() {
        const uint64_t result = rotl(s[1] * 5, 7) * 9;

        const uint64_t t = s[1] << 17;

        s[2] ^= s[0];
        s[3] ^= s[1];
        s[1] ^= s[2];
        s[0] ^= s[3];

        s[2] ^= t;

        s[3] = rotl(s[3], 45);

        return result;
    }

    // For compatibility with <random>
    static constexpr uint64_t min() { return 0; }
    static constexpr uint64_t max() { return UINT64_MAX; }

private:
    static inline uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }
};

uint64_t RandomRange(uint64_t r, Xoshiro256StarStar& rng) {
    // returns 0..r-1 uniformly
    uint64_t x;
    uint64_t limit = UINT64_MAX - (UINT64_MAX % r);
    do {
        x = rng.next();
    } while (x >= limit);
    return x % r;
}
