#version 450#extension GL_ARB_separate_shader_objects : enable#define N_SPOTLIGHTS 3layout(location = 0) in vec3 fragPos;layout(location = 1) in vec3 fragNorm;layout(location = 2) in vec2 fragUV;layout(location = 0) out vec4 outColor;struct SpotLight {    float beta;   // decay exponent of the spotlight    float g;      // target distance of the spotlight    float cosout; // cosine of the outer angle of the spotlight    float cosin;  // cosine of the inner angle of the spotlight    vec3 lightPos;    vec3 lightDir;    vec4 lightColor;};layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {    vec3 DlightDir;     // direction of the direct light    vec3 DlightColor;   // color of the direct light    vec3 AmbLightColor; // ambient light    vec3 eyePos;        // position of the viewer    SpotLight lights[N_SPOTLIGHTS];} gubo;layout(set = 1, binding = 0) uniform UniformBufferObject {    float amb;    float gamma;    vec3 sColor;    mat4 mvpMat;    mat4 mMat;    mat4 nMat;} ubo;layout(set = 1, binding = 1) uniform sampler2D tex;void main() {    vec3 N = normalize(fragNorm);              // surface normal    vec3 EyeDir = normalize(gubo.eyePos - fragPos); // viewer direction    vec3 albedo = texture(tex, fragUV).rgb;    // main color    vec3 MD = albedo*0.95f;    vec3 MS = ubo.sColor;    vec3 MA = albedo * ubo.amb;    vec3 LA = gubo.AmbLightColor;    // Lambert    vec3 f_diffuse_DIRECT = MD * max(dot(gubo.DlightDir, N), 0.0f);    // Blinn    vec3 f_specular_DIRECT = MS * pow(clamp(dot(N, normalize(gubo.DlightDir + EyeDir)), 0.0f, 1.0f), ubo.gamma);    vec3 BRDF_DIRECT = f_diffuse_DIRECT + f_specular_DIRECT;    vec3 sl = vec3(0.0f, 0.0f, 0.0f);    for (int i = 0; i < N_SPOTLIGHTS; i++) {        vec3 lightDir = normalize(gubo.lights[i].lightPos - fragPos);        // Lambert        vec3 f_diffuse = MD * max(dot(lightDir, N), 0.0f);        // Blinn        vec3 f_specular = MS * pow(clamp(dot(N, normalize(lightDir + EyeDir)), 0.0f, 1.0f), ubo.gamma);        sl = sl + (f_diffuse + f_specular) * (gubo.lights[i].lightColor.rgb *                  (pow(gubo.lights[i].g / length(gubo.lights[i].lightPos - fragPos), gubo.lights[i].beta)) *                  clamp(((dot(normalize(gubo.lights[i].lightPos - fragPos), gubo.lights[i].lightDir)) - gubo.lights[i].cosout) /                         (gubo.lights[i].cosin - gubo.lights[i].cosout), 0.0f, 1.0f));    }    outColor = vec4(clamp(BRDF_DIRECT*LA + sl + MA, 0.0f, 1.0f), 1.0f);}