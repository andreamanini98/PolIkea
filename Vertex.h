//
// Created by Matteo Carrara on 22/07/23.
//

#ifndef VTEMPLATE_VERTEX_H
#define VTEMPLATE_VERTEX_H

#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 UV;
};

struct VertexOverlay {
    glm::vec2 pos;
    glm::vec2 UV;
};

struct VertexVColor {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec3 color;
};

struct VertexWithTextID {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 UV;
    uint8_t texID;
};

#endif //VTEMPLATE_VERTEX_H
