#pragma once
#include "Types.hpp"
#include <limits>

namespace vg {

// Deterministic Permuted Congruential Generator (NFR-1.3)
class PCG32 {
public:
    explicit constexpr PCG32(u64 seed = 0x853c49e6748fea9bULL, u64 seq = 0xda3e39cb94b95bdbULL) noexcept {
        reseed(seed, seq);
    }

    constexpr void reseed(u64 seed, u64 seq = 0xda3e39cb94b95bdbULL) noexcept {
        m_state = 0U;
        m_inc = (seq << 1u) | 1u;
        next_u32();
        m_state += seed;
        next_u32();
    }

    constexpr u32 next_u32() noexcept {
        const u64 oldstate = m_state;
        m_state = oldstate * 6364136223846793005ULL + m_inc;
        const u32 xorshifted = static_cast<u32>(((oldstate >> 18u) ^ oldstate) >> 27u);
        const u32 rot = static_cast<u32>(oldstate >> 59u);
        return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
    }

    constexpr f32 next_f32() noexcept {
        return static_cast<f32>(next_u32() >> 8) * (1.0f / 16777216.0f);
    }

    constexpr f32 range_f32(f32 min_v, f32 max_v) noexcept {
        return min_v + (max_v - min_v) * next_f32();
    }

    constexpr i32 range_i32(i32 min_v, i32 max_v) noexcept {
        if (max_v <= min_v) return min_v;
        const u32 span = static_cast<u32>(max_v - min_v + 1);
        return min_v + static_cast<i32>(next_u32() % span);
    }

private:
    u64 m_state = 0;
    u64 m_inc = 0;
};

} // namespace vg
