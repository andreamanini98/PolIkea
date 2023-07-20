// To check if a point is inside a bounding rectangle, you have to check if its x and z coordinates are in between
// the ones of the bottom left and top right (additionally you can also check for the y coordinate if needed).
struct BoundingRectangle {
    alignas(16) glm::vec3 bottomLeft;
    alignas(16) glm::vec3 topRight;
};


glm::vec3 getPolikeaBuildingPosition() {
    return {5.0f, 0.0f, -15.0f};
}


// Used to get the position of the objects to put in polikea
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


// Usd to get the positions where to put lights in polikea
std::vector<glm::vec3> getPolikeaPositionedLightsPos() {
    return {
            getPolikeaBuildingPosition() + glm::vec3(-4.75f, 7.5f, -5.5f),
            getPolikeaBuildingPosition() + glm::vec3(4.75f, 7.5f, -5.5f),
            getPolikeaBuildingPosition() + glm::vec3(-4.75f, 7.5f, -14.5f),
            getPolikeaBuildingPosition() + glm::vec3(4.75f, 7.5f, -14.5f)
    };
}


// Used to get the bounding for polikea and fences
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


// Used to check if a point is inside a bounding rectangle
bool checkIfInBoundingRectangle(
        glm::vec3 pointToCheck,
        BoundingRectangle boundingRectangle,
        float rectanglesPadding = 0.0f
) {
    glm::vec3 bl = boundingRectangle.bottomLeft + glm::vec3(-rectanglesPadding, 0.0f, rectanglesPadding);
    glm::vec3 tr = boundingRectangle.topRight + glm::vec3(rectanglesPadding, 0.0f, -rectanglesPadding);
    return (pointToCheck.x >= bl.x && pointToCheck.x <= tr.x && pointToCheck.z <= bl.z && pointToCheck.z >= tr.z);
}


// Used to get the area between polikea building and the fences
std::vector<BoundingRectangle> getPolikeaExternalAreaBoundings(
        glm::vec3 polikeaBuildingPosition,
        float frontOffset,
        float sideOffset,
        float backOffset
) {
    return {
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - sideOffset - 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z + frontOffset + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + sideOffset + 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - sideOffset - 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z + frontOffset + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x - 10.0f,
                                        0.0f,
                                        polikeaBuildingPosition.z - backOffset - 0.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x - sideOffset - 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z - 20.0f),
                              glm::vec3(polikeaBuildingPosition.x + sideOffset + 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z - backOffset - 0.5f)},
            BoundingRectangle{glm::vec3(polikeaBuildingPosition.x + 10.0f,
                                        0.0f,
                                        polikeaBuildingPosition.z + frontOffset + 0.5f),
                              glm::vec3(polikeaBuildingPosition.x + sideOffset + 0.5f,
                                        0.0f,
                                        polikeaBuildingPosition.z - backOffset - 0.5f)}

    };
}