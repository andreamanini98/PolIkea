#version 450

layout (location = 0) out vec2 outUV;

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inUV;

void main()
{
    gl_Position = vec4(inPosition, 0.5f, 1.0f);
    outUV = inUV;
}