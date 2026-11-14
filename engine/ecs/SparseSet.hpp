#pragma once
#include "EntityHandle.hpp"
#include <vector>
#include <array>
#include <memory>
#include <utility>
#include <cassert>

namespace vg {

class ISparseSet {
public:
    virtual ~ISparseSet() = default;
    virtual void remove_if_present(u32 entity_index) noexcept = 0;
    virtual void clear() noexcept = 0;
    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual bool contains_index(u32 entity_index) const noexcept = 0;
    [[nodiscard]] virtual const u32* raw_entity_indices() const noexcept = 0;
};

// Paged Sparse-Set for O(1) lookup/insertion/removal and contiguous dense iteration (FR-1.2)
template <typename T>
class SparseSet final : public ISparseSet {
public:
    static constexpr u32 PAGE_SIZE = 4096;
    static constexpr u32 INVALID_DENSE = 0xFFFFFFFFu;

    explicit SparseSet(std::size_t initial_reserve = 256) {
        m_dense_indices.reserve(initial_reserve);
        m_dense_data.reserve(initial_reserve);
    }

    template <typename... Args>
    T& emplace(u32 entity_index, Args&&... args) {
        if (contains_index(entity_index)) {
            const u32 dense_idx = sparse_slot(entity_index);
            m_dense_data[dense_idx] = T(std::forward<Args>(args)...);
            return m_dense_data[dense_idx];
        }
        ensure_page(entity_index);
        const u32 dense_idx = static_cast<u32>(m_dense_indices.size());
        sparse_slot(entity_index) = dense_idx;
        m_dense_indices.push_back(entity_index);
        m_dense_data.emplace_back(std::forward<Args>(args)...);
        return m_dense_data.back();
    }

    void remove_if_present(u32 entity_index) noexcept override {
        if (!contains_index(entity_index)) return;
        const u32 dense_idx = sparse_slot(entity_index);
        const u32 last_dense = static_cast<u32>(m_dense_indices.size() - 1);
        const u32 last_entity_idx = m_dense_indices[last_dense];

        m_dense_indices[dense_idx] = last_entity_idx;
        m_dense_data[dense_idx] = std::move(m_dense_data[last_dense]);
        sparse_slot(last_entity_idx) = dense_idx;

        m_dense_indices.pop_back();
        m_dense_data.pop_back();
        sparse_slot(entity_index) = INVALID_DENSE;
    }

    void clear() noexcept override {
        for (u32 idx : m_dense_indices) {
            sparse_slot(idx) = INVALID_DENSE;
        }
        m_dense_indices.clear();
        m_dense_data.clear();
    }

    [[nodiscard]] bool contains_index(u32 entity_index) const noexcept override {
        const u32 page = entity_index / PAGE_SIZE;
        if (page >= m_pages.size() || !m_pages[page]) return false;
        return (*m_pages[page])[entity_index % PAGE_SIZE] != INVALID_DENSE;
    }

    [[nodiscard]] T& get(u32 entity_index) noexcept {
        assert(contains_index(entity_index));
        return m_dense_data[sparse_slot(entity_index)];
    }

    [[nodiscard]] const T& get(u32 entity_index) const noexcept {
        assert(contains_index(entity_index));
        return m_dense_data[sparse_slot(entity_index)];
    }

    [[nodiscard]] std::size_t size() const noexcept override { return m_dense_indices.size(); }
    [[nodiscard]] const u32* raw_entity_indices() const noexcept override { return m_dense_indices.data(); }
    [[nodiscard]] std::vector<T>& dense_data() noexcept { return m_dense_data; }

private:
    using Page = std::array<u32, PAGE_SIZE>;

    void ensure_page(u32 entity_index) {
        const u32 page = entity_index / PAGE_SIZE;
        if (page >= m_pages.size()) {
            m_pages.resize(page + 1);
        }
        if (!m_pages[page]) {
            m_pages[page] = std::make_unique<Page>();
            m_pages[page]->fill(INVALID_DENSE);
        }
    }

    u32& sparse_slot(u32 entity_index) noexcept {
        return (*m_pages[entity_index / PAGE_SIZE])[entity_index % PAGE_SIZE];
    }

    const u32& sparse_slot(u32 entity_index) const noexcept {
        return (*m_pages[entity_index / PAGE_SIZE])[entity_index % PAGE_SIZE];
    }

    std::vector<std::unique_ptr<Page>> m_pages;
    std::vector<u32> m_dense_indices;
    std::vector<T> m_dense_data;
};

} // namespace vg
