#pragma once
#include "engine/core/Types.hpp"
#include "engine/core/RingBuffer.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <vector>
#include <fstream>
#include <filesystem>

namespace vg::game {

enum class InputAction : u8 {
    None        = 0,
    LightAttack = 1,
    HeavyAttack = 2,
    Dodge       = 3,
    Parry       = 4,
    Special     = 5
};

struct BufferedInputEvent {
    u64 timestamp_tick = 0;
    InputAction action = InputAction::None;
    bool consumed = false;
};

struct FrameInput {
    u64 tick = 0;
    glm::vec2 move_dir{0.0f};
    glm::vec2 aim_dir{1.0f, 0.0f};
    InputAction action = InputAction::None;
};

// Fixed-capacity 250ms (15-tick) Circular Input Buffer (FR-4.1)
class InputBuffer {
public:
    static constexpr u64 BUFFER_WINDOW_TICKS = 15;

    void push_input(u64 tick, InputAction action) noexcept {
        if (action == InputAction::None) return;
        m_ring.push({ tick, action, false });
    }

    [[nodiscard]] std::optional<InputAction> consume_valid(u64 current_tick, InputAction desired) noexcept {
        for (std::size_t i = m_ring.size(); i > 0; --i) {
            auto& ev = m_ring[i - 1];
            if (ev.consumed) continue;
            if (current_tick >= ev.timestamp_tick && (current_tick - ev.timestamp_tick) <= BUFFER_WINDOW_TICKS) {
                if (ev.action == desired) {
                    ev.consumed = true;
                    return ev.action;
                }
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] std::optional<InputAction> consume_any_attack_or_special(u64 current_tick) noexcept {
        for (std::size_t i = m_ring.size(); i > 0; --i) {
            auto& ev = m_ring[i - 1];
            if (ev.consumed) continue;
            if (current_tick >= ev.timestamp_tick && (current_tick - ev.timestamp_tick) <= BUFFER_WINDOW_TICKS) {
                if (ev.action == InputAction::LightAttack ||
                    ev.action == InputAction::HeavyAttack ||
                    ev.action == InputAction::Special) {
                    ev.consumed = true;
                    return ev.action;
                }
            }
        }
        return std::nullopt;
    }

    void clear() noexcept { m_ring.clear(); }

private:
    RingBuffer<BufferedInputEvent, 16> m_ring;
};

// Deterministic Binary Input Replay Recorder/Player (.rep files, FR-7.4, NFR-1.3)
class ReplayStream {
public:
    struct Header {
        u32 magic = 0x56475250; // "VGRP"
        u64 seed = 0;
        u64 frame_count = 0;
    };

    void start_recording(u64 seed) {
        m_seed = seed;
        m_frames.clear();
        m_recording = true;
        m_replaying = false;
    }

    void record_frame(const FrameInput& in) {
        if (m_recording) m_frames.push_back(in);
    }

    bool stop_and_save(const std::filesystem::path& path) {
        m_recording = false;
        std::ofstream ofs(path, std::ios::binary);
        if (!ofs.is_open()) return false;
        Header h{ 0x56475250, m_seed, static_cast<u64>(m_frames.size()) };
        ofs.write(reinterpret_cast<const char*>(&h), sizeof(Header));
        ofs.write(reinterpret_cast<const char*>(m_frames.data()),
                  static_cast<std::streamsize>(m_frames.size() * sizeof(FrameInput)));
        return true;
    }

    bool load_for_replay(const std::filesystem::path& path) {
        std::ifstream ifs(path, std::ios::binary);
        if (!ifs.is_open()) return false;
        Header h{};
        ifs.read(reinterpret_cast<char*>(&h), sizeof(Header));
        if (h.magic != 0x56475250) return false;
        m_seed = h.seed;
        m_frames.resize(static_cast<std::size_t>(h.frame_count));
        ifs.read(reinterpret_cast<char*>(m_frames.data()),
                 static_cast<std::streamsize>(h.frame_count * sizeof(FrameInput)));
        m_replay_cursor = 0;
        m_recording = false;
        m_replaying = true;
        return true;
    }

    [[nodiscard]] bool pop_replay_frame(FrameInput& out_input) noexcept {
        if (!m_replaying || m_replay_cursor >= m_frames.size()) {
            m_replaying = false;
            return false;
        }
        out_input = m_frames[m_replay_cursor++];
        return true;
    }

    [[nodiscard]] u64 seed() const noexcept { return m_seed; }
    [[nodiscard]] bool is_recording() const noexcept { return m_recording; }
    [[nodiscard]] bool is_replaying() const noexcept { return m_replaying; }

private:
    u64 m_seed = 0;
    bool m_recording = false;
    bool m_replaying = false;
    std::size_t m_replay_cursor = 0;
    std::vector<FrameInput> m_frames;
};

} // namespace vg::game
