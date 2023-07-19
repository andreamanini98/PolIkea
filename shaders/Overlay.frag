#version 450#extension GL_ARB_separate_shader_objects : enablelayout(binding = 0) uniform UniformBufferObject {	int visible;	int overlayTex;} ubo;layout(location = 0) in vec2 fragUV;layout(location = 0) out vec4 outColor;layout(binding = 1) uniform sampler2D tex;layout(binding = 2) uniform sampler2D tex2;void main() {	if (ubo.overlayTex == 0) {		outColor = vec4(texture(tex, fragUV).rgb, 1.0f);	// output color	} else if (ubo.overlayTex == 1) {		outColor = vec4(texture(tex2, fragUV).rgb, 1.0f);	// output color	}}