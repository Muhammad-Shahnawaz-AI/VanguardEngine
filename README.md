# VanguardEngine

[![Direct Download Windows Installer (.exe)](https://img.shields.io/badge/Direct%20Download-VanguardEngine--Setup--x64.exe-0078D6?style=for-the-badge&logo=windows&logoColor=white)](https://github.com/Muhammad-Shahnawaz-AI/VanguardEngine/releases/latest/download/VanguardEngine-Setup-x64.exe)
[![All Releases](https://img.shields.io/github/v/release/Muhammad-Shahnawaz-AI/VanguardEngine?style=for-the-badge&label=Latest%20Release)](https://github.com/Muhammad-Shahnawaz-AI/VanguardEngine/releases/latest)

**VanguardEngine** is a deterministic, data-oriented C++20 2D action-combat engine built from scratch around cache-friendly memory layouts, fixed-timestep simulation, and real-time frame-data debugging.

---

## Direct Install (Windows 10 / 11)

1. **[Click here to download `VanguardEngine-Setup-x64.exe` directly](https://github.com/Muhammad-Shahnawaz-AI/VanguardEngine/releases/latest/download/VanguardEngine-Setup-x64.exe)** *(starts downloading immediately)*.
2. Double-click **`VanguardEngine-Setup-x64.exe`** to run the installation wizard (creates Start Menu and optional Desktop shortcuts).
3. Click **Launch VanguardEngine** at the end of setup—or run it anytime from your Desktop or Start Menu.

> **Prefer a portable folder without installation?** Download [`VanguardEngine-Windows-x64.zip`](https://github.com/Muhammad-Shahnawaz-AI/VanguardEngine/releases/latest/download/VanguardEngine-Windows-x64.zip), extract it anywhere, and run `VanguardGame.exe` (keep the `assets/` folder in the same directory).

---

## Controls

| Action | Keyboard / Mouse |
| :--- | :--- |
| **Move** | `W` `A` `S` `D` or `Arrow Keys` |
| **Light Attack** | `J` or `Left Mouse Button` |
| **Heavy Attack** | `K` or `Right Mouse Button` |
| **Dash / Dodge (i-Frames)** | `Space` or `Left Shift` |
| **Toggle DevTools Inspector** | `F1` or `~` |

---

## Engine Architecture & Key Systems

- **Generational Sparse-Set ECS (`engine/ecs/`):** $O(1)$ entity creation, destruction, and component lookup with 32-bit versioned handles to eliminate dangling references, contiguous dense arrays for cache-line iteration, and smallest-pool multi-component views.
- **Deterministic Fixed-Timestep Simulation (`engine/core/`):** Decoupled 60 Hz (`16.666 ms`) logic accumulator with frame-arena linear allocators (`FrameArena`) for zero heap churn during gameplay ticks.
- **Oriented Bounding Box (OBB) Physics & Spatial Hash (`engine/physics/`):** Separating Axis Theorem (SAT) collision detection supporting arbitrary entity rotation, minimum translation vector (MTV) resolution, collision layer bitmasks, and a uniform spatial hash broadphase.
- **Data-Driven Combat DAG & Input Buffer (`game/combat/`):** Circular 250 ms (`15`-tick) input buffer with edge-triggered buffering, startup/active/recovery/cancel-window frame state machines, hitstop freeze frames, and hot-reloadable JSON weapon move graphs (`assets/weapons/`).
- **Instanced OpenGL 4.5 Batch Renderer (`engine/render/`):** Single-draw-call instanced quad submission with hardware attribute divisors and camera shake trauma.
- **Integrated Dear ImGui & ImPlot DevTools (`engine/devtools/`):** Live hitbox/hurtbox wireframe overlays, real-time frame-time telemetry plots, frame-stepping (`Pause` / `Step 1 Tick`), and live weapon JSON hot-reload.

---

## Building from Source (Linux / Windows / macOS)

### Prerequisites
- CMake `3.24+`
- C++20 compiler (`GCC 12+`, `Clang 15+`, or `MSVC 2022`)
- OpenGL 4.5 capable GPU/driver *(on Ubuntu/Debian, install X11/Wayland/GL dev headers: `sudo apt-get install -y libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxi-dev libxfixes-dev libxss-dev libwayland-dev libxkbcommon-dev libgl1-mesa-dev libasound2-dev`)*

### Compile & Run Tests
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DVANGUARD_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure