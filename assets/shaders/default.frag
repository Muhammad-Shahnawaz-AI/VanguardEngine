#version 450 core

in vec4 v_Color;
in float v_HitFlash;

out vec4 FragColor;

void main() {
    vec3 final_rgb = mix(v_Color.rgb, vec3(1.0, 1.0, 1.0), clamp(v_HitFlash, 0.0, 1.0));
    FragColor = vec4(final_rgb, v_Color.a);
}
