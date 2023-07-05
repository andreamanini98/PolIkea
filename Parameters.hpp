struct BoundingRectangle {
    glm::vec3 bottomLeft;
    glm::vec3 topRight;
};

std::vector<glm::vec3> getPolikeaBuildingOffsets() {
    return {
            glm::vec3(-7.5f, 0.625f, -17.5f),
            glm::vec3(-3.75f, 0.625f, -17.5f),
            glm::vec3(0.0f, 0.625f, -17.5f),
            glm::vec3(7.5f, 0.625f, -17.5f),
            glm::vec3(-7.5f, 0.625f, -12.5f),
            glm::vec3(-3.75f, 0.625f, -12.5f),
            glm::vec3(0.0f, 0.625f, -12.5f),
            glm::vec3(7.5f, 0.625f, -12.5f),
            glm::vec3(-7.5f, 0.625f, -7.5f),
            glm::vec3(-3.75f, 0.625f, -7.5f),
            glm::vec3(0.0f, 0.625f, -7.5f),
            glm::vec3(7.5f, 0.625f, -7.5f),
            glm::vec3(-7.5f, 0.625f, -2.5f),
            glm::vec3(-3.75f, 0.625f, -2.5f),
            glm::vec3(0.0f, 0.625f, -2.5f)
    };
}

std::vector<BoundingRectangle> getBoundingRectangles(glm::vec3 polikeaBuildingPosition) {
    return {
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - 10.5f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x - 9.0f, 0.0f, polikeaBuildingPosition.z - 20.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - 10.5f, 0.0f, polikeaBuildingPosition.z - 18.5f),
                              glm::vec3(polikeaBuildingPosition.x + 10.5f, 0.0f, polikeaBuildingPosition.z - 20.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x + 9.0f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + 10.5f, 0.0f, polikeaBuildingPosition.z - 20.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - 10.5f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + 3.5f, 0.0f, polikeaBuildingPosition.z - 1.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x + 6.5f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + 10.5f, 0.0f, polikeaBuildingPosition.z - 1.5f)}

    };
}