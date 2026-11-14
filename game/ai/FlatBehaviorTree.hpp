#pragma once
#include "engine/core/Types.hpp"
#include <vector>
#include <cstdint>

namespace vg::game {

enum class BTNodeType : u8 {
    Selector  = 0,
    Sequence  = 1,
    Condition = 2,
    Action    = 3
};

enum class BTStatus : u8 {
    Success = 0,
    Failure = 1,
    Running = 2
};

enum class BTConditionOp : u8 {
    TargetInAttackRange = 0,
    TargetInAggroRange  = 1,
    IsBossPhase2        = 2,
    AttackOffCooldown   = 3,
    IsNotStaggered      = 4
};

enum class BTActionOp : u8 {
    Idle             = 0,
    FlankAndApproach = 1,
    MeleeSlash       = 2,
    BossHeavySlam    = 3,
    RangedBarrage    = 4
};

struct alignas(16) FlatBTNode {
    BTNodeType type = BTNodeType::Action;
    u8 op_code = 0;
    u16 subtree_size = 1;
    f32 param = 0.0f;
    u32 pad = 0;
};

struct BTBlackboard {
    f32 dist_to_player = 999.0f;
    f32 attack_range = 1.75f;
    f32 aggro_range = 16.0f;
    bool is_phase_2 = false;
    bool attack_ready = false;
    bool is_staggered = false;
    BTActionOp chosen_action = BTActionOp::Idle;
};

class FlatBehaviorTree {
public:
    void build_default_enemy_tree() {
        m_nodes.clear();
        m_nodes.push_back({ BTNodeType::Sequence, 0, 8, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Condition, static_cast<u8>(BTConditionOp::IsNotStaggered), 1, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Selector, 0, 6, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Sequence, 0, 3, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Condition, static_cast<u8>(BTConditionOp::TargetInAttackRange), 1, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Action, static_cast<u8>(BTActionOp::MeleeSlash), 1, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Sequence, 0, 2, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Action, static_cast<u8>(BTActionOp::FlankAndApproach), 1, 0.0f, 0 });
    }

    void build_boss_tree() {
        m_nodes.clear();
        m_nodes.push_back({ BTNodeType::Sequence, 0, 12, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Condition, static_cast<u8>(BTConditionOp::IsNotStaggered), 1, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Selector, 0, 10, 0.0f, 0 });

        m_nodes.push_back({ BTNodeType::Sequence, 0, 4, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Condition, static_cast<u8>(BTConditionOp::IsBossPhase2), 1, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Condition, static_cast<u8>(BTConditionOp::AttackOffCooldown), 1, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Action, static_cast<u8>(BTActionOp::RangedBarrage), 1, 0.0f, 0 });

        m_nodes.push_back({ BTNodeType::Sequence, 0, 3, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Condition, static_cast<u8>(BTConditionOp::TargetInAttackRange), 1, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Action, static_cast<u8>(BTActionOp::BossHeavySlam), 1, 0.0f, 0 });

        m_nodes.push_back({ BTNodeType::Sequence, 0, 2, 0.0f, 0 });
        m_nodes.push_back({ BTNodeType::Action, static_cast<u8>(BTActionOp::FlankAndApproach), 1, 0.0f, 0 });
    }

    BTStatus evaluate(BTBlackboard& bb) const noexcept {
        if (m_nodes.empty()) return BTStatus::Failure;
        return eval_node(0, bb);
    }

private:
    BTStatus eval_node(u16 idx, BTBlackboard& bb) const noexcept {
        const FlatBTNode& node = m_nodes[idx];
        switch (node.type) {
            case BTNodeType::Condition: {
                bool ok = false;
                switch (static_cast<BTConditionOp>(node.op_code)) {
                    case BTConditionOp::TargetInAttackRange: ok = bb.dist_to_player <= bb.attack_range; break;
                    case BTConditionOp::TargetInAggroRange:  ok = bb.dist_to_player <= bb.aggro_range; break;
                    case BTConditionOp::IsBossPhase2:        ok = bb.is_phase_2; break;
                    case BTConditionOp::AttackOffCooldown:   ok = bb.attack_ready; break;
                    case BTConditionOp::IsNotStaggered:      ok = !bb.is_staggered; break;
                }
                return ok ? BTStatus::Success : BTStatus::Failure;
            }
            case BTNodeType::Action: {
                bb.chosen_action = static_cast<BTActionOp>(node.op_code);
                return BTStatus::Success;
            }
            case BTNodeType::Sequence: {
                u16 cursor = idx + 1;
                const u16 end = idx + node.subtree_size;
                while (cursor < end) {
                    BTStatus st = eval_node(cursor, bb);
                    if (st != BTStatus::Success) return st;
                    cursor += m_nodes[cursor].subtree_size;
                }
                return BTStatus::Success;
            }
            case BTNodeType::Selector: {
                u16 cursor = idx + 1;
                const u16 end = idx + node.subtree_size;
                while (cursor < end) {
                    BTStatus st = eval_node(cursor, bb);
                    if (st != BTStatus::Failure) return st;
                    cursor += m_nodes[cursor].subtree_size;
                }
                return BTStatus::Failure;
            }
        }
        return BTStatus::Failure;
    }

    std::vector<FlatBTNode> m_nodes;
};

} // namespace vg::game
