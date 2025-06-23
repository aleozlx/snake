#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
uniform vec2 u_offset;
uniform vec2 u_scale;
out vec2 texCoord;
out vec2 fragTexCoord;
void main() {
    texCoord = aPos;
    fragTexCoord = aTexCoord;
    vec2 pos = (aPos * u_scale) + u_offset;
    gl_Position = vec4(pos, 0.0, 1.0);
} 