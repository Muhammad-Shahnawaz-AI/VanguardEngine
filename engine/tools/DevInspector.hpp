#pragma once
#include "../core/Types.hpp"
#include "../core/RingBuffer.hpp"
#include "../ecs/Registry.hpp"
#include <imgui.h>
#include <implot.h>
#include <functional>
#include <vector>

namespace vg {

struct EngineTelemetry {
    f32 sim_ms = 0.0f;
    u32 entity_count = 0;
    u32 broadphase_pairs = 0;
    u32 narrowphase_hits = 0;
    std::size_t arena_used_bytes = 0;
    std::size_t arena_peak_bytes = 0;
    std::size_t arena_cap_bytes = 1;
};

struct FrameTimelineEntry {
    u64 tick = 0;
    u8 phase = 0;
    bool buffered_input_fired = false;
    char move_label[24]{};
};

struct DevInspectorState {
    bool visible = true;
    bool show_hitboxes = true;
    bool show_spatial_grid = false;
    bool paused = false;
    bool step_one_frame = false;
    f32 time_scale = 1.0f;

    bool is_recording = false;
    bool is_replaying = false;
    char replay_path[128] = "combat_session.rep";

    Entity selected_entity = NULL_ENTITY;
    RingBuffer<FrameTimelineEntry, 90> frame_timeline;
    RingBuffer<f32, 180> sim_ms_history;
    RingBuffer<f32, 180> render_ms_history;

    std::function<void()> on_reset_encounter;
    std::function<void()> on_start_record;
    std::function<void()> on_stop_record;
    std::function<void()> on_start_replay;
    std::function<void(Registry&, Entity)> custom_entity_inspector;
};

class DevInspector {
public:
    static void render_ui(DevInspectorState& st, Registry& reg, const EngineTelemetry& tel) {
        if (!st.visible) return;

        ImGui::Begin("VanguardEngine DevTools (F1 / ~)");

        // Simulation & Deterministic Replay Controls (FR-7.4)
        if (ImGui::CollapsingHeader("Simulation & Replay Controls", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::Button(st.paused ? "Resume (60Hz)" : "Pause")) st.paused = !st.paused;
            ImGui::SameLine();
            if (ImGui::Button("Step +1 Frame")) {
                st.paused = true;
                st.step_one_frame = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Reset Encounter") && st.on_reset_encounter) st.on_reset_encounter();

            ImGui::SliderFloat("Time Scale", &st.time_scale, 0.1f, 2.0f, "%.2fx");
            ImGui::Checkbox("Show Hitboxes / Hurtboxes", &st.show_hitboxes);
            ImGui::SameLine();
            ImGui::Checkbox("Show Spatial Hash Grid", &st.show_spatial_grid);

            ImGui::InputText("Replay File", st.replay_path, sizeof(st.replay_path));
            if (!st.is_recording) {
                if (ImGui::Button("Record .rep") && st.on_start_record) st.on_start_record();
            } else {
                if (ImGui::Button("Stop & Save .rep") && st.on_stop_record) st.on_stop_record();
            }
            ImGui::SameLine();
            if (ImGui::Button("Play .rep") && st.on_start_replay) st.on_start_replay();
        }

        // Fighting-Game Frame-Data Timeline (FR-7.2)
        if (ImGui::CollapsingHeader("Live Frame-Data Timeline (Last 90 Ticks)", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p0 = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            const float h = 28.0f;
            dl->AddRectFilled(p0, ImVec2(p0.x + w, p0.y + h), IM_COL32(25, 28, 34, 255));

            const std::size_t n = st.frame_timeline.size();
            const float cell_w = w / 90.0f;
            for (std::size_t i = 0; i < n; ++i) {
                const auto& e = st.frame_timeline[i];
                ImU32 col = IM_COL32(60, 65, 75, 255); // Neutral
                if (e.phase == 1) col = IM_COL32(245, 180, 40, 255);  // Startup (Amber)
                if (e.phase == 2) col = IM_COL32(235, 55, 55, 255);   // Active (Red)
                if (e.phase == 3) col = IM_COL32(65, 145, 245, 255);  // Recovery (Blue)
                if (e.phase == 4) col = IM_COL32(70, 220, 240, 255);  // Dodge I-Frames
                if (e.phase == 5) col = IM_COL32(250, 240, 60, 255);  // Parry Window

                const float x0 = p0.x + static_cast<float>(i) * cell_w;
                dl->AddRectFilled(ImVec2(x0, p0.y + 2.0f), ImVec2(x0 + cell_w - 1.0f, p0.y + h - 2.0f), col);
                if (e.buffered_input_fired) {
                    dl->AddCircleFilled(ImVec2(x0 + cell_w * 0.5f, p0.y + h * 0.5f), 3.0f, IM_COL32(255, 255, 255, 255));
                }
            }
            ImGui::Dummy(ImVec2(w, h + 4.0f));
            if (n > 0) {
                ImGui::Text("Current Move: %s | Legend: [Amber=Startup] [Red=Active] [Blue=Recovery] [Yellow=Parry]",
                            st.frame_timeline[n - 1].move_label);
            }
        }

        // Telemetry & ImPlot Profiler (FR-7.5)
        if (ImGui::CollapsingHeader("Engine Telemetry & Profiler", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("Entities Alive: %u | Broadphase Pairs: %u | Narrowphase Hits: %u",
                        tel.entity_count, tel.broadphase_pairs, tel.narrowphase_hits);
            ImGui::Text("FrameArena Peak: %.2f KB / %.2f KB",
                        static_cast<f32>(tel.arena_peak_bytes) / 1024.0f,
                        static_cast<f32>(tel.arena_cap_bytes) / 1024.0f);

            if (ImPlot::BeginPlot("Tick & Render Frame Time (ms)", ImVec2(-1, 140))) {
                ImPlot::SetupAxes("Frames", "ms", ImPlotAxisFlags_AutoFit, ImPlotAxisFlags_AutoFit);
                std::vector<f32> sim_v(st.sim_ms_history.size());
                for (std::size_t i = 0; i < sim_v.size(); ++i) sim_v[i] = st.sim_ms_history[i];
                if (!sim_v.empty()) {
                    ImPlot::PlotLine("Sim Tick (ms)", sim_v.data(), static_cast<int>(sim_v.size()));
                }
                ImPlot::EndPlot();
            }
        }

        // ECS Entity Inspector (FR-7.3)
        if (ImGui::CollapsingHeader("ECS Entity Inspector (Click entity in world)", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (reg.is_alive(st.selected_entity)) {
                ImGui::Text("Handle: Index=%u, Version=%u", st.selected_entity.index(), st.selected_entity.version());
                if (st.custom_entity_inspector) {
                    st.custom_entity_inspector(reg, st.selected_entity);
                }
            } else {
                ImGui::TextDisabled("No entity selected. Left-click any combatant in viewport.");
            }
        }

        ImGui::End();
    }
};

} // namespace vg
