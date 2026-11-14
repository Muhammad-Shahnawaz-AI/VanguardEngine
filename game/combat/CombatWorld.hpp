#pragma once
#include "engine/core/Types.hpp"
#include "engine/core/FrameArena.hpp"
#include "engine/core/PCG32.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/physics/SpatialHash.hpp"
#include "engine/render/BatchRenderer.hpp"
#include "engine/render/Camera2D.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/tools/DevInspector.hpp"
#include "../components/CombatComponents.hpp"
#include "../ai/FlatBehaviorTree.hpp"
#include "../ai/NavGridAStar.hpp"
#include "InputBuffer.hpp"
#include "ComboGraph.hpp"
#include <filesystem>

namespace vg::game {

class CombatWorld {
public:
    bool init(const std::filesystem::path& assets_dir, AudioEngine* audio);
    void reset_encounter(u64 seed = 0x1337BEEF42ULL);

    void buffer_player_action(InputAction action) noexcept;
    void step_tick(const FrameInput& raw_input, DevInspectorState& inspector);
    void render(BatchRenderer& renderer, Camera2D& camera, f32 alpha,
                const DevInspectorState& inspector);

    [[nodiscard]] Registry& registry() noexcept { return m_registry; }
    [[nodiscard]] const EngineTelemetry& telemetry() const noexcept { return m_telemetry; }
    [[nodiscard]] Entity player() const noexcept { return m_player; }
    [[nodiscard]] u64 tick_count() const noexcept { return m_tick; }
    [[nodiscard]] ReplayStream& replay_stream() noexcept { return m_replay; }

private:
    void execute_move(Entity actor, const MoveNode& move);
    void spawn_projectile(Entity owner, const glm::vec2& pos, const glm::vec2& dir,
                          bool is_player, f32 speed, f32 damage);
    void spawn_hit_particles(const glm::vec2& origin, const glm::vec2& dir,
                             const glm::vec4& color, u32 count);
    void spawn_wave(u32 wave_num);

    void system_player_fsm(const FrameInput& input, bool& out_buffered_fired);
    void system_ai_and_pathfinding();
    void system_combat_phases_and_kinematics();
    void system_collision_and_damage();
    void system_projectiles_and_particles();

    std::filesystem::path m_assets_dir;
    AudioEngine* m_audio = nullptr;

    FrameArena m_arena{ 4 * 1024 * 1024 };
    Registry m_registry{ 2048 };
    SpatialHash m_spatial_hash{ 2.0f, 2048 };
    PCG32 m_rng;

    ComboGraph m_player_combo_graph;
    InputBuffer m_input_buffer;
    ReplayStream m_replay;
    NavGridAStar m_nav_grid;
    FlatBehaviorTree m_grunt_bt;
    FlatBehaviorTree m_boss_bt;

    Entity m_player = NULL_ENTITY;
    u64 m_tick = 0;
    u32 m_wave = 1;
    EngineTelemetry m_telemetry{};
};

} // namespace vg::game
