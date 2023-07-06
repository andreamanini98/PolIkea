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

void main() {
		float rot = ubo.rot[gl_InstanceIndex];
		mat4 rotation;
		if (rot == 3) {
			rotation = rotationMatrix(vec3(0.0, 1.0, 0.0), 1.5708);
		} else if (rot == 4) {
			rotation = rotationMatrix(vec3(0.0, 1.0, 0.0), 3.14159);
		} else if (rot == 1) {
			rotation = rotationMatrix(vec3(0.0, 1.0, 0.0), 4.71239);
		} else {
			rotation = rotationMatrix(vec3(0.0, 1.0, 0.0), 0.0);
		}

		vec3 rotatedPosition = (rotation * vec4(inPosition, 1.0)).xyz;
		vec3 rotatedNormal = (rotation * vec4(inNorm, 0.0)).xyz;

		gl_Position = ubo.mvpMat * vec4(rotatedPosition + instancePos, 1.0);
		fragPos = (ubo.mMat * vec4(rotatedPosition + instancePos, 1.0)).xyz;
		fragNorm = (ubo.nMat * vec4(rotatedNormal, 0.0)).xyz;
		outUV = inUV;
}