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
#define N_POS_LIGHTS (4 + 5) // 4 lights for polikea + one light for each room
#define N_SPOTLIGHTS 50
#define N_POINTLIGHTS 50

struct ModelInstance {
    float baseRot;
    glm::vec3 pos;
    int instanceType; // 0 == doors, 1 == lights (used in the shader)
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
};

struct UniformBlockDoors {
    alignas(4) float amb;
    alignas(4) float gamma;
    alignas(16) glm::vec3 sColor;
    alignas(16) glm::mat4 prjViewMat;
    alignas(16) glm::vec4 door[N_ROOMS - 1 + 2]; // We have 2 more doors for polikea
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

struct HouseBindings {
    BoundingRectangle roomsArea[N_ROOMS];
    BoundingRectangle externPolikeaBoundings[4];
};

#endif //VTEMPLATE_UNIFORMBUFFERS_H
