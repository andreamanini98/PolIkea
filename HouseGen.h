//
// Created by Matteo Carrara on 22/07/23.
//

#ifndef VTEMPLATE_HOUSEGEN_H
#define VTEMPLATE_HOUSEGEN_H


#include <vector>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Vertex.h"

#define WALL_TEXTURES_PER_PIXEL (1.0f/4.0)
//REMEMBER TO UPDATE THIS FIELD ALSO IN THE ShaderInstanced.vert
#define N_ROOMS 5
#define N_POS_LIGHTS (4 + 5) // 4 lights for polikea + one light for each room

#define MIN_DIMENSION (12.5f)
#define MAX_DIMENSION (18.0f)
#define DOOR_HWIDTH (0.5f)
#define WALL_WIDTH (0.1f)

#define ROOM_CEILING_HEIGHT 3.0f
#define DOOR_HEIGHT 2.5f


// ----- ENUM ----- //

enum DoorState {
    OPEN,
    CLOSED,
    OPENING,
    CLOSING,
    WAITING_OPEN
};

enum DoorOpeningDirection {
    CLOCKWISE,
    COUNTERCLOCKWISE
};

enum Direction {
    NORTH,
    EAST,
    SOUTH,
    WEST
};

struct OpenableDoor {
    glm::vec3 doorPos;
    float baseRot;
    float doorRot;
    float doorSpeed;
    DoorState doorState;
    float time;
    DoorOpeningDirection doorOpeningDirection;
};

struct Door {
    float offset;
    Direction direction;
};

struct Room {
    float startX;
    float startY;
    float width;
    float depth;
    std::vector<Door> doors;
};

class VertexStorage {
    uint32_t vertexCurIdx = 0;
    std::vector<VertexWithTextID> &vPos;
    std::vector<uint32_t> &vIdx;
    std::vector<OpenableDoor> &openableDoors;
public:
    VertexStorage(
            std::vector<VertexWithTextID> &vPos,
            std::vector<uint32_t> &vIdx,
            std::vector<OpenableDoor> &openableDoors
    ) : vPos(vPos), vIdx(vIdx), vertexCurIdx(vPos.size()), openableDoors(openableDoors) {}

    uint32_t addVertex(VertexWithTextID color) {
        vPos.push_back(color);
        return vertexCurIdx++;
    }

    void drawRect(glm::vec3 bottomLeft, glm::vec3 bottomRight, glm::vec3 topRight, glm::vec3 topLeft, int vecDir,
                  uint8_t texID, glm::vec2 uvOffset = glm::vec2(0.0f, 0.0f)) {
        auto width = glm::length(bottomRight - bottomLeft);
        auto height = glm::length(topLeft - bottomLeft);

        glm::vec3 norm = glm::abs(glm::normalize(glm::cross(bottomRight - bottomLeft, topRight - bottomLeft))) *
                         (vecDir > 0 ? 1.0f : -1.0f);
        auto i0 = addVertex({bottomLeft, norm, glm::vec2{0.0f, 0.0f} + uvOffset, texID});
        auto i1 = addVertex({bottomRight, norm, glm::vec2{width * WALL_TEXTURES_PER_PIXEL, 0.0f} + uvOffset, texID});
        auto i2 = addVertex({topRight, norm,
                             glm::vec2{width * WALL_TEXTURES_PER_PIXEL, height * WALL_TEXTURES_PER_PIXEL} + uvOffset,
                             texID});
        auto i3 = addVertex({topLeft, norm, glm::vec2{0.0f, height * WALL_TEXTURES_PER_PIXEL} + uvOffset, texID});

        //printf("%d %d %d %d\n", i0, i1, i2, i3);

        addIndex(i0, i1, i2);
        addIndex(i2, i3, i0);
    }

    void drawDoorFrame(glm::vec3 hingeCorner, Direction doorDirection, uint8_t tex) {
        glm::vec3 UP = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 doorWidthDir =
                doorDirection == Direction::NORTH || doorDirection == Direction::SOUTH ? glm::vec3(1.0f, 0.0f, 0.0f)
                                                                                       : glm::vec3(0.0f, 0.0f, 1.0f);
        glm::vec3 doorDepthDir =
                doorDirection == Direction::NORTH || doorDirection == Direction::SOUTH ? glm::vec3(0.0f, 0.0f, 1.0f)
                                                                                       : glm::vec3(1.0f, 0.0f, 0.0f);

        //glm::vec3 hingeCorner = base - doorWidthDir * DOOR_HWIDTH;
        float DOOR_WIDTH = 2 * DOOR_HWIDTH;

        //TOP
        drawRect(
                hingeCorner,
                hingeCorner + doorWidthDir * DOOR_WIDTH,
                hingeCorner + doorWidthDir * DOOR_WIDTH + doorDepthDir * WALL_WIDTH,
                hingeCorner + doorDepthDir * WALL_WIDTH,
                1,
                tex
        );

        //BOTTOM
        drawRect(
                hingeCorner + DOOR_HEIGHT * UP,
                hingeCorner + doorWidthDir * DOOR_WIDTH + DOOR_HEIGHT * UP,
                hingeCorner + doorWidthDir * DOOR_WIDTH + doorDepthDir * WALL_WIDTH + DOOR_HEIGHT * UP,
                hingeCorner + doorDepthDir * WALL_WIDTH + DOOR_HEIGHT * UP,
                -1,
                tex
        );

        //LEFT
        drawRect(
                hingeCorner,
                hingeCorner + doorDepthDir * WALL_WIDTH,
                hingeCorner + doorDepthDir * WALL_WIDTH + DOOR_HEIGHT * UP,
                hingeCorner + DOOR_HEIGHT * UP,
                1,
                tex
        );

        //RIGHT
        drawRect(
                hingeCorner + doorWidthDir * DOOR_WIDTH,
                hingeCorner + doorWidthDir * DOOR_WIDTH + doorDepthDir * WALL_WIDTH,
                hingeCorner + doorWidthDir * DOOR_WIDTH + doorDepthDir * WALL_WIDTH + DOOR_HEIGHT * UP,
                hingeCorner + doorWidthDir * DOOR_WIDTH + DOOR_HEIGHT * UP,
                -1,
                tex
        );
    }

    void drawRectWithOpening(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, int vecDir, float openingOffset, Direction doorDirection, std::vector<BoundingRectangle> *bounds, uint8_t texID) {
        glm::vec3 norm = glm::normalize(glm::cross(v1 - v0, v2 - v0)) * (vecDir > 0 ? 1.0f : -1.0f);

        glm::vec3 openingDir = glm::normalize(v1 - v0); //TODO
        glm::vec3 openingUpDir = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 openingV0 = openingDir * (openingOffset - DOOR_HWIDTH) + v0;
        glm::vec3 openingV1 = openingDir * (openingOffset + DOOR_HWIDTH) + v0;
        glm::vec3 openingV2 = openingV1 + openingUpDir * DOOR_HEIGHT;
        glm::vec3 openingV3 = openingV0 + openingUpDir * DOOR_HEIGHT;

        glm::vec3 ceilingOpeningV03 = openingV0 + openingUpDir * ROOM_CEILING_HEIGHT;
        glm::vec3 ceilingOpeningV12 = openingV1 + openingUpDir * ROOM_CEILING_HEIGHT;

        drawRect(v0, openingV0, ceilingOpeningV03, v3, vecDir, texID);

        auto UVWidth = glm::length(openingV0 - v0) * WALL_TEXTURES_PER_PIXEL;
        auto UVHeight = DOOR_HEIGHT * WALL_TEXTURES_PER_PIXEL;
        drawRect(openingV3, openingV2, ceilingOpeningV12, ceilingOpeningV03, vecDir, texID, {UVWidth, UVHeight});

        UVWidth += DOOR_HWIDTH * 2 * WALL_TEXTURES_PER_PIXEL;
        drawRect(openingV1, v1, v2, ceilingOpeningV12, vecDir, texID, {UVWidth, 0});

        glm::vec3 bOffset = glm::vec3(-0.1, 0.0, 0.1);
        glm::vec3 tOffset = glm::vec3(0.1, 0.0, -0.1);
        // Placing boundings on walls with doors
        if (doorDirection == NORTH || doorDirection == SOUTH) {
            bounds->push_back(
                    BoundingRectangle{v0 + bOffset, openingV0 + tOffset});
            bounds->push_back(
                    BoundingRectangle{openingV1 + bOffset, v1 + tOffset});
        } else if (doorDirection == EAST || doorDirection == WEST) {
            bounds->push_back(
                    BoundingRectangle{openingV0 + bOffset, v0 + tOffset});
            bounds->push_back(
                    BoundingRectangle{v1 + bOffset, openingV1 + tOffset});
        }

        printf("%f %f %f\n", openingV0.x, openingV0.y, openingV0.z);

        if (/*doorIndices.find(openingV0) == doorIndices.end()*/ doorDirection == NORTH || doorDirection == EAST) {
            //doorIndices.insert(openingV0);
            float rotType;
            if (doorDirection == NORTH || doorDirection == SOUTH) {
                rotType = glm::radians(0.0f);
            } else {
                rotType = glm::radians(90.0f);
            }
            openableDoors.push_back(
                    OpenableDoor{
                            openingV0 + glm::vec3(doorDirection == EAST ? WALL_WIDTH / 2 : 0.0f, 0.0f,
                                                  doorDirection == NORTH ? WALL_WIDTH / 2 : 0.0f),
                            rotType,
                            0.0f,
                            glm::radians(90.0f),
                            CLOSED,
                            0.0f,
                            CLOCKWISE
                    });
        }
    }

    void addIndex(uint32_t v0, uint32_t v1, uint32_t v2) {
        vIdx.push_back(v0);
        vIdx.push_back(v1);
        vIdx.push_back(v2);
    }
};

inline std::vector<Room> generateFloorplan(float dimension) {
    // Seed the random number generator
    std::random_device rd;
    std::mt19937 gen(rd());

    std::vector<Room> rooms;

    float currentX = 0;
    float currentY = 0;

    float minWidth = MIN_DIMENSION;
    float minDepth = MIN_DIMENSION;

    std::uniform_int_distribution<int> boolDistr(0, 1);

    uint32_t n_doors = 0;

    Door prevDoor{};
    for (int i = 0; i < N_ROOMS; i++) {
        std::uniform_real_distribution<float> distribution_w(minWidth + DOOR_HWIDTH, dimension);
        std::uniform_real_distribution<float> distribution_h(minDepth + DOOR_HWIDTH, dimension);
        // Generate a random room
        float roomWidth = distribution_w(gen);
        float roomDepth = distribution_h(gen);

        prevDoor.direction = prevDoor.direction == Direction::NORTH ? Direction::SOUTH : Direction::WEST;
        auto &room = rooms.emplace_back(
                Room{
                        currentX,
                        currentY,
                        roomWidth,
                        roomDepth,
                        i != 0 ? std::vector<Door>({prevDoor}) : std::vector<Door>()
                }
        );

        if (static_cast<bool>(boolDistr(gen))) {
            std::uniform_real_distribution<float> distribution_door(0 + DOOR_HWIDTH, roomDepth - DOOR_HWIDTH);

            currentX += roomWidth + WALL_WIDTH;
            minWidth = MIN_DIMENSION;
            minDepth = distribution_door(gen);
            prevDoor = Door{minDepth, Direction::EAST};
        } else {
            std::uniform_real_distribution<float> distribution_door(0 + DOOR_HWIDTH, roomWidth - DOOR_HWIDTH);

            currentY += roomDepth + WALL_WIDTH;
            minWidth = distribution_door(gen);
            minDepth = MIN_DIMENSION;
            prevDoor = Door{minWidth, Direction::NORTH};
        }

        if (i != N_ROOMS - 1) {
            room.doors.push_back(prevDoor);
            n_doors++;
        }
    }

    printf("N DOORS at generateFloorplan %d\n", n_doors);

    return std::move(rooms);
}

inline void
floorPlanToVerIndexes(const std::vector<Room> &rooms, std::vector<VertexWithTextID> &vPos, std::vector<uint32_t> &vIdx,
                      std::vector<OpenableDoor> &openableDoors, std::vector<BoundingRectangle> *bounds,
                      std::vector<glm::vec3> *positionedLightPos, std::vector<glm::vec3> *roomCenters,
                      std::vector<BoundingRectangle> *roomOccupiedArea) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> floorTexDistribution(1, 4);

    VertexStorage storage(vPos, vIdx, openableDoors);
    int test = 0;

    for (auto &room: rooms) {
        int floorTex = floorTexDistribution(gen);
        int ceilingTex = 0;
        int wallTex = 0;

        glm::vec3 roomCenter = glm::vec3(room.startX + room.width / 2, ROOM_CEILING_HEIGHT,
                                         room.startY + room.depth / 2);
        positionedLightPos->push_back(roomCenter);
        roomCenters->push_back(roomCenter - glm::vec3(0.0f, ROOM_CEILING_HEIGHT, 0.0f));

        auto color = glm::vec3((test % 3) == 0, (test % 3) == 1, (test % 3) == 2);

        // Here the bottom left and the top right are, respectively, the room's topLeft and bottomRight
        // Maybe there is a misunderstanding in the convention
        roomOccupiedArea->push_back(BoundingRectangle{
                glm::vec3(room.startX, 0, room.startY + room.depth),
                glm::vec3(room.startX + room.width, 0, room.startY)});

        storage.drawRect(
                glm::vec3(room.startX, 0, room.startY),
                glm::vec3(room.startX + room.width, 0, room.startY),
                glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                glm::vec3(room.startX, 0, room.startY + room.depth),
                1,
                floorTex
        );

        storage.drawRect(
                glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                -1,
                ceilingTex
        );

        bool hasDoorNorth = false;
        float offsetNorth;
        bool hasDoorSouth = false;
        float offsetSouth;
        bool hasDoorEast = false;
        float offsetEast;
        bool hasDoorWest = false;
        float offsetWest;

        for (auto &door: room.doors) {
            if (door.direction == NORTH) {
                hasDoorNorth = true;
                offsetNorth = door.offset;
            }
            if (door.direction == SOUTH) {
                hasDoorSouth = true;
                offsetSouth = door.offset;
            }
            if (door.direction == EAST) {
                hasDoorEast = true;
                offsetEast = door.offset;
            }
            if (door.direction == WEST) {
                hasDoorWest = true;
                offsetWest = door.offset;
            }
        }

        glm::vec3 bOffset = glm::vec3(-0.1, 0.0, 0.1);
        glm::vec3 tOffset = glm::vec3(0.1, 0.0, -0.1);
        // Checks to put boundings on walls without doors
        if (!hasDoorSouth) {
            bounds->push_back(
                    BoundingRectangle{glm::vec3(room.startX, 0, room.startY) + bOffset,
                                      glm::vec3(room.startX + room.width, 0, room.startY) + tOffset});
        }
        if (!hasDoorNorth) {
            bounds->push_back(
                    BoundingRectangle{glm::vec3(room.startX, 0, room.startY + room.depth) + bOffset,
                                      glm::vec3(room.startX + room.width, 0, room.startY + room.depth) + tOffset});
        }
        if (!hasDoorWest) {
            bounds->push_back(
                    BoundingRectangle{glm::vec3(room.startX, 0, room.startY + room.depth) + bOffset,
                                      glm::vec3(room.startX, 0, room.startY) + tOffset});
        }
        if (!hasDoorEast) {
            bounds->push_back(BoundingRectangle{
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth) + bOffset,
                    glm::vec3(room.startX + room.width, 0, room.startY) + tOffset});
        }

        if (hasDoorSouth) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    offsetSouth,
                    SOUTH,
                    bounds,
                    wallTex
            );
        } else {
            storage.drawRect(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    wallTex
            );
        }

        if (hasDoorNorth) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    -1,
                    offsetNorth,
                    NORTH,
                    bounds,
                    wallTex
            );

            storage.drawDoorFrame(glm::vec3(room.startX + offsetNorth - DOOR_HWIDTH, 0, room.startY + room.depth),
                                  NORTH, wallTex);
        } else {
            storage.drawRect(
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    -1,
                    wallTex
            );
        }

        if (hasDoorWest) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    offsetWest,
                    WEST,
                    bounds,
                    wallTex
            );
        } else {
            storage.drawRect(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    wallTex
            );
        }

        if (hasDoorEast) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    -1,
                    offsetEast,
                    EAST,
                    bounds,
                    wallTex
            );

            storage.drawDoorFrame(glm::vec3(room.startX + room.width, 0, room.startY + offsetEast - DOOR_HWIDTH), EAST,
                                  wallTex);
        } else {
            storage.drawRect(
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    -1,
                    wallTex
            );
        }

        test++;
    }
}


inline void
insertRectVertices(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, std::vector<Vertex> *vPos,
                   float aspect_ratio, float vecDir) {
    float repeat_x = glm::abs(glm::distance(v0, v1)) / (8.0f);
    float repeat_y = glm::abs(glm::distance(v2, v1)) / (8.0f * aspect_ratio);
    glm::vec3 norm = glm::normalize(glm::cross(v1 - v0, v3 - v0) * vecDir);

    vPos->push_back({v0, norm, {0.0f, repeat_y}});
    vPos->push_back({v1, norm, {repeat_x, repeat_y}});
    vPos->push_back({v2, norm, {repeat_x, 0.0f}});
    vPos->push_back({v3, norm, {0.0f, 0.0f}});
}

#define FENCE_ASPECT_RATIO (1333.0f/2000.0f)

inline void initPolikeaSurroundings(std::vector<Vertex> *vPosGround, std::vector<uint32_t> *vIdxGround,
                                    std::vector<Vertex> *vPosFence, std::vector<uint32_t> *vIdxFence,
                                    glm::vec3 polikeaPos, float frontOffset, float sideOffset, float backOffset) {
    // Vertices of the ground as seen from a top view
    glm::vec3 bottomLeft = polikeaPos + glm::vec3(-sideOffset, 0.0f, frontOffset);
    glm::vec3 topLeft = polikeaPos + glm::vec3(-sideOffset, 0.0f, -backOffset);
    glm::vec3 topRight = polikeaPos + glm::vec3(sideOffset, 0.0f, -backOffset);
    glm::vec3 bottomRight = polikeaPos + glm::vec3(sideOffset, 0.0f, frontOffset);
    glm::vec3 h = glm::vec3(0.0f, 15.0f, 0.0f);

    insertRectVertices(bottomLeft, topLeft, topRight, bottomRight, vPosGround, 1.0f, -1.0f);
    vIdxGround->push_back(0);
    vIdxGround->push_back(1);
    vIdxGround->push_back(2);
    vIdxGround->push_back(0);
    vIdxGround->push_back(2);
    vIdxGround->push_back(3);

    insertRectVertices(bottomLeft, topLeft, topLeft + h, bottomLeft + h, vPosFence, FENCE_ASPECT_RATIO, 1.0f);
    insertRectVertices(topLeft, topRight, topRight + h, topLeft + h, vPosFence, FENCE_ASPECT_RATIO, 1.0f);
    insertRectVertices(topRight, bottomRight, bottomRight + h, topRight + h, vPosFence, FENCE_ASPECT_RATIO, 1.0f);
    insertRectVertices(bottomRight, bottomLeft, bottomLeft + h, bottomRight + h, vPosFence, FENCE_ASPECT_RATIO, 1.0f);

    for (int i = 0; i < 4; i++) {
        vIdxFence->push_back(i * 4 + 0);
        vIdxFence->push_back(i * 4 + 1);
        vIdxFence->push_back(i * 4 + 2);
        vIdxFence->push_back(i * 4 + 0);
        vIdxFence->push_back(i * 4 + 2);
        vIdxFence->push_back(i * 4 + 3);
    }
}


#endif //VTEMPLATE_HOUSEGEN_H
