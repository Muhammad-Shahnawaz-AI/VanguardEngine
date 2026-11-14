#pragma once
#include "Shapes.hpp"
#include "CollisionSolver.hpp"
#include "../core/FrameArena.hpp"
#include <span>
#include <cmath>
#include <algorithm>

namespace vg {

// Zero-allocation Spatial Hash Grid backed by FrameArena (FR-2.1, FR-2.3)
class SpatialHash {
public:
    struct QueryStats {
        u32 broadphase_candidate_pairs = 0;
        u32 narrowphase_hits = 0;
    };

    explicit SpatialHash(f32 cell_size = 2.0f, u32 max_proxies = 2048)
        : m_cell_size(cell_size), m_inv_cell_size(1.0f / cell_size), m_max_proxies(max_proxies) {}

    void begin_frame(FrameArena& arena) noexcept {
        m_arena = &arena;
        m_proxies = arena.allocate_array<ColliderProxy>(m_max_proxies);
        m_proxy_count = 0;

        m_bucket_heads = arena.allocate_array<i32>(BUCKET_COUNT);
        for (u32 i = 0; i < BUCKET_COUNT; ++i) m_bucket_heads[i] = -1;

        m_nodes = arena.allocate_array<CellNode>(MAX_NODES);
        m_node_count = 0;
    }

    void insert(const ColliderProxy& proxy) noexcept {
        if (m_proxy_count >= m_max_proxies || !m_proxies) return;
        const u32 p_idx = m_proxy_count++;
        m_proxies[p_idx] = proxy;

        const i32 min_x = static_cast<i32>(std::floor(proxy.broadphase_aabb.min.x * m_inv_cell_size));
        const i32 max_x = static_cast<i32>(std::floor(proxy.broadphase_aabb.max.x * m_inv_cell_size));
        const i32 min_y = static_cast<i32>(std::floor(proxy.broadphase_aabb.min.y * m_inv_cell_size));
        const i32 max_y = static_cast<i32>(std::floor(proxy.broadphase_aabb.max.y * m_inv_cell_size));

        for (i32 cy = min_y; cy <= max_y; ++cy) {
            for (i32 cx = min_x; cx <= max_x; ++cx) {
                if (m_node_count >= MAX_NODES) return;
                const u32 h = hash_cell(cx, cy);
                const i32 n_idx = static_cast<i32>(m_node_count++);
                m_nodes[n_idx] = { cx, cy, p_idx, m_bucket_heads[h] };
                m_bucket_heads[h] = n_idx;
            }
        }
    }

    [[nodiscard]] std::span<ContactManifold> resolve_all(QueryStats& out_stats) noexcept {
        out_stats = {};
        if (!m_arena || m_proxy_count < 2) return {};

        constexpr u32 MAX_MANIFOLDS = 1024;
        auto* manifolds = m_arena->allocate_array<ContactManifold>(MAX_MANIFOLDS);
        u32 manifold_count = 0;

        // Deduplicate candidate pairs using a small FrameArena hash table
        constexpr u32 PAIR_TABLE_SIZE = 4096;
        auto* seen_pairs = m_arena->allocate_array<u64>(PAIR_TABLE_SIZE);
        for (u32 i = 0; i < PAIR_TABLE_SIZE; ++i) seen_pairs[i] = 0;

        for (u32 b = 0; b < BUCKET_COUNT; ++b) {
            for (i32 n1 = m_bucket_heads[b]; n1 != -1; n1 = m_nodes[n1].next) {
                for (i32 n2 = m_nodes[n1].next; n2 != -1; n2 = m_nodes[n2].next) {
                    if (m_nodes[n1].cx != m_nodes[n2].cx || m_nodes[n1].cy != m_nodes[n2].cy) continue;

                    const u32 i1 = m_nodes[n1].proxy_idx;
                    const u32 i2 = m_nodes[n2].proxy_idx;
                    if (i1 == i2) continue;

                    const ColliderProxy& a = m_proxies[i1];
                    const ColliderProxy& b_prox = m_proxies[i2];
                    if (a.entity == b_prox.entity) continue;

                    // Bitmask Collision Layer Filter (FR-2.3)
                    const bool match = ((a.category_bits & b_prox.mask_bits) != 0) ||
                                       ((b_prox.category_bits & a.mask_bits) != 0);
                    if (!match) continue;

                    const u32 lo = std::min(i1, i2);
                    const u32 hi = std::max(i1, i2);
                    const u64 pair_key = (static_cast<u64>(lo + 1) << 32) | static_cast<u64>(hi + 1);
                    u32 slot = static_cast<u32>((pair_key * 11400714819323198485ULL) >> 52);
                    bool already_tested = false;
                    while (seen_pairs[slot] != 0) {
                        if (seen_pairs[slot] == pair_key) {
                            already_tested = true;
                            break;
                        }
                        slot = (slot + 1) & (PAIR_TABLE_SIZE - 1);
                    }
                    if (already_tested) continue;
                    seen_pairs[slot] = pair_key;

                    ++out_stats.broadphase_candidate_pairs;
                    if (!a.broadphase_aabb.overlaps(b_prox.broadphase_aabb)) continue;

                    if (auto contact = CollisionSolver::test_proxies(a, b_prox)) {
                        ++out_stats.narrowphase_hits;
                        if (manifold_count < MAX_MANIFOLDS) {
                            contact->a = a.entity;
                            contact->b = b_prox.entity;
                            manifolds[manifold_count++] = *contact;
                        }
                    }
                }
            }
        }
        return { manifolds, manifold_count };
    }

    template <typename Func>
    void for_each_occupied_cell(Func&& fn) const {
        for (u32 i = 0; i < m_node_count; ++i) {
            const glm::vec2 min_p{ m_nodes[i].cx * m_cell_size, m_nodes[i].cy * m_cell_size };
            fn(AABB{ min_p, min_p + glm::vec2{m_cell_size} });
        }
    }

private:
    struct CellNode {
        i32 cx = 0;
        i32 cy = 0;
        u32 proxy_idx = 0;
        i32 next = -1;
    };

    static constexpr u32 BUCKET_COUNT = 1024;
    static constexpr u32 MAX_NODES    = 8192;

    [[nodiscard]] static u32 hash_cell(i32 cx, i32 cy) noexcept {
        const u32 h = static_cast<u32>(cx * 73856093) ^ static_cast<u32>(cy * 19349663);
        return h & (BUCKET_COUNT - 1);
    }

    f32 m_cell_size = 2.0f;
    f32 m_inv_cell_size = 0.5f;
    u32 m_max_proxies = 2048;

    FrameArena* m_arena = nullptr;
    ColliderProxy* m_proxies = nullptr;
    u32 m_proxy_count = 0;
    i32* m_bucket_heads = nullptr;
    CellNode* m_nodes = nullptr;
    u32 m_node_count = 0;
};

} // namespace vg
