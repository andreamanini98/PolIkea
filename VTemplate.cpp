#include <string>
#include <iostream>
#include <filesystem>
#include "Starter.hpp"
#include "WorldView.hpp"
#include "Parameters.hpp"
#include "Logic.hpp"
#include <random>
#include <unordered_set>
#include "HouseGen.h"
#include "UniformBuffers.h"

#define MAX_OBJECTS_IN_POLIKEA 15 // Do not exceed 15 since the model is pre-generated using Blender

#define FRONTOFFSET 10.0f
#define SIDEOFFSET 20.0f
#define BACKOFFSET 30.0f

namespace fs = std::filesystem;

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
    DescriptorSetLayout DSLMesh, DSLMeshMultiTex, DSLDoor, DSLGubo, DSLOverlay, DSLVertexWithColors;

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
    DescriptorSet DSPolikeaExternFloor, DSFence, DSGubo, DSOverlayMoveObject, DSPolikeaBuilding, DSBuilding;
    // Textures
    Texture TAsphalt, TFurniture, TFence, TPlankWall, TOverlayMoveObject, TBathFloor, TDarkFloor, TTiledStones, TOverlayBuyObject, TCharacter;
    // C++ storage for uniform variables
    UniformBlock uboPolikeaExternFloor, uboFence, uboPolikea, uboBuilding;
    GlobalUniformBlock gubo;
    OverlayUniformBlock uboMoveOrBuyOverlay;

    // Other application parameters
    // A vector containing one element for each model loaded where we want to keep track of its information
    std::vector<ModelInfo> MV;
    ModelInfo MVCharacter;

    //Used for placing automatically the objects inside polikea.
    glm::vec3 polikeaBuildingPosition = getPolikeaBuildingPosition();
    std::vector<glm::vec3> polikeaBuildingOffsets = getPolikeaBuildingOffsets();

    std::vector<BoundingRectangle> buildingBoundingRectangle = getPolikeaBoundingRectangles(polikeaBuildingPosition,
                                                                                            FRONTOFFSET,
                                                                                            SIDEOFFSET, BACKOFFSET);
    std::vector<glm::vec3> positionedLightPos = getPolikeaPositionedLightsPos();

    glm::vec3 newCharacterPos;
    glm::vec3 &characterPos = MVCharacter.modelPos;
    float characterYaw = MVCharacter.modelRot;
    float cameraYaw = 0.0f;
    float camPitch = 0.0f;
    float camRoll = 0.0f;
    bool OnlyMoveCam = true;
    uint32_t MoveObjIndex = 0;
    bool isLookAt = false; // Tells if we use the look at technique or not.
    bool fly = false;

    std::vector<glm::vec3> roomCenters;
    std::vector<BoundingRectangle> roomOccupiedArea;

    Model<Vertex, ModelInstance> MDoor, MPositionedLights;
    DescriptorSet DSDoor, DSPositionedLights;
    UniformBlockInstancedWithRot uboDoor, uboPositionedLights;

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
        DSLMeshMultiTex.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT, 5},
        });
        DSLDoor.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
        });
        DSLGubo.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS}
        });
        DSLOverlay.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         VK_SHADER_STAGE_ALL_GRAPHICS},
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
                {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},
        });
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
                {0, sizeof(VertexWithTextID), VK_VERTEX_INPUT_RATE_VERTEX}
        }, {
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
                                                                                pos),          sizeof(glm::vec3), OTHER}
                            });

        // Pipelines [Shader couples]
        // The second parameter is the pointer to the vertex definition
        // Third and fourth parameters are respectively the vertex and fragment shaders
        // The last array, is a vector of pointer to the layouts of the sets that will
        // be used in this pipeline. The first element will be set 0, and so on
        PMesh.init(this, &VMesh, "shaders_c/Shader.vert.spv", "shaders_c/Shader.frag.spv",
                   {&DSLGubo, &DSLMesh});
        PMesh.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        PMeshMultiTexture.init(this, &VMeshTexID, "shaders_c/ShaderMultiTexture.vert.spv",
                               "shaders_c/ShaderMultiTexture.frag.spv", {&DSLGubo, &DSLMeshMultiTex});
        PMeshMultiTexture.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        POverlay.init(this, &VOverlay, "shaders_c/Overlay.vert.spv", "shaders_c/Overlay.frag.spv", {&DSLOverlay});
        POverlay.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        PVertexWithColors.init(this, &VVertexWithColor, "shaders_c/VColor.vert.spv", "shaders_c/VColor.frag.spv",
                               {&DSLGubo, &DSLVertexWithColors});

        PMeshInstanced.init(this, &VMeshInstanced, "shaders_c/ShaderInstanced.vert.spv", "shaders_c/ShaderInstanced.frag.spv",
                            {&DSLGubo, &DSLDoor});
        PMeshInstanced.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);

        // Models, textures and Descriptors (values assigned to the uniforms)

        loadModels("models/furniture", this, &VMesh, &MV, MGCG);
        loadModels("models/lights", this, &VMesh, &MV, MGCG);
        MVCharacter = loadCharacter("models/character/character.obj", this, &VMesh, OBJ);

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


        //Procedural (random) generation of the building + lights
        auto floorplan = generateFloorplan(MAX_DIMENSION);
        floorPlanToVerIndexes(floorplan, MBuilding.vertices, MBuilding.indices, doors, &buildingBoundingRectangle,
                              &positionedLightPos, &roomCenters, &roomOccupiedArea);
        MBuilding.initMesh(this, &VMeshTexID);

        MPolikeaBuilding.init(this, &VVertexWithColor, "models/polikeaBuilding.obj", OBJ);

        MDoor.instanceBufferPresent = true;
        MDoor.instances.reserve(doors.size() + 2);
        // we insert 2 doors for polikea at the end (the others were generated by the floorplan)
        doors.push_back(
                OpenableDoor{
                        getPolikeaBuildingPosition() + glm::vec3(3.0, 0.25, -DOOR_HWIDTH / 2),
                        glm::radians(0.0f),
                        0.0f,
                        glm::radians(90.0f),
                        CLOSED,
                        0.0f,
                        COUNTERCLOCKWISE
                });
        doors.push_back(
                OpenableDoor{
                        getPolikeaBuildingPosition() + glm::vec3(5.0947, 0.25, -DOOR_HWIDTH / 2),
                        glm::radians(180.0f),
                        0.0f,
                        glm::radians(90.0f),
                        CLOSED,
                        0.0f,
                        CLOCKWISE
                });
        for (auto &door: doors) {
            MDoor.instances.push_back({door.baseRot, door.doorPos});
        }
        MDoor.init(this, &VMeshInstanced, "models/door_009_Mesh.112.mgcg", MGCG);

        MPositionedLights.instanceBufferPresent = true;
        MPositionedLights.instances.reserve(N_POS_LIGHTS);
        for (int i = 0; i < N_POS_LIGHTS; i++) {
            MPositionedLights.instances.push_back({0.0f, positionedLightPos[i]});
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
        TOverlayBuyObject.init(this, "textures/BuyObject.png");
        TCharacter.init(this, "textures/character.png");
    }

    inline ModelInfo loadCharacter(const std::string &path, VTemplate *thisVTemplate, VertexDescriptor *VMeshRef, ModelType modelType) {
        ModelInfo MIChar;
        MIChar.model.init(thisVTemplate, VMeshRef, path, modelType);
        newCharacterPos = MIChar.modelPos = glm::vec3(2.0, 0.0, 3.45706);
        MIChar.modelRot = 0.0f;

        MIChar.minCoords = glm::vec3(std::numeric_limits<float>::max());
        MIChar.maxCoords = glm::vec3(std::numeric_limits<float>::lowest());

        for (const auto &vertex: MIChar.model.vertices) {
            MIChar.minCoords = glm::min(MIChar.minCoords, vertex.pos);
            MIChar.maxCoords = glm::max(MIChar.maxCoords, vertex.pos);
        }
        MIChar.center = (MIChar.minCoords + MIChar.maxCoords) / 2.0f;
        MIChar.size = MIChar.maxCoords - MIChar.minCoords;

        MIChar.cylinderRadius = glm::distance(glm::vec3(MIChar.maxCoords.x, 0, MIChar.maxCoords.z),
                                              glm::vec3(MIChar.minCoords.x, 0, MIChar.minCoords.z)) / 2;
        MIChar.cylinderHeight = MIChar.maxCoords.y - MIChar.minCoords.y;

        return MIChar;
    }

    inline void loadModels(const std::string &path, VTemplate *thisVTemplate, VertexDescriptor *VMeshRef,
                           std::vector<ModelInfo> *MVRef, ModelType modelType) {
        int posOffset = 0;
        static int polikeaBuildingOffsetsIndex = 0; // Used to count how many objects have been drawn inside polikea

        for (const auto &entry: fs::directory_iterator(path)) {
            // Added this check since in MacOS this hidden file could be created in a directory
            if (static_cast<std::string>(entry.path()).find("DS_Store") != std::string::npos)
                continue;

            if (static_cast<std::string>(entry.path()).find("polilamp") != std::string::npos)
                continue;

            ModelInfo MI;
            // The second parameter is the pointer to the vertex definition for this model
            // The third parameter is the file name
            // The last is a constant specifying the file type: currently only OBJ or GLTF
            MI.model.init(thisVTemplate, VMeshRef, entry.path(), modelType);
            if (polikeaBuildingOffsetsIndex < MAX_OBJECTS_IN_POLIKEA) {
                MI.modelPos = polikeaBuildingPosition + polikeaBuildingOffsets[polikeaBuildingOffsetsIndex];
                polikeaBuildingOffsetsIndex++;
            } else {
                //TODO what is this MI.modelPos = CamPos - glm::vec3(0.0f + posOffset * 2.0f, CamPos.y, 0.0f);
                throw std::invalid_argument("Not implemented");
            }
            MI.modelRot = (MAX_OBJECTS_IN_POLIKEA - polikeaBuildingOffsetsIndex < 3) ? glm::radians(180.0f) : 0.0f;

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

            MVRef->push_back(MI);
            posOffset++;
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
                {0, UNIFORM, sizeof(UniformBlock), nullptr},
                {1, TEXTURE, 0,                    &TAsphalt}
        });
        DSFence.init(this, &DSLMesh, {
                {0, UNIFORM, sizeof(UniformBlock), nullptr},
                {1, TEXTURE, 0,                    &TFence}
        });
        DSGubo.init(this, &DSLGubo, {
                {0, UNIFORM, sizeof(GlobalUniformBlock), nullptr}
        });
        DSOverlayMoveObject.init(this, &DSLOverlay, {
                {0, UNIFORM, sizeof(OverlayUniformBlock), nullptr},
                {1, TEXTURE, 0,                           &TOverlayMoveObject},
                {2, TEXTURE, 0,                           &TOverlayBuyObject}
        });
        DSPolikeaBuilding.init(this, &DSLVertexWithColors, {
                {0, UNIFORM, sizeof(UniformBlock), nullptr}
        });
        DSDoor.init(this, &DSLDoor, {
                {0, UNIFORM, sizeof(UniformBlockInstancedWithRot), nullptr},
                {1, TEXTURE, 0,                                    &TFurniture}
        });
        DSPositionedLights.init(this, &DSLDoor, {
                {0, UNIFORM, sizeof(UniformBlockInstancedWithRot), nullptr},
                {1, TEXTURE, 0,                                    &TFurniture}
        });
        DSBuilding.init(this, &DSLMeshMultiTex, {
                {0, UNIFORM, sizeof(UniformBlock), nullptr},
                {1, TEXTURE, 0,                    &TPlankWall,   0},
                {1, TEXTURE, 0,                    &TAsphalt,     1},
                {1, TEXTURE, 0,                    &TBathFloor,   2},
                {1, TEXTURE, 0,                    &TDarkFloor,   3},
                {1, TEXTURE, 0,                    &TTiledStones, 4}
        });

        for (auto &mInfo: MV) {
            mInfo.dsModel.init(this, &DSLMesh, {
                    {0, UNIFORM, sizeof(UniformBlock), nullptr},
                    {1, TEXTURE, 0,                    &TFurniture}
            });
        }

        MVCharacter.dsModel.init(this, &DSLMesh, {
                {0, UNIFORM, sizeof(UniformBlock), nullptr},
                {1, TEXTURE, 0,                    &TCharacter}
        });
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
        DSOverlayMoveObject.cleanup();
        DSPolikeaBuilding.cleanup();
        DSDoor.cleanup();
        DSPositionedLights.cleanup();
        DSBuilding.cleanup();

        for (auto &mInfo: MV)
            mInfo.dsModel.cleanup();
        MVCharacter.dsModel.cleanup();
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
        TOverlayBuyObject.cleanup();
        TPlankWall.cleanup();
        TTiledStones.cleanup();
        TDarkFloor.cleanup();
        TBathFloor.cleanup();
        TCharacter.cleanup();

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
        MVCharacter.model.cleanup();

        // Cleanup descriptor set layouts
        DSLMesh.cleanup();
        DSLMeshMultiTex.cleanup();
        DSLGubo.cleanup();
        DSLOverlay.cleanup();
        DSLVertexWithColors.cleanup();
        DSLDoor.cleanup();

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

        //--- GRID ---
        // binds the model
        // For a Model object, this command binds the corresponding index and vertex buffer
        // to the command buffer passed in its parameter
        DSPolikeaExternFloor.bind(commandBuffer, PMesh, 1, currentImage);
        MPolikeaExternFloor.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MPolikeaExternFloor.indices.size()), 1, 0, 0, 0);

        DSFence.bind(commandBuffer, PMesh, 1, currentImage);
        MFence.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MFence.indices.size()), 1, 0, 0, 0);
        // the second parameter is the number of indexes to be drawn. For a Model object,
        // this can be retrieved with the .indices.size() method.

        //--- MODELS ---
        for (auto &mInfo: MV) {
            mInfo.dsModel.bind(commandBuffer, PMesh, 1, currentImage);
            mInfo.model.bind(commandBuffer);
            vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mInfo.model.indices.size()), 1, 0, 0, 0);
        }

        MVCharacter.dsModel.bind(commandBuffer, PMesh, 1, currentImage);
        MVCharacter.model.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MVCharacter.model.indices.size()), 1, 0, 0, 0);

        // --- PIPELINE OVERLAY ---
        POverlay.bind(commandBuffer);
        MOverlay.bind(commandBuffer);
        DSOverlayMoveObject.bind(commandBuffer, POverlay, 0, currentImage);
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

        DSDoor.bind(commandBuffer, PMeshInstanced, 1, currentImage);
        MDoor.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(MDoor.indices.size()), N_ROOMS - 1 + 2, 0, 0, 0);

        DSPositionedLights.bind(commandBuffer, PMeshInstanced, 1, currentImage);
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

        static bool debounce, lightDebounce, roomCyclingDebounce, isLookAtDebounce, kDebounce, hDebounce, flyDebounce = false;
        static int curDebounce, curLightDebounce, curRoomCyclingDebounce, curIsLookAtDebounce, curKDebounce, curHDebounce, curFlyDebounce = 0;

        float deltaT;
        auto m = glm::vec3(0.0f), r = glm::vec3(0.0f);
        bool fire, lightSwitch, cycleRoom, isLookAtFire, isKPressed, isHPressed, isVPressed = false;
        getSixAxis(deltaT, m, r, fire, lightSwitch, cycleRoom, isLookAtFire, isKPressed, isHPressed, isVPressed);

        const float minPitch = glm::radians(-30.0f);
        const float maxPitch = glm::radians(30.0f);
        cameraYaw = cameraYaw - ROT_SPEED * deltaT * r.y;

        camPitch = camPitch - ROT_SPEED * deltaT * r.x * (isLookAt ? -1.0f : 1.0f);
        camPitch = camPitch < minPitch ? minPitch : (camPitch > maxPitch ? maxPitch : camPitch);
        camRoll = camRoll - ROT_SPEED * deltaT * r.z;
        camRoll = camRoll < glm::radians(-180.0f) ? glm::radians(-180.0f) :
                  (camRoll > glm::radians(180.0f) ? glm::radians(180.0f) : camRoll);

        glm::vec3 ux = glm::rotate(glm::mat4(1.0f), cameraYaw, glm::vec3(0, 1, 0)) * glm::vec4(1, 0, 0, 1);
        glm::vec3 uz = glm::rotate(glm::mat4(1.0f), cameraYaw, glm::vec3(0, 1, 0)) * glm::vec4(0, 0, -1, 1);

        static glm::vec3 oldCharacterPos = newCharacterPos;
        newCharacterPos += MOVE_SPEED * m.x * ux * deltaT;
        if (fly) {
            newCharacterPos += MOVE_SPEED * m.y * glm::vec3(0, 1, 0) * deltaT;
        }
        newCharacterPos += MOVE_SPEED * m.z * uz * deltaT;
        characterPos = oldCharacterPos * std::exp(-10 * deltaT) + newCharacterPos * (1 - std::exp(-10 * deltaT));

        //Handle the two steps to enter polikea
        if(!fly) {
            if (checkIfInBoundingRectangle(characterPos,getSecondStepBoundingRectangle()))
                characterPos.y = newCharacterPos.y = 0.5f;
            else if (checkIfInBoundingRectangle(characterPos,getFirstStepBoundingRectangle()))
                characterPos.y = newCharacterPos.y = 0.25f;
            else
                characterPos.y = newCharacterPos.y = 0.0f;
        }

        // We update the rotation of the character
        float newAngle = characterYaw;
        if (std::abs(m.x) > 0 || std::abs(m.z) > 0)
            newAngle = normalizeAngle(cameraYaw + glm::radians(90.0f) + std::atan2(-m.x, m.z));
        int dir;
        auto diff = shortestAngularDistance(characterYaw, newAngle, dir);
        characterYaw += static_cast<float>(dir) * std::min(deltaT * ROT_SPEED * 4, diff);

        for (auto &boundingRectangle: buildingBoundingRectangle)
            if (checkIfInBoundingRectangle(characterPos, boundingRectangle))
                newCharacterPos = characterPos = oldCharacterPos;

        if (isLookAtFire) {
            if (!isLookAtDebounce) {
                isLookAtDebounce = true;
                curIsLookAtDebounce = GLFW_KEY_Z;
                // If isLookAt is false that means that we're going to see the character and that a potential teleport without the character
                // may have occurred. Thus, we need to adjust the character's position.
                isLookAt = !isLookAt; // Flip this boolean to change lookAt mode.
                characterYaw = cameraYaw + glm::radians(90.0f);
            }
        } else if ((curIsLookAtDebounce == GLFW_KEY_Z) && isLookAtDebounce) {
            isLookAtDebounce = false;
            curIsLookAtDebounce = 0;
        }

        if (isVPressed) {
            if (!flyDebounce) {
                flyDebounce = true;
                curFlyDebounce = GLFW_KEY_V;
                fly = !fly;
            }
        } else if ((curFlyDebounce == GLFW_KEY_V) && flyDebounce) {
            flyDebounce = false;
            curFlyDebounce = 0;
        }

        if (!OnlyMoveCam) {
            //isLookAt = false; // When we move objects we don't want to see also the character.
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
                const glm::vec3 modelPos = glm::vec3(characterPos.x, characterPos.y, characterPos.z - 2.0f);
                glm::vec3 oldPos = MV[MoveObjIndex].modelPos;
                MV[MoveObjIndex].modelPos = rotateTargetRespectToCam(characterPos, cameraYaw, camPitch, modelPos);

                for (int i = 0; i < MV.size(); i++) {
                    if (i != MoveObjIndex) {
                        if (MV[MoveObjIndex].checkCollision(MV[i])) {
                            MV[MoveObjIndex].modelPos = oldPos;
                            break;
                        }
                    }
                }

                MV[MoveObjIndex].modelRot = cameraYaw;

                if (cycleRoom) {
                    if (!roomCyclingDebounce) {
                        roomCyclingDebounce = true;
                        curRoomCyclingDebounce = GLFW_KEY_0;

                        if (MV[MoveObjIndex].roomCycling < N_ROOMS) {
                            MV[MoveObjIndex].roomCycling++;
                            if (MV[MoveObjIndex].roomCycling >= N_ROOMS)
                                MV[MoveObjIndex].roomCycling = 0;
                            newCharacterPos = characterPos = roomCenters[MV[MoveObjIndex].roomCycling];
                            MV[MoveObjIndex].modelPos = rotateTargetRespectToCam(characterPos, cameraYaw, camPitch,modelPos);
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

                if (MV[MoveObjIndex].modelPos.y < 0.0f) MV[MoveObjIndex].modelPos.y = 0.0f;
                float ceilingOverload =
                        MV[MoveObjIndex].modelPos.y + MV[MoveObjIndex].cylinderHeight - ROOM_CEILING_HEIGHT;
                if (ceilingOverload > 0)
                    MV[MoveObjIndex].modelPos.y -= ceilingOverload;
            }
        }

        for (auto &Door: doors) {
            if (glm::distance(characterPos, Door.doorPos) <= 3.0f && Door.doorState == CLOSED) {
                Door.doorState = OPENING;
            }
            if (glm::distance(characterPos, Door.doorPos) > 1.5 && Door.doorState == OPEN) {
                Door.doorState = CLOSING;
            }
            if (Door.doorState == OPENING) {
                if (Door.doorOpeningDirection == CLOCKWISE) {
                    Door.doorRot -= Door.doorSpeed * deltaT;
                    if (Door.doorRot <= -glm::radians(90.0f)) {
                        Door.doorState = WAITING_OPEN;
                        Door.doorRot = -glm::radians(90.0f);
                    }
                }
                if (Door.doorOpeningDirection == COUNTERCLOCKWISE) {
                    Door.doorRot += Door.doorSpeed * deltaT;
                    if (Door.doorRot >= glm::radians(90.0f)) {
                        Door.doorState = WAITING_OPEN;
                        Door.doorRot = glm::radians(90.0f);
                    }
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
                if (Door.doorOpeningDirection == CLOCKWISE) {
                    Door.doorRot += Door.doorSpeed * deltaT;
                    if (Door.doorRot >= 0.0f) {
                        Door.doorState = CLOSED;
                        Door.doorRot = 0.0f;
                    }
                }
                if (Door.doorOpeningDirection == COUNTERCLOCKWISE) {
                    Door.doorRot -= Door.doorSpeed * deltaT;
                    if (Door.doorRot <= 0.0f) {
                        Door.doorState = CLOSED;
                        Door.doorRot = 0.0f;
                    }
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
                    float distance = glm::distance(characterPos, MV[0].modelPos);
                    MoveObjIndex = 0;
                    for (std::size_t i = 1; i < MV.size(); ++i) {
                        float newDistance = glm::distance(characterPos, MV[i].modelPos);
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

        if (isKPressed) {
            if (!kDebounce) {
                kDebounce = true;
                curKDebounce = GLFW_KEY_K;
                newCharacterPos = characterPos = polikeaBuildingPosition + glm::vec3(5.0f, 0.0f, 5.0f);
                characterYaw = glm::radians(90.0f);
                cameraYaw = camPitch = camRoll = 0.0f;
            }
        } else if ((curKDebounce == GLFW_KEY_K) && kDebounce) {
            kDebounce = false;
            curKDebounce = 0;
        }
        if (isHPressed) {
            if (!hDebounce) {
                hDebounce = true;
                curHDebounce = GLFW_KEY_H;
                newCharacterPos = characterPos = roomCenters[0] + glm::vec3(0.0f, 0.0f, 3.0f);
                characterYaw = glm::radians(90.0f);
                cameraYaw = camPitch = camRoll = 0.0f;
            }
        } else if ((curHDebounce == GLFW_KEY_H) && hDebounce) {
            hDebounce = false;
            curHDebounce = 0;
        }

        // ----- CHARACTER MANIPULATION AND MATRIX GENERATION ----- //

        glm::mat4 World, WorldCharacter, ViewPrj;
        // We check the bounding of the character for surroundings
        for (const auto &boundingRectangle: buildingBoundingRectangle)
            if (checkIfInBoundingRectangle(characterPos, boundingRectangle, 0.15f))
                newCharacterPos = characterPos = oldCharacterPos;
        // We check the bounding of the character for furniture
        for (const auto &modelInfo: MV) {
            if (MVCharacter.checkCollision(modelInfo)) {
                newCharacterPos = characterPos = oldCharacterPos;
                break;
            }
        }

        const float camHeight = CAM_HEIGHT;
        const float camDist = 2.2f;
        glm::vec3 camPos;
        if (isLookAt) {
            camPos = glm::translate(glm::mat4(1.0f), characterPos) *
                     glm::rotate(glm::mat4(1), cameraYaw, glm::vec3(0.0f, 1.0f, 0.0f)) *
                     glm::rotate(glm::mat4(1), camPitch, glm::vec3(1.0f, 0.0f, 0.0f)) *
                     glm::vec4(0, camHeight, camDist, 1);

            // Next we call the GameLogic() function to compute the lookAt matrices
            getLookAt(Ar, ViewPrj, WorldCharacter, deltaT, camPos, characterPos, characterYaw);
            // At the end put this to false since the adjustment has occurred (if it was needed).
        } else {
            // Otherwise we normally build our View-Projection matrix.
            camPos = glm::translate(glm::mat4(1.0f), characterPos) * glm::vec4(0, camHeight, 0, 1);
            ViewPrj = MakeViewProjectionMatrix(Ar, cameraYaw, camPitch, camRoll, camPos);
        }

        // ----- END CHARACTER MANIPULATION AND MATRIX GENERATION ----- //

        gubo.DlightDir = glm::normalize(glm::vec3(1, 2, 3));
        gubo.DlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gubo.AmbLightColor = glm::vec3(1.0f);
        gubo.eyePos = camPos;

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

        // UBO POLIKEA
        uboPolikea.amb = 0.05f;
        uboPolikea.gamma = 180.0f;
        uboPolikea.sColor = glm::vec3(1.0f);
        uboPolikea.worldMat = glm::translate(glm::mat4(1), polikeaBuildingPosition) * glm::scale(glm::mat4(1), glm::vec3(5.0f));
        uboPolikea.nMat = glm::inverse(uboPolikea.worldMat);
        uboPolikea.mvpMat = ViewPrj * uboPolikea.worldMat;
        uboPolikea.diffuseLight = 1.0f;
        uboPolikea.internalLightsFactor = 1.0f;
        DSPolikeaBuilding.map(currentImage, &uboPolikea, sizeof(uboPolikea), 0);
        //END UBO POLIKEA

        uboBuilding.amb = 0.05f;
        uboBuilding.gamma = 180.0f;
        uboBuilding.sColor = glm::vec3(1.0f);
        uboBuilding.worldMat = glm::mat4(1.0f);
        uboBuilding.nMat = glm::inverse(uboBuilding.worldMat);
        uboBuilding.mvpMat = ViewPrj * uboBuilding.worldMat;
        uboBuilding.diffuseLight = 0.0f;
        uboBuilding.internalLightsFactor = 1.0f;
        DSBuilding.map(currentImage, &uboBuilding, sizeof(uboBuilding), 0);

        bool displayBuyOrMoveOverlay = false;
        for (auto &modelInfo: MV) {
            float distance = glm::distance(characterPos, modelInfo.modelPos);
            if (distance <= threshold) {
                displayBuyOrMoveOverlay = true;
                break;
            }
        }

        uboMoveOrBuyOverlay.visible = (OnlyMoveCam && displayBuyOrMoveOverlay) ? 1.0f : 0.0f;
        bool buyOrMoveOverlay = displayBuyOrMoveOverlay &&
                                checkIfInBoundingRectangle(
                                        characterPos,
                                        BoundingRectangle{
                                                glm::vec3(polikeaBuildingPosition.x - 10.5f, 0.0f,
                                                          polikeaBuildingPosition.z + 0.5f),
                                                glm::vec3(polikeaBuildingPosition.x + 10.5f, 0.0f,
                                                          polikeaBuildingPosition.z - 20.5f)}
                                );
        uboMoveOrBuyOverlay.overlayTex = buyOrMoveOverlay ? 1.0f : 0.0f;
        DSOverlayMoveObject.map(currentImage, &uboMoveOrBuyOverlay, sizeof(uboMoveOrBuyOverlay), 0);

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
        uboDoor.internalLightsFactor = 1.0;
        uboDoor.diffuseLight = 1.0;
        //The door in the house receive no light from the outside
        for (int i = 0; i < N_ROOMS - 1; i++) {
            uboDoor.rotOffset[i] = glm::vec4(doors[i].doorRot, /*diffuseLight = */ 0.0f, /* internalLightsFactor = */1.0f, /*unused = */ 0);
        }
        //The two polikea doors have the light
        for (int i = N_ROOMS - 1; i < N_ROOMS - 1 + 2; i++) {
            uboDoor.rotOffset[i] = glm::vec4(doors[i].doorRot, /*diffuseLight = */ 1.0f, /* internalLightsFactor = */1.0f, /*unused = */ 0);
        }
        DSDoor.map(currentImage, &uboDoor, sizeof(uboDoor), 0);

        uboPositionedLights.amb = 0.05f;
        uboPositionedLights.gamma = 180.0f;
        uboPositionedLights.sColor = glm::vec3(1.0f);
        uboPositionedLights.prjViewMat = ViewPrj;
        uboPositionedLights.internalLightsFactor = 1.0;
        uboPositionedLights.diffuseLight = 1.0;
        for (int i = 0; i < MAXIMUM_INSTANCES_PER_BUFFER; i++) {
            uboPositionedLights.rotOffset[i] = glm::vec4(/* rot = */ 0.0f, /*diffuseLight = */ 0.0f, /* internalLightsFactor = */1.0f, /*unused = */ 0);
        }
        DSPositionedLights.map(currentImage, &uboPositionedLights, sizeof(uboPositionedLights), 0);

        for (auto &mInfo: MV) {
            World = MakeWorldMatrix(mInfo.modelPos, mInfo.modelRot, glm::vec3(1.0f, 1.0f, 1.0f)) * glm::mat4(1.0f);;
            mInfo.modelUBO.amb = 0.05f;
            mInfo.modelUBO.gamma = 180.0f;
            mInfo.modelUBO.sColor = glm::vec3(1.0f);
            mInfo.modelUBO.worldMat = World;
            mInfo.modelUBO.nMat = glm::inverse(mInfo.modelUBO.worldMat);
            mInfo.modelUBO.mvpMat = ViewPrj * mInfo.modelUBO.worldMat;
            mInfo.modelUBO.diffuseLight = 0.0f;
            mInfo.modelUBO.internalLightsFactor = 1.0f;
            mInfo.dsModel.map(currentImage, &mInfo.modelUBO, sizeof(mInfo.modelUBO), 0);
        }

        MVCharacter.modelUBO.amb = 0.05f;
        MVCharacter.modelUBO.gamma = 180.0f;
        MVCharacter.modelUBO.sColor = glm::vec3(1.0f);
        MVCharacter.modelUBO.worldMat = WorldCharacter * glm::scale(glm::mat4(1), (isLookAt) ? glm::vec3(1.5f, 1.8f, 1.5f) : glm::vec3(0.0f));
        MVCharacter.modelUBO.nMat = glm::inverse(MVCharacter.modelUBO.worldMat);
        MVCharacter.modelUBO.mvpMat = ViewPrj * MVCharacter.modelUBO.worldMat;

        bool insideBuilding = isInsideBuilding(characterPos);

        MVCharacter.modelUBO.diffuseLight = insideBuilding ? 0.0f : 1.0f; //TODO
        MVCharacter.modelUBO.internalLightsFactor = insideBuilding ? 1.0f : 0.0f; //TODO
        MVCharacter.dsModel.map(currentImage, &MVCharacter.modelUBO, sizeof(MVCharacter.modelUBO), 0);

        oldCharacterPos = characterPos;
    }

    bool isInsideBuilding(glm::vec3 characterPos) {
        for (auto bounding: roomOccupiedArea)
            if(checkIfInBoundingRectangle(characterPos, bounding))
                return true;
        return checkIfInBoundingRectangle(characterPos, getPolikeaOccupiedArea());
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
