#pragma once
#include "engine/core/Types.hpp"
#include "engine/ecs/EntityHandle.hpp"
#include <glm/glm.hpp>
#include <array>

namespace vg::game {

enum CollisionLayer : u32 {
    LAYER_NONE           = 0,
    LAYER_PLAYER_HURTBOX = 1 << 0,
    LAYER_PLAYER_HITBOX  = 1 << 1,
    LAYER_ENEMY_HURTBOX  = 1 << 2,
    LAYER_ENEMY_HITBOX   = 1 << 3,
    LAYER_PARRY_SHIELD   = 1 << 4,
    LAYER_PROJECTILE     = 1 << 5,
    LAYER_ENVIRONMENT    = 1 << 6
};

enum class ActionPhase : u8 {
    Neutral   = 0,
    Startup   = 1,
    Active    = 2,
    Recovery  = 3,
    Dodge     = 4,
    Parry     = 5,
    Hitstun   = 6,
    Staggered = 7
};

struct PlayerTag {
    u32 combo_counter = 0;
    u16 combo_reset_timer = 0;
};

struct EnemyTag {
    u8 archetype = 0;
    f32 preferred_range = 1.75f;
    f32 flank_angle_offset = 0.0f;
    u16 attack_cooldown = 40;
};

struct Transform {
    glm::vec2 pos{0.0f};
    glm::vec2 prev_pos{0.0f};
    glm::vec2 facing{1.0f, 0.0f};
    glm::vec2 scale{1.0f};
    f32 rotation_rad = 0.0f;
};

struct Kinematics {
    glm::vec2 vel{0.0f};
    glm::vec2 knockback_vel{0.0f};
    f32 move_speed = 6.0f;
    f32 friction = 18.0f;
};

struct HealthPosture {
    f32 hp = 100.0f;
    f32 max_hp = 100.0f;
    f32 posture = 0.0f;
    f32 max_posture = 100.0f;
    u8 hit_flash_ticks = 0;
    bool is_boss = false;
    u8 boss_phase = 1;
};

struct Hurtbox {
    glm::vec2 half_extents{0.4f, 0.4f};
    u32 category_bits = LAYER_NONE;
    u32 mask_bits = LAYER_NONE;
    bool invulnerable = false;
};

struct CombatState {
    ActionPhase phase = ActionPhase::Neutral;
    u16 phase_tick = 0;
    u16 current_move_id = 0;
    char move_name[24] = "Idle";

    u16 startup_ticks = 0;
    u16 active_ticks = 0;
    u16 recovery_ticks = 0;
    u16 cancel_window_start = 0;

    glm::vec2 hitbox_offset{1.0f, 0.0f};
    glm::vec2 hitbox_half_extents{0.8f, 0.6f};
    f32 damage = 0.0f;
    f32 posture_damage = 0.0f;
    f32 knockback_force = 0.0f;
    u8 hitstop_ticks = 0;
    u8 frozen_ticks = 0;
    bool hyper_armor = false;

    std::array<Entity, 16> hit_entities{};
    u8 hit_count = 0;

    void clear_hit_registry() noexcept {
        hit_count = 0;
    }

    [[nodiscard]] bool has_already_hit(Entity e) const noexcept {
        for (u8 i = 0; i < hit_count; ++i) {
            if (hit_entities[i] == e) return true;
        }
        return false;
    }

    void record_hit(Entity e) noexcept {
        if (hit_count < hit_entities.size()) {
            hit_entities[hit_count++] = e;
        }
    }
};

struct Projectile {
    Entity owner = NULL_ENTITY;
    f32 damage = 12.0f;
    f32 posture_damage = 10.0f;
    f32 knockback = 5.0f;
    f32 radius = 0.22f;
    u16 lifetime_ticks = 120;
    bool is_player_owned = true;
};

struct Particle {
    glm::vec2 pos{0.0f};
    glm::vec2 prev_pos{0.0f};
    glm::vec2 vel{0.0f};
    glm::vec4 color{1.0f};
    f32 size = 0.1f;
    u16 life = 15;
    u16 max_life = 15;
};

} // namespace vg::game
