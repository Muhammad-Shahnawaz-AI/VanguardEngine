#include "CombatWorld.hpp"
#include <SDL3/SDL.h>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace vg::game {

bool CombatWorld::init(const std::filesystem::path& assets_dir, AudioEngine* audio) {
    m_assets_dir = assets_dir;
    m_audio = audio;

    if (!m_player_combo_graph.load_from_file(m_assets_dir / "configs" / "combos.json")) {
        return false;
    }
    m_grunt_bt.build_default_enemy_tree();
    m_boss_bt.build_boss_tree();
    reset_encounter(0x1337BEEF42ULL);
    return true;
}

void CombatWorld::reset_encounter(u64 seed) {
    m_registry.clear();
    m_input_buffer.clear();
    m_rng.reseed(seed);
    m_tick = 0;
    m_wave = 1;

    m_player = m_registry.create();
    m_registry.emplace<PlayerTag>(m_player);
    m_registry.emplace<Transform>(m_player, Transform{ {-5.0f, 0.0f}, {-5.0f, 0.0f}, {1.0f, 0.0f}, {0.9f, 0.9f}, 0.0f });
    m_registry.emplace<Kinematics>(m_player, Kinematics{ {0.0f, 0.0f}, {0.0f, 0.0f}, 8.5f, 22.0f });
    m_registry.emplace<HealthPosture>(m_player, HealthPosture{ 150.0f, 150.0f, 0.0f, 100.0f, 0, false, 1 });
    m_registry.emplace<Hurtbox>(m_player, Hurtbox{ {0.42f, 0.42f}, LAYER_PLAYER_HURTBOX, LAYER_ENEMY_HITBOX | LAYER_PROJECTILE, false });
    m_registry.emplace<CombatState>(m_player);

    const glm::vec2 pillar_positions[2] = { {-6.5f, 4.5f}, {6.5f, -4.5f} };
    for (const auto& p : pillar_positions) {
        Entity pillar = m_registry.create();
        m_registry.emplace<Transform>(pillar, Transform{ p, p, {1.0f, 0.0f}, {1.8f, 1.8f}, 0.0f });
        m_registry.emplace<Hurtbox>(pillar, Hurtbox{ {0.9f, 0.9f}, LAYER_ENVIRONMENT, 0, true });
        m_nav_grid.set_obstacle_box(p, {0.9f, 0.9f});
    }

    spawn_wave(m_wave);
}

void CombatWorld::spawn_wave(u32 wave_num) {
    const u32 grunt_count = 2 + wave_num;
    for (u32 i = 0; i < grunt_count; ++i) {
        Entity e = m_registry.create();
        const f32 angle = (6.2831853f / static_cast<f32>(grunt_count)) * static_cast<f32>(i);
        const glm::vec2 spawn_pos{ 5.5f + std::cos(angle) * 3.0f, std::sin(angle) * 5.0f };
        m_registry.emplace<EnemyTag>(e, EnemyTag{ 0, 1.65f, angle, static_cast<u16>(30 + i * 15) });
        m_registry.emplace<Transform>(e, Transform{ spawn_pos, spawn_pos, {-1.0f, 0.0f}, {0.85f, 0.85f}, 0.0f });
        m_registry.emplace<Kinematics>(e, Kinematics{ {0.0f, 0.0f}, {0.0f, 0.0f}, 5.2f, 18.0f });
        m_registry.emplace<HealthPosture>(e, HealthPosture{ 65.0f, 65.0f, 0.0f, 60.0f, 0, false, 1 });
        m_registry.emplace<Hurtbox>(e, Hurtbox{ {0.42f, 0.42f}, LAYER_ENEMY_HURTBOX, LAYER_PLAYER_HITBOX | LAYER_PROJECTILE, false });
        m_registry.emplace<CombatState>(e);
    }

    Entity boss = m_registry.create();
    const glm::vec2 boss_pos{ 8.0f, 0.0f };
    m_registry.emplace<EnemyTag>(boss, EnemyTag{ 1, 2.3f, 0.0f, 45 });
    m_registry.emplace<Transform>(boss, Transform{ boss_pos, boss_pos, {-1.0f, 0.0f}, {1.45f, 1.45f}, 0.0f });
    m_registry.emplace<Kinematics>(boss, Kinematics{ {0.0f, 0.0f}, {0.0f, 0.0f}, 4.6f, 16.0f });
    m_registry.emplace<HealthPosture>(boss, HealthPosture{ 260.0f, 260.0f, 0.0f, 120.0f, 0, true, 1 });
    m_registry.emplace<Hurtbox>(boss, Hurtbox{ {0.72f, 0.72f}, LAYER_ENEMY_HURTBOX, LAYER_PLAYER_HITBOX | LAYER_PROJECTILE, false });
    m_registry.emplace<CombatState>(boss);
}

void CombatWorld::buffer_player_action(InputAction action) noexcept {
    m_input_buffer.push_input(m_tick, action);
}

void CombatWorld::execute_move(Entity actor, const MoveNode& move) {
    auto& fsm = m_registry.get<CombatState>(actor);
    auto& tf = m_registry.get<Transform>(actor);
    auto& kin = m_registry.get<Kinematics>(actor);

    fsm.phase = ActionPhase::Startup;
    fsm.phase_tick = 0;
    fsm.current_move_id = move.id;
    std::strncpy(fsm.move_name, move.name.c_str(), sizeof(fsm.move_name) - 1);
    fsm.move_name[sizeof(fsm.move_name) - 1] = '\0';

    fsm.startup_ticks = move.startup_ticks;
    fsm.active_ticks = move.active_ticks;
    fsm.recovery_ticks = move.recovery_ticks;
    fsm.cancel_window_start = move.cancel_window_start;
    fsm.hitbox_offset = move.hitbox_offset;
    fsm.hitbox_half_extents = move.hitbox_half_extents;
    fsm.damage = move.damage;
    fsm.posture_damage = move.posture_damage;
    fsm.knockback_force = move.knockback;
    fsm.hitstop_ticks = move.hitstop_ticks;
    fsm.hyper_armor = move.hyper_armor;
    fsm.clear_hit_registry();

    kin.vel += tf.facing * move.lunge_impulse;

    if (m_audio) {
        m_audio->play(move.damage >= 30.0f ? SfxCue::SwingHeavy : SfxCue::SwingLight, 0.85f);
    }
}

void CombatWorld::spawn_projectile(Entity owner, const glm::vec2& pos, const glm::vec2& dir,
                                   bool is_player, f32 speed, f32 damage) {
    Entity p = m_registry.create();
    m_registry.emplace<Transform>(p, Transform{ pos, pos, dir, {0.42f, 0.42f}, std::atan2(dir.y, dir.x) });
    m_registry.emplace<Kinematics>(p, Kinematics{ dir * speed, {0.0f, 0.0f}, speed, 0.0f });
    m_registry.emplace<Projectile>(p, Projectile{ owner, damage, 12.0f, 5.0f, 0.22f, 150, is_player });
}

void CombatWorld::spawn_hit_particles(const glm::vec2& origin, const glm::vec2& dir,
                                      const glm::vec4& color, u32 count) {
    const f32 base_angle = std::atan2(dir.y, dir.x);
    for (u32 i = 0; i < count; ++i) {
        Entity pt = m_registry.create();
        const f32 ang = base_angle + m_rng.range_f32(-1.1f, 1.1f);
        const f32 spd = m_rng.range_f32(3.5f, 12.5f);
        const u16 life = static_cast<u16>(m_rng.range_i32(10, 22));
        m_registry.emplace<Particle>(pt, Particle{
            origin, origin, { std::cos(ang) * spd, std::sin(ang) * spd },
            color, m_rng.range_f32(0.08f, 0.18f), life, life
        });
    }
}

void CombatWorld::step_tick(const FrameInput& raw_input, DevInspectorState& inspector) {
    const u64 start_ns = SDL_GetTicksNS();

    if ((m_tick % 15) == 0) {
        m_player_combo_graph.poll_hot_reload();
    }

    FrameInput active_input = raw_input;
    active_input.tick = m_tick;
    if (m_replay.is_replaying()) {
        if (!m_replay.pop_replay_frame(active_input)) {
            inspector.is_replaying = false;
        } else if (active_input.action != InputAction::None) {
            m_input_buffer.push_input(m_tick, active_input.action);
        }
    } else if (m_replay.is_recording()) {
        m_replay.record_frame(active_input);
    }

    m_arena.reset();

    m_registry.view<Transform>([](Entity, Transform& tf) {
        tf.prev_pos = tf.pos;
    });
    m_registry.view<Particle>([](Entity, Particle& pt) {
        pt.prev_pos = pt.pos;
    });

    bool buffered_fired = false;
    system_player_fsm(active_input, buffered_fired);
    system_ai_and_pathfinding();
    system_combat_phases_and_kinematics();
    system_collision_and_damage();
    system_projectiles_and_particles();

    if (m_registry.is_alive(m_player)) {
        const auto& pfsm = m_registry.get<CombatState>(m_player);
        FrameTimelineEntry entry{};
        entry.tick = m_tick;
        entry.phase = static_cast<u8>(pfsm.phase);
        entry.buffered_input_fired = buffered_fired;
        std::snprintf(entry.move_label, sizeof(entry.move_label), "%s", pfsm.move_name);
        inspector.frame_timeline.push(entry);
    }

    if (m_registry.pool<EnemyTag>().size() == 0) {
        ++m_wave;
        spawn_wave(m_wave);
    }

    ++m_tick;

    const u64 end_ns = SDL_GetTicksNS();
    m_telemetry.sim_ms = static_cast<f32>(end_ns - start_ns) / 1'000'000.0f;
    m_telemetry.entity_count = static_cast<u32>(m_registry.alive_count());
    m_telemetry.arena_used_bytes = m_arena.used_bytes();
    m_telemetry.arena_peak_bytes = m_arena.peak_bytes();
    m_telemetry.arena_cap_bytes = m_arena.capacity_bytes();
}

void CombatWorld::system_player_fsm(const FrameInput& input, bool& out_buffered_fired) {
    if (!m_registry.is_alive(m_player)) return;

    auto& tf = m_registry.get<Transform>(m_player);
    auto& kin = m_registry.get<Kinematics>(m_player);
    auto& fsm = m_registry.get<CombatState>(m_player);
    auto& hurt = m_registry.get<Hurtbox>(m_player);

    if (fsm.frozen_ticks > 0) return;

    if (glm::length(input.aim_dir) > 0.01f &&
        (fsm.phase == ActionPhase::Neutral || fsm.phase == ActionPhase::Startup)) {
        tf.facing = glm::normalize(input.aim_dir);
        tf.rotation_rad = std::atan2(tf.facing.y, tf.facing.x);
    }

    const bool can_dodge = (fsm.phase == ActionPhase::Neutral ||
                            fsm.phase == ActionPhase::Startup ||
                            fsm.phase == ActionPhase::Recovery);
    if (can_dodge && m_input_buffer.consume_valid(m_tick, InputAction::Dodge)) {
        fsm.phase = ActionPhase::Dodge;
        fsm.phase_tick = 0;
        fsm.current_move_id = 0;
        std::strncpy(fsm.move_name, "ShadowDash", sizeof(fsm.move_name));
        hurt.invulnerable = true;
        const glm::vec2 dash_dir = (glm::length(input.move_dir) > 0.1f)
                                   ? glm::normalize(input.move_dir) : tf.facing;
        kin.vel = dash_dir * 19.5f;
        out_buffered_fired = true;
        if (m_audio) m_audio->play(SfxCue::DodgeWhoosh, 0.9f);
        return;
    }

    if (can_dodge && m_input_buffer.consume_valid(m_tick, InputAction::Parry)) {
        fsm.phase = ActionPhase::Parry;
        fsm.phase_tick = 0;
        fsm.current_move_id = 0;
        std::strncpy(fsm.move_name, "DeflectGuard", sizeof(fsm.move_name));
        kin.vel *= 0.2f;
        out_buffered_fired = true;
        return;
    }

    const bool in_cancel_window = (fsm.phase == ActionPhase::Recovery &&
                                   fsm.phase_tick >= fsm.cancel_window_start);
    if (fsm.phase == ActionPhase::Neutral || in_cancel_window) {
        if (auto act = m_input_buffer.consume_any_attack_or_special(m_tick)) {
            const u16 next_id = m_player_combo_graph.find_transition(fsm.current_move_id, *act);
            if (next_id != 0) {
                const MoveNode& node = m_player_combo_graph.get_node(next_id);
                execute_move(m_player, node);
                if (node.spawns_projectile) {
                    spawn_projectile(m_player, tf.pos + tf.facing * 0.9f, tf.facing, true, 16.0f, node.damage);
                }
                out_buffered_fired = true;
                return;
            }
        }
    }

    if (fsm.phase == ActionPhase::Neutral) {
        fsm.current_move_id = 0;
        std::strncpy(fsm.move_name, "Idle", sizeof(fsm.move_name));
        if (glm::length(input.move_dir) > 0.01f) {
            kin.vel = glm::normalize(input.move_dir) * kin.move_speed;
        }
    }
}

void CombatWorld::system_ai_and_pathfinding() {
    if (!m_registry.is_alive(m_player)) return;
    const glm::vec2 player_pos = m_registry.get<Transform>(m_player).pos;

    m_registry.view<EnemyTag, Transform, Kinematics, CombatState, HealthPosture>(
        [&](Entity e, EnemyTag& ai, Transform& tf, Kinematics& kin, CombatState& fsm, HealthPosture& hp) {
            if (fsm.frozen_ticks > 0) return;

            if (hp.is_boss && hp.boss_phase == 1 && hp.hp <= hp.max_hp * 0.5f) {
                hp.boss_phase = 2;
                kin.move_speed *= 1.35f;
                hp.hit_flash_ticks = 12;
                spawn_hit_particles(tf.pos, {0.0f, 1.0f}, {1.0f, 0.25f, 0.1f, 1.0f}, 24);
                if (m_audio) m_audio->play(SfxCue::StaggerBreak, 1.0f);
            }

            if (ai.attack_cooldown > 0) --ai.attack_cooldown;
            if (fsm.phase != ActionPhase::Neutral) return;

            const glm::vec2 to_player = player_pos - tf.pos;
            const f32 dist = glm::length(to_player);
            if (dist > 0.01f) {
                tf.facing = to_player / dist;
                tf.rotation_rad = std::atan2(tf.facing.y, tf.facing.x);
            }

            BTBlackboard bb{};
            bb.dist_to_player = dist;
            bb.attack_range = ai.preferred_range;
            bb.is_phase_2 = (hp.boss_phase >= 2);
            bb.attack_ready = (ai.attack_cooldown == 0);
            bb.is_staggered = (fsm.phase == ActionPhase::Staggered);

            const FlatBehaviorTree& tree = hp.is_boss ? m_boss_bt : m_grunt_bt;
            tree.evaluate(bb);

            switch (bb.chosen_action) {
                case BTActionOp::MeleeSlash: {
                    if (ai.attack_cooldown == 0) {
                        MoveNode slash{};
                        slash.id = 100;
                        slash.name = "EnemySlash";
                        slash.startup_ticks = 16;
                        slash.active_ticks = 4;
                        slash.recovery_ticks = 18;
                        slash.damage = 14.0f;
                        slash.posture_damage = 12.0f;
                        slash.knockback = 6.0f;
                        slash.lunge_impulse = 4.5f;
                        slash.hitbox_offset = {0.95f, 0.0f};
                        slash.hitbox_half_extents = {0.75f, 0.6f};
                        slash.hitstop_ticks = 4;
                        execute_move(e, slash);
                        ai.attack_cooldown = 65;
                    }
                    break;
                }
                case BTActionOp::BossHeavySlam: {
                    if (ai.attack_cooldown == 0) {
                        MoveNode slam{};
                        slam.id = 200;
                        slam.name = "VanguardCleave";
                        slam.startup_ticks = (hp.boss_phase == 2) ? 14 : 20;
                        slam.active_ticks = 6;
                        slam.recovery_ticks = 22;
                        slam.damage = 24.0f;
                        slam.posture_damage = 25.0f;
                        slam.knockback = 10.5f;
                        slam.lunge_impulse = 7.0f;
                        slam.hitbox_offset = {1.35f, 0.0f};
                        slam.hitbox_half_extents = {1.2f, 1.0f};
                        slam.hitstop_ticks = 6;
                        slam.hyper_armor = true;
                        execute_move(e, slam);
                        ai.attack_cooldown = (hp.boss_phase == 2) ? 50 : 75;
                    }
                    break;
                }
                case BTActionOp::RangedBarrage: {
                    for (int k = -1; k <= 1; ++k) {
                        const f32 ang = tf.rotation_rad + static_cast<f32>(k) * 0.25f;
                        spawn_projectile(e, tf.pos + glm::vec2{std::cos(ang), std::sin(ang)} * 1.1f,
                                         {std::cos(ang), std::sin(ang)}, false, 11.5f, 12.0f);
                    }
                    ai.attack_cooldown = 90;
                    break;
                }
                case BTActionOp::FlankAndApproach: {
                    const glm::vec2 flank_goal = player_pos + glm::vec2{
                        std::cos(ai.flank_angle_offset + static_cast<f32>(m_tick) * 0.01f),
                        std::sin(ai.flank_angle_offset + static_cast<f32>(m_tick) * 0.01f)
                    } * (ai.preferred_range * 0.85f);

                    const glm::vec2 waypoint = m_nav_grid.find_next_waypoint(m_arena, tf.pos, flank_goal);
                    glm::vec2 steer = waypoint - tf.pos;
                    if (glm::length(steer) > 0.01f) steer = glm::normalize(steer);

                    m_registry.view<EnemyTag, Transform>([&](Entity other, EnemyTag&, Transform& otf) {
                        if (other == e) return;
                        const glm::vec2 diff = tf.pos - otf.pos;
                        const f32 d = glm::length(diff);
                        if (d > 0.001f && d < 1.35f) {
                            steer += (diff / d) * ((1.35f - d) * 1.8f);
                        }
                    });

                    if (glm::length(steer) > 0.01f) {
                        kin.vel = glm::normalize(steer) * kin.move_speed;
                    }
                    break;
                }
                default: break;
            }
        });
}

void CombatWorld::system_combat_phases_and_kinematics() {
    m_registry.view<Transform, Kinematics, CombatState, Hurtbox>(
        [&](Entity, Transform& tf, Kinematics& kin, CombatState& fsm, Hurtbox& hurt) {
            if (fsm.frozen_ticks > 0) {
                --fsm.frozen_ticks;
                return;
            }

            ++fsm.phase_tick;
            switch (fsm.phase) {
                case ActionPhase::Startup:
                    if (fsm.phase_tick >= fsm.startup_ticks) {
                        fsm.phase = ActionPhase::Active;
                        fsm.phase_tick = 0;
                    }
                    break;
                case ActionPhase::Active:
                    if (fsm.phase_tick >= fsm.active_ticks) {
                        fsm.phase = ActionPhase::Recovery;
                        fsm.phase_tick = 0;
                    }
                    break;
                case ActionPhase::Recovery:
                    if (fsm.phase_tick >= fsm.recovery_ticks) {
                        fsm.phase = ActionPhase::Neutral;
                        fsm.phase_tick = 0;
                        fsm.current_move_id = 0;
                    }
                    break;
                case ActionPhase::Dodge:
                    if (fsm.phase_tick >= 12) {
                        hurt.invulnerable = false;
                        fsm.phase = ActionPhase::Neutral;
                        fsm.phase_tick = 0;
                    }
                    break;
                case ActionPhase::Parry:
                    if (fsm.phase_tick >= 14) {
                        fsm.phase = ActionPhase::Neutral;
                        fsm.phase_tick = 0;
                    }
                    break;
                case ActionPhase::Hitstun:
                    if (fsm.phase_tick >= 12) {
                        fsm.phase = ActionPhase::Neutral;
                        fsm.phase_tick = 0;
                    }
                    break;
                case ActionPhase::Staggered:
                    if (fsm.phase_tick >= 60) {
                        fsm.phase = ActionPhase::Neutral;
                        fsm.phase_tick = 0;
                    }
                    break;
                default: break;
            }

            tf.pos += (kin.vel + kin.knockback_vel) * FIXED_DT_F;
            const f32 damp = std::max(0.0f, 1.0f - kin.friction * FIXED_DT_F);
            kin.vel *= damp;
            kin.knockback_vel *= damp;

            tf.pos.x = std::clamp(tf.pos.x, -17.0f, 17.0f);
            tf.pos.y = std::clamp(tf.pos.y, -10.0f, 10.0f);
        });
}

void CombatWorld::system_collision_and_damage() {
    m_spatial_hash.begin_frame(m_arena);

    m_registry.view<Transform, Hurtbox>([&](Entity e, Transform& tf, Hurtbox& hurt) {
        if (auto* hp = m_registry.try_get<HealthPosture>(e)) {
            if (hp->hit_flash_ticks > 0) --hp->hit_flash_ticks;
        }
        if (hurt.invulnerable) return;

        ColliderProxy proxy{};
        proxy.entity = e;
        proxy.category_bits = hurt.category_bits;
        proxy.mask_bits = hurt.mask_bits;
        proxy.type = ShapeType::AABB;
        proxy.broadphase_aabb = { tf.pos - hurt.half_extents, tf.pos + hurt.half_extents };
        m_spatial_hash.insert(proxy);
    });

    m_registry.view<Transform, CombatState>([&](Entity e, Transform& tf, CombatState& fsm) {
        const bool is_player = m_registry.has<PlayerTag>(e);

        if (fsm.phase == ActionPhase::Parry && fsm.phase_tick <= 8) {
            ColliderProxy parry_proxy{};
            parry_proxy.entity = e;
            parry_proxy.category_bits = LAYER_PARRY_SHIELD;
            parry_proxy.mask_bits = LAYER_ENEMY_HITBOX | LAYER_PROJECTILE;
            parry_proxy.type = ShapeType::Circle;
            parry_proxy.circle = { tf.pos + tf.facing * 0.45f, 0.95f };
            parry_proxy.broadphase_aabb = {
                parry_proxy.circle.center - glm::vec2{0.95f},
                parry_proxy.circle.center + glm::vec2{0.95f}
            };
            m_spatial_hash.insert(parry_proxy);
        }

        if (fsm.phase == ActionPhase::Active) {
            const f32 c = std::cos(tf.rotation_rad);
            const f32 s = std::sin(tf.rotation_rad);
            const glm::vec2 world_offset{
                fsm.hitbox_offset.x * c - fsm.hitbox_offset.y * s,
                fsm.hitbox_offset.x * s + fsm.hitbox_offset.y * c
            };

            OBB obb{ tf.pos + world_offset, fsm.hitbox_half_extents, tf.rotation_rad };
            ColliderProxy atk_proxy{};
            atk_proxy.entity = e;
            atk_proxy.category_bits = is_player ? LAYER_PLAYER_HITBOX : LAYER_ENEMY_HITBOX;
            atk_proxy.mask_bits = is_player ? LAYER_ENEMY_HURTBOX : (LAYER_PLAYER_HURTBOX | LAYER_PARRY_SHIELD);
            atk_proxy.type = ShapeType::OBB;
            atk_proxy.obb = obb;
            atk_proxy.broadphase_aabb = obb.compute_enclosing_aabb();
            m_spatial_hash.insert(atk_proxy);
        }
    });

    m_registry.view<Transform, Projectile>([&](Entity e, Transform& tf, Projectile& proj) {
        ColliderProxy p_proxy{};
        p_proxy.entity = e;
        p_proxy.category_bits = LAYER_PROJECTILE;
        p_proxy.mask_bits = proj.is_player_owned
                            ? (LAYER_ENEMY_HURTBOX | LAYER_ENVIRONMENT)
                            : (LAYER_PLAYER_HURTBOX | LAYER_PARRY_SHIELD | LAYER_ENVIRONMENT);
        p_proxy.type = ShapeType::Circle;
        p_proxy.circle = { tf.pos, proj.radius };
        p_proxy.broadphase_aabb = { tf.pos - glm::vec2{proj.radius}, tf.pos + glm::vec2{proj.radius} };
        m_spatial_hash.insert(p_proxy);
    });

    SpatialHash::QueryStats stats{};
    auto manifolds = m_spatial_hash.resolve_all(stats);
    m_telemetry.broadphase_pairs = stats.broadphase_candidate_pairs;
    m_telemetry.narrowphase_hits = stats.narrowphase_hits;

    for (const auto& m : manifolds) {
        if (!m_registry.is_alive(m.a) || !m_registry.is_alive(m.b)) continue;

        auto* hurt_a = m_registry.try_get<Hurtbox>(m.a);
        auto* hurt_b = m_registry.try_get<Hurtbox>(m.b);
        if (hurt_a && hurt_b) {
            if (hurt_a->category_bits == LAYER_ENVIRONMENT && m_registry.has<Transform>(m.b)) {
                m_registry.get<Transform>(m.b).pos += m.normal * m.penetration;
            } else if (hurt_b->category_bits == LAYER_ENVIRONMENT && m_registry.has<Transform>(m.a)) {
                m_registry.get<Transform>(m.a).pos -= m.normal * m.penetration;
            }
        }

        auto process_strike = [&](Entity attacker, Entity victim) {
            auto* atk_fsm = m_registry.try_get<CombatState>(attacker);
            auto* vic_fsm = m_registry.try_get<CombatState>(victim);
            auto* vic_hp  = m_registry.try_get<HealthPosture>(victim);
            if (!atk_fsm || atk_fsm->phase != ActionPhase::Active || !vic_hp) return;
            if (atk_fsm->has_already_hit(victim)) return;

            if (vic_fsm && vic_fsm->phase == ActionPhase::Parry && vic_fsm->phase_tick <= 8) {
                atk_fsm->record_hit(victim);
                atk_fsm->phase = ActionPhase::Hitstun;
                atk_fsm->phase_tick = 0;
                atk_fsm->frozen_ticks = 6;
                vic_fsm->frozen_ticks = 6;

                if (auto* atk_hp = m_registry.try_get<HealthPosture>(attacker)) {
                    atk_hp->posture += 40.0f;
                    if (atk_hp->posture >= atk_hp->max_posture) {
                        atk_hp->posture = 0.0f;
                        atk_fsm->phase = ActionPhase::Staggered;
                        atk_fsm->phase_tick = 0;
                        if (m_audio) m_audio->play(SfxCue::StaggerBreak, 1.0f);
                    }
                }
                spawn_hit_particles(m.contact_point, m.normal, {1.0f, 0.95f, 0.25f, 1.0f}, 18);
                if (m_audio) m_audio->play(SfxCue::ParryClang, 1.0f);
                return;
            }

            atk_fsm->record_hit(victim);
            const f32 crit_mult = (vic_fsm && vic_fsm->phase == ActionPhase::Staggered) ? 1.5f : 1.0f;
            vic_hp->hp -= atk_fsm->damage * crit_mult;
            vic_hp->posture += atk_fsm->posture_damage;
            vic_hp->hit_flash_ticks = 5;

            atk_fsm->frozen_ticks = atk_fsm->hitstop_ticks;
            if (vic_fsm) {
                vic_fsm->frozen_ticks = atk_fsm->hitstop_ticks;
                if (!vic_fsm->hyper_armor && vic_fsm->phase != ActionPhase::Staggered) {
                    vic_fsm->phase = ActionPhase::Hitstun;
                    vic_fsm->phase_tick = 0;
                }
                if (vic_hp->posture >= vic_hp->max_posture) {
                    vic_hp->posture = 0.0f;
                    vic_fsm->phase = ActionPhase::Staggered;
                    vic_fsm->phase_tick = 0;
                    if (m_audio) m_audio->play(SfxCue::StaggerBreak, 1.0f);
                }
            }

            if (auto* vic_kin = m_registry.try_get<Kinematics>(victim)) {
                const glm::vec2 dir = glm::normalize(
                    m_registry.get<Transform>(victim).pos - m_registry.get<Transform>(attacker).pos + glm::vec2{1e-4f, 0.0f});
                vic_kin->knockback_vel += dir * atk_fsm->knockback_force;
            }

            if (auto* ptag = m_registry.try_get<PlayerTag>(attacker)) {
                ++ptag->combo_counter;
                ptag->combo_reset_timer = 180;
            }

            spawn_hit_particles(m.contact_point, m.normal, {1.0f, 0.3f, 0.15f, 1.0f}, 12);
            if (m_audio) m_audio->play(SfxCue::HitImpact, 1.0f);
        };

        process_strike(m.a, m.b);
        process_strike(m.b, m.a);

        auto process_projectile = [&](Entity proj_ent, Entity other) {
            auto* proj = m_registry.try_get<Projectile>(proj_ent);
            if (!proj || other == proj->owner) return;

            if (auto* other_fsm = m_registry.try_get<CombatState>(other)) {
                if (other_fsm->phase == ActionPhase::Parry && other_fsm->phase_tick <= 8) {
                    auto& pkin = m_registry.get<Kinematics>(proj_ent);
                    pkin.vel = -pkin.vel * 1.35f;
                    proj->is_player_owned = !proj->is_player_owned;
                    proj->owner = other;
                    spawn_hit_particles(m.contact_point, -pkin.vel, {1.0f, 0.95f, 0.25f, 1.0f}, 14);
                    if (m_audio) m_audio->play(SfxCue::ParryClang, 0.95f);
                    return;
                }
            }

            if (auto* hp = m_registry.try_get<HealthPosture>(other)) {
                hp->hp -= proj->damage;
                hp->hit_flash_ticks = 4;
                if (auto* kin = m_registry.try_get<Kinematics>(other)) {
                    kin->knockback_vel += glm::normalize(m_registry.get<Kinematics>(proj_ent).vel) * proj->knockback;
                }
                if (m_audio) m_audio->play(SfxCue::HitImpact, 0.8f);
            }
            spawn_hit_particles(m.contact_point, m.normal, {0.35f, 0.8f, 1.0f, 1.0f}, 10);
            m_registry.destroy(proj_ent);
        };

        process_projectile(m.a, m.b);
        process_projectile(m.b, m.a);
    }

    m_registry.view<EnemyTag, HealthPosture>([&](Entity e, EnemyTag&, HealthPosture& hp) {
        if (hp.hp <= 0.0f) {
            const glm::vec2 pos = m_registry.get<Transform>(e).pos;
            spawn_hit_particles(pos, {0.0f, 1.0f}, {1.0f, 0.45f, 0.1f, 1.0f}, 24);
            m_registry.destroy(e);
        }
    });
}

void CombatWorld::system_projectiles_and_particles() {
    m_registry.view<Transform, Kinematics, Projectile>(
        [&](Entity e, Transform& tf, Kinematics& kin, Projectile& proj) {
            tf.pos += kin.vel * FIXED_DT_F;
            if (proj.lifetime_ticks == 0 || std::abs(tf.pos.x) > 18.0f || std::abs(tf.pos.y) > 11.0f) {
                m_registry.destroy(e);
            } else {
                --proj.lifetime_ticks;
            }
        });

    m_registry.view<Particle>([&](Entity e, Particle& pt) {
        pt.pos += pt.vel * FIXED_DT_F;
        pt.vel *= 0.90f;
        if (pt.life == 0) {
            m_registry.destroy(e);
        } else {
            --pt.life;
        }
    });
}

void CombatWorld::render(BatchRenderer& renderer, Camera2D& camera, f32 alpha,
                         const DevInspectorState& inspector) {
    glm::vec2 p_pos{0.0f}, p_vel{0.0f};
    if (m_registry.is_alive(m_player)) {
        const auto& tf = m_registry.get<Transform>(m_player);
        p_pos = glm::mix(tf.prev_pos, tf.pos, alpha);
        p_vel = m_registry.get<Kinematics>(m_player).vel;
    }

    f32 max_spread = 0.0f;
    m_registry.view<EnemyTag, Transform>([&](Entity, EnemyTag&, Transform& tf) {
        max_spread = std::max(max_spread, glm::distance(p_pos, tf.pos));
    });
    camera.update(FIXED_DT_F, p_pos, p_vel, max_spread);

    renderer.begin_frame(camera.view_proj());

    renderer.draw_quad({ {0.0f, 0.0f}, {36.0f, 22.0f}, 0.0f, {0.09f, 0.10f, 0.13f, 1.0f}, 0.0f, -5.0f });
    renderer.draw_wire_box({0.0f, 0.0f}, {17.5f, 10.5f}, 0.0f, {0.28f, 0.32f, 0.42f, 1.0f}, 0.12f);

    m_registry.view<Transform>([&](Entity e, Transform& tf) {
        const glm::vec2 interp_pos = glm::mix(tf.prev_pos, tf.pos, alpha);
        glm::vec4 col{0.55f, 0.58f, 0.65f, 1.0f};
        f32 flash = 0.0f;
        f32 z = 1.0f;

        if (auto* hp = m_registry.try_get<HealthPosture>(e)) {
            flash = (hp->hit_flash_ticks > 0) ? 1.0f : 0.0f;
            if (m_registry.has<PlayerTag>(e)) {
                col = {0.22f, 0.78f, 1.0f, 1.0f};
                z = 2.0f;
            } else if (hp->is_boss) {
                col = (hp->boss_phase == 2)
                      ? glm::vec4{0.95f, 0.18f, 0.25f, 1.0f}
                      : glm::vec4{0.75f, 0.22f, 0.85f, 1.0f};
            } else {
                col = {0.92f, 0.38f, 0.22f, 1.0f};
            }

            const f32 hp_frac = std::clamp(hp->hp / hp->max_hp, 0.0f, 1.0f);
            const f32 bar_w = tf.scale.x * 1.2f;
            renderer.draw_quad({ interp_pos + glm::vec2{0.0f, tf.scale.y * 0.8f}, {bar_w, 0.12f}, 0.0f, {0.15f, 0.15f, 0.15f, 0.9f}, 0.0f, 5.0f });
            renderer.draw_quad({ interp_pos + glm::vec2{-(bar_w * (1.0f - hp_frac)) * 0.5f, tf.scale.y * 0.8f},
                                 {bar_w * hp_frac, 0.10f}, 0.0f, {0.2f, 0.92f, 0.35f, 1.0f}, 0.0f, 5.1f });
        } else if (auto* proj = m_registry.try_get<Projectile>(e)) {
            col = proj->is_player_owned ? glm::vec4{0.3f, 0.95f, 1.0f, 1.0f} : glm::vec4{1.0f, 0.45f, 0.15f, 1.0f};
            z = 3.0f;
        }

        renderer.draw_quad({ interp_pos, tf.scale, tf.rotation_rad, col, flash, z });

        if (m_registry.has<CombatState>(e)) {
            renderer.draw_quad({ interp_pos + tf.facing * (tf.scale.x * 0.65f),
                                 {0.55f, 0.14f}, tf.rotation_rad, {0.95f, 0.95f, 1.0f, 1.0f}, 0.0f, z + 0.1f });
        }
    });

    m_registry.view<Particle>([&](Entity, Particle& pt) {
        const glm::vec2 pos = glm::mix(pt.prev_pos, pt.pos, alpha);
        glm::vec4 c = pt.color;
        c.a = static_cast<f32>(pt.life) / static_cast<f32>(pt.max_life);
        renderer.draw_quad({ pos, {pt.size, pt.size}, 0.0f, c, 0.0f, 4.0f });
    });

    if (inspector.visible && inspector.show_spatial_grid) {
        m_spatial_hash.for_each_occupied_cell([&](const AABB& cell) {
            renderer.draw_wire_box(cell.center(), cell.half_extents(), 0.0f,
                                   {0.45f, 0.48f, 0.55f, 0.35f}, 0.02f);
        });
    }

    if (inspector.visible && inspector.show_hitboxes) {
        m_registry.view<Transform, Hurtbox>([&](Entity, Transform& tf, Hurtbox& hurt) {
            const glm::vec4 col = hurt.invulnerable
                                  ? glm::vec4{0.2f, 0.65f, 1.0f, 0.95f}
                                  : glm::vec4{0.2f, 0.95f, 0.35f, 0.95f};
            renderer.draw_wire_box(tf.pos, hurt.half_extents, 0.0f, col, 0.04f);
        });

        m_registry.view<Transform, CombatState>([&](Entity, Transform& tf, CombatState& fsm) {
            if (fsm.phase == ActionPhase::Active) {
                const f32 c = std::cos(tf.rotation_rad);
                const f32 s = std::sin(tf.rotation_rad);
                const glm::vec2 world_offset{
                    fsm.hitbox_offset.x * c - fsm.hitbox_offset.y * s,
                    fsm.hitbox_offset.x * s + fsm.hitbox_offset.y * c
                };
                renderer.draw_wire_box(tf.pos + world_offset, fsm.hitbox_half_extents,
                                       tf.rotation_rad, {1.0f, 0.18f, 0.18f, 1.0f}, 0.05f);
            } else if (fsm.phase == ActionPhase::Parry && fsm.phase_tick <= 8) {
                renderer.draw_wire_circle(tf.pos + tf.facing * 0.45f, 0.95f,
                                          {1.0f, 0.95f, 0.2f, 1.0f}, 24, 0.05f);
            }
        });
    }

    renderer.end_frame();
}

} // namespace vg::game
