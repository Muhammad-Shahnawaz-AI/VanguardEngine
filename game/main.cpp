#include "engine/core/Types.hpp"
#include "engine/render/BatchRenderer.hpp"
#include "engine/render/Camera2D.hpp"
#include "engine/audio/AudioEngine.hpp"
#include "engine/tools/DevInspector.hpp"
#include "combat/CombatWorld.hpp"

#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <imgui.h>
#include <implot.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>
#include <algorithm>

using namespace vg;
using namespace vg::game;

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow(
        "VanguardEngine - Data-Oriented C++20 Action Combat",
        1600, 900, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) return 1;

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_ctx);
    SDL_GL_SetSwapInterval(0);

    if (!gladLoadGL(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))) {
        std::fprintf(stderr, "Failed to load OpenGL 4.5 via GLAD\n");
        return 1;
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL3_InitForOpenGL(window, gl_ctx);
    ImGui_ImplOpenGL3_Init("#version 450 core");

    const std::filesystem::path assets_dir = ASSETS_DIR;

    AudioEngine audio;
    audio.init();

    BatchRenderer renderer;
    if (!renderer.init(assets_dir / "shaders")) {
        std::fprintf(stderr, "Failed to initialize BatchRenderer\n");
        return 1;
    }

    Camera2D camera;
    CombatWorld world;
    if (!world.init(assets_dir, &audio)) {
        std::fprintf(stderr, "Failed to initialize CombatWorld\n");
        return 1;
    }

    DevInspectorState inspector{};
    inspector.on_reset_encounter = [&]() { world.reset_encounter(0x1337BEEF42ULL); };
    inspector.on_start_record = [&]() {
        world.reset_encounter(0x1337BEEF42ULL);
        world.replay_stream().start_recording(0x1337BEEF42ULL);
        inspector.is_recording = true;
    };
    inspector.on_stop_record = [&]() {
        world.replay_stream().stop_and_save(inspector.replay_path);
        inspector.is_recording = false;
    };
    inspector.on_start_replay = [&]() {
        if (world.replay_stream().load_for_replay(inspector.replay_path)) {
            world.reset_encounter(world.replay_stream().seed());
            inspector.is_replaying = true;
        }
    };
    inspector.custom_entity_inspector = [](Registry& reg, Entity e) {
        if (auto* tf = reg.try_get<Transform>(e)) {
            ImGui::DragFloat2("Position", &tf->pos.x, 0.05f);
        }
        if (auto* hp = reg.try_get<HealthPosture>(e)) {
            ImGui::SliderFloat("HP", &hp->hp, 0.0f, hp->max_hp);
            ImGui::SliderFloat("Posture", &hp->posture, 0.0f, hp->max_posture);
            ImGui::Text("Boss Phase: %u", hp->boss_phase);
        }
        if (auto* fsm = reg.try_get<CombatState>(e)) {
            ImGui::Text("FSM Move: %s | Phase: %u (Tick %u)",
                        fsm->move_name, static_cast<u32>(fsm->phase), fsm->phase_tick);
        }
    };

    bool running = true;
    u64 prev_counter = SDL_GetPerformanceCounter();
    const f64 perf_freq = static_cast<f64>(SDL_GetPerformanceFrequency());
    f64 accumulator = 0.0;
    FrameInput current_input{};

    while (running) {
        const u64 now_counter = SDL_GetPerformanceCounter();
        f64 frame_dt = static_cast<f64>(now_counter - prev_counter) / perf_freq;
        prev_counter = now_counter;
        frame_dt = std::min(frame_dt, 0.25);

        current_input.action = InputAction::None;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            ImGui_ImplSDL3_ProcessEvent(&ev);
            if (ev.type == SDL_EVENT_QUIT) running = false;

            if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
                if (ev.key.scancode == SDL_SCANCODE_F1 || ev.key.scancode == SDL_SCANCODE_GRAVE) {
                    inspector.visible = !inspector.visible;
                } else if (ev.key.scancode == SDL_SCANCODE_SPACE) {
                    current_input.action = InputAction::Dodge;
                    world.buffer_player_action(InputAction::Dodge);
                } else if (ev.key.scancode == SDL_SCANCODE_E || ev.key.scancode == SDL_SCANCODE_LSHIFT) {
                    current_input.action = InputAction::Parry;
                    world.buffer_player_action(InputAction::Parry);
                } else if (ev.key.scancode == SDL_SCANCODE_Q) {
                    current_input.action = InputAction::Special;
                    world.buffer_player_action(InputAction::Special);
                }
            }

            if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !io.WantCaptureMouse) {
                if (ev.button.button == SDL_BUTTON_LEFT) {
                    current_input.action = InputAction::LightAttack;
                    world.buffer_player_action(InputAction::LightAttack);

                    int w, h;
                    SDL_GetWindowSize(window, &w, &h);
                    const glm::vec2 click_w = camera.screen_to_world({ev.button.x, ev.button.y},
                                                                     {static_cast<f32>(w), static_cast<f32>(h)});
                    world.registry().view<Transform>([&](Entity e, Transform& tf) {
                        if (glm::distance(click_w, tf.pos) <= tf.scale.x * 0.75f) {
                            inspector.selected_entity = e;
                        }
                    });
                } else if (ev.button.button == SDL_BUTTON_RIGHT) {
                    current_input.action = InputAction::HeavyAttack;
                    world.buffer_player_action(InputAction::HeavyAttack);
                }
            }
        }

        const bool* keys = SDL_GetKeyboardState(nullptr);
        glm::vec2 move{0.0f};
        if (keys[SDL_SCANCODE_W]) move.y += 1.0f;
        if (keys[SDL_SCANCODE_S]) move.y -= 1.0f;
        if (keys[SDL_SCANCODE_A]) move.x -= 1.0f;
        if (keys[SDL_SCANCODE_D]) move.x += 1.0f;
        current_input.move_dir = move;

        f32 mx, my;
        SDL_GetMouseState(&mx, &my);
        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        camera.aspect_ratio = static_cast<f32>(win_w) / static_cast<f32>(std::max(1, win_h));
        if (world.registry().is_alive(world.player())) {
            const glm::vec2 mouse_world = camera.screen_to_world({mx, my}, {static_cast<f32>(win_w), static_cast<f32>(win_h)});
            const glm::vec2 player_pos = world.registry().get<Transform>(world.player()).pos;
            if (glm::distance(mouse_world, player_pos) > 0.05f) {
                current_input.aim_dir = glm::normalize(mouse_world - player_pos);
            }
        }

        if (!inspector.paused) {
            accumulator += frame_dt * static_cast<f64>(inspector.time_scale);
            while (accumulator >= FIXED_DT) {
                world.step_tick(current_input, inspector);
                inspector.sim_ms_history.push(world.telemetry().sim_ms);
                accumulator -= FIXED_DT;
            }
        } else if (inspector.step_one_frame) {
            world.step_tick(current_input, inspector);
            inspector.sim_ms_history.push(world.telemetry().sim_ms);
            inspector.step_one_frame = false;
        }

        const u64 render_start_ns = SDL_GetTicksNS();
        renderer.poll_hot_reload();

        glViewport(0, 0, win_w, win_h);
        glClearColor(0.05f, 0.06f, 0.08f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        const f32 alpha = inspector.paused ? 1.0f : static_cast<f32>(accumulator / FIXED_DT);
        world.render(renderer, camera, alpha, inspector);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        DevInspector::render_ui(inspector, world.registry(), world.telemetry());
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(window);
        const f32 render_ms = static_cast<f32>(SDL_GetTicksNS() - render_start_ns) / 1'000'000.0f;
        inspector.render_ms_history.push(render_ms);
    }

    renderer.shutdown();
    audio.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
