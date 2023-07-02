#include <string>
#include <iostream>
#include <filesystem>
#include "Starter.hpp"
#include "WorldView.hpp"

#define N_SPOTLIGHTS 50

namespace fs = std::filesystem;

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

struct SpotLight {
    alignas(4) float beta;   // decay exponent of the spotlight
    alignas(4) float g;      // target distance of the spotlight
    alignas(4) float cosout; // cosine of the outer angle of the spotlight
    alignas(4) float cosin;  // cosine of the inner angle of the spotlight
    alignas(16) glm::vec3 lightPos;
    alignas(16) glm::vec3 lightDir;
    alignas(16) glm::vec4 lightColor;
};

struct GlobalUniformBlock {
    alignas(16) glm::vec3 DlightDir;
    alignas(16) glm::vec3 DlightColor;
    alignas(16) glm::vec3 AmbLightColor;
    alignas(16) glm::vec3 eyePos;
    SpotLight lights[N_SPOTLIGHTS];
    alignas(4) int nLights;
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

// Struct used to store data related to a model
struct ModelInfo {
    Model<Vertex> model;
    DescriptorSet dsModel;
    UniformBlock modelUBO{};
    glm::vec3 modelPos{};
    float modelRot = 0.0;

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
    DescriptorSetLayout DSL, DSLGubo, DSLOverlay, DSLVColor;

    // Vertex formats
    VertexDescriptor VD, VOverlay, VVColor;

    // Pipelines [Shader couples]
    Pipeline P, POverlay, PVColor;

    // Models, textures and Descriptors (values assigned to the uniforms)
    // Please note that Model objects depends on the corresponding vertex structure
    // Models
    Model<Vertex> MGrid;
    Model<VertexOverlay> MKey;
    Model<VertexVColor> Polikea;
    // Descriptor sets
    DescriptorSet DSGrid, DSGubo, DSKey, DSPolikea;
    // Textures
    Texture T1, T2, TKey;
    // C++ storage for uniform variables
    UniformBlock uboGrid, uboPolikea;
    GlobalUniformBlock gubo;
    OverlayUniformBlock uboKey;

    // Other application parameters
    // A vector containing one element for each model loaded where we want to keep track of its information
    std::vector<ModelInfo> MV;

    glm::vec3 CamPos = glm::vec3(2.0, 0.7, 3.45706);;
    float CamAlpha = 0.0f;
    float CamBeta = 0.0f;
    float CamRho = 0.0f;
    bool OnlyMoveCam = true;
    uint32_t MoveObjIndex = 0;

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
        DSL.init(this, {
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
        // TODO may bring the DSLVColor to VK_SHADER_STAGE_FRAGMENT_BIT if it is used only there
        DSLVColor.init(this, {
                {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS}
        });

        // Vertex descriptors
        VD.init(this, {
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

        VVColor.init(this, {
                {0, sizeof(VertexVColor), VK_VERTEX_INPUT_RATE_VERTEX}
        }, {
                             {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexVColor, pos),
                                     sizeof(glm::vec3), POSITION},
                             {0, 1, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexVColor, norm),
                                     sizeof(glm::vec3), NORMAL},
                             {0, 2, VK_FORMAT_R32G32B32_SFLOAT, offsetof(VertexVColor, color),
                                     sizeof(glm::vec3), COLOR}
                     });

        // Pipelines [Shader couples]
        // The second parameter is the pointer to the vertex definition
        // Third and fourth parameters are respectively the vertex and fragment shaders
        // The last array, is a vector of pointer to the layouts of the sets that will
        // be used in this pipeline. The first element will be set 0, and so on
        P.init(this, &VD, "shaders/ShaderVert.spv", "shaders/ShaderFrag.spv", {&DSLGubo, &DSL});
        P.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);
        POverlay.init(this, &VOverlay, "shaders/OverlayVert.spv", "shaders/OverlayFrag.spv", {&DSLOverlay});
        POverlay.setAdvancedFeatures(VK_COMPARE_OP_LESS, VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE, false);
        PVColor.init(this, &VVColor, "shaders/VColorVert.spv", "shaders/VColorFrag.spv", {&DSLGubo, &DSLVColor});

        // Models, textures and Descriptors (values assigned to the uniforms)

        // Create models
        // TODO maybe move to external function
        int i = 0;
        std::string path = "models/lights";
        for (const auto &entry: fs::directory_iterator(path)) {
            // Added this check since in MacOS this hidden file could be created in a directory
            if (static_cast<std::string>(entry.path()).find("DS_Store") != std::string::npos)
                continue;

            ModelInfo MI;
            // The second parameter is the pointer to the vertex definition for this model
            // The third parameter is the file name
            // The last is a constant specifying the file type: currently only OBJ or GLTF
            MI.model.init(this, &VD, entry.path(), MGCG);
            MI.modelPos = glm::vec3(0.0 + i * 2, 0.0, 0.0);
            MI.modelRot = 0.0f;

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

            MV.push_back(MI);
            i++;
        }

        // Creates a mesh with direct enumeration of vertices and indices
        MGrid.vertices = {{{-6, 0, -6}, {0.0, 1.0, 0.0}, {0.0f, 0.0f}},
                          {{-6, 0, 6},  {0.0, 1.0, 0.0}, {0.0f, 1.0f}},
                          {{6,  0, -6}, {0.0, 1.0, 0.0}, {1.0f, 0.0f}},
                          {{6,  0, 6},  {0.0, 1.0, 0.0}, {1.0f, 1.0f}}};
        MGrid.indices = {0, 1, 2, 1, 3, 2};
        MGrid.initMesh(this, &VD);

        MKey.vertices = {{{-0.7f, 0.70f}, {0.0f, 0.0f}},
                         {{-0.7f, 0.93f}, {0.0f, 1.0f}},
                         {{0.7f,  0.70f}, {1.0f, 0.0f}},
                         {{0.7f,  0.93f}, {1.0f, 1.0f}}};
        MKey.indices = {0, 1, 2, 3, 2, 1};
        MKey.initMesh(this, &VOverlay);

        Polikea.init(this, &VVColor, "models/polikea.obj", OBJ);

        // Create the textures
        // The second parameter is the file name
        T1.init(this, "textures/Checker.png");
        T2.init(this, "textures/Textures_Forniture.png");
        TKey.init(this, "textures/MoveBanner.png");

        // Init local variables
    }

    // Here you create your pipelines and Descriptor Sets!
    void pipelinesAndDescriptorSetsInit() {
        // This creates a new pipeline (with the current surface), using its shaders
        P.create();
        POverlay.create();
        PVColor.create();

        // Here you define the data set
        DSGrid.init(this, &DSL, {
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
        DSKey.init(this, &DSLOverlay, {
                {0, UNIFORM, sizeof(OverlayUniformBlock), nullptr},
                {1, TEXTURE, 0,                           &TKey}
        });
        DSPolikea.init(this, &DSLVColor, {
                {0, UNIFORM, sizeof(UniformBlock), nullptr}
        });

        for (auto &mInfo: MV) {
            mInfo.dsModel.init(this, &DSL, {
                    {0, UNIFORM, sizeof(UniformBlock), nullptr},
                    {1, TEXTURE, 0,                    &T2}
            });
        }
    }

    // Here you destroy your pipelines and Descriptor Sets!
    // All the object classes defined in Starter.hpp have a method .cleanup() for this purpose
    void pipelinesAndDescriptorSetsCleanup() {
        // Cleanup pipelines
        P.cleanup();
        POverlay.cleanup();
        PVColor.cleanup();

        // Cleanup datasets
        DSGrid.cleanup();
        DSGubo.cleanup();
        DSKey.cleanup();
        DSPolikea.cleanup();

        for (auto &mInfo: MV) mInfo.dsModel.cleanup();
    }

    // Here you destroy all the Models, Texture and Desc. Set Layouts you created!
    // All the object classes defined in Starter.hpp have a method .cleanup() for this purpose
    // You also have to destroy the pipelines: since they need to be rebuilt, they have two different
    // methods: .cleanup() recreates them, while .destroy() delete them completely
    void localCleanup() {
        // Cleanup textures
        T1.cleanup();
        T2.cleanup();
        TKey.cleanup();

        // Cleanup models
        MGrid.cleanup();
        MKey.cleanup();
        Polikea.cleanup();
        for (auto &mInfo: MV) mInfo.model.cleanup();

        // Cleanup descriptor set layouts
        DSL.cleanup();
        DSLGubo.cleanup();
        DSLOverlay.cleanup();
        DSLVColor.cleanup();

        // Destroys the pipelines
        P.destroy();
        POverlay.destroy();
        PVColor.destroy();
    }

    // Here it is the creation of the command buffer:
    // You send to the GPU all the objects you want to draw,
    // with their buffers and textures

    void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
        // binds the pipeline
        // For a pipeline object, this command binds the corresponding pipeline to the command buffer passed in its parameter
        P.bind(commandBuffer);

        // binds the data set
        // For a Dataset object, this command binds the corresponding dataset
        // to the command buffer and pipeline passed in its first and second parameters.
        // The third parameter is the number of the set being bound
        // As described in the Vulkan tutorial, a different dataset is required for each image in the swap chain.
        // This is done automatically in file Starter.hpp, however the command here needs also the index
        // of the current image in the swap chain, passed in its last parameter
        DSGubo.bind(commandBuffer, P, 0, currentImage);
        DSGrid.bind(commandBuffer, P, 1, currentImage);

        // binds the model
        // For a Model object, this command binds the corresponding index and vertex buffer
        // to the command buffer passed in its parameter
        MGrid.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer,
                         static_cast<uint32_t>(MGrid.indices.size()), 1, 0, 0, 0);
        // the second parameter is the number of indexes to be drawn. For a Model object,
        // this can be retrieved with the .indices.size() method.

        for (auto &mInfo: MV) {
            mInfo.dsModel.bind(commandBuffer, P, 1, currentImage);
            mInfo.model.bind(commandBuffer);
            vkCmdDrawIndexed(commandBuffer,
                             static_cast<uint32_t>(mInfo.model.indices.size()), 1, 0, 0, 0);
        }

        POverlay.bind(commandBuffer);
        MKey.bind(commandBuffer);
        DSKey.bind(commandBuffer, POverlay, 0, currentImage);
        vkCmdDrawIndexed(commandBuffer,
                         static_cast<uint32_t>(MKey.indices.size()), 1, 0, 0, 0);

        PVColor.bind(commandBuffer);
        // TODO check this DSGubo bind
        DSGubo.bind(commandBuffer, PVColor, 0, currentImage);
        DSPolikea.bind(commandBuffer, PVColor, 1, currentImage);
        Polikea.bind(commandBuffer);
        vkCmdDrawIndexed(commandBuffer,
                         static_cast<uint32_t>(Polikea.indices.size()), 1, 0, 0, 0);
    }

    // Here is where you update the uniforms.
    // Very likely this will be where you will be writing the logic of your application.
    void updateUniformBuffer(uint32_t currentImage) {
        // Standard procedure to quit when the ESC key is pressed
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        // TODO maybe move to external function
        // ----- MOVE CAMERA AND OBJECTS LOGIC ----- //
        const float ROT_SPEED = glm::radians(120.0f);
        const float MOVE_SPEED = 2.0f;

        static bool debounce = false;
        static int curDebounce = 0;

        float deltaT;
        auto m = glm::vec3(0.0f), r = glm::vec3(0.0f);
        bool fire = false;
        getSixAxis(deltaT, m, r, fire);

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
        if (!OnlyMoveCam) {
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
                    if (distance > threshold) OnlyMoveCam = true;
                }
            }
        } else {
            if ((curDebounce == GLFW_KEY_SPACE) && debounce) {
                debounce = false;
                curDebounce = 0;
            }
        }

        gubo.DlightDir = glm::normalize(glm::vec3(1, 2, 3));
        gubo.DlightColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
        gubo.AmbLightColor = glm::vec3(0.2f);
        gubo.eyePos = CamPos;

        size_t index = 0;
        for(auto& modelInfo : MV) {
            for(auto light: modelInfo.model.lights) {
                gubo.lights[index].beta = 3.5f;
                gubo.lights[index].g = 3;
                gubo.lights[index].cosout = 0.6f;
                gubo.lights[index].cosin = 0.85f;
                gubo.lights[index].lightPos = glm::vec3(glm::rotate(glm::mat4(1.0), modelInfo.modelRot, glm::vec3(0, 1, 0)) * glm::vec4(std::get<0>(light), 1.0f)) + modelInfo.modelPos;
                gubo.lights[index].lightDir = glm::rotate(glm::mat4(1.0), modelInfo.modelRot, glm::vec3(0, 1, 0)) * glm::vec4(std::get<1>(light), 1.0f);
                gubo.lights[index].lightColor = glm::vec4(0.6f, 0.6f, 0.6f, 1.0f);
                index++;
            }
        }
        gubo.nLights = index;

        glm::mat4 ViewPrj = MakeViewProjectionMatrix(Ar, CamAlpha, CamBeta, CamRho, CamPos);
        glm::mat4 baseTr = glm::mat4(1.0f);
        glm::mat4 World = glm::scale(glm::mat4(1), glm::vec3(5.0f));

        uboPolikea.amb = 0.05f;
        uboPolikea.gamma = 180.0f;
        uboPolikea.sColor = glm::vec3(1.0f);
        uboPolikea.mvpMat = ViewPrj * glm::translate(glm::mat4(1), glm::vec3(15.0, 0.0, -15.0)) * World;
        uboPolikea.mMat = glm::translate(glm::mat4(1), glm::vec3(15.0, 0.0, -15.0)) * World;
        uboPolikea.nMat = glm::inverse(glm::transpose(World));
        DSPolikea.map(currentImage, &uboPolikea, sizeof(uboPolikea), 0);

        bool displayKey = false;
        for (std::size_t i = 1; i < MV.size(); ++i) {
            float distance = glm::distance(CamPos, MV[i].modelPos);
            if (distance <= threshold) {
                displayKey = true;
                break;
            }
        }

        uboKey.visible = (OnlyMoveCam && displayKey) ? 1.0f : 0.0f;
        DSKey.map(currentImage, &uboKey, sizeof(uboKey), 0);

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
        DSGrid.map(currentImage, &uboGrid, sizeof(uboGrid), 0);
        DSGubo.map(currentImage, &gubo, sizeof(gubo), 0);

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