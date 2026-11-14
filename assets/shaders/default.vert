#version 450 core

layout (location = 0) in vec2 a_LocalPos;
layout (location = 1) in vec2 i_WorldPos;
layout (location = 2) in vec2 i_Size;
layout (location = 3) in float i_Rotation;
layout (location = 4) in vec4 i_Color;
layout (location = 5) in float i_HitFlash;

uniform mat4 u_ViewProj;

out vec4 v_Color;
out float v_HitFlash;

void main() {
    vec2 scaled = a_LocalPos * i_Size;
    float c = cos(i_Rotation);
    float s = sin(i_Rotation);
    vec2 rotated = vec2(
        scaled.x * c - scaled.y * s,
        scaled.x * s + scaled.y * c
    );
    gl_Position = u_ViewProj * vec4(i_WorldPos + rotated, 0.0, 1.0);
    v_Color = i_Color;
    v_HitFlash = i_HitFlash;
}
