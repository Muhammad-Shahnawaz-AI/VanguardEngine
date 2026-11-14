#include "BatchRenderer.hpp"
#include "../core/Logger.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

namespace vg {

namespace {
struct GpuInstanceData {
    glm::vec2 pos;
    glm::vec2 size;
    f32 rotation;
    glm::vec4 color;
    f32 hit_flash;
};

std::string read_text_file(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs.is_open()) return {};
    std::ostringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}
} // namespace

bool BatchRenderer::init(const std::filesystem::path& shader_dir) {
    m_shader_dir = shader_dir;
    m_instances.reserve(MAX_INSTANCES);

    if (!compile_shaders()) return false;

    constexpr f32 quad_verts[] = {
        -0.5f, -0.5f,
         0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f, -0.5f,
         0.5f,  0.5f,
        -0.5f,  0.5f
    };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_quad_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_quad_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_verts), quad_verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32), nullptr);

    glGenBuffers(1, &m_instance_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    glBufferData(GL_ARRAY_BUFFER, MAX_INSTANCES * sizeof(GpuInstanceData), nullptr, GL_DYNAMIC_DRAW);

    const GLsizei stride = sizeof(GpuInstanceData);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(GpuInstanceData, pos)));
    glVertexAttribDivisor(1, 1);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(GpuInstanceData, size)));
    glVertexAttribDivisor(2, 1);

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(GpuInstanceData, rotation)));
    glVertexAttribDivisor(3, 1);

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(GpuInstanceData, color)));
    glVertexAttribDivisor(4, 1);

    glEnableVertexAttribArray(5);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(offsetof(GpuInstanceData, hit_flash)));
    glVertexAttribDivisor(5, 1);

    glBindVertexArray(0);
    return true;
}

void BatchRenderer::shutdown() {
    if (m_instance_vbo) glDeleteBuffers(1, &m_instance_vbo);
    if (m_quad_vbo) glDeleteBuffers(1, &m_quad_vbo);
    if (m_vao) glDeleteVertexArrays(1, &m_vao);
    if (m_program) glDeleteProgram(m_program);
}

bool BatchRenderer::compile_shaders() {
    const auto vp = m_shader_dir / "default.vert";
    const auto fp = m_shader_dir / "default.frag";
    const std::string vsrc = read_text_file(vp);
    const std::string fsrc = read_text_file(fp);
    if (vsrc.empty() || fsrc.empty()) return false;

    auto compile_stage = [](GLenum type, const std::string& src) -> GLuint {
        GLuint sh = glCreateShader(type);
        const char* c = src.c_str();
        glShaderSource(sh, 1, &c, nullptr);
        glCompileShader(sh);
        GLint ok = 0;
        glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[512];
            glGetShaderInfoLog(sh, sizeof(log), nullptr, log);
            log_msg(LogLevel::Error, "Shader", log);
            glDeleteShader(sh);
            return 0;
        }
        return sh;
    };

    GLuint vs = compile_stage(GL_VERTEX_SHADER, vsrc);
    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, fsrc);
    if (!vs || !fs) return false;

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    GLint linked = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(prog);
        return false;
    }

    if (m_program) glDeleteProgram(m_program);
    m_program = prog;
    m_view_proj_loc = glGetUniformLocation(m_program, "u_ViewProj");

    std::error_code ec;
    m_vert_time = std::filesystem::last_write_time(vp, ec);
    m_frag_time = std::filesystem::last_write_time(fp, ec);
    return true;
}

void BatchRenderer::poll_hot_reload() {
    std::error_code ec;
    const auto vt = std::filesystem::last_write_time(m_shader_dir / "default.vert", ec);
    const auto ft = std::filesystem::last_write_time(m_shader_dir / "default.frag", ec);
    if (!ec && (vt != m_vert_time || ft != m_frag_time)) {
        if (compile_shaders()) {
            log_msg(LogLevel::Info, "Shader", "Hot-reloaded GLSL shaders");
        }
    }
}

void BatchRenderer::begin_frame(const glm::mat4& view_proj) {
    m_view_proj = view_proj;
    m_instances.clear();
}

void BatchRenderer::draw_quad(const QuadInstance& quad) {
    if (m_instances.size() < MAX_INSTANCES) {
        m_instances.push_back(quad);
    }
}

void BatchRenderer::draw_wire_box(const glm::vec2& center, const glm::vec2& half_extents,
                                  f32 rotation_rad, const glm::vec4& color, f32 line_thickness) {
    const f32 c = std::cos(rotation_rad);
    const f32 s = std::sin(rotation_rad);
    auto rot = [&](const glm::vec2& v) -> glm::vec2 {
        return center + glm::vec2{ v.x * c - v.y * s, v.x * s + v.y * c };
    };
    const f32 w = half_extents.x * 2.0f;
    const f32 h = half_extents.y * 2.0f;
    draw_quad({ rot({ 0.0f,  half_extents.y}), {w, line_thickness}, rotation_rad, color, 0.0f, 8.0f });
    draw_quad({ rot({ 0.0f, -half_extents.y}), {w, line_thickness}, rotation_rad, color, 0.0f, 8.0f });
    draw_quad({ rot({-half_extents.x, 0.0f}),  {line_thickness, h}, rotation_rad, color, 0.0f, 8.0f });
    draw_quad({ rot({ half_extents.x, 0.0f}),  {line_thickness, h}, rotation_rad, color, 0.0f, 8.0f });
}

void BatchRenderer::draw_wire_circle(const glm::vec2& center, f32 radius,
                                     const glm::vec4& color, u32 segments, f32 line_thickness) {
    const f32 step = 6.2831853f / static_cast<f32>(segments);
    for (u32 i = 0; i < segments; ++i) {
        const f32 a0 = step * static_cast<f32>(i);
        const f32 a1 = step * static_cast<f32>(i + 1);
        const glm::vec2 p0 = center + glm::vec2{std::cos(a0), std::sin(a0)} * radius;
        const glm::vec2 p1 = center + glm::vec2{std::cos(a1), std::sin(a1)} * radius;
        const glm::vec2 mid = (p0 + p1) * 0.5f;
        const glm::vec2 d = p1 - p0;
        draw_quad({ mid, {glm::length(d), line_thickness}, std::atan2(d.y, d.x), color, 0.0f, 8.0f });
    }
}

void BatchRenderer::end_frame() {
    if (m_instances.empty()) return;

    std::sort(m_instances.begin(), m_instances.end(),
              [](const QuadInstance& a, const QuadInstance& b) { return a.z_order < b.z_order; });

    static std::vector<GpuInstanceData> gpu_buf;
    gpu_buf.resize(m_instances.size());
    for (std::size_t i = 0; i < m_instances.size(); ++i) {
        gpu_buf[i] = {
            m_instances[i].pos,
            m_instances[i].size,
            m_instances[i].rotation_rad,
            m_instances[i].color,
            m_instances[i].hit_flash
        };
    }

    glUseProgram(m_program);
    glUniformMatrix4fv(m_view_proj_loc, 1, GL_FALSE, glm::value_ptr(m_view_proj));

    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_instance_vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(gpu_buf.size() * sizeof(GpuInstanceData)),
                    gpu_buf.data());

    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, static_cast<GLsizei>(gpu_buf.size()));
    glBindVertexArray(0);
}

} // namespace vg
