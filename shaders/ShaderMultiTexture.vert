#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform UniformBufferObject {
	float amb;
	float gamma;
	vec3 sColor;
	mat4 mvpMat;
	mat4 worldMat;
	mat4 nMat;
	mat4 shadowDepthBiasMVP;
} ubo;

layout (binding = 2) uniform sampler2D shadowMap;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;
layout(location = 3) in uint inFragTextureID;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec2 outUV;
layout(location = 3) out uint outFragTextureID;
layout (location = 4) out vec4 outShadowCoord;

const mat4 biasMat = mat4(
	0.5, 0.0, 0.0, 0.0,
	0.0, 0.5, 0.0, 0.0,
	0.0, 0.0, 1.0, 0.0,
	0.5, 0.5, 0.0, 1.0 );

void main() {
	gl_Position = ubo.mvpMat * vec4(inPosition, 1.0);
	fragPos = (ubo.worldMat * vec4(inPosition, 1.0)).xyz;
	fragNorm = (ubo.nMat * vec4(inNorm, 0.0)).xyz;
	outUV = inUV;
	outFragTextureID = inFragTextureID;

	outShadowCoord = ( biasMat * ubo.shadowDepthBiasMVP ) * ubo.worldMat * vec4(inPosition, 1.0);
}