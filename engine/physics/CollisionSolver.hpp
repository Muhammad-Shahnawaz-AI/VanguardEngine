#pragma once
#include "Shapes.hpp"
#include <optional>
#include <cmath>
#include <algorithm>

namespace vg {

class CollisionSolver {
public:
    [[nodiscard]] static std::optional<ContactManifold> test_aabb_aabb(const AABB& a, const AABB& b) noexcept {
        const glm::vec2 d = b.center() - a.center();
        const glm::vec2 e_a = a.half_extents();
        const glm::vec2 e_b = b.half_extents();
        const f32 ox = (e_a.x + e_b.x) - std::abs(d.x);
        const f32 oy = (e_a.y + e_b.y) - std::abs(d.y);
        if (ox <= 0.0f || oy <= 0.0f) return std::nullopt;

        ContactManifold m{};
        if (ox < oy) {
            m.penetration = ox;
            m.normal = { (d.x < 0.0f) ? -1.0f : 1.0f, 0.0f };
        } else {
            m.penetration = oy;
            m.normal = { 0.0f, (d.y < 0.0f) ? -1.0f : 1.0f };
        }
        m.contact_point = (a.center() + b.center()) * 0.5f;
        return m;
    }

    [[nodiscard]] static std::optional<ContactManifold> test_circle_circle(const Circle& a, const Circle& b) noexcept {
        const glm::vec2 d = b.center - a.center;
        const f32 r = a.radius + b.radius;
        const f32 dist_sq = glm::dot(d, d);
        if (dist_sq >= r * r) return std::nullopt;

        const f32 dist = std::sqrt(dist_sq);
        ContactManifold m{};
        m.normal = (dist > 1e-5f) ? (d / dist) : glm::vec2{1.0f, 0.0f};
        m.penetration = r - dist;
        m.contact_point = a.center + m.normal * a.radius;
        return m;
    }

    [[nodiscard]] static std::optional<ContactManifold> test_circle_obb(const Circle& c, const OBB& box) noexcept {
        const f32 cos_r = std::cos(-box.rotation_rad);
        const f32 sin_r = std::sin(-box.rotation_rad);
        const glm::vec2 rel = c.center - box.center;
        const glm::vec2 local_c{
            rel.x * cos_r - rel.y * sin_r,
            rel.x * sin_r + rel.y * cos_r
        };

        const glm::vec2 closest = glm::clamp(local_c, -box.half_extents, box.half_extents);
        const glm::vec2 diff = local_c - closest;
        const f32 dist_sq = glm::dot(diff, diff);
        if (dist_sq > c.radius * c.radius) return std::nullopt;

        const f32 dist = std::sqrt(dist_sq);
        glm::vec2 local_n = (dist > 1e-5f) ? (diff / dist) : glm::vec2{0.0f, 1.0f};

        const f32 w_cos = std::cos(box.rotation_rad);
        const f32 w_sin = std::sin(box.rotation_rad);
        ContactManifold m{};
        m.normal = {
            -(local_n.x * w_cos - local_n.y * w_sin),
            -(local_n.x * w_sin + local_n.y * w_cos)
        };
        m.penetration = c.radius - dist;
        m.contact_point = c.center + m.normal * c.radius;
        return m;
    }

    // Narrow-Phase Separating Axis Theorem (SAT) for two Oriented Bounding Boxes (FR-2.2)
    [[nodiscard]] static std::optional<ContactManifold> test_obb_obb_sat(const OBB& a, const OBB& b) noexcept {
        const glm::vec2 axes[4] = {
            { std::cos(a.rotation_rad),  std::sin(a.rotation_rad) },
            { -std::sin(a.rotation_rad), std::cos(a.rotation_rad) },
            { std::cos(b.rotation_rad),  std::sin(b.rotation_rad) },
            { -std::sin(b.rotation_rad), std::cos(b.rotation_rad) }
        };

        const glm::vec2 t = b.center - a.center;
        f32 min_overlap = 1e9f;
        glm::vec2 best_axis{1.0f, 0.0f};

        for (int i = 0; i < 4; ++i) {
            const glm::vec2& L = axes[i];
            const f32 proj_a = a.half_extents.x * std::abs(glm::dot(axes[0], L)) +
                               a.half_extents.y * std::abs(glm::dot(axes[1], L));
            const f32 proj_b = b.half_extents.x * std::abs(glm::dot(axes[2], L)) +
                               b.half_extents.y * std::abs(glm::dot(axes[3], L));
            const f32 dist = std::abs(glm::dot(t, L));
            const f32 overlap = (proj_a + proj_b) - dist;

            if (overlap <= 0.0f) return std::nullopt;
            if (overlap < min_overlap) {
                min_overlap = overlap;
                best_axis = (glm::dot(t, L) < 0.0f) ? -L : L;
            }
        }

        ContactManifold m{};
        m.normal = best_axis;
        m.penetration = min_overlap;
        m.contact_point = (a.center + b.center) * 0.5f;
        return m;
    }

    [[nodiscard]] static std::optional<ContactManifold> test_proxies(const ColliderProxy& a, const ColliderProxy& b) noexcept {
        auto to_obb = [](const ColliderProxy& p) -> OBB {
            if (p.type == ShapeType::OBB) return p.obb;
            return { p.broadphase_aabb.center(), p.broadphase_aabb.half_extents(), 0.0f };
        };

        if (a.type == ShapeType::AABB && b.type == ShapeType::AABB) {
            return test_aabb_aabb(a.broadphase_aabb, b.broadphase_aabb);
        }
        if (a.type == ShapeType::Circle && b.type == ShapeType::Circle) {
            return test_circle_circle(a.circle, b.circle);
        }
        if (a.type == ShapeType::Circle) {
            return test_circle_obb(a.circle, to_obb(b));
        }
        if (b.type == ShapeType::Circle) {
            auto res = test_circle_obb(b.circle, to_obb(a));
            if (res) res->normal = -res->normal;
            return res;
        }
        return test_obb_obb_sat(to_obb(a), to_obb(b));
    }
};

} // namespace vg
