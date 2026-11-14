#pragma once
#include "../core/Types.hpp"
#include <glad/gl.h>
#include <glm/glm.hpp>
#include <vector>
#include <filesystem>

namespace vg {

struct QuadInstance {
    glm::vec2 pos{0.0f};
    glm::vec2 size{1.0f};
    f32 rotation_rad = 0.0f;
    glm::vec4 color{1.0f};
    f32 hit_flash = 0.0f;
    f32 z_order = 0.0f;
};

// OpenGL 4.5 Instanced Batch Renderer with Shader Hot-Reloading (FR-3.1, FR-3.3)
class BatchRenderer {
public:
    static constexpr std::size_t MAX_INSTANCES = 8192;

    bool init(const std::filesystem::path& shader_dir);
    void shutdown();
    void poll_hot_reload();

    void begin_frame(const glm::mat4& view_proj);
    void draw_quad(const QuadInstance& quad);
    void draw_wire_box(const glm::vec2& center, const glm::vec2& half_extents,
                       f32 rotation_rad, const glm::vec4& color, f32 line_thickness = 0.04f);
    void draw_wire_circle(const glm::vec2& center, f32 radius,
                          const glm::vec4& color, u32 segments = 24, f32 line_thickness = 0.04f);
    void end_frame();

private:
    bool compile_shaders();

    std::filesystem::path m_shader_dir;
    std::filesystem::file_time_type m_vert_time{};
    std::filesystem::file_time_type m_frag_time{};

    GLuint m_program = 0;
    GLuint m_vao = 0;
    GLuint m_quad_vbo = 0;
    GLuint m_instance_vbo = 0;
    GLint  m_view_proj_loc = -1;

    glm::mat4 m_view_proj{1.0f};
    std::vector<QuadInstance> m_instances;
};

} // namespace vg
