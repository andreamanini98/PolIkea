#include <string>
#include <iostream>
#include <filesystem>
#include "Starter.hpp"
#include "WorldView.hpp"
#include "Parameters.hpp"
#include <random>
#include <unordered_set>

#define N_SPOTLIGHTS 50
#define N_POINTLIGHTS 50
#define MAX_OBJECTS_IN_POLIKEA 15 // Do not exceed 15 since the model is pre-generated using Blender

#define FRONTOFFSET 10.0f
#define SIDEOFFSET 20.0f
#define BACKOFFSET 30.0f

namespace fs = std::filesystem;

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


// ----- GENERIC STRUCT ----- //

struct ModelInstance {
    float baseRot;
    glm::vec3 pos;
    float instanceType; // 0 == doors, 1 == lights (used in the shader)
};

struct OpenableDoor {
    glm::vec3 doorPos;
    float baseRot;
    float doorRot;
    float doorSpeed;
    float doorRange;
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


// ----- LIGHT STRUCT ----- //

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


// ----- UNIFORM BUFFERS STRUCT ----- //

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
    alignas(16) glm::vec4 door[N_ROOMS - 1];
};

struct UniformBlockPositionedLights {
    alignas(4) float amb;
    alignas(4) float gamma;
    alignas(16) glm::vec3 sColor;
    alignas(16) glm::mat4 prjViewMat;
    alignas(16) glm::vec4 lights[N_POS_LIGHTS];
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
    alignas(4) float visible;
};

struct HouseBindings {
    BoundingRectangle roomsArea[N_ROOMS];
};


// ----- VERTEX STRUCT ----- //

struct Vertex {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 UV;
};

struct VertexWithTextID {
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 UV;
    uint8_t texID;
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


// ----- HOME GENERATION ----- //

#define WALL_TEXTURES_PER_PIXEL (1.0f/4.0)

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

    void drawRectWithOpening(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, int vecDir, float openingOffset,
                             Direction doorDirection, std::vector<BoundingRectangle> *bounds, uint8_t texID) {
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

        glm::vec3 bOffset = glm::vec3(-0.2, 0.0, 0.2);
        glm::vec3 tOffset = glm::vec3(0.2, 0.0, -0.2);
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
                            glm::radians(90.0f),
                            CLOSED,
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

        glm::vec3 bOffset = glm::vec3(-0.2, 0.0, 0.2);
        glm::vec3 tOffset = glm::vec3(0.2, 0.0, -0.2);
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
insertRectVertices(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, glm::vec3 norm, std::vector<Vertex> *vPos,
                   float aspect_ratio) {
    float repeat_x = glm::abs(glm::distance(v0, v1)) / (8.0f);
    float repeat_y = glm::abs(glm::distance(v2, v1)) / (8.0f * aspect_ratio);

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

    insertRectVertices(bottomLeft, topLeft, topRight, bottomRight, {0.0f, 1.0f, 0.0f}, vPosGround, 1.0f);
    vIdxGround->push_back(0);
    vIdxGround->push_back(1);
    vIdxGround->push_back(2);
    vIdxGround->push_back(0);
    vIdxGround->push_back(2);
    vIdxGround->push_back(3);

    // TODO CHECK THESE NORMALS
    insertRectVertices(bottomLeft, topLeft, topLeft + h, bottomLeft + h, {1.0f, 0.0f, 0.0f}, vPosFence,
                       FENCE_ASPECT_RATIO);
    insertRectVertices(topLeft, topRight, topRight + h, topLeft + h, {0.0f, 0.0f, 1.0f}, vPosFence, FENCE_ASPECT_RATIO);
    insertRectVertices(topRight, bottomRight, bottomRight + h, topRight + h, {-1.0f, 0.0f, 0.0f}, vPosFence,
                       FENCE_ASPECT_RATIO);
    insertRectVertices(bottomRight, bottomLeft, bottomLeft + h, bottomRight + h, {0.0f, 0.0f, -1.0f}, vPosFence,
                       FENCE_ASPECT_RATIO);

    for (int i = 0; i < 4; i++) {
        vIdxFence->push_back(i * 4 + 0);
        vIdxFence->push_back(i * 4 + 1);
        vIdxFence->push_back(i * 4 + 2);
        vIdxFence->push_back(i * 4 + 0);
        vIdxFence->push_back(i * 4 + 2);
        vIdxFence->push_back(i * 4 + 3);
    }
}


// ----- MODEL INFO STRUCT ----- //

// Struct used to store data related to a model
struct ModelInfo {
    Model<Vertex> model;
    DescriptorSet dsModel;
    UniformBlock modelUBO{};
    glm::vec3 modelPos{};
    float modelRot = 0.0;
    bool hasBeenBought = false;

    float cylinderRadius;
    float cylinderHeight;
    glm::vec3 minCoords;
    glm::vec3 maxCoords;
    glm::vec3 size;
    glm::vec3 center;
    uint8_t roomCycling = 0;

    glm::vec3 getMinCoordPos() const { return minCoords + modelPos; }

    glm::vec3 getMaxCoordsPos() const { return maxCoords + modelPos; }

    bool checkCollision(const ModelInfo &other) const {
        // Cylindrical body collision check
        glm::vec3 centerHoriz = glm::vec3(modelPos.x + center.x, 0.0, modelPos.z + center.z);
        glm::vec3 centerHorizB = glm::vec3(other.modelPos.x + other.center.x, 0.0, other.modelPos.z + other.center.z);

        float distCenter = glm::distance(centerHoriz, centerHorizB);
        if (distCenter > cylinderRadius + other.cylinderRadius) {
            return false; // Cylindrical bodies don't intersect, no collision
        }

        // Top and bottom cap collision check
        float heightA = cylinderHeight;
        float heightB = other.cylinderHeight;
        float distCaps =
                std::abs(modelPos.y + center.y - other.modelPos.y - other.center.y) - (heightA + heightB) * 0.5f;
        if (distCaps > 0.0f) {
            return false; // Top and bottom caps don't intersect, no collision
        }

        // Step 4: Both cylindrical body and top/bottom caps pass the collision check
        return true; // Collision detected
    }
};


// ----- MAIN ----- //

class VTemplate : public BaseProject {
protected:

    // Current aspect ratio (used by the callback that resized the window)
    float Ar;

    // Descriptor Layouts ["classes" of what will be passed to the shaders]
    DescriptorSetLayout DSLMesh, DSLMeshMultiTex, DSLDoor, DSLPositionedLights, DSLGubo, DSLOverlay, DSLVertexWithColors, DSLHouseBindings;

    // Vertex formats
    VertexDescriptor VMesh, VMeshTexID, VOverlay, VVertexWithColor, VMeshInstanced;

    // Pipelines [Shader couples]
    Pipeline PMesh, PMeshMultiTexture, POverlay, PVertexWithColors, PMeshInstanced;

    // Models, textures and Descriptors (values assigned to the uniforms)
    // Please note that Model objects depends on the corresponding vertex structure
    // Models
    Model<Vertex> MPolikeaExternFloor, MFence;
    Model<VertexOverlay> MOverlay;
    Model<VertexVColor> MPolikeaBuilding;
    Model<VertexWithTextID> MBuilding;

    // Descriptor sets
    DescriptorSet DSPolikeaExternFloor, DSFence, DSGubo, DSOverlayMoveOject, DSPolikeaBuilding, DSBuilding, DSHouseBindings;
    // Textures
    Texture TAsphalt, TFurniture, TFence, TPlankWall, TOverlayMoveObject, TBathFloor, TDarkFloor, TTiledStones;
    // C++ storage for uniform variables
    UniformBlock uboPolikeaExternFloor, uboFence, uboPolikea, uboBuilding;
    GlobalUniformBlock gubo;
    OverlayUniformBlock uboKey;
    HouseBindings uboHouseBindings;

    // Other application parameters
    // A vector containing one element for each model loaded where we want to keep track of its information
    std::vector<ModelInfo> MV;

    glm::vec3 polikeaBuildingPosition = getPolikeaBuildingPosition();
    std::vector<glm::vec3> polikeaBuildingOffsets = getPolikeaBuildingOffsets();
    std::vector<BoundingRectangle> boundingRectangles = getBoundingRectangles(polikeaBuildingPosition, FRONTOFFSET,
                                                                              SIDEOFFSET, BACKOFFSET);
    std::vector<glm::vec3> positionedLightPos = getPolikeaPositionedLightsPos();

    glm::vec3 CamPos = glm::vec3(2.0, 1.0, 3.45706);
    float CamAlpha = 0.0f;
    float CamBeta = 0.0f;
    float CamRho = 0.0f;
    bool OnlyMoveCam = true;
    uint32_t MoveObjIndex = 0;

    std::vector<glm::vec3> roomCenters;
    std::vector<BoundingRectangle> roomOccupiedArea;

    Model<Vertex, ModelInstance> MDoor, MPositionedLights;
    DescriptorSet DSDoor, DSPositionedLights;
    UniformBlockDoors uboDoor;
    UniformBlockPositionedLights uboPositionedLights;

    std::vector<OpenableDoor> doors;

    inline glm::vec3
    rotateTargetRespectToCam(glm::vec3 CamPosition, float CameraAlpha, float CameraBeta, glm::vec3 modelPosition) {
        return glm::translate(glm::mat4(1.0f), CamPosition) *
               glm::rotate(glm::mat4(1), CameraAlpha, glm::vec3(0.0f, 1.0f, 0.0f)) *
               glm::rotate(glm::mat4(1), CameraBeta, glm::vec3(1.0f, 0.0f, 0.0f)) *
               glm::translate(glm::mat4(1.0f), -CamPosition) *
               glm::vec4(modelPosition, 1.0f);
    }

    // Here you set the main application parameters
    void setWindowParameters() {
        // Window size, title and initial background
        windowWidth = 800;
        windowHeight = 600;
        windowTitle = "PolIkea";
        windowResizable = GLFW_TRUE;
        initialBackgroundColor = {0.4f, 1.0f, 1.0f, 1.0f};

        // Descriptor pool sizes
        // TODO resize to match the actual descriptors used in the code
        uniformBlocksInPool = 100;
        texturesInPool = 100;
        setsInPool = 500;

        Ar = (float) windowWidth / (float) windowHeight;
    }

    // What to do when the window changes size
    void onWindowResize(int w, int h) {
        Ar = (float) w / (float) h;
    }

    // Here you load and setup all your Vulkan Models and Textures
    // Here you also create your Descriptor set layouts and load the shaders for the pipelines
    void localInit() {
        // Descriptor Layouts [what will be passed to the shaders]
        DSLMesh.init(this, {
                // This array contains the bindings:
                // first  element : the binding number
                // second element : the type of element (buffer or texture)
                //                  using the corresponding Vulkan constant
                // third  element : the pipeline stage where it will be used
                //                  using the corresponding Vulkan constant
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
        });
        DSLHouseBindings.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS}
        });
        DSLMeshMultiTex.init(this, {
                // This array contains the bindings:
                // first  element : the binding number
                // second element : the type of element (buffer or texture)
                //                  using the corresponding Vulkan constant
                // third  element : the pipeline stage where it will be used
                //                  using the corresponding Vulkan constant
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5},
        });
        DSLDoor.init(this, {
                // This array contains the bindings:
                // first  element : the binding number
                // second element : the type of element (buffer or texture)
                //                  using the corresponding Vulkan constant
                // third  element : the pipeline stage where it will be used
                //                  using the corresponding Vulkan constant
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
        });
        DSLPositionedLights.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS}
        });
        // TODO may bring the DSLGUBO to VK_SHADER_STAGE_FRAGMENT_BIT if it is used only there
        DSLGubo.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS}
        });
        DSLOverlay.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
        });
        // TODO may bring the DSLVertexWithColors to VK_SHADER_STAGE_FRAGMENT_BIT if it is used only there
        DSLVertexWithColors.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS}
        });

        // Vertex descriptors
        VMesh.init(this, {
                // This array contains the bindings:
                // first  element : the binding number
                // second element : the stride of this binding
                // third  element : whether this parameter changes per vertex or per instance
                //                  using the corresponding Vulkan constant
                {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}
        }, {
                           // This array contains the location:
                           // first  element : the binding number
                           // second element : the location number
                           // third  element : the offset of this element in the memory record
                           // fourth element : the data type of the element
                           //                  using the corresponding Vulkan constant
                           // fifth  element : the size in byte of the element
                           // sixth  element : a constant defining the element usage
                           //                   POSITION - a vec3 with the position
                           //                   NORMAL   - a vec3 with the normal vector
                           //                   UV       - a vec2 with a UV coordinate
                           //                   COLOR    - a vec4 with a RGBA color
                           //                   TANGENT  - a vec4 with the tangent vector
                           //                   OTHER    - anything else
                           //
                           // ***************** DOUBLE CHECK ********************
                           //  That the Vertex data structure you use in the "offsetoff" and
                           //	in the "sizeof" in the previous array, refers to the correct one,
                           //	if you have more than one vertex format!
                           // ***************************************************
                           {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos),
                                   sizeof(glm::vec3), POSITION},
                           {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, norm),
                                   sizeof(glm::vec3), NORMAL},
                           {0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, UV),
                                   sizeof(glm::vec2), UV},
                   });

        VMeshTexID.init(this, {
                // This array contains the bindings:
                // first  element : the binding number
                // second element : the stride of this binding
                // third  element : whether this parameter changes per vertex or per instance
                //                  using the corresponding Vulkan constant
                {0, sizeof(VertexWithTextID), VK_VERTEX_INPUT_RATE_VERTEX}
        }, {
                                // This array contains the location:
                                // first  element : the binding number
                                // second element : the location number
                                // third  element : the offset of this element in the memory record
                                // fourth element : the data type of the element
                                //                  using the corresponding Vulkan constant
                                // fifth  element : the size in byte of the element
                                // sixth  element : a constant defining the element usage
                                //                   POSITION - a vec3 with the position
                                //                   NORMAL   - a vec3 with the normal vector
                                //                   UV       - a vec2 with a UV coordinate
                                //                   COLOR    - a vec4 with a RGBA color
                                //                   TANGENT  - a vec4 with the tangent vector
                                //                   OTHER    - anything else
                                //
                                // ***************** DOUBLE CHECK ********************
                                //  That the Vertex data structure you use in the "offsetoff" and
                                //	in the "sizeof" in the previous array, refers to the correct one,
                                //	if you have more than one vertex format!
                                // ***************************************************
                                {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexWithTextID, pos),
                                        sizeof(glm::vec3), POSITION},
                                {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexWithTextID, norm),
                                        sizeof(glm::vec3), NORMAL},
                                {0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(VertexWithTextID, UV),
                                        sizeof(glm::vec2), UV},
                                {0, 3, VK_FORMAT_R8_UINT,          offsetof(VertexWithTextID, texID),
                                        sizeof(uint8_t),   OTHER},
                        });

        VOverlay.init(this, {
                {0, sizeof(VertexOverlay), VK_VERTEX_INPUT_RATE_VERTEX}
        }, {
                              {0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexOverlay, pos),
                                      sizeof(glm::vec2), OTHER},
                              {0, 1, VK_FORMAT_R32G32_SFLOAT, offsetof(VertexOverlay, UV),
                                      sizeof(glm::vec2), UV}
                      });

        VVertexWithColor.init(this, {
                {0, sizeof(VertexVColor), VK_VERTEX_INPUT_RATE_VERTEX}
        }, {
                                      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexVColor, pos),
                                              sizeof(glm::vec3), POSITION},
                                      {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexVColor, norm),
                                              sizeof(glm::vec3), NORMAL},
                                      {0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexVColor, color),
                                              sizeof(glm::vec3), COLOR}
                              });

        VMeshInstanced.init(this, {
                {0, sizeof(Vertex),        VK_VERTEX_INPUT_RATE_VERTEX},
                {1, sizeof(ModelInstance), VK_VERTEX_INPUT_RATE_INSTANCE}
        }, {
                                    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos),
                                                                                               sizeof(glm::vec3), POSITION},
                                    {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, norm),
                                                                                               sizeof(glm::vec3), NORMAL},
                                    {0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, UV),
                                                                                               sizeof(glm::vec2), UV},
                                    {1, 3, VK_FORMAT_R32_SFLOAT,       offsetof(ModelInstance,
                                                                                baseRot),      sizeof(float),     OTHER},
                                    {1, 4, VK_FORMAT_R32G32B32_SFLOAT, offsetof(ModelInstance,
                                                                                pos),          sizeof(glm::vec3), OTHER},
                                    {1, 5, VK_FORMAT_R32_SFLOAT,       offsetof(ModelInstance,
                                                                                instanceType), sizeof(float),     OTHER}
                            });

        // Pipelines [Shader couples]
        // The second parameter is the pointer to the vertex definition
        // Third and fourth parameters are respectively the vertex and fragment shaders
        // The last array, is a vector of pointer to the layouts of the sets that will
        // be used in this pipeline. The first element will be set 0, and so on
        PMesh.init(this, &VMesh, "shaders/ShaderVert.spv", "shaders/ShaderFrag.spv", {&DSLGubo, &DSLHouseBindings, &DSLMesh});
        PMesh.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        PMeshMultiTexture.init(this, &VMeshTexID, "shaders/ShaderVertMultiTexture.spv",
                               "shaders/ShaderFragMultiTexture.spv", {&DSLGubo, &DSLMeshMultiTex});
        PMeshMultiTexture.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        POverlay.init(this, &VOverlay, "shaders/OverlayVert.spv", "shaders/OverlayFrag.spv", {&DSLOverlay});
        POverlay.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        PVertexWithColors.init(this, &VVertexWithColor, "shaders/VColorVert.spv", "shaders/VColorFrag.spv",
                               {&DSLGubo, &DSLVertexWithColors});
        //PVertexWithColors.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        PMeshInstanced.init(this, &VMeshInstanced, "shaders/ShaderVertInstanced.spv", "shaders/ShaderFraInstanced.spv",
                            {&DSLGubo, &DSLHouseBindings, &DSLDoor, &DSLPositionedLights});
        PMeshInstanced.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        // Models, textures and Descriptors (values assigned to the uniforms)

        loadModels("models/furniture", this, &VMesh, &MV);
        loadModels("models/lights", this, &VMesh, &MV);

        // Creates a mesh with direct enumeration of vertices and indices
        initPolikeaSurroundings(&MPolikeaExternFloor.vertices,
                                &MPolikeaExternFloor.indices,
                                &MFence.vertices,
                                &MFence.indices,
                                getPolikeaBuildingPosition(), FRONTOFFSET, SIDEOFFSET, BACKOFFSET);
        MPolikeaExternFloor.initMesh(this, &VMesh);
        MFence.initMesh(this, &VMesh);

        MOverlay.vertices = {{{-0.7f, 0.70f}, {0.0f, 0.0f}},
                             {{-0.7f, 0.93f}, {0.0f, 1.0f}},
                             {{0.7f,  0.70f}, {1.0f, 0.0f}},
                             {{0.7f,  0.93f}, {1.0f, 1.0f}}};
        MOverlay.indices = {0, 1, 2, 3, 2, 1};
        MOverlay.initMesh(this, &VOverlay);


        auto floorplan = generateFloorplan(MAX_DIMENSION);
        floorPlanToVerIndexes(floorplan, MBuilding.vertices, MBuilding.indices, doors, &boundingRectangles,
                              &positionedLightPos, &roomCenters, &roomOccupiedArea);
        MBuilding.initMesh(this, &VMeshTexID);

        MPolikeaBuilding.init(this, &VVertexWithColor, "models/polikeaBuilding.obj", OBJ);

        MDoor.instanceBufferPresent = true;
        MDoor.instances.reserve(doors.size());
        for (auto &door: doors) {
            MDoor.instances.push_back({door.baseRot, door.doorPos, 0});
        }
        MDoor.init(this, &VMeshInstanced, "models/door_009_Mesh.112.mgcg", MGCG);

        MPositionedLights.instanceBufferPresent = true;
        MPositionedLights.instances.reserve(N_POS_LIGHTS);
        for (int i = 0; i < N_POS_LIGHTS; i++) {
            MPositionedLights.instances.push_back({0.0f, positionedLightPos[i], 1});
        }
        MPositionedLights.init(this, &VMeshInstanced, "models/lights/polilamp.mgcg", MGCG);

        // Create the textures
        // The second parameter is the file name
        TAsphalt.init(this, "textures/Asphalt.jpg");
        TFurniture.init(this, "textures/Textures_Furniture.png");
        TFence.init(this, "textures/Fence.jpg");
        TPlankWall.init(this, "textures/plank_wall.jpg");
        TBathFloor.init(this, "textures/bath_floor.jpg");
        TDarkFloor.init(this, "textures/dark_floor.jpg");
        TTiledStones.init(this, "textures/tiled_stones.jpg");
        TOverlayMoveObject.init(this, "textures/MoveBanner.png");
    }

    inline void
    loadModels(const std::string &path, VTemplate *thisVTemplate, VertexDescriptor *VMesh, std::vector<ModelInfo> *MV) {
        int i = 0;
        int polikeaBuildingOffsetsIndex = 0;
        bool loadingInPolikeaBuilding = path.find("furniture") != std::string::npos;

        for (const auto &entry: fs::directory_iterator(path)) {
            // Added this check since in MacOS this hidden file could be created in a directory
            if (static_cast<std::string>(entry.path()).find("DS_Store") != std::string::npos)
                continue;

            // TODO this is only to skip other lights
            if (static_cast<std::string>(entry.path()).find("lamp") != std::string::npos)
                continue;

            ModelInfo MI;
            // The second parameter is the pointer to the vertex definition for this model
            // The third parameter is the file name
            // The last is a constant specifying the file type: currently only OBJ or GLTF
            MI.model.init(thisVTemplate, VMesh, entry.path(), MGCG);
            if (polikeaBuildingOffsetsIndex < MAX_OBJECTS_IN_POLIKEA && loadingInPolikeaBuilding) {
                MI.modelPos = polikeaBuildingPosition + polikeaBuildingOffsets[polikeaBuildingOffsetsIndex];
                polikeaBuildingOffsetsIndex++;
            } else {
                MI.modelPos = glm::vec3(0.0f + i * 2, 0.0f, 0.0f);
            }
            if (MAX_OBJECTS_IN_POLIKEA - polikeaBuildingOffsetsIndex < 3 && loadingInPolikeaBuilding) {
                MI.modelRot = glm::radians(180.0f);
            } else {
                MI.modelRot = 0.0f;
            }

            MI.minCoords = glm::vec3(std::numeric_limits<float>::max());
            MI.maxCoords = glm::vec3(std::numeric_limits<float>::lowest());

            for (const auto &vertex: MI.model.vertices) {
                MI.minCoords = glm::min(MI.minCoords, vertex.pos);
                MI.maxCoords = glm::max(MI.maxCoords, vertex.pos);
            }
            MI.center = (MI.minCoords + MI.maxCoords) / 2.0f;
            MI.size = MI.maxCoords - MI.minCoords;

            MI.cylinderRadius = glm::distance(glm::vec3(MI.maxCoords.x, 0, MI.maxCoords.z),
                                              glm::vec3(MI.minCoords.x, 0, MI.minCoords.z)) / 2;
            MI.cylinderHeight = MI.maxCoords.y - MI.minCoords.y;

            MI.modelPos += glm::vec3(0.0, -std::min(0.0f, MI.minCoords.y), 0.0);

            MV->push_back(MI);
            i++;
        }
    }

    // Here you create your pipelines and Descriptor Sets!
    void pipelinesAndDescriptorSetsInit() {
        // This creates a new pipeline (with the current surface), using its shaders
        PMesh.create();
        PMeshMultiTexture.create();
        POverlay.create();
        PVertexWithColors.create();
        PMeshInstanced.create();

        // Here you define the data set
        DSPolikeaExternFloor.init(this, &DSLMesh, {
                // the second parameter is a pointer to the Uniform Set Layout of this set
                // the last parameter is an array, with one element per binding of the set.
                // first  element : the binding number
                // second element : UNIFORM or TEXTURE (an enum) depending on the type
                // third  element : only for UNIFORMS, the size of the corresponding C++ object. For texture, just put 0
                // fourth element : only for TEXTURES, the pointer to the corresponding texture object. For uniforms, use nullptr
                {0, UNIFORM, sizeof(UniformBlock),      nullptr},
                {1, TEXTURE, 0,                         &TAsphalt}
        });
        DSFence.init(this, &DSLMesh, {
                {0, UNIFORM, sizeof(UniformBlock),      nullptr},
                {1, TEXTURE, 0,                         &TFence}
        });
        DSHouseBindings.init(this, &DSLHouseBindings, {
                {0, UNIFORM, sizeof(HouseBindings),      nullptr}
        });
        DSGubo.init(this, &DSLGubo, {
                {0, UNIFORM, sizeof(GlobalUniformBlock), nullptr}
        });
        DSOverlayMoveOject.init(this, &DSLOverlay, {
                {0, UNIFORM, sizeof(OverlayUniformBlock), nullptr},
                {1, TEXTURE, 0,                           &TOverlayMoveObject}
        });
        DSPolikeaBuilding.init(this, &DSLVertexWithColors, {
                {0, UNIFORM, sizeof(UniformBlock),      nullptr}
        });
        DSDoor.init(this, &DSLDoor, {
                {0, UNIFORM, sizeof(UniformBlockDoors), nullptr},
                {1, TEXTURE, 0,                         &TFurniture}
        });
        DSPositionedLights.init(this, &DSLPositionedLights, {
                {0, UNIFORM, sizeof(UniformBlockPositionedLights), nullptr}
        });
        DSBuilding.init(this, &DSLMeshMultiTex, {
                {0, UNIFORM, sizeof(UniformBlock),      nullptr},
                {1, TEXTURE, 0,                         &TPlankWall,   0},
                {1, TEXTURE, 0,                         &TAsphalt,     1},
                {1, TEXTURE, 0,                         &TBathFloor,   2},
                {1, TEXTURE, 0,                         &TDarkFloor,   3},
                {1, TEXTURE, 0,                         &TTiledStones, 4}
        });

        for (auto &mInfo: MV) {
            mInfo.dsModel.init(this, &DSLMesh, {
                    {0, UNIFORM, sizeof(UniformBlock),      nullptr},
                    {1, TEXTURE, 0,                         &TFurniture}
            });
        }
    }

    // Here you destroy your pipelines and Descriptor Sets!
    // All the object classes defined in Starter.hpp have a method .cleanup() for this purpose
    void pipelinesAndDescriptorSetsCleanup() {
        // Cleanup pipelines
        PMesh.cleanup();
        PMeshInstanced.cleanup();
        POverlay.cleanup();
        PVertexWithColors.cleanup();
        PMeshMultiTexture.cleanup();

        // Cleanup datasets
        DSPolikeaExternFloor.cleanup();
        DSFence.cleanup();
        DSGubo.cleanup();
        DSOverlayMoveOject.cleanup();
        DSPolikeaBuilding.cleanup();
        DSDoor.cleanup();
        DSPositionedLights.cleanup();
        DSBuilding.cleanup();
        DSHouseBindings.cleanup();

        for (auto &mInfo: MV)
            mInfo.dsModel.cleanup();
    }

    // Here you destroy all the Models, Texture and Desc. Set Layouts you created!
    // All the object classes defined in Starter.hpp have a method .cleanup() for this purpose
    // You also have to destroy the pipelines: since they need to be rebuilt, they have two different
    // methods: .cleanup() recreates them, while .destroy() delete them completely
    void localCleanup() {
        // Cleanup textures
        TAsphalt.cleanup();
        TFurniture.cleanup();
        TFence.cleanup();
        TOverlayMoveObject.cleanup();
        TPlankWall.cleanup();
        TTiledStones.cleanup();
        TDarkFloor.cleanup();
        TBathFloor.cleanup();

        // Cleanup models
        MPolikeaExternFloor.cleanup();
        MFence.cleanup();
        MOverlay.cleanup();
        MPolikeaBuilding.cleanup();
        MDoor.cleanup();
        MPositionedLights.cleanup();
        MBuilding.cleanup();
        for (auto &mInfo: MV)
            mInfo.model.cleanup();

        // Cleanup descriptor set layouts
        DSLMesh.cleanup();
        DSLMeshMultiTex.cleanup();
        DSLGubo.cleanup();
        DSLOverlay.cleanup();
        DSLVertexWithColors.cleanup();
        DSLDoor.cleanup();
        DSLPositionedLights.cleanup();
        DSLHouseBindings.cleanup();

        // Destroys the pipelines
        PMesh.destroy();
        PMeshMultiTexture.destroy();
        POverlay.destroy();
        PVertexWithColors.destroy();
        PMeshInstanced.destroy();
    }

    // Here it is the creation of the command buffer:
    // You send to the GPU all the objects you want to draw,
    // with their buffers and textures

    void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
        // binds the pipeline
        // For a pipeline object, this command binds the corresponding pipeline to the command buffer passed in its parameter
        PMeshMultiTexture.bind(commandBuffer);

        // binds the data set
        // For a Dataset object, this command binds the corresponding dataset
        // to the command buffer and pipeline passed in its first and second parameters.
        // The third parameter is the number of the set being bound
        // As described in the Vulkan tutorial, a different dataset is required for each image in the swap chain.
        // This is done automatically in file Starter.hpp, however the command here needs also the index
        // of the current image in the swap chain, passed in its last parameter
        DSGubo.bind(commandBuffer, PMeshMultiTexture, 0, currentImage);

        DSBuilding.bind(commandBuffer, PMeshMultiTexture, 1, currentImage);
        MBuilding.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MBuilding.indices.size()), 1, 0, 0, 0);

        // binds the pipeline
        // For a pipeline object, this command binds the corresponding pipeline to the command buffer passed in its parameter
        PMesh.bind(commandBuffer);

        // binds the data set
        // For a Dataset object, this command binds the corresponding dataset
        // to the command buffer and pipeline passed in its first and second parameters.
        // The third parameter is the number of the set being bound
        // As described in the Vulkan tutorial, a different dataset is required for each image in the swap chain.
        // This is done automatically in file Starter.hpp, however the command here needs also the index
        // of the current image in the swap chain, passed in its last parameter
        DSGubo.bind(commandBuffer, PMesh, 0, currentImage);
        DSHouseBindings.bind(commandBuffer, PMesh, 1, currentImage);

        //--- GRID ---
        // binds the model
        // For a Model object, this command binds the corresponding index and vertex buffer
        // to the command buffer passed in its parameter
        DSPolikeaExternFloor.bind(commandBuffer, PMesh, 2, currentImage);
        MPolikeaExternFloor.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MPolikeaExternFloor.indices.size()), 1, 0, 0, 0);

        DSFence.bind(commandBuffer, PMesh, 2, currentImage);
        MFence.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MFence.indices.size()), 1, 0, 0, 0);
        // the second parameter is the number of indexes to be drawn. For a Model object,
        // this can be retrieved with the .indices.size() method.

        //--- MODELS ---
        for (auto &mInfo: MV) {
            mInfo.dsModel.bind(commandBuffer, PMesh, 2, currentImage);
            mInfo.model.bind(commandBuffer);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mInfo.model.indices.size()), 1, 0, 0, 0);
        }

        // --- PIPELINE OVERLAY ---
        POverlay.bind(commandBuffer);
        MOverlay.bind(commandBuffer);
        DSOverlayMoveOject.bind(commandBuffer, POverlay, 0, currentImage);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MOverlay.indices.size()), 1, 0, 0, 0);

        // --- PIPELINE VERTEX WITH COLORS ---
        PVertexWithColors.bind(commandBuffer);
        DSGubo.bind(commandBuffer, PVertexWithColors, 0, currentImage);
        DSPolikeaBuilding.bind(commandBuffer, PVertexWithColors, 1, currentImage);
        MPolikeaBuilding.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MPolikeaBuilding.indices.size()), 1, 0, 0, 0);

        //--- PIPELINE INSTANCED ---
        PMeshInstanced.bind(commandBuffer);
        DSGubo.bind(commandBuffer, PMeshInstanced, 0, currentImage);
        DSHouseBindings.bind(commandBuffer, PMeshInstanced, 1, currentImage);
        DSDoor.bind(commandBuffer, PMeshInstanced, 2, currentImage);
        DSPositionedLights.bind(commandBuffer, PMeshInstanced, 3, currentImage);

        MDoor.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MDoor.indices.size()), N_ROOMS - 1, 0, 0, 0);
        MPositionedLights.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MPositionedLights.indices.size()), N_POS_LIGHTS, 0, 0, 0);
    }

    // Here is where you update the uniforms.
    // Very likely this will be where you will be writing the logic of your application.
    void updateUniformBuffer(uint32_t currentImage) {
        // Standard procedure to quit when the ESC key is pressed
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        // ----- MOVE CAMERA AND OBJECTS LOGIC ----- //
        const float ROT_SPEED = glm::radians(120.0f);
        const float MOVE_SPEED = 4.0f;

        static bool debounce, lightDebounce, roomCyclingDebounce = false;
        static int curDebounce, curLightDebounce, curRoomCyclingDebounce = 0;

        float deltaT;
        auto m = glm::vec3(0.0f), r = glm::vec3(0.0f);
        bool fire, lightSwitch, cycleRoom = false;
        getSixAxis(deltaT, m, r, fire, lightSwitch, cycleRoom);

        CamAlpha = CamAlpha - ROT_SPEED * deltaT * r.y;
        CamBeta = CamBeta - ROT_SPEED * deltaT * r.x;
        CamBeta = CamBeta < glm::radians(-90.0f) ? glm::radians(-90.0f) :
                  (CamBeta > glm::radians(90.0f) ? glm::radians(90.0f) : CamBeta);
        CamRho = CamRho - ROT_SPEED * deltaT * r.z;
        CamRho = CamRho < glm::radians(-180.0f) ? glm::radians(-180.0f) :
                 (CamRho > glm::radians(180.0f) ? glm::radians(180.0f) : CamRho);

        glm::mat3 CamDir = glm::rotate(glm::mat4(1.0f), CamAlpha, glm::vec3(0, 1, 0)) *
                           glm::rotate(glm::mat4(1.0f), CamBeta, glm::vec3(1, 0, 0)) *
                           glm::rotate(glm::mat4(1.0f), CamRho, glm::vec3(0, 0, 1));

        glm::vec3 ux = glm::rotate(glm::mat4(1.0f), CamAlpha, glm::vec3(0, 1, 0)) * glm::vec4(1, 0, 0, 1);
        glm::vec3 uz = glm::rotate(glm::mat4(1.0f), CamAlpha, glm::vec3(0, 1, 0)) * glm::vec4(0, 0, -1, 1);

        glm::vec3 oldCamPos = CamPos;
        CamPos = CamPos + MOVE_SPEED * m.x * ux * deltaT;
        CamPos = CamPos + MOVE_SPEED * m.y * glm::vec3(0, 1, 0) * deltaT; //Do not allow to fly
        CamPos = CamPos + MOVE_SPEED * m.z * uz * deltaT;

        for (auto &boundingRectangle: boundingRectangles)
            if (checkIfInBoundingRectangle(CamPos, boundingRectangle))
                CamPos = oldCamPos;

        if (!OnlyMoveCam) {
            //Checks to see if an object can be bought
            if (!MV[MoveObjIndex].hasBeenBought) {
                bool isObjectAllowedToMove = true;
                for (auto &i: MV)
                    isObjectAllowedToMove = isObjectAllowedToMove && !(i.modelPos == roomCenters[0]);
                if (isObjectAllowedToMove) {
                    MV[MoveObjIndex].modelPos = roomCenters[0];
                    MV[MoveObjIndex].hasBeenBought = true;
                }
                OnlyMoveCam = true;
            } else {
                const glm::vec3 modelPos = glm::vec3(CamPos.x, CamPos.y - 1.0f, CamPos.z - 2.0f);
                glm::vec3 oldPos = MV[MoveObjIndex].modelPos;
                MV[MoveObjIndex].modelPos = rotateTargetRespectToCam(CamPos, CamAlpha, CamBeta, modelPos);

                if (MV[MoveObjIndex].modelPos.y < 0.0f) MV[MoveObjIndex].modelPos.y = 0.0f;

                for (int i = 0; i < MV.size(); i++) {
                    if (i != MoveObjIndex) {
                        if (MV[MoveObjIndex].checkCollision(MV[i])) {
                            MV[MoveObjIndex].modelPos = oldPos;
                            break;
                        }
                    }
                }

                MV[MoveObjIndex].modelRot = CamAlpha;

                if (cycleRoom) {
                    if (!roomCyclingDebounce) {
                        roomCyclingDebounce = true;
                        curRoomCyclingDebounce = GLFW_KEY_0;

                        if (MV[MoveObjIndex].roomCycling < N_ROOMS) {
                            MV[MoveObjIndex].roomCycling++;
                            if (MV[MoveObjIndex].roomCycling >= N_ROOMS)
                                MV[MoveObjIndex].roomCycling = 0;
                            CamPos = roomCenters[MV[MoveObjIndex].roomCycling] + glm::vec3(0.0f, 1.0f, 0.0f);
                            MV[MoveObjIndex].modelPos = rotateTargetRespectToCam(CamPos, CamAlpha, CamBeta, modelPos);
                        }
                    }
                } else if ((curRoomCyclingDebounce == GLFW_KEY_0) && roomCyclingDebounce) {
                    roomCyclingDebounce = false;
                    curRoomCyclingDebounce = 0;
                }

                if (!checkIfInBoundingRectangle(MV[MoveObjIndex].modelPos,
                                                roomOccupiedArea[MV[MoveObjIndex].roomCycling],
                                                -MV[MoveObjIndex].cylinderRadius)) {
                    MV[MoveObjIndex].modelPos = oldPos;
                }
            }
        }

        for (auto &Door: doors) {
            if (glm::distance(CamPos, Door.doorPos) <= 1.5 && Door.doorState == CLOSED) {
                Door.doorState = OPENING;
            }
            if (glm::distance(CamPos, Door.doorPos) > 1.5 && Door.doorState == OPEN) {
                Door.doorState = CLOSING;
            }
            if (Door.doorState == OPENING) {
                Door.doorRot -= Door.doorSpeed * deltaT;
                if (Door.doorRot <= -glm::radians(90.0f)) {
                    Door.doorState = WAITING_OPEN;
                    Door.doorRot = -glm::radians(90.0f);
                }
            }
            if (Door.doorState == WAITING_OPEN) {
                Door.time += deltaT;
                if (Door.time > 5) {
                    Door.time = 0.0f;
                    Door.doorState = OPEN;
                }
            }
            if (Door.doorState == CLOSING) {
                Door.doorRot += Door.doorSpeed * deltaT;
                if (Door.doorRot >= 0.0f) {
                    Door.doorState = CLOSED;
                    Door.doorRot = 0.0f;
                }
            }
        }

        static bool turnOffLight = false;
        if (lightSwitch) {
            if (!lightDebounce) {
                lightDebounce = true;
                curLightDebounce = GLFW_KEY_L;
                turnOffLight = !turnOffLight;
            }
        } else if ((curLightDebounce == GLFW_KEY_L) && lightDebounce) {
            lightDebounce = false;
            curLightDebounce = 0;
        }

        float threshold = 2.0f;
        if (fire) {
            if (!debounce) {
                debounce = true;
                curDebounce = GLFW_KEY_SPACE;
                OnlyMoveCam = !OnlyMoveCam;

                if (!OnlyMoveCam) {
                    float distance = glm::distance(CamPos, MV[0].modelPos);
                    MoveObjIndex = 0;
                    for (std::size_t i = 1; i < MV.size(); ++i) {
                        float newDistance = glm::distance(CamPos, MV[i].modelPos);
                        if (newDistance < distance) {
                            distance = newDistance;
                            MoveObjIndex = i;
                        }
                    }
                    if (distance > threshold)
                        OnlyMoveCam = true;
                }
            }
        } else if ((curDebounce == GLFW_KEY_SPACE) && debounce) {
            debounce = false;
            curDebounce = 0;
        }

        if (glfwGetKey(window, GLFW_KEY_H)) {
            CamPos = roomCenters[0] + glm::vec3(0.0f, 1.0, 3.0f);
            CamAlpha = CamBeta = CamRho = 0.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_K)) {
            CamPos = polikeaBuildingPosition + glm::vec3(5.0f, 1.0, 5.0f);
            CamAlpha = CamBeta = CamRho = 0.0f;
        }

        gubo.DlightDir = glm::normalize(glm::vec3(1, 2, 3));
        gubo.DlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gubo.AmbLightColor = glm::vec3(1.0f);
        gubo.eyePos = CamPos;

        size_t indexSpot = 0;
        size_t indexPoint = 0;
        for (auto &modelInfo: MV) {
            for (auto light: modelInfo.model.lights) {
                if (light.type == SPOT) {
                    gubo.spotLights[indexSpot].beta = light.parameters.spot.beta;
                    gubo.spotLights[indexSpot].g = light.parameters.spot.g;
                    gubo.spotLights[indexSpot].cosout = light.parameters.spot.cosout;
                    gubo.spotLights[indexSpot].cosin = light.parameters.spot.cosin;
                    gubo.spotLights[indexSpot].lightPos = glm::vec3(
                            glm::rotate(glm::mat4(1.0), modelInfo.modelRot, glm::vec3(0, 1, 0)) *
                            glm::vec4(light.position, 1.0f)) + modelInfo.modelPos;
                    gubo.spotLights[indexSpot].lightDir =
                            glm::rotate(glm::mat4(1.0), modelInfo.modelRot, glm::vec3(0, 1, 0)) *
                            glm::vec4(light.direction, 1.0f);
                    gubo.spotLights[indexSpot].lightColor = glm::vec4(light.lightColor, 1.0f);
                    indexSpot++;
                } else if (light.type == POINT) {
                    gubo.pointLights[indexPoint].beta = light.parameters.point.beta;
                    gubo.pointLights[indexPoint].g = light.parameters.point.g;
                    gubo.pointLights[indexPoint].lightPos =
                            glm::vec3(glm::vec4(light.position, 1.0f)) + modelInfo.modelPos;
                    gubo.pointLights[indexPoint].lightColor = glm::vec4(light.lightColor, 1.0f);
                    indexPoint++;
                }
            }
        }

        for (int i = 0; i < N_POS_LIGHTS; i++) {
            for (auto light: MPositionedLights.lights) {
                gubo.pointLights[indexPoint].beta = light.parameters.point.beta;
                gubo.pointLights[indexPoint].g = light.parameters.point.g;
                gubo.pointLights[indexPoint].lightPos =
                        glm::vec3(glm::vec4(light.position, 1.0f)) + positionedLightPos[i];
                gubo.pointLights[indexPoint].lightColor = glm::vec4(
                        turnOffLight ? glm::vec3(0.0f, 0.0f, 0.0f) : light.lightColor, 1.0f);
                indexPoint++;
            }
        }
        gubo.nSpotLights = indexSpot;
        gubo.nPointLights = indexPoint;
        DSGubo.map(currentImage, &gubo, sizeof(gubo), 0);

        glm::mat4 ViewPrj = MakeViewProjectionMatrix(Ar, CamAlpha, CamBeta, CamRho, CamPos);

        uboPolikea.amb = 0.05f;
        uboPolikea.gamma = 180.0f;
        uboPolikea.sColor = glm::vec3(1.0f);
        uboPolikea.worldMat = glm::translate(glm::mat4(1), polikeaBuildingPosition) * glm::scale(glm::mat4(1), glm::vec3(5.0f));
        uboPolikea.nMat = glm::inverse(uboPolikea.worldMat);
        uboPolikea.mvpMat = ViewPrj * uboPolikea.worldMat;
        DSPolikeaBuilding.map(currentImage, &uboPolikea, sizeof(uboPolikea), 0);

        uboBuilding.amb = 0.05f;
        uboBuilding.gamma = 180.0f;
        uboBuilding.sColor = glm::vec3(1.0f);
        uboBuilding.worldMat = glm::mat4(1.0f);
        uboBuilding.nMat = glm::inverse(uboBuilding.worldMat);
        uboBuilding.mvpMat = ViewPrj * uboBuilding.worldMat;
        DSBuilding.map(currentImage, &uboBuilding, sizeof(uboBuilding), 0);

        bool displayKey = false;
        for (std::size_t i = 1; i < MV.size(); ++i) {
            float distance = glm::distance(CamPos, MV[i].modelPos);
            if (distance <= threshold) {
                displayKey = true;
                break;
            }
        }

        uboKey.visible = (OnlyMoveCam && displayKey) ? 1.0f : 0.0f;
        DSOverlayMoveOject.map(currentImage, &uboKey, sizeof(uboKey), 0);

        for (int i = 0; i < N_ROOMS; i++)
            uboHouseBindings.roomsArea[i] = roomOccupiedArea[i];
        DSHouseBindings.map(currentImage, &uboHouseBindings, sizeof(uboHouseBindings), 0);

        uboPolikeaExternFloor.amb = 0.05f;
        uboPolikeaExternFloor.gamma = 180.0f;
        uboPolikeaExternFloor.sColor = glm::vec3(1.0f);
        uboPolikeaExternFloor.worldMat = glm::mat4(1.0f);
        uboPolikeaExternFloor.nMat = glm::inverse(uboPolikeaExternFloor.worldMat);
        uboPolikeaExternFloor.mvpMat = ViewPrj * uboPolikeaExternFloor.worldMat;
        // the .map() method of a DataSet object, requires the current image of the swap chain as first parameter
        // the second parameter is the pointer to the C++ data structure to transfer to the GPU
        // the third parameter is its size
        // the fourth parameter is the location inside the descriptor set of this uniform block
        DSPolikeaExternFloor.map(currentImage, &uboPolikeaExternFloor, sizeof(uboPolikeaExternFloor), 0);

        uboFence.amb = 0.05f;
        uboFence.gamma = 180.0f;
        uboFence.sColor = glm::vec3(1.0f);
        uboFence.worldMat = glm::mat4(1.0f);
        uboFence.nMat = glm::inverse(uboFence.worldMat);
        uboFence.mvpMat = ViewPrj * uboFence.worldMat;
        DSFence.map(currentImage, &uboFence, sizeof(uboFence), 0);

        uboDoor.amb = 0.05f;
        uboDoor.gamma = 180.0f;
        uboDoor.sColor = glm::vec3(1.0f);
        uboDoor.prjViewMat = ViewPrj;
        for (int i = 0; i < N_ROOMS - 1; i++)
            uboDoor.door[i] = glm::vec4(doors[i].doorRot);
        DSDoor.map(currentImage, &uboDoor, sizeof(uboDoor), 0);

        uboPositionedLights.amb = 0.05f;
        uboPositionedLights.gamma = 180.0f;
        uboPositionedLights.sColor = glm::vec3(1.0f);
        uboPositionedLights.prjViewMat = ViewPrj;
        for (auto &light: uboPositionedLights.lights)
            light = glm::vec4(0.0f);
        DSPositionedLights.map(currentImage, &uboPositionedLights, sizeof(uboPositionedLights), 0);

        for (auto &mInfo: MV) {
            glm::mat4 World = MakeWorldMatrix(mInfo.modelPos, mInfo.modelRot, glm::vec3(1.0f, 1.0f, 1.0f)) * glm::mat4(1.0f);;
            mInfo.modelUBO.amb = 0.05f;
            mInfo.modelUBO.gamma = 180.0f;
            mInfo.modelUBO.sColor = glm::vec3(1.0f);
            mInfo.modelUBO.worldMat = World;
            mInfo.modelUBO.nMat = glm::inverse(World);
            mInfo.modelUBO.mvpMat = ViewPrj * World;
            mInfo.dsModel.map(currentImage, &mInfo.modelUBO, sizeof(mInfo.modelUBO), 0);
        }
    }
};


// This is the main: probably you do not need to touch this!
int main() {
    VTemplate app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
