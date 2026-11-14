#include <gtest/gtest.h>
#include "engine/core/FrameArena.hpp"
#include "engine/core/RingBuffer.hpp"
#include "engine/core/PCG32.hpp"
#include "engine/ecs/Registry.hpp"
#include "engine/physics/SpatialHash.hpp"
#include "engine/physics/CollisionSolver.hpp"
#include "game/combat/CombatWorld.hpp"
#include <chrono>
#include <cmath>

using namespace vg;
using namespace vg::game;

TEST(ECSTests, GenerationalHandlesPreventDanglingAccess) {
    Registry reg;
    Entity e1 = reg.create();
    EXPECT_EQ(e1.index(), 0u);
    EXPECT_EQ(e1.version(), 0u);

    reg.emplace<int>(e1, 42);
    EXPECT_TRUE(reg.has<int>(e1));
    EXPECT_EQ(reg.get<int>(e1), 42);

    reg.destroy(e1);
    EXPECT_FALSE(reg.is_alive(e1));
    EXPECT_FALSE(reg.has<int>(e1));

    Entity e2 = reg.create();
    EXPECT_EQ(e2.index(), 0u);
    EXPECT_EQ(e2.version(), 1u);
    EXPECT_NE(e1, e2);
    EXPECT_FALSE(reg.is_alive(e1));
    EXPECT_TRUE(reg.is_alive(e2));
}

TEST(ECSTests, MultiComponentViewSelectsSmallestPool) {
    Registry reg;
    for (int i = 0; i < 100; ++i) {
        Entity e = reg.create();
        reg.emplace<f32>(e, static_cast<f32>(i));
        if (i % 20 == 0) {
            reg.emplace<i32>(e, i);
        }
    }

    int count = 0;
    reg.view<f32, i32>([&](Entity, f32& f, i32& idx) {
        EXPECT_FLOAT_EQ(f, static_cast<f32>(idx));
        ++count;
    });
    EXPECT_EQ(count, 5);
}

TEST(PhysicsTests, OBB_SAT_DetectsRotatedOverlapAndSeparation) {
    OBB box_a{ {0.0f, 0.0f}, {1.0f, 0.5f}, 0.0f };
    OBB box_b{ {1.2f, 0.6f}, {1.0f, 0.5f}, 0.785398f };

    auto hit = CollisionSolver::test_obb_obb_sat(box_a, box_b);
    ASSERT_TRUE(hit.has_value());
    EXPECT_GT(hit->penetration, 0.0f);
    EXPECT_NEAR(glm::length(hit->normal), 1.0f, 1e-4f);

    OBB box_far{ {5.0f, 5.0f}, {1.0f, 0.5f}, 0.5f };
    EXPECT_FALSE(CollisionSolver::test_obb_obb_sat(box_a, box_far).has_value());
}

TEST(PhysicsTests, SpatialHashPrunesDistantPairsAndFiltersMasks) {
    FrameArena arena(1024 * 1024);
    SpatialHash grid(2.0f, 512);
    grid.begin_frame(arena);

    ColliderProxy p1{};
    p1.entity = Entity(1, 0);
    p1.category_bits = LAYER_PLAYER_HITBOX;
    p1.mask_bits = LAYER_ENEMY_HURTBOX;
    p1.type = ShapeType::AABB;
    p1.broadphase_aabb = { {0.0f, 0.0f}, {1.0f, 1.0f} };

    ColliderProxy p2 = p1;
    p2.entity = Entity(2, 0);
    p2.category_bits = LAYER_ENEMY_HURTBOX;
    p2.mask_bits = LAYER_PLAYER_HITBOX;

    ColliderProxy p3 = p2;
    p3.entity = Entity(3, 0);
    p3.broadphase_aabb = { {50.0f, 50.0f}, {51.0f, 51.0f} };

    grid.insert(p1);
    grid.insert(p2);
    grid.insert(p3);

    SpatialHash::QueryStats stats{};
    auto manifolds = grid.resolve_all(stats);
    EXPECT_EQ(stats.broadphase_candidate_pairs, 1u);
    EXPECT_EQ(manifolds.size(), 1u);
}

TEST(InputTests, CircularInputBufferRespects250msWindow) {
    InputBuffer buf;
    buf.push_input(100, InputAction::LightAttack);

    EXPECT_FALSE(buf.consume_valid(120, InputAction::LightAttack).has_value());

    buf.push_input(125, InputAction::HeavyAttack);
    auto consumed = buf.consume_valid(138, InputAction::HeavyAttack);
    ASSERT_TRUE(consumed.has_value());
    EXPECT_EQ(*consumed, InputAction::HeavyAttack);
    EXPECT_FALSE(buf.consume_valid(138, InputAction::HeavyAttack).has_value());
}

TEST(DeterminismAndPerfTests, BitIdenticalSimulationAndSub2msBudget) {
    auto run_simulation = [](u64 seed) -> glm::vec2 {
        CombatWorld world;
        EXPECT_TRUE(world.init(ASSETS_DIR, nullptr));
        world.reset_encounter(seed);

        for (int i = 0; i < 50; ++i) {
            Entity e = world.registry().create();
            world.registry().emplace<EnemyTag>(e, EnemyTag{ 0, 1.5f, static_cast<f32>(i), 30 });
            world.registry().emplace<Transform>(e, Transform{
                { static_cast<f32>(i % 10) - 5.0f, static_cast<f32>(i / 10) - 2.5f },
                { 0.0f, 0.0f }, { -1.0f, 0.0f }, { 0.8f, 0.8f }, 0.0f
            });
            world.registry().emplace<Kinematics>(e);
            world.registry().emplace<HealthPosture>(e);
            world.registry().emplace<Hurtbox>(e, Hurtbox{ {0.4f, 0.4f}, LAYER_ENEMY_HURTBOX, LAYER_PLAYER_HITBOX, false });
            world.registry().emplace<CombatState>(e);
        }

        for (int i = 0; i < 200; ++i) {
            Entity p = world.registry().create();
            world.registry().emplace<Transform>(p, Transform{ {0.0f, 0.0f}, {0.0f, 0.0f}, {1.0f, 0.0f}, {0.2f, 0.2f}, 0.0f });
            world.registry().emplace<Kinematics>(p, Kinematics{ {3.0f, 1.0f}, {0.0f, 0.0f}, 3.0f, 0.0f });
            world.registry().emplace<Projectile>(p, Projectile{ NULL_ENTITY, 5.0f, 5.0f, 2.0f, 0.2f, 300, true });
        }

        DevInspectorState dummy_inspector{};
        f64 total_ms = 0.0;

        for (u64 t = 0; t < 120; ++t) {
            FrameInput in{};
            in.move_dir = { 1.0f, (t % 20 < 10) ? 0.5f : -0.5f };
            in.aim_dir = { 1.0f, 0.0f };
            if (t % 18 == 0) {
                in.action = InputAction::LightAttack;
                world.buffer_player_action(InputAction::LightAttack);
            }

            const auto t0 = std::chrono::high_resolution_clock::now();
            world.step_tick(in, dummy_inspector);
            const auto t1 = std::chrono::high_resolution_clock::now();
            total_ms += std::chrono::duration<f64, std::milli>(t1 - t0).count();
        }

        const f64 avg_tick_ms = total_ms / 120.0;
        #if defined(NDEBUG) && !defined(__SANITIZE_ADDRESS__)
    EXPECT_LT(avg_tick_ms, 2.0) << "Simulation tick exceeded 2.0ms NFR-1.1 budget";
#else
    EXPECT_LT(avg_tick_ms, 25.0) << "Sanitized/Debug simulation tick exceeded 25.0ms budget";
#endif
        return world.registry().get<Transform>(world.player()).pos;
    };

    const glm::vec2 pos_run_1 = run_simulation(0xDEADBEEF12345678ULL);
    const glm::vec2 pos_run_2 = run_simulation(0xDEADBEEF12345678ULL);

    EXPECT_EQ(pos_run_1.x, pos_run_2.x);
    EXPECT_EQ(pos_run_1.y, pos_run_2.y);
}
