#version 410 core

uniform vec4 u_color;

out vec4 FragColor;

void main() {
    FragColor = u_color;
}