#pragma once
#include "Types.hpp"
#include <array>
#include <cstddef>

namespace vg {

template <typename T, std::size_t Capacity>
class RingBuffer {
    static_assert(Capacity > 0, "RingBuffer capacity must be > 0");
public:
    constexpr void push(const T& item) noexcept {
        m_data[m_head] = item;
        m_head = (m_head + 1) % Capacity;
        if (m_size < Capacity) {
            ++m_size;
        }
    }

    constexpr void clear() noexcept {
        m_head = 0;
        m_size = 0;
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return m_size; }
    [[nodiscard]] constexpr std::size_t capacity() const noexcept { return Capacity; }
    [[nodiscard]] constexpr bool empty() const noexcept { return m_size == 0; }

    // Index 0 = oldest element, size()-1 = newest element
    [[nodiscard]] constexpr T& operator[](std::size_t logical_idx) noexcept {
        const std::size_t start = (m_head + Capacity - m_size) % Capacity;
        return m_data[(start + logical_idx) % Capacity];
    }

    [[nodiscard]] constexpr const T& operator[](std::size_t logical_idx) const noexcept {
        const std::size_t start = (m_head + Capacity - m_size) % Capacity;
        return m_data[(start + logical_idx) % Capacity];
    }

private:
    std::array<T, Capacity> m_data{};
    std::size_t m_head = 0;
    std::size_t m_size = 0;
};

} // namespace vg
