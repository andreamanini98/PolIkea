#version 450
#extension GL_ARB_separate_shader_objects : enable

#define N_ROOMS 6

layout(set = 1, binding = 0) uniform UniformBufferObject {
	float amb;
	float gamma;
	vec3 sColor;
	mat4 mvpMat;
	mat4 mMat;
	mat4 nMat;
	float rot[N_ROOMS-1];
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 instancePos;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec2 outUV;

void main() {
	gl_Position = ubo.mvpMat * vec4(inPosition + instancePos, 1.0);
	fragPos = (ubo.mMat * vec4(inPosition + instancePos, 1.0)).xyz;
	fragNorm = (ubo.nMat * vec4(inNorm, 0.0)).xyz;
	outUV = inUV;
}