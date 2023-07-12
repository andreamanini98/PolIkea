#version 450

layout (location = 0) in vec3 inPos;

layout (set = 0, binding = 0) uniform UBO
{
    mat4 prjViewMat;
} ubo;

layout (set = 1, binding = 0) uniform UBOWorld
{
    mat4 worldMat;
} uboWorld;

out gl_PerVertex
{
    vec4 gl_Position;
};

void main()
{
    gl_Position =  ubo.prjViewMat * uboWorld.worldMat * vec4(inPos, 1.0);
}