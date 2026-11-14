#pragma once
#include "../core/Types.hpp"
#include "../ecs/EntityHandle.hpp"
#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace vg {

enum class ShapeType : u8 {
    AABB   = 0,
    Circle = 1,
    OBB    = 2
};

struct AABB {
    glm::vec2 min{0.0f};
    glm::vec2 max{0.0f};

    [[nodiscard]] glm::vec2 center() const noexcept { return (min + max) * 0.5f; }
    [[nodiscard]] glm::vec2 half_extents() const noexcept { return (max - min) * 0.5f; }

    [[nodiscard]] bool overlaps(const AABB& o) const noexcept {
        return (min.x <= o.max.x && max.x >= o.min.x) &&
               (min.y <= o.max.y && max.y >= o.min.y);
    }
};

struct Circle {
    glm::vec2 center{0.0f};
    f32 radius = 0.5f;
};

struct OBB {
    glm::vec2 center{0.0f};
    glm::vec2 half_extents{0.5f};
    f32 rotation_rad = 0.0f;

    [[nodiscard]] AABB compute_enclosing_aabb() const noexcept {
        const f32 c = std::abs(std::cos(rotation_rad));
        const f32 s = std::abs(std::sin(rotation_rad));
        const glm::vec2 extent{
            half_extents.x * c + half_extents.y * s,
            half_extents.x * s + half_extents.y * c
        };
        return { center - extent, center + extent };
    }
};

struct ColliderProxy {
    Entity entity = NULL_ENTITY;
    u32 category_bits = 0;
    u32 mask_bits = 0;
    ShapeType type = ShapeType::AABB;
    AABB broadphase_aabb{};
    Circle circle{};
    OBB obb{};
};

struct ContactManifold {
    Entity a = NULL_ENTITY;
    Entity b = NULL_ENTITY;
    glm::vec2 normal{0.0f, 1.0f};
    glm::vec2 contact_point{0.0f};
    f32 penetration = 0.0f;
};

} // namespace vg
