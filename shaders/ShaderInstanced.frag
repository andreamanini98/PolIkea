#version 450#extension GL_ARB_separate_shader_objects : enable#define N_SPOTLIGHTS 50#define N_POINTLIGHTS 50#define N_ROOMS 5layout(location = 0) in vec3 fragPos;layout(location = 1) in vec3 fragNorm;layout(location = 2) in vec2 fragUV;layout(location = 0) out vec4 outColor;struct SpotLight {    float beta;   // decay exponent of the spotlight    float g;      // target distance of the spotlight    float cosout; // cosine of the outer angle of the spotlight    float cosin;  // cosine of the inner angle of the spotlight    vec3 lightPos;    vec3 lightDir;    vec4 lightColor;};struct PointLight {    float beta;   // decay exponent of the spotlight    float g;      // target distance of the spotlight    vec3 lightPos;    vec4 lightColor;};struct HouseBindings {    vec3 bottomLeft;    vec3 topRight;};layout(set = 0, binding = 0) uniform GlobalUniformBufferObject {    vec3 DlightDir;     // direction of the direct light    vec3 DlightColor;   // color of the direct light    vec3 AmbLightColor; // ambient light    vec3 eyePos;        // position of the viewer    SpotLight spotLights[N_SPOTLIGHTS];    PointLight pointLights[N_POINTLIGHTS];    int nSpotLights;    int nPointLights;} gubo;layout(set = 1, binding = 0) uniform UniformBufferObjectHouseBindings {    HouseBindings houseBindings[N_ROOMS];    HouseBindings externPolikeaBoundings[4];} uboHouse;layout(set = 2, binding = 0) uniform UniformBufferObject {    float amb;    float gamma;    vec3 sColor;    mat4 mvpMat;    mat4 mMat;    mat4 nMat;} ubo;layout(set = 2, binding = 1) uniform sampler2D tex;bool checkIfAmbient(vec3 pos) {    return (pos.x >= -4.55f && pos.x <= 14.55f && pos.z <= -15.5f && pos.z >= -34.5f && pos.y >= 0.5f && pos.y <= 8.0f);}bool isInHome(vec3 pos) {    bool inHome = false;    for (int i = 0; i < N_ROOMS; i++) {        inHome = inHome || (pos.x >= uboHouse.houseBindings[i].bottomLeft.x && pos.x <= uboHouse.houseBindings[i].topRight.x &&                            pos.z <= uboHouse.houseBindings[i].bottomLeft.z && pos.z >= uboHouse.houseBindings[i].topRight.z);    }    return inHome;}vec3 OrenNayarBRDF(vec3 V, vec3 N, vec3 L, vec3 Md, float sigma) {    //vec3 V  - direction of the viewer == omega_r    //vec3 N  - normal vector to the surface    //vec3 L  - light vector (from the light model)    //vec3 Md - main color of the surface    //float sigma - Roughness of the model    float tetha_i = acos(dot(L, N));    float tetha_r = acos(dot(V, N));    float alpha = max(tetha_i, tetha_r);    float beta = min(tetha_i, tetha_r);    float A = 1 - 0.5 * (pow(sigma, 2) / (pow(sigma, 2) + 0.33));    float B = 0.45 * (pow(sigma, 2) / (pow(sigma, 2) + 0.09));    vec3 v_i = normalize(L - dot(L, N) * N);    vec3 v_r = normalize(V - dot(V, N) * N);    float G = max(0, dot(v_i, v_r));    vec3 Ll = Md * clamp(dot(L, N), 0.0, 1.0);    vec3 f_diffuse = Ll * (A + B * G * sin(alpha) * tan(beta));    return f_diffuse;}void main() {    vec3 N = normalize(fragNorm);              // surface normal    vec3 EyeDir = normalize(gubo.eyePos - fragPos); // viewer direction    vec3 albedo = texture(tex, fragUV).rgb;    // main color    vec3 MD = albedo*0.95f;    vec3 MS = ubo.sColor;    vec3 MA = albedo * ubo.amb;    vec3 LA = gubo.AmbLightColor;    float attenuationFactor = checkIfAmbient(fragPos) ? 0.0f : 1.0f;    float homeAttenuationFactor = isInHome(fragPos) ? 0.0f : 1.0f;    // Lambert    //vec3 f_diffuse_DIRECT = MD * max(dot(gubo.DlightDir, N), 0.0f);    // Blinn    //vec3 f_specular_DIRECT = MS * pow(clamp(dot(N, normalize(gubo.DlightDir + EyeDir)), 0.0f, 1.0f), ubo.gamma);    //vec3 BRDF_DIRECT = f_diffuse_DIRECT + f_specular_DIRECT;    vec3 DiffSpeco = OrenNayarBRDF(EyeDir, N, gubo.DlightDir, albedo, 1.1f);    vec3 sl = vec3(0.0f, 0.0f, 0.0f);    for (int i = 0; i < gubo.nSpotLights; i++) {        vec3 spotLightDir = normalize(gubo.spotLights[i].lightPos - fragPos);        vec3 DiffSpec = OrenNayarBRDF(EyeDir, N, spotLightDir, albedo, 1.1f);        // Lambert        //vec3 f_diffuse_SPOT = MD * max(dot(spotLightDir, N), 0.0f);        // Blinn        //vec3 f_specular_SPOT = MS * pow(clamp(dot(N, normalize(spotLightDir + EyeDir)), 0.0f, 1.0f), ubo.gamma);        //BRDF * SPOTLIGHT_LIGHT_MODEL        sl = sl + DiffSpec * (gubo.spotLights[i].lightColor.rgb *                  (pow(gubo.spotLights[i].g / length(gubo.spotLights[i].lightPos - fragPos), gubo.spotLights[i].beta)) *                  clamp(((dot(normalize(gubo.spotLights[i].lightPos - fragPos), gubo.spotLights[i].lightDir)) - gubo.spotLights[i].cosout) /                         (gubo.spotLights[i].cosin - gubo.spotLights[i].cosout), 0.0f, 1.0f));    }    vec3 pl = vec3(0.0f, 0.0f, 0.0f);    for (int i = 0; i < gubo.nPointLights; i++) {        vec3 pointLightDir = normalize(gubo.pointLights[i].lightPos - fragPos);        vec3 DiffSpec = OrenNayarBRDF(EyeDir, N, pointLightDir, albedo, 1.1f);        // Lambert        //vec3 f_diffuse_POINT = MD * max(dot(pointLightDir, N), 0.0f);        // Blinn        //vec3 f_specular_POINT = MS * pow(clamp(dot(N, normalize(pointLightDir + EyeDir)), 0.0f, 1.0f), ubo.gamma);        //BRDF * POINTLIGHT_LIGHT_MODEL        pl = pl + DiffSpec *                  (gubo.pointLights[i].lightColor.rgb *                  (pow(gubo.pointLights[i].g / length(gubo.pointLights[i].lightPos - fragPos), gubo.pointLights[i].beta)));    }    outColor = vec4(clamp(DiffSpeco*LA*attenuationFactor*homeAttenuationFactor + sl + pl + MA, 0.0f, 1.0f), 1.0f);}