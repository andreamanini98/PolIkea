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

namespace fs = std::filesystem;

namespace std {
    template<>
    struct hash<glm::vec3> {
        size_t operator()(const glm::vec3 &t) const {
            // Calculate the hash using the member variables
            size_t seed = 0;
            hash<float> hasher;
            seed ^= hasher(t.x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hasher(t.y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= hasher(t.z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

//REMEMBER TO UPDATE THIS FIELD ALSO IN THE ShaderInstanced.vert
#define N_ROOMS 6

#define MIN_DIMENSION 12.5f
#define MAX_DIMENSION 18.0f
#define DOOR_HWIDTH 0.5f

#define ROOM_CEILING_HEIGHT 3.0f
#define DOOR_HEIGHT 2.5f

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

struct DoorModelInstance {
    glm::vec3 pos;
    float baseRot;
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

enum Direction {
    NORTH,
    EAST,
    SOUTH,
    WEST
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
    alignas(16) glm::mat4 mMat;
    alignas(16) glm::mat4 nMat;
};

struct UniformBlockDoors {
    alignas(4) float amb;
    alignas(4) float gamma;
    alignas(16) glm::vec3 sColor;
    alignas(16) glm::mat4 mvpMat;
    alignas(16) glm::mat4 mMat;
    alignas(16) glm::mat4 nMat;
    alignas(16) glm::vec4 door[N_ROOMS - 1];
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

// The vertices data structures
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


class VertexStorage {
    uint32_t vertexCurIdx = 0;
    std::vector<VertexVColor> &vPos;
    std::vector<uint32_t> &vIdx;
    std::vector<OpenableDoor> &openableDoors;
public:
    VertexStorage(
            std::vector<VertexVColor> &vPos,
            std::vector<uint32_t> &vIdx,
            std::vector<OpenableDoor> &openableDoors
    ) : vPos(vPos), vIdx(vIdx), vertexCurIdx(vPos.size()), openableDoors(openableDoors) {}

    uint32_t addVertex(VertexVColor color) {
        vPos.push_back(color);
        return vertexCurIdx++;
    }

    void drawRect(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, int vecDir, glm::vec3 color) {
        glm::vec3 norm = glm::normalize(glm::cross(v1 - v0, v2 - v0)) * (vecDir > 0 ? 1.0f : -1.0f);
        auto i0 = addVertex({v0, norm, color});
        auto i1 = addVertex({v1, norm, color});
        auto i2 = addVertex({v2, norm, color});
        auto i3 = addVertex({v3, norm, color});

        //printf("%d %d %d %d\n", i0, i1, i2, i3);

        if (vecDir > 0) {
            addIndex(i0, i1, i2);
            addIndex(i2, i3, i0);
        } else {
            addIndex(i2, i1, i0);
            addIndex(i0, i3, i2);
        }
    }

    void drawRectWithOpening(glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3, int vecDir, glm::vec3 color,
                             float openingOffset, Direction doordirection) {
        glm::vec3 norm = glm::normalize(glm::cross(v1 - v0, v2 - v0)) * (vecDir > 0 ? 1.0f : -1.0f);

        glm::vec3 openingDir = glm::normalize(v1 - v0); //TODO
        glm::vec3 openingUpDir = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 openingV0 = openingDir * (openingOffset - DOOR_HWIDTH) + v0;
        glm::vec3 openingV1 = openingDir * (openingOffset + DOOR_HWIDTH) + v0;
        glm::vec3 openingV2 = openingV1 + openingUpDir * DOOR_HEIGHT;
        glm::vec3 openingV3 = openingV0 + openingUpDir * DOOR_HEIGHT;

        glm::vec3 ceilingOpeningV03 = openingV0 + openingUpDir * ROOM_CEILING_HEIGHT;
        glm::vec3 ceilingOpeningV12 = openingV1 + openingUpDir * ROOM_CEILING_HEIGHT;

        drawRect(v0, openingV0, ceilingOpeningV03, v3, vecDir, color);
        drawRect(openingV3, openingV2, ceilingOpeningV12, ceilingOpeningV03, vecDir, color);
        drawRect(openingV1, v1, v2, ceilingOpeningV12, vecDir, color);

        printf("%f %f %f\n", openingV0.x, openingV0.y, openingV0.z);

        if (/*doorIndices.find(openingV0) == doorIndices.end()*/ doordirection == NORTH || doordirection == EAST) {
            //doorIndices.insert(openingV0);
            float rotType;
            if (doordirection == NORTH || doordirection == SOUTH) {
                rotType = glm::radians(0.0f);
            } else {
                rotType = glm::radians(90.0f);
            }
            openableDoors.push_back(
                    OpenableDoor{openingV0, rotType, 0.0f, glm::radians(90.0f), glm::radians(90.0f), CLOSED,
                                 CLOCKWISE});
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

            currentX += roomWidth;
            minWidth = MIN_DIMENSION;
            minDepth = distribution_door(gen);
            prevDoor = Door{minDepth, Direction::EAST};
        } else {
            std::uniform_real_distribution<float> distribution_door(0 + DOOR_HWIDTH, roomWidth - DOOR_HWIDTH);

            currentY += roomDepth;
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

inline glm::vec3
floorPlanToVerIndexes(const std::vector<Room> &rooms, std::vector<VertexVColor> &vPos, std::vector<uint32_t> &vIdx,
                      std::vector<OpenableDoor> &openableDoors) {
    VertexStorage storage(vPos, vIdx, openableDoors);
    int test = 0;
    glm::vec3 startingRoomCenter = glm::vec3(0.0f, 0.0f, 0.0f);

    for (auto &room: rooms) {
        if (test == 0) {
            startingRoomCenter.x = room.startX + room.width / 2;
            startingRoomCenter.z = room.startY + room.depth / 2;
        }

        auto color = glm::vec3((test % 3) == 0, (test % 3) == 1, (test % 3) == 2);

        storage.drawRect(
                glm::vec3(room.startX, 0, room.startY),
                glm::vec3(room.startX + room.width, 0, room.startY),
                glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                glm::vec3(room.startX, 0, room.startY + room.depth),
                -1,
                color
        );

        storage.drawRect(
                glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                1,
                color
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

        if (hasDoorSouth) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    color,
                    offsetSouth, SOUTH
            );
        } else {
            storage.drawRect(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    color
            );
        }

        if (hasDoorNorth) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    -1,
                    color,
                    offsetNorth, NORTH
            );
        } else {
            storage.drawRect(
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    -1,
                    color
            );
        }

        if (hasDoorWest) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    -1,
                    color,
                    offsetWest, WEST
            );
        } else {
            storage.drawRect(
                    glm::vec3(room.startX, 0, room.startY),
                    glm::vec3(room.startX, 0, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX, ROOM_CEILING_HEIGHT, room.startY),
                    -1,
                    color
            );
        }

        if (hasDoorEast) {
            storage.drawRectWithOpening(
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    color,
                    offsetEast, EAST
            );
        } else {
            storage.drawRect(
                    glm::vec3(room.startX + room.width, 0, room.startY),
                    glm::vec3(room.startX + room.width, 0, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY + room.depth),
                    glm::vec3(room.startX + room.width, ROOM_CEILING_HEIGHT, room.startY),
                    1,
                    color
            );
        }

        test++;
    }

    return startingRoomCenter;
}

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


// MAIN !
class VTemplate : public BaseProject {
protected:

    // Current aspect ratio (used by the callback that resized the window)
    float Ar;

    // Descriptor Layouts ["classes" of what will be passed to the shaders]
    DescriptorSetLayout DSLMesh, DSLDoor, DSLGubo, DSLOverlay, DSLVertexWithColors;

    // Vertex formats
    VertexDescriptor VMesh, VOverlay, VVertexWithColor, VMeshInstanced;

    // Pipelines [Shader couples]
    Pipeline PMesh, POverlay, PVertexWithColors, PMeshInstanced;

    // Models, textures and Descriptors (values assigned to the uniforms)
    // Please note that Model objects depends on the corresponding vertex structure
    // Models
    Model<Vertex> MFloorGrid;
    Model<VertexOverlay> MOverlay;
    Model<VertexVColor> MPolikeaBuilding;
    Model<VertexVColor> MBuilding;

    // Descriptor sets
    DescriptorSet DSFloorGrid, DSGubo, DSOverlayMoveOject, DSPolikeaBuilding, DSBuilding;
    // Textures
    Texture T1, T2, TOverlayMoveObject;
    // C++ storage for uniform variables
    UniformBlock uboGrid, uboPolikea, uboBuilding;
    GlobalUniformBlock gubo;
    OverlayUniformBlock uboKey;

    // Other application parameters
    // A vector containing one element for each model loaded where we want to keep track of its information
    std::vector<ModelInfo> MV;

    glm::vec3 polikeaBuildingPosition = glm::vec3(5.0f, 0.0f, -15.0f);
    std::vector<glm::vec3> polikeaBuildingOffsets = getPolikeaBuildingOffsets();
    std::vector<BoundingRectangle> boundingRectangles = getBoundingRectangles(polikeaBuildingPosition);

    glm::vec3 CamPos = glm::vec3(2.0, 0.7, 3.45706);;
    float CamAlpha = 0.0f;
    float CamBeta = 0.0f;
    float CamRho = 0.0f;
    bool OnlyMoveCam = true;
    uint32_t MoveObjIndex = 0;
    glm::vec3 startingRoomCenter;

    Model<Vertex, DoorModelInstance> MDoor;
    DescriptorSet DSDoor;
    UniformBlockDoors uboDoor;

    std::vector<OpenableDoor> doors;

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
        setsInPool = 100;

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
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
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
                {0, sizeof(Vertex),            VK_VERTEX_INPUT_RATE_VERTEX},
                {1, sizeof(DoorModelInstance), VK_VERTEX_INPUT_RATE_INSTANCE}
        }, {
                                    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, pos),
                                            sizeof(glm::vec3), POSITION},
                                    {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, norm),
                                            sizeof(glm::vec3), NORMAL},
                                    {0, 2, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, UV),
                                            sizeof(glm::vec2), UV},
                                    {1, 3, VK_FORMAT_R32G32B32_SFLOAT, offsetof(DoorModelInstance, pos),
                                            sizeof(glm::vec3), OTHER},
                                    {1, 4, VK_FORMAT_R32_SFLOAT,       offsetof(DoorModelInstance, baseRot),
                                            sizeof(float),     OTHER}
                            });

        // Pipelines [Shader couples]
        // The second parameter is the pointer to the vertex definition
        // Third and fourth parameters are respectively the vertex and fragment shaders
        // The last array, is a vector of pointer to the layouts of the sets that will
        // be used in this pipeline. The first element will be set 0, and so on
        PMesh.init(this, &VMesh, "shaders/ShaderVert.spv", "shaders/ShaderFrag.spv", {&DSLGubo, &DSLMesh});
        PMesh.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        POverlay.init(this, &VOverlay, "shaders/OverlayVert.spv", "shaders/OverlayFrag.spv", {&DSLOverlay});
        POverlay.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        PVertexWithColors.init(this, &VVertexWithColor, "shaders/VColorVert.spv", "shaders/VColorFrag.spv",
                               {&DSLGubo, &DSLVertexWithColors});
        //PVertexWithColors.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        PMeshInstanced.init(this, &VMeshInstanced, "shaders/ShaderVertInstanced.spv", "shaders/ShaderFrag.spv",
                            {&DSLGubo, &DSLDoor});
        PMeshInstanced.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);


        // Models, textures and Descriptors (values assigned to the uniforms)

        loadModels("models/furniture", this, &VMesh, &MV);
        loadModels("models/lights", this, &VMesh, &MV);

        // Creates a mesh with direct enumeration of vertices and indices
        MFloorGrid.vertices = {{{-6, 0, -6}, {0.0, 1.0, 0.0}, {0.0f, 0.0f}},
                               {{-6, 0, 6},  {0.0, 1.0, 0.0}, {0.0f, 1.0f}},
                               {{6,  0, -6}, {0.0, 1.0, 0.0}, {1.0f, 0.0f}},
                               {{6,  0, 6},  {0.0, 1.0, 0.0}, {1.0f, 1.0f}}};
        MFloorGrid.indices = {0, 1, 2, 1, 3, 2};
        MFloorGrid.initMesh(this, &VMesh);

        MOverlay.vertices = {{{-0.7f, 0.70f}, {0.0f, 0.0f}},
                             {{-0.7f, 0.93f}, {0.0f, 1.0f}},
                             {{0.7f,  0.70f}, {1.0f, 0.0f}},
                             {{0.7f,  0.93f}, {1.0f, 1.0f}}};
        MOverlay.indices = {0, 1, 2, 3, 2, 1};
        MOverlay.initMesh(this, &VOverlay);


        auto floorplan = generateFloorplan(MAX_DIMENSION);
        startingRoomCenter = floorPlanToVerIndexes(floorplan, MBuilding.vertices, MBuilding.indices, doors);
        MBuilding.initMesh(this, &VVertexWithColor);

        MPolikeaBuilding.init(this, &VVertexWithColor, "models/polikeaBuilding.obj", OBJ);

        MDoor.instanceBufferPresent = true;
        MDoor.instances.reserve(doors.size());
        for (auto &door: doors) {
            MDoor.instances.push_back({door.doorPos, door.baseRot});
        }
        MDoor.init(this, &VMeshInstanced, "models/door_009_Mesh.112.mgcg", MGCG);

        // Create the textures
        // The second parameter is the file name
        T1.init(this, "textures/Checker.png");
        T2.init(this, "textures/Textures_Forniture.png");
        TOverlayMoveObject.init(this, "textures/MoveBanner.png");

        // Init local variables
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
        POverlay.create();
        PVertexWithColors.create();
        PMeshInstanced.create();

        // Here you define the data set
        DSFloorGrid.init(this, &DSLMesh, {
                // the second parameter is a pointer to the Uniform Set Layout of this set
                // the last parameter is an array, with one element per binding of the set.
                // first  element : the binding number
                // second element : UNIFORM or TEXTURE (an enum) depending on the type
                // third  element : only for UNIFORMS, the size of the corresponding C++ object. For texture, just put 0
                // fourth element : only for TEXTURES, the pointer to the corresponding texture object. For uniforms, use nullptr
                {0, UNIFORM, sizeof(UniformBlock), nullptr},
                {1, TEXTURE, 0,                    &T1}
        });
        DSGubo.init(this, &DSLGubo, {
                {0, UNIFORM, sizeof(GlobalUniformBlock), nullptr}
        });
        DSOverlayMoveOject.init(this, &DSLOverlay, {
                {0, UNIFORM, sizeof(OverlayUniformBlock), nullptr},
                {1, TEXTURE, 0,                           &TOverlayMoveObject}
        });
        DSPolikeaBuilding.init(this, &DSLVertexWithColors, {
                {0, UNIFORM, sizeof(UniformBlock), nullptr}
        });
        DSDoor.init(this, &DSLDoor, {
                {0, UNIFORM, sizeof(UniformBlockDoors), nullptr},
                {1, TEXTURE, 0,                         &T2}
        });

        DSBuilding.init(this, &DSLVertexWithColors, {
                {0, UNIFORM, sizeof(UniformBlock), nullptr}
        });

        for (auto &mInfo: MV) {
            mInfo.dsModel.init(this, &DSLMesh, {
                    {0, UNIFORM, sizeof(UniformBlock), nullptr},
                    {1, TEXTURE, 0,                    &T2}
            });
        }
    }

    // Here you destroy your pipelines and Descriptor Sets!
    // All the object classes defined in Starter.hpp have a method .cleanup() for this purpose
    void pipelinesAndDescriptorSetsCleanup() {
        // Cleanup pipelines
        PMesh.cleanup();
        POverlay.cleanup();
        PVertexWithColors.cleanup();
        PMeshInstanced.cleanup();

        // Cleanup datasets
        DSFloorGrid.cleanup();
        DSGubo.cleanup();
        DSOverlayMoveOject.cleanup();
        DSPolikeaBuilding.cleanup();
        DSDoor.cleanup();
        DSBuilding.cleanup();

        for (auto &mInfo: MV)
            mInfo.dsModel.cleanup();
    }

    // Here you destroy all the Models, Texture and Desc. Set Layouts you created!
    // All the object classes defined in Starter.hpp have a method .cleanup() for this purpose
    // You also have to destroy the pipelines: since they need to be rebuilt, they have two different
    // methods: .cleanup() recreates them, while .destroy() delete them completely
    void localCleanup() {
        // Cleanup textures
        T1.cleanup();
        T2.cleanup();
        TOverlayMoveObject.cleanup();

        // Cleanup models
        MFloorGrid.cleanup();
        MOverlay.cleanup();
        MPolikeaBuilding.cleanup();
        MDoor.cleanup();
        MBuilding.cleanup();
        for (auto &mInfo: MV)
            mInfo.model.cleanup();

        // Cleanup descriptor set layouts
        DSLMesh.cleanup();
        DSLGubo.cleanup();
        DSLOverlay.cleanup();
        DSLVertexWithColors.cleanup();
        DSLDoor.cleanup();

        // Destroys the pipelines
        PMesh.destroy();
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
        PMesh.bind(commandBuffer);

        // binds the data set
        // For a Dataset object, this command binds the corresponding dataset
        // to the command buffer and pipeline passed in its first and second parameters.
        // The third parameter is the number of the set being bound
        // As described in the Vulkan tutorial, a different dataset is required for each image in the swap chain.
        // This is done automatically in file Starter.hpp, however the command here needs also the index
        // of the current image in the swap chain, passed in its last parameter
        DSGubo.bind(commandBuffer, PMesh, 0, currentImage);

        //--- GRID ---
        DSFloorGrid.bind(commandBuffer, PMesh, 1, currentImage);
        // binds the model
        // For a Model object, this command binds the corresponding index and vertex buffer
        // to the command buffer passed in its parameter
        MFloorGrid.bind(commandBuffer);
        //vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MFloorGrid.indices.size()), 1, 0, 0, 0);
        // the second parameter is the number of indexes to be drawn. For a Model object,
        // this can be retrieved with the .indices.size() method.

        //--- MODELS ---
        for (auto &mInfo: MV) {
            mInfo.dsModel.bind(commandBuffer, PMesh, 1, currentImage);
            mInfo.model.bind(commandBuffer);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mInfo.model.indices.size()), 1, 0, 0, 0);
        }

        //--- PIPELINE OVERLAY ---
        POverlay.bind(commandBuffer);
        MOverlay.bind(commandBuffer);
        DSOverlayMoveOject.bind(commandBuffer, POverlay, 0, currentImage);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MOverlay.indices.size()), 1, 0, 0, 0);

        PVertexWithColors.bind(commandBuffer);
        DSGubo.bind(commandBuffer, PVertexWithColors, 0, currentImage);
        DSPolikeaBuilding.bind(commandBuffer, PVertexWithColors, 1, currentImage);
        MPolikeaBuilding.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MPolikeaBuilding.indices.size()), 1, 0, 0, 0);

        DSBuilding.bind(commandBuffer, PVertexWithColors, 1, currentImage);
        MBuilding.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MBuilding.indices.size()), 1, 0, 0, 0);

        PMeshInstanced.bind(commandBuffer);
        DSGubo.bind(commandBuffer, PMeshInstanced, 0, currentImage);
        DSDoor.bind(commandBuffer, PMeshInstanced, 1, currentImage);
        MDoor.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MDoor.indices.size()), N_ROOMS - 1, 0, 0, 0);
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

        static bool debounce = false;
        static int curDebounce = 0;
        static bool doorDebounce = false;
        static int curDoorDebounce = 0;

        float deltaT;
        auto m = glm::vec3(0.0f), r = glm::vec3(0.0f);
        bool fire = false;
        //static bool openDoor = false;
        getSixAxis(deltaT, m, r, fire /*openDoor see line 1833 in Starter.hpp. Useless if doors are automatic*/);

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
        CamPos = CamPos + MOVE_SPEED * m.x * ux * deltaT;
        CamPos = CamPos + MOVE_SPEED * m.y * glm::vec3(0, 1, 0) * deltaT; //Do not allow to fly
        CamPos = CamPos + MOVE_SPEED * m.z * uz * deltaT;

        for (auto &boundingRectangle: boundingRectangles) {
            if (CamPos.x >= boundingRectangle.bottomLeft.x && CamPos.x <= boundingRectangle.topRight.x &&
                CamPos.z <= boundingRectangle.bottomLeft.z && CamPos.z >= boundingRectangle.topRight.z) {
                CamPos = CamPos - MOVE_SPEED * m.x * ux * deltaT;
                CamPos = CamPos - MOVE_SPEED * m.y * glm::vec3(0, 1, 0) * deltaT;
                CamPos = CamPos - MOVE_SPEED * m.z * uz * deltaT;
            }
        }

        if (!OnlyMoveCam) {
            //Checks to see if an object can be bought
            if (!MV[MoveObjIndex].hasBeenBought) {
                bool isObjectAllowedToMove = true;
                for (auto &i: MV)
                    isObjectAllowedToMove = isObjectAllowedToMove && !(i.modelPos == startingRoomCenter);
                if (isObjectAllowedToMove) {
                    MV[MoveObjIndex].modelPos = startingRoomCenter;
                    MV[MoveObjIndex].hasBeenBought = true;
                }
                OnlyMoveCam = true;
            } else {
                const glm::vec3 modelPos = glm::vec3(
                        CamPos.x,
                        CamPos.y - 0.7f,
                        CamPos.z - 2.0f
                );

                glm::vec3 oldPos = MV[MoveObjIndex].modelPos;

                MV[MoveObjIndex].modelPos =
                        glm::translate(glm::mat4(1.0f), CamPos) *
                        glm::rotate(glm::mat4(1), CamAlpha, glm::vec3(0.0f, 1.0f, 0.0f)) *
                        glm::rotate(glm::mat4(1), CamBeta, glm::vec3(1.0f, 0.0f, 0.0f)) *
                        glm::translate(glm::mat4(1.0f), -CamPos) *
                        glm::vec4(modelPos, 1.0f);

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
        } else {
            if ((curDebounce == GLFW_KEY_SPACE) && debounce) {
                debounce = false;
                curDebounce = 0;
            }
        }

        if (glfwGetKey(window, GLFW_KEY_H)) {
            CamPos = startingRoomCenter + glm::vec3(0.0f, 0.7, 3.0f);
            CamAlpha = CamBeta = CamRho = 0.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_K)) {
            CamPos = polikeaBuildingPosition + glm::vec3(5.0f, 0.7, 5.0f);
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
        gubo.nSpotLights = indexSpot;
        gubo.nPointLights = indexPoint;

        glm::mat4 ViewPrj = MakeViewProjectionMatrix(Ar, CamAlpha, CamBeta, CamRho, CamPos);
        glm::mat4 baseTr = glm::mat4(1.0f);
        glm::mat4 World = glm::scale(glm::mat4(1), glm::vec3(5.0f));

        uboPolikea.amb = 0.05f;
        uboPolikea.gamma = 180.0f;
        uboPolikea.sColor = glm::vec3(1.0f);
        uboPolikea.mvpMat = ViewPrj * glm::translate(glm::mat4(1), polikeaBuildingPosition) * World;
        uboPolikea.mMat = glm::translate(glm::mat4(1), polikeaBuildingPosition) * World;
        uboPolikea.nMat = glm::inverse(glm::transpose(World));
        DSPolikeaBuilding.map(currentImage, &uboPolikea, sizeof(uboPolikea), 0);

        World = baseTr;
        uboBuilding.amb = 0.05f;
        uboBuilding.gamma = 180.0f;
        uboBuilding.sColor = glm::vec3(1.0f);
        uboBuilding.mvpMat = ViewPrj * World;
        uboBuilding.mMat = World;
        uboBuilding.nMat = glm::inverse(glm::transpose(World));
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

        uboGrid.amb = 0.05f;
        uboGrid.gamma = 180.0f;
        uboGrid.sColor = glm::vec3(1.0f);
        uboGrid.mvpMat = ViewPrj * World;
        uboGrid.mMat = World;
        uboGrid.nMat = glm::inverse(glm::transpose(World));
        // the .map() method of a DataSet object, requires the current image of the swap chain as first parameter
        // the second parameter is the pointer to the C++ data structure to transfer to the GPU
        // the third parameter is its size
        // the fourth parameter is the location inside the descriptor set of this uniform block
        DSFloorGrid.map(currentImage, &uboGrid, sizeof(uboGrid), 0);
        DSGubo.map(currentImage, &gubo, sizeof(gubo), 0);

        World = baseTr;
        uboDoor.amb = 0.05f;
        uboDoor.gamma = 180.0f;
        uboDoor.sColor = glm::vec3(1.0f);
        uboDoor.mvpMat = ViewPrj * World;
        uboDoor.mMat = World;
        uboDoor.nMat = glm::inverse(glm::transpose(World));
        for (int i = 0; i < N_ROOMS - 1; i++) {
            uboDoor.door[i] = glm::vec4(doors[i].doorRot);
        }
        DSDoor.map(currentImage, &uboDoor, sizeof(uboDoor), 0);

        for (auto &mInfo: MV) {
            World = MakeWorldMatrix(mInfo.modelPos, mInfo.modelRot, glm::vec3(1.0f, 1.0f, 1.0f)) * baseTr;
            mInfo.modelUBO.amb = 0.05f;
            mInfo.modelUBO.gamma = 180.0f;
            mInfo.modelUBO.sColor = glm::vec3(1.0f);
            mInfo.modelUBO.mvpMat = ViewPrj * World;
            mInfo.modelUBO.mMat = World;
            mInfo.modelUBO.nMat = glm::inverse(glm::transpose(World));
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
