#version 460

#define PI 3.14159265358979323846

layout(location = 0) in vec4 pos;
layout(location = 1) in vec4 color;

layout(location = 2) uniform mat4 viewProj;

out vec4 fColor;

void main() {
  gl_Position = viewProj * pos;
  fColor = color;
}