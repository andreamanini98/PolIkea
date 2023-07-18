struct BoundingRectangle {
    alignas(16) glm::vec3 bottomLeft;
    alignas(16) glm::vec3 topRight;
};


glm::vec3 getPolikeaBuildingPosition() {
    return {5.0f, 0.0f, -15.0f};
}

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

std::vector<glm::vec3> getPolikeaPositionedLightsPos() {
    return {
            getPolikeaBuildingPosition() + glm::vec3(-4.75f, 7.5f, -5.5f),
            getPolikeaBuildingPosition() + glm::vec3(4.75f, 7.5f, -5.5f),
            getPolikeaBuildingPosition() + glm::vec3(-4.75f, 7.5f, -14.5f),
            getPolikeaBuildingPosition() + glm::vec3(4.75f, 7.5f, -14.5f)
    };
}

std::vector<BoundingRectangle> getBoundingRectangles(
        glm::vec3 polikeaBuildingPosition,
        float frontOffset,
        float sideOffset,
        float backOffset
) {
    return {
            //Polikea building bounding
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - 10.5f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x - 9.0f, 0.0f, polikeaBuildingPosition.z - 20.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - 10.5f, 0.0f, polikeaBuildingPosition.z - 18.5f),
                              glm::vec3(polikeaBuildingPosition.x + 10.5f, 0.0f, polikeaBuildingPosition.z - 20.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x + 9.0f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + 10.5f, 0.0f, polikeaBuildingPosition.z - 20.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - 10.5f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + 3.5f, 0.0f, polikeaBuildingPosition.z - 1.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x + 4.4053f, 0.0f, polikeaBuildingPosition.z + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + 10.5f, 0.0f, polikeaBuildingPosition.z - 1.5f)},

            // Polikea surrounding bounding
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - sideOffset - 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z + frontOffset + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + sideOffset + 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z + frontOffset - 0.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - sideOffset - 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z + frontOffset + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x - sideOffset + 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z - backOffset - 0.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - sideOffset - 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z - backOffset + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + sideOffset + 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z - backOffset - 0.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x + sideOffset - 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z + frontOffset + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + sideOffset + 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z - backOffset - 0.5f)}
    };
}

bool checkIfInBoundingRectangle(
        glm::vec3 pointToCheck,
        BoundingRectangle boundingRectangle,
        float rectanglesPadding = 0.0f
) {
    glm::vec3 bl = boundingRectangle.bottomLeft + glm::vec3(-rectanglesPadding, 0.0f, rectanglesPadding);
    glm::vec3 tr = boundingRectangle.topRight + glm::vec3(rectanglesPadding, 0.0f, -rectanglesPadding);
    return (pointToCheck.x >= bl.x && pointToCheck.x <= tr.x && pointToCheck.z <= bl.z && pointToCheck.z >= tr.z);
}