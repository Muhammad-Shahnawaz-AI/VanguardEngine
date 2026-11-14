#pragma once
#include "../core/Types.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace vg {

class Camera2D {
public:
    glm::vec2 position{0.0f, 0.0f};
    f32 zoom = 1.0f;
    f32 aspect_ratio = 16.0f / 9.0f;
    f32 view_height = 18.0f;
    f32 trauma = 0.0f;

    void add_trauma(f32 amount) noexcept {
        trauma = std::clamp(trauma + amount, 0.0f, 1.0f);
    }

    void update(f32 dt, const glm::vec2& target_pos, const glm::vec2& target_vel, f32 encounter_spread) noexcept {
        const glm::vec2 look_ahead = target_vel * 0.14f;
        const glm::vec2 desired_pos = glm::clamp(target_pos + look_ahead, glm::vec2{-6.0f, -3.5f}, glm::vec2{6.0f, 3.5f});
        position = glm::mix(position, desired_pos, std::clamp(dt * 8.0f, 0.0f, 1.0f));

        const f32 target_zoom = std::clamp(1.15f - encounter_spread * 0.018f, 0.82f, 1.15f);
        zoom = glm::mix(zoom, target_zoom, std::clamp(dt * 3.5f, 0.0f, 1.0f));

        m_time += dt;
        trauma = std::max(0.0f, trauma - dt * 1.8f);
    }

    [[nodiscard]] glm::mat4 view_proj() const noexcept {
        const f32 half_h = (view_height * 0.5f) / zoom;
        const f32 half_w = half_h * aspect_ratio;

        const f32 shake = trauma * trauma * 0.45f;
        const glm::vec2 offset{
            shake * std::sin(m_time * 53.0f),
            shake * std::cos(m_time * 47.0f)
        };

        const glm::vec2 c = position + offset;
        return glm::ortho(c.x - half_w, c.x + half_w, c.y - half_h, c.y + half_h, -10.0f, 10.0f);
    }

    [[nodiscard]] glm::vec2 screen_to_world(const glm::vec2& screen_px, const glm::vec2& screen_size) const noexcept {
        const f32 ndc_x = (screen_px.x / std::max(1.0f, screen_size.x)) * 2.0f - 1.0f;
        const f32 ndc_y = 1.0f - (screen_px.y / std::max(1.0f, screen_size.y)) * 2.0f;
        const f32 half_h = (view_height * 0.5f) / zoom;
        const f32 half_w = half_h * aspect_ratio;
        return { position.x + ndc_x * half_w, position.y + ndc_y * half_h };
    }

private:
    f32 m_time = 0.0f;
};

} // namespace vg
