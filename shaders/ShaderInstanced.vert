#version 450
#extension GL_ARB_separate_shader_objects : enable

#define N_ROOMS 6

layout(std140, set = 1, binding = 0) uniform UniformBufferObject {
	float amb;
	float gamma;
	vec3 sColor;
	mat4 prjViewMat;
    vec4 door[N_ROOMS-1];
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNorm;
layout(location = 2) in vec2 inUV;
layout(location = 3) in float instanceRot;
layout(location = 4) in vec3 shift;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragNorm;
layout(location = 2) out vec2 outUV;

mat4 rotationMatrix(vec3 axis, float angle)
{
	axis = normalize(axis);
	float s = sin(angle);
	float c = cos(angle);
	float oc = 1.0 - c;

	return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
				oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
				oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
				0.0,                                0.0,                                0.0,                                1.0);
}

mat4 shiftMatFun(vec3 shift)
{
	return mat4(1.0, 0.0, 0.0, shift.x,
				0.0, 1.0, 0.0, shift.y,
				0.0, 0.0, 1.0, shift.z,
				0.0, 0.0, 0.0, 1.0);
}

void main() {
	mat4 shiftMat = shiftMatFun(shift);
	mat4 rotation = rotationMatrix(vec3(0.0, 1.0, 0.0), instanceRot + ubo.door[gl_InstanceIndex].x);
	mat4 worldMat = shiftMat * rotation;

	gl_Position = ubo.prjViewMat * worldMat * vec4(inPosition,1.0);
	fragPos = (worldMat * vec4(inPosition,1.0)).xyz;
	fragNorm = (inverse(worldMat) * vec4(inNorm, 0.0)).xyz;

	outUV = inUV;
}