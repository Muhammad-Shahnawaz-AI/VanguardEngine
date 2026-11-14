#pragma once
#include "../core/Types.hpp"

namespace vg {

// 32-bit Generational Entity Handle: 20-bit index + 12-bit version (FR-1.1)
struct Entity {
    static constexpr u32 INDEX_BITS   = 20;
    static constexpr u32 VERSION_BITS = 12;
    static constexpr u32 INDEX_MASK   = (1u << INDEX_BITS) - 1u;
    static constexpr u32 VERSION_MASK = (1u << VERSION_BITS) - 1u;

    u32 raw = 0xFFFFFFFFu;

    constexpr Entity() noexcept = default;
    constexpr explicit Entity(u32 raw_val) noexcept : raw(raw_val) {}
    constexpr Entity(u32 idx, u32 ver) noexcept
        : raw(((ver & VERSION_MASK) << INDEX_BITS) | (idx & INDEX_MASK)) {}

    [[nodiscard]] constexpr u32 index() const noexcept { return raw & INDEX_MASK; }
    [[nodiscard]] constexpr u32 version() const noexcept { return (raw >> INDEX_BITS) & VERSION_MASK; }
    [[nodiscard]] constexpr bool is_null() const noexcept { return raw == 0xFFFFFFFFu; }

    constexpr bool operator==(const Entity&) const noexcept = default;
};

constexpr Entity NULL_ENTITY{};

} // namespace vg
