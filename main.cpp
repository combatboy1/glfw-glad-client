#include <iostream>
#include <vector>
#include <chrono>
#include <filesystem>

#include "gl_includes.h"
#include "mini_glm.h"
#include "block.h"
#include "world.h"
#include "playercontroller.h"
#include "rendering.h"
#include "texture.h"

// OpenGL debug callback (function exists only when the debug extension / GL 4.3 is available)
static void APIENTRY GLDebugMessageCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length,
    const GLchar* message, const void* userParam)
{
    (void)source; (void)type; (void)id; (void)length; (void)userParam;
    // Filter out low-priority notifications to reduce noise
#if defined(GL_DEBUG_SEVERITY_NOTIFICATION)
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
#endif
    std::cerr << "GL DEBUG (severity=" << severity << "): " << message << "\n";
}

static std::string GetExeDir() {
    try {
        return std::filesystem::current_path().string();
    }
    catch (...) {
        return std::string();
    }
}

int main() {
    std::cout << "Starting application\n";

    // --- load blocks.json ---
    Block stoneBlock;
    std::unordered_map<std::string, Block> blocksByName;
    std::vector<Block> loaded;
    try {
        loaded = Block::LoadBlocksFromFile("blocks.json");
        for (const auto& b : loaded) blocksByName[b.getId()] = b;
        std::cout << "Loaded " << blocksByName.size() << " blocks from blocks.json\n";
    }
    catch (const std::exception& ex) {
        std::cerr << "Failed to load blocks.json: " << ex.what() << "\n";
    }

    auto it = blocksByName.find("Grass");
    if (it != blocksByName.end()) stoneBlock = it->second;
    else if (!blocksByName.empty()) stoneBlock = blocksByName.begin()->second;
    else {
        stoneBlock.setId("Stone"); stoneBlock.setDisplayName("Stone");
        stoneBlock.setHardness(1.5f); stoneBlock.setSolid(true);
    }

    if (!glfwInit()) {
        std::cerr << "glfwInit failed\n";
        return -1;
    }

    // Request modern OpenGL 3.3 core
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Textured Blocks (Modern GL)", nullptr, nullptr);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    // Load GL functions with GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        glfwTerminate();
        return -1;
    }
    std::cout << "GLAD initialized\n";

    // Enable GL debug output if available.
    // Wrap in preprocessor checks so this compiles even when GLAD was not generated with debug/4.3 support.
#if defined(GLAD_GL_KHR_debug) || defined(GLAD_GL_VERSION_4_3)
    // GL_DEBUG_OUTPUT and GL_DEBUG_OUTPUT_SYNCHRONOUS constants and glDebugMessageCallback are only defined
    // when the KHR_debug extension or OpenGL 4.3 is available.
    glEnable(GL_DEBUG_OUTPUT);
#  if defined(GL_DEBUG_OUTPUT_SYNCHRONOUS)
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#  endif
    // glDebugMessageCallback will be available if the extension/version is present
    glDebugMessageCallback(GLDebugMessageCallback, nullptr);
    std::cout << "GL debug callback enabled\n";
#else
    std::cout << "GL debug not available (GLAD built without KHR_debug / GL 4.3), skipping GL debug callback\n";
#endif

    glEnable(GL_DEPTH_TEST);

    PlayerController player(window);

    // Load atlas from executable dir to be robust
    std::string exeDir = GetExeDir();
    std::string atlasPath = exeDir.empty() ? "atlas.png" : (exeDir + "/atlas.png");
    std::cout << "Loading atlas from: " << atlasPath << "\n";

    TextureAtlas atlas;
    bool atlasLoaded = atlas.LoadFromFile(atlasPath, /*tileW=*/16, /*tileH=*/16, /*flipVertically=*/true);
    if (!atlasLoaded) {
        std::cerr << "Warning: failed to load atlas.png from: " << atlasPath << " -- rendering will use solid colors.\n";
    }
    else {
        std::cout << "Atlas loaded: cols=" << atlas.GetColumns() << " rows=" << atlas.GetRows()
            << " texId=" << atlas.GetTextureId() << "\n";
        rendering::SetAtlas(&atlas);
    }

    // initialize plane (after atlas is set so mesh builds with UVs)
    try {
        rendering::InitFlatPlane(stoneBlock, /*planeY=*/0);
        std::cout << "InitFlatPlane done\n";
    }
    catch (const std::exception& e) {
        std::cerr << "InitFlatPlane failed: " << e.what() << "\n";
    }

    double lastTime = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        double now = glfwGetTime();
        float dt = static_cast<float>(now - lastTime);
        lastTime = now;
        player.update(dt);

        int fbw, fbh;
        glfwGetFramebufferSize(window, &fbw, &fbh);
        if (fbh == 0) fbh = 1;
        glViewport(0, 0, fbw, fbh);

        float fov = 60.0f;
        glm::mat4 proj = glm::perspective(fov, float(fbw) / float(fbh), 0.1f, 200.0f);

        auto pos = player.getPosition();
        float yaw = player.getYaw(), pitch = player.getPitch();
        float yawRad = glm::radians(yaw), pitchRad = glm::radians(pitch);
        glm::vec3 camPos(pos.x, pos.y, pos.z);
        glm::vec3 front;
        front.x = std::sin(yawRad) * std::cos(pitchRad);
        front.y = std::sin(pitchRad);
        front.z = std::cos(yawRad) * std::cos(pitchRad);
        glm::vec3 center = camPos + glm::normalize(front);
        glm::mat4 view = glm::lookAt(camPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        rendering::RenderVisible(proj, view);

        glfwSwapBuffers(window);
    }

    rendering::Shutdown();
    glfwTerminate();
    return 0;
}