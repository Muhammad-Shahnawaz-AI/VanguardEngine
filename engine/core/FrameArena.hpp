#pragma once
#include "Types.hpp"
#include <cstdlib>
#include <cstddef>
#include <new>
#include <algorithm>
#include <type_traits>

namespace vg {

// Linear Bump Allocator with O(1) per-tick reset (FR-1.4, NFR-1.2)
class FrameArena {
public:
    explicit FrameArena(std::size_t capacity_bytes = 4 * 1024 * 1024)
        : m_capacity(capacity_bytes) {
        m_buffer = static_cast<u8*>(std::malloc(m_capacity));
        if (!m_buffer) throw std::bad_alloc{};
    }

    ~FrameArena() {
        std::free(m_buffer);
    }

    FrameArena(const FrameArena&) = delete;
    FrameArena& operator=(const FrameArena&) = delete;

    void* allocate_raw(std::size_t bytes, std::size_t alignment = alignof(std::max_align_t)) noexcept {
        const std::size_t current_addr = reinterpret_cast<std::size_t>(m_buffer + m_offset);
        const std::size_t misalignment = current_addr & (alignment - 1);
        const std::size_t padding = (misalignment == 0) ? 0 : (alignment - misalignment);

        if (m_offset + padding + bytes > m_capacity) {
            return nullptr;
        }
        m_offset += padding;
        void* ptr = m_buffer + m_offset;
        m_offset += bytes;
        m_peak = std::max(m_peak, m_offset);
        return ptr;
    }

    template <typename T>
    [[nodiscard]] T* allocate_array(std::size_t count) noexcept {
        static_assert(std::is_trivially_destructible_v<T>, "FrameArena types must be trivially destructible");
        if (count == 0) return nullptr;
        void* mem = allocate_raw(sizeof(T) * count, alignof(T));
        return static_cast<T*>(mem);
    }

    void reset() noexcept {
        m_offset = 0;
    }

    [[nodiscard]] std::size_t used_bytes() const noexcept { return m_offset; }
    [[nodiscard]] std::size_t peak_bytes() const noexcept { return m_peak; }
    [[nodiscard]] std::size_t capacity_bytes() const noexcept { return m_capacity; }

private:
    u8* m_buffer = nullptr;
    std::size_t m_capacity = 0;
    std::size_t m_offset = 0;
    std::size_t m_peak = 0;
};

} // namespace vg
