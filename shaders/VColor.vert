#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform UniformBufferObject {
	float amb;
	float gamma;
	vec3 sColor;
	mat4 prjViewMat;
} ubo;

layout(set = 1, binding = 1) uniform UniformWorldBufferObject {
	mat4 worldMat;
} uboWorld;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec3 inColor;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec3 fragColor;

void main() {
	gl_Position = ubo.prjViewMat * uboWorld.worldMat * vec4(inPosition, 1.0);
	fragPos = (uboWorld.worldMat * vec4(inPosition, 1.0)).xyz;
	fragNorm = (inverse(uboWorld.worldMat) * vec4(inNorm, 0.0)).xyz;
	fragColor = inColor;
}