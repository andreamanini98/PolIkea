//
// Created by Matteo Carrara on 22/07/23.
//

#ifndef VTEMPLATE_UNIFORMBUFFERS_H
#define VTEMPLATE_UNIFORMBUFFERS_H

#include <vector>
#include <glm/glm.hpp>
#include "Parameters.hpp"

//REMEMBER TO UPDATE THIS FIELD ALSO IN THE ShaderInstanced.vert
#define N_ROOMS 5
#define N_POS_LIGHTS (4 + N_ROOMS) // 4 lights for polikea + one light for each room
#define N_SPOTLIGHTS 50
#define N_POINTLIGHTS 50

struct ModelInstance {
    float baseRot;
    glm::vec3 pos;
};

struct SpotLight {
    alignas(4) float beta;   // decay exponent of the spotlight
    alignas(4) float g;      // target distance of the spotlight
    alignas(4) float cosout; // cosine of the outer angle of the spotlight
    alignas(4) float cosin;  // cosine of the inner angle of the spotlight
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
};

struct PointLight {
    alignas(4) float beta;   // decay exponent of the spotlight
    alignas(4) float g;      // target distance of the spotlight
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec4 lightColor;
};

// The uniform buffer objects data structures
// Remember to use the correct alignas(...) value
//     float : alignas(4)
//     vec2  : alignas(8)
//     vec3  : alignas(16)
//     vec4  : alignas(16)
//     mat3  : alignas(16)
//     mat4  : alignas(16)
struct UniformBlock {
    alignas(4) float amb;
    alignas(4) float gamma;
    alignas(16) glm::vec3 sColor;
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 worldMat;
    alignas(16) glm::mat4 nMat;

    //These factors are used to decide if the light is on or off
    alignas(4) float diffuseLight = 1.0f;
    alignas(4) float internalLightsFactor = 0.0f;
};


// Door requires (N_ROOMS - 1) + 2 instances
// Lights require N_ROOMS + 4 instances
// We take the max
#define MAXIMUM_INSTANCES_PER_BUFFER (N_ROOMS + 4)

struct UniformBlockInstance {
    alignas(4) float amb;
    alignas(4) float gamma;
    alignas(16) glm::vec3 sColor;
    alignas(16) glm::mat4 prjViewMat;
    alignas(16) glm::vec4 rotOffsetAndLights[MAXIMUM_INSTANCES_PER_BUFFER];
    alignas(4) float diffuseLight = 1.0f;
    alignas(4) float internalLightsFactor = 0.0f;
};

struct GlobalUniformBlock {
    alignas(16) glm::vec3 DlightDir;
    alignas(16) glm::vec3 DlightColor;
    alignas(16) glm::vec3 AmbLightColor;
    alignas(16) glm::vec3 eyePos;
    SpotLight spotLights[N_SPOTLIGHTS];
    PointLight pointLights[N_POINTLIGHTS];
    alignas(4) int nSpotLights;
    alignas(4) int nPointLights;
};

struct OverlayUniformBlock {
    alignas(4) int visible;
    alignas(4) int overlayTex;
};

#endif //VTEMPLATE_UNIFORMBUFFERS_H
