#include <string>
#include <iostream>
#include <filesystem>
#include "Starter.hpp"
#include "WorldView.hpp"

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
    alignas(16) glm::mat4 mvpMat;
};

// The vertices data structures
struct Vertex {
    glm::vec3 pos;
    glm::vec2 UV;
};

// Struct used to store data related to a model
struct ModelInfo {
    Model<Vertex> model;
    DescriptorSet dsModel;
    UniformBlock modelUBO{};
    glm::vec3 modelPos{};
    glm::quat modelRot{};
};


// MAIN !
class VTemplate : public BaseProject {
protected:

    // Current aspect ratio (used by the callback that resized the window)
    float Ar;

    // Descriptor Layouts ["classes" of what will be passed to the shaders]
    DescriptorSetLayout DSL;

    // Vertex formats
    VertexDescriptor VD;

    // Pipelines [Shader couples]
    Pipeline P;

    // Models, textures and Descriptors (values assigned to the uniforms)
    // Please note that Model objects depends on the corresponding vertex structure
    // Models
    Model<Vertex> MGrid;
    // Descriptor sets
    DescriptorSet DSGrid;
    // Textures
    Texture T1, T2;
    // C++ storage for uniform variables
    UniformBlock uboGrid;

    // Other application parameters
    // A vector containing one element for each model loaded where we want to keep track of its information
    std::vector<ModelInfo> MV;

    glm::vec3 CamPos = glm::vec3(0.440019, 0.5, 3.45706);;
    float CamAlpha = 0.0f;
    float CamBeta = 0.0f;
    float CamRho = 0.0f;

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
                {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT}
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
                        {0, 1, VK_FORMAT_R32G32_SFLOAT,    offsetof(Vertex, UV),
                                sizeof(glm::vec2), UV}
                });

        // Pipelines [Shader couples]
        // The second parameter is the pointer to the vertex definition
        // Third and fourth parameters are respectively the vertex and fragment shaders
        // The last array, is a vector of pointer to the layouts of the sets that will
        // be used in this pipeline. The first element will be set 0, and so on
        P.init(this, &VD, "shaders/ShaderVert.spv", "shaders/ShaderFrag.spv", {&DSL});

        // Models, textures and Descriptors (values assigned to the uniforms)

        // Create models
        // TODO maybe move to external function
        int i = 0;
        std::string path = "models";
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
            MI.modelRot = glm::quat(glm::vec3(0, 0, 0)) *
                          glm::quat(glm::vec3(0, 0, 0)) *
                          glm::quat(glm::vec3(0, 0, 0));
            MV.push_back(MI);
            i++;
        }

        // Creates a mesh with direct enumeration of vertices and indices
        MGrid.vertices = {{{-6, -2, -6}, {0.0f, 0.0f}},
                          {{-6, -2, 6},  {0.0f, 1.0f}},
                          {{6,  -2, -6}, {1.0f, 0.0f}},
                          {{6,  -2, 6},  {1.0f, 1.0f}}};
        MGrid.indices = {0, 1, 2, 1, 3, 2};
        MGrid.initMesh(this, &VD);

        // Create the textures
        // The second parameter is the file name
        T1.init(this, "textures/Checker.png");
        T2.init(this, "textures/Textures_Forniture.png");

        // Init local variables
    }

    // Here you create your pipelines and Descriptor Sets!
    void pipelinesAndDescriptorSetsInit() {
        // This creates a new pipeline (with the current surface), using its shaders
        P.create();

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

        // Cleanup datasets
        DSGrid.cleanup();

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

        // Cleanup models
        MGrid.cleanup();
        for (auto &mInfo: MV)
            mInfo.model.cleanup();

        // Cleanup descriptor set layouts
        DSL.cleanup();

        // Destroys the pipelines
        P.destroy();
    }

    // Here it is the creation of the command buffer:
    // You send to the GPU all the objects you want to draw,
    // with their buffers and textures

    void populateCommandBuffer(VkCommandBuffer commandBuffer, int currentImage) {
        // binds the pipeline
        P.bind(commandBuffer);
        // For a pipeline object, this command binds the corresponding pipeline to the command buffer passed in its parameter

        // binds the data set
        DSGrid.bind(commandBuffer, P, 0, currentImage);
        // For a Dataset object, this command binds the corresponding dataset
        // to the command buffer and pipeline passed in its first and second parameters.
        // The third parameter is the number of the set being bound
        // As described in the Vulkan tutorial, a different dataset is required for each image in the swap chain.
        // This is done automatically in file Starter.hpp, however the command here needs also the index
        // of the current image in the swap chain, passed in its last parameter

        // binds the model
        MGrid.bind(commandBuffer);
        // For a Model object, this command binds the corresponding index and vertex buffer
        // to the command buffer passed in its parameter
        vkCmdDrawIndexed(commandBuffer,
                         static_cast<uint32_t>(MGrid.indices.size()), 1, 0, 0, 0);
        // the second parameter is the number of indexes to be drawn. For a Model object,
        // this can be retrieved with the .indices.size() method.

        for (auto &mInfo: MV) {
            mInfo.dsModel.bind(commandBuffer, P, 0, currentImage);
            mInfo.model.bind(commandBuffer);
            vkCmdDrawIndexed(commandBuffer,
                             static_cast<uint32_t>(mInfo.model.indices.size()), 1, 0, 0, 0);
        }
    }

    // Here is where you update the uniforms.
    // Very likely this will be where you will be writing the logic of your application.
    void updateUniformBuffer(uint32_t currentImage) {
        // Standard procedure to quit when the ESC key is pressed
        if (glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            glfwSetWindowShouldClose(window, GL_TRUE);
        }

        // TODO maybe move to external function
        const float ROT_SPEED = glm::radians(120.0f);
        const float MOVE_SPEED = 2.0f;

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
        CamPos = CamPos + MOVE_SPEED * m.y * glm::vec3(0, 1, 0) * deltaT;
        CamPos = CamPos + MOVE_SPEED * m.z * uz * deltaT;

        glm::mat4 ViewPrj = MakeViewProjectionMatrix(Ar, CamAlpha, CamBeta, CamRho, CamPos);
        glm::mat4 baseTr = glm::mat4(1.0f);

        glm::mat4 World = glm::translate(glm::mat4(1), glm::vec3(0, -5, 0)) *
                          glm::scale(glm::mat4(1), glm::vec3(5.0f));
        uboGrid.mvpMat = ViewPrj * World;
        // the .map() method of a DataSet object, requires the current image of the swap chain as first parameter
        // the second parameter is the pointer to the C++ data structure to transfer to the GPU
        // the third parameter is its size
        // the fourth parameter is the location inside the descriptor set of this uniform block
        DSGrid.map(currentImage, &uboGrid, sizeof(uboGrid), 0);

        for (auto &mInfo: MV) {
            World = MakeWorldMatrix(mInfo.modelPos, mInfo.modelRot, glm::vec3(1.0f, 1.0f, 1.0f)) * baseTr;
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