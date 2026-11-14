#pragma once
#include "engine/core/Types.hpp"
#include "engine/core/FrameArena.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include <algorithm>

namespace vg::game {

class NavGridAStar {
public:
    static constexpr i32 GRID_W = 36;
    static constexpr i32 GRID_H = 22;

    NavGridAStar() {
        m_blocked.resize(GRID_W * GRID_H, false);
    }

    void set_obstacle_box(const glm::vec2& center, const glm::vec2& half_extents) {
        const i32 min_x = std::clamp(world_to_gx(center.x - half_extents.x), 0, GRID_W - 1);
        const i32 max_x = std::clamp(world_to_gx(center.x + half_extents.x), 0, GRID_W - 1);
        const i32 min_y = std::clamp(world_to_gy(center.y - half_extents.y), 0, GRID_H - 1);
        const i32 max_y = std::clamp(world_to_gy(center.y + half_extents.y), 0, GRID_H - 1);
        for (i32 y = min_y; y <= max_y; ++y) {
            for (i32 x = min_x; x <= max_x; ++x) {
                m_blocked[y * GRID_W + x] = true;
            }
        }
    }

    [[nodiscard]] glm::vec2 find_next_waypoint(FrameArena& arena,
                                               const glm::vec2& start_w,
                                               const glm::vec2& goal_w) const noexcept {
        const i32 sx = std::clamp(world_to_gx(start_w.x), 0, GRID_W - 1);
        const i32 sy = std::clamp(world_to_gy(start_w.y), 0, GRID_H - 1);
        const i32 gx = std::clamp(world_to_gx(goal_w.x), 0, GRID_W - 1);
        const i32 gy = std::clamp(world_to_gy(goal_w.y), 0, GRID_H - 1);

        if (sx == gx && sy == gy) return goal_w;

        constexpr std::size_t N = GRID_W * GRID_H;
        auto g_score = arena.allocate_array<f32>(N);
        auto f_score = arena.allocate_array<f32>(N);
        auto came_from = arena.allocate_array<i32>(N);
        auto state = arena.allocate_array<u8>(N);

        for (std::size_t i = 0; i < N; ++i) {
            g_score[i] = 1e9f;
            f_score[i] = 1e9f;
            came_from[i] = -1;
            state[i] = 0;
        }

        const i32 start_idx = sy * GRID_W + sx;
        const i32 goal_idx = gy * GRID_W + gx;
        g_score[start_idx] = 0.0f;
        f_score[start_idx] = heuristic(sx, sy, gx, gy);
        state[start_idx] = 1;

        constexpr i32 dx[8] = { 1, -1, 0, 0,  1,  1, -1, -1 };
        constexpr i32 dy[8] = { 0,  0, 1, -1, 1, -1,  1, -1 };
        constexpr f32 cost[8] = { 1.0f, 1.0f, 1.0f, 1.0f, 1.414f, 1.414f, 1.414f, 1.414f };

        for (std::size_t iter = 0; iter < N; ++iter) {
            i32 current = -1;
            f32 best_f = 1e9f;
            for (std::size_t i = 0; i < N; ++i) {
                if (state[i] == 1 && f_score[i] < best_f) {
                    best_f = f_score[i];
                    current = static_cast<i32>(i);
                }
            }
            if (current == -1) break;
            if (current == goal_idx) {
                i32 trace = goal_idx;
                while (came_from[trace] != -1 && came_from[trace] != start_idx) {
                    trace = came_from[trace];
                }
                return grid_to_world(trace % GRID_W, trace / GRID_W);
            }

            state[current] = 2;
            const i32 cx = current % GRID_W;
            const i32 cy = current / GRID_W;

            for (int d = 0; d < 8; ++d) {
                const i32 nx = cx + dx[d];
                const i32 ny = cy + dy[d];
                if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) continue;
                const i32 n_idx = ny * GRID_W + nx;
                if (m_blocked[n_idx] && n_idx != goal_idx) continue;
                if (state[n_idx] == 2) continue;

                const f32 tentative_g = g_score[current] + cost[d];
                if (tentative_g < g_score[n_idx]) {
                    came_from[n_idx] = current;
                    g_score[n_idx] = tentative_g;
                    f_score[n_idx] = tentative_g + heuristic(nx, ny, gx, gy);
                    state[n_idx] = 1;
                }
            }
        }
        return goal_w;
    }

private:
    [[nodiscard]] static i32 world_to_gx(f32 wx) noexcept {
        return static_cast<i32>(std::floor(wx + (GRID_W * 0.5f)));
    }
    [[nodiscard]] static i32 world_to_gy(f32 wy) noexcept {
        return static_cast<i32>(std::floor(wy + (GRID_H * 0.5f)));
    }
    [[nodiscard]] static glm::vec2 grid_to_world(i32 gx, i32 gy) noexcept {
        return {
            (static_cast<f32>(gx) - (GRID_W * 0.5f)) + 0.5f,
            (static_cast<f32>(gy) - (GRID_H * 0.5f)) + 0.5f
        };
    }
    [[nodiscard]] static f32 heuristic(i32 x0, i32 y0, i32 x1, i32 y1) noexcept {
        return std::hypot(static_cast<f32>(x1 - x0), static_cast<f32>(y1 - y0));
    }

    std::vector<bool> m_blocked;
};

} // namespace vg::game
