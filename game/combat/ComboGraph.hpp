#pragma once
#include "InputBuffer.hpp"
#include "../components/CombatComponents.hpp"
#include <nlohmann/json.hpp>
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <filesystem>

namespace vg::game {

struct MoveNode {
    u16 id = 0;
    std::string name = "Idle";
    u16 startup_ticks = 6;
    u16 active_ticks = 4;
    u16 recovery_ticks = 10;
    u16 cancel_window_start = 2;
    f32 damage = 20.0f;
    f32 posture_damage = 15.0f;
    f32 knockback = 7.0f;
    f32 lunge_impulse = 5.0f;
    glm::vec2 hitbox_offset{1.0f, 0.0f};
    glm::vec2 hitbox_half_extents{0.9f, 0.7f};
    u8 hitstop_ticks = 4;
    bool hyper_armor = false;
    bool spawns_projectile = false;

    struct Transition {
        InputAction trigger = InputAction::None;
        u16 target_move_id = 0;
    };
    std::vector<Transition> transitions;
};

// Directed Acyclic Graph (DAG) of Weapon Movesets with Hot-Reloading (FR-3.3 & FR-4.2)
class ComboGraph {
public:
    bool load_from_file(const std::filesystem::path& path) {
        m_path = path;
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;

        nlohmann::json j;
        try {
            ifs >> j;
        } catch (...) {
            return false;
        }

        std::vector<MoveNode> new_nodes;
        std::unordered_map<std::string, u16> name_to_id;

        MoveNode idle{};
        idle.id = 0;
        idle.name = "Idle";
        new_nodes.push_back(std::move(idle));
        name_to_id["Idle"] = 0;

        for (const auto& item : j["moves"]) {
            MoveNode node{};
            node.id = static_cast<u16>(new_nodes.size());
            node.name = item.value("name", "Attack");
            node.startup_ticks = item.value("startup", 6);
            node.active_ticks = item.value("active", 4);
            node.recovery_ticks = item.value("recovery", 10);
            node.cancel_window_start = item.value("cancel_start", 1);
            node.damage = item.value("damage", 20.0f);
            node.posture_damage = item.value("posture_damage", 15.0f);
            node.knockback = item.value("knockback", 6.0f);
            node.lunge_impulse = item.value("lunge", 4.0f);
            node.hitbox_offset = { item["hitbox_offset"][0].get<f32>(), item["hitbox_offset"][1].get<f32>() };
            node.hitbox_half_extents = { item["hitbox_extents"][0].get<f32>(), item["hitbox_extents"][1].get<f32>() };
            node.hitstop_ticks = item.value("hitstop", 4);
            node.hyper_armor = item.value("hyper_armor", false);
            node.spawns_projectile = item.value("spawns_projectile", false);
            name_to_id[node.name] = node.id;

            // Register root starter transitions on node 0 if marked as starter
            if (item.contains("starter_input")) {
                const std::string st = item["starter_input"].get<std::string>();
                if (st == "Light")   new_nodes[0].transitions.push_back({ InputAction::LightAttack, node.id });
                if (st == "Heavy")   new_nodes[0].transitions.push_back({ InputAction::HeavyAttack, node.id });
                if (st == "Special") new_nodes[0].transitions.push_back({ InputAction::Special, node.id });
            }

            new_nodes.push_back(std::move(node));
        }

        std::size_t idx = 1;
        for (const auto& item : j["moves"]) {
            if (item.contains("links")) {
                for (const auto& link : item["links"]) {
                    const std::string act_str = link.value("input", "Light");
                    const std::string target_str = link.value("target", "Idle");
                    InputAction act = InputAction::LightAttack;
                    if (act_str == "Heavy") act = InputAction::HeavyAttack;
                    else if (act_str == "Special") act = InputAction::Special;

                    if (auto it = name_to_id.find(target_str); it != name_to_id.end()) {
                        new_nodes[idx].transitions.push_back({ act, it->second });
                    }
                }
            }
            ++idx;
        }

        m_nodes = std::move(new_nodes);
        std::error_code ec;
        m_last_write = std::filesystem::last_write_time(m_path, ec);
        return true;
    }

    bool poll_hot_reload() {
        if (m_path.empty()) return false;
        std::error_code ec;
        auto wt = std::filesystem::last_write_time(m_path, ec);
        if (!ec && wt != m_last_write) {
            return load_from_file(m_path);
        }
        return false;
    }

    [[nodiscard]] u16 find_transition(u16 current_move_id, InputAction action) const noexcept {
        if (current_move_id >= m_nodes.size()) return 0;
        for (const auto& edge : m_nodes[current_move_id].transitions) {
            if (edge.trigger == action) return edge.target_move_id;
        }
        if (current_move_id != 0) {
            for (const auto& edge : m_nodes[0].transitions) {
                if (edge.trigger == action) return edge.target_move_id;
            }
        }
        return 0;
    }

    [[nodiscard]] const MoveNode& get_node(u16 id) const noexcept {
        return (id < m_nodes.size()) ? m_nodes[id] : m_nodes[0];
    }

private:
    std::filesystem::path m_path;
    std::filesystem::file_time_type m_last_write{};
    std::vector<MoveNode> m_nodes;
};

} // namespace vg::game
