#pragma once
#include "EntityHandle.hpp"
#include "SparseSet.hpp"
#include <vector>
#include <memory>
#include <tuple>
#include <algorithm>
#include <utility>
#include <cassert>

namespace vg {

class Registry {
public:
    explicit Registry(std::size_t reserve_entities = 1024) {
        m_versions.reserve(reserve_entities);
        m_alive_flags.reserve(reserve_entities);
        m_free_indices.reserve(reserve_entities / 2);
    }

    [[nodiscard]] Entity create() {
        u32 idx = 0;
        if (!m_free_indices.empty()) {
            idx = m_free_indices.back();
            m_free_indices.pop_back();
            m_alive_flags[idx] = 1;
        } else {
            idx = static_cast<u32>(m_versions.size());
            m_versions.push_back(0);
            m_alive_flags.push_back(1);
        }
        ++m_alive_count;
        return Entity(idx, m_versions[idx]);
    }

    void destroy(Entity e) noexcept {
        if (!is_alive(e)) return;
        const u32 idx = e.index();
        for (auto& pool : m_pools) {
            if (pool) pool->remove_if_present(idx);
        }
        m_alive_flags[idx] = 0;
        m_versions[idx] = (m_versions[idx] + 1) & Entity::VERSION_MASK;
        m_free_indices.push_back(idx);
        --m_alive_count;
    }

    void clear() noexcept {
        for (auto& pool : m_pools) {
            if (pool) pool->clear();
        }
        m_versions.clear();
        m_alive_flags.clear();
        m_free_indices.clear();
        m_alive_count = 0;
    }

    [[nodiscard]] bool is_alive(Entity e) const noexcept {
        if (e.is_null()) return false;
        const u32 idx = e.index();
        return idx < m_versions.size() &&
               m_alive_flags[idx] != 0 &&
               m_versions[idx] == e.version();
    }

    template <typename T, typename... Args>
    T& emplace(Entity e, Args&&... args) {
        assert(is_alive(e));
        return pool<T>().emplace(e.index(), std::forward<Args>(args)...);
    }

    template <typename T>
    [[nodiscard]] bool has(Entity e) const noexcept {
        if (!is_alive(e)) return false;
        const std::size_t tid = type_id<T>();
        if (tid >= m_pools.size() || !m_pools[tid]) return false;
        return m_pools[tid]->contains_index(e.index());
    }

    template <typename T>
    [[nodiscard]] T& get(Entity e) noexcept {
        assert(has<T>(e));
        return pool<T>().get(e.index());
    }

    template <typename T>
    [[nodiscard]] const T& get(Entity e) const noexcept {
        assert(has<T>(e));
        return static_cast<const SparseSet<T>*>(m_pools[type_id<T>()].get())->get(e.index());
    }

    template <typename T>
    [[nodiscard]] T* try_get(Entity e) noexcept {
        if (!has<T>(e)) return nullptr;
        return &pool<T>().get(e.index());
    }

    template <typename T>
    SparseSet<T>& pool() {
        const std::size_t tid = type_id<T>();
        if (tid >= m_pools.size()) {
            m_pools.resize(tid + 1);
        }
        if (!m_pools[tid]) {
            m_pools[tid] = std::make_unique<SparseSet<T>>();
        }
        return *static_cast<SparseSet<T>*>(m_pools[tid].get());
    }

    // Multi-component view driving iteration from the smallest dense pool (FR-1.3)
    template <typename... Cs, typename Func>
    void view(Func&& fn) {
        static_assert(sizeof...(Cs) > 0, "view requires at least one component type");
        auto pools = std::make_tuple(&pool<Cs>()...);
        const ISparseSet* pool_ptrs[sizeof...(Cs)] = { static_cast<const ISparseSet*>(std::get<SparseSet<Cs>*>(pools))... };

        const ISparseSet* driver = pool_ptrs[0];
        for (std::size_t i = 1; i < sizeof...(Cs); ++i) {
            if (pool_ptrs[i]->size() < driver->size()) {
                driver = pool_ptrs[i];
            }
        }

        const std::size_t count = driver->size();
        const u32* indices = driver->raw_entity_indices();

        // Snapshot indices into temporary stack/local vector only if > 512, otherwise stack buffer
        u32 stack_buf[512];
        std::vector<u32> heap_buf;
        const u32* iter_indices = indices;
        if (count <= 512) {
            for (std::size_t i = 0; i < count; ++i) stack_buf[i] = indices[i];
            iter_indices = stack_buf;
        } else {
            heap_buf.assign(indices, indices + count);
            iter_indices = heap_buf.data();
        }

        for (std::size_t i = 0; i < count; ++i) {
            const u32 idx = iter_indices[i];
            if (idx >= m_alive_flags.size() || !m_alive_flags[idx]) continue;
            if ((std::get<SparseSet<Cs>*>(pools)->contains_index(idx) && ...)) {
                Entity ent(idx, m_versions[idx]);
                fn(ent, std::get<SparseSet<Cs>*>(pools)->get(idx)...);
            }
        }
    }

    template <typename Func>
    void for_each_alive(Func&& fn) const {
        for (u32 idx = 0; idx < static_cast<u32>(m_versions.size()); ++idx) {
            if (m_alive_flags[idx]) {
                fn(Entity(idx, m_versions[idx]));
            }
        }
    }

    [[nodiscard]] std::size_t alive_count() const noexcept { return m_alive_count; }

private:
    static std::size_t next_type_id() noexcept {
        static std::size_t counter = 0;
        return counter++;
    }

    template <typename T>
    static std::size_t type_id() noexcept {
        static const std::size_t id = next_type_id();
        return id;
    }

    std::vector<u16> m_versions;
    std::vector<u8> m_alive_flags;
    std::vector<u32> m_free_indices;
    std::vector<std::unique_ptr<ISparseSet>> m_pools;
    std::size_t m_alive_count = 0;
};

} // namespace vg
