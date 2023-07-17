#version 450

layout (binding = 0) uniform sampler2D samplerColor;

layout (location = 0) in vec2 inUV;

layout (location = 0) out vec4 outFragColor;

float LinearizeDepth(float depth)
{
    float n = 0.1f;
    float f = 50.0f;
    float z = depth;
    return (2.0 * n) / (f + n - z * (f - n));
}

void main()
{
    float depth = texture(samplerColor, inUV).r;
    outFragColor = vec4(vec3(1.0-LinearizeDepth(depth)), 1.0);
}