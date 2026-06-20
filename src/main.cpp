#include "engine/Common.hpp"
#include "engine/Scene.hpp"
#include "engine/Physics.hpp"
#include "engine/Renderer.hpp"
#include "engine/ScriptEngine.hpp"
#include "editor/Editor.hpp"
#include "player/Player.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <windows.h>
#include <ImGuizmo.h>

std::string checkEmbeddedScene() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return "";
    
    std::stringstream ss;
    ss << file.rdbuf();
    std::string content = ss.str();
    
    std::string marker = std::string("//GM_") + "SCENE_START//";
    size_t pos = content.find(marker);
    if (pos != std::string::npos) {
        return content.substr(pos + marker.length());
    }
    return "";
}

bool exportToExe(const Scene& scene, const std::string& outputPath) {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    
    // Read current exe
    std::ifstream src(path, std::ios::binary);
    if (!src.is_open()) return false;
    
    // Write target exe
    std::ofstream dst(outputPath, std::ios::binary);
    if (!dst.is_open()) return false;
    
    dst << src.rdbuf(); // Copy the binary
    
    // Append the scene payload
    dst << "//GM_" << "SCENE_START//";
    dst << scene.serializeToString();
    
    return true;
}

int main(int argc, char* argv[]) {
    // 1. Determine Mode
    bool playMode = false;
    std::string sceneData = checkEmbeddedScene();
    std::string loadSceneFile = "";

    if (!sceneData.empty()) {
        playMode = true;
    } else {
        // Check arguments
        for (int i = 1; i < argc; ++i) {
            if (std::string(argv[i]) == "--play" && i + 1 < argc) {
                loadSceneFile = argv[i + 1];
                playMode = true;
                break;
            }
        }
    }

    // 2. Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 3. Create Window
    std::string windowTitle = playMode ? "GM Player" : "GM Studio - GM Engine";
    GLFWwindow* window = glfwCreateWindow(1280, 720, windowTitle.c_str(), NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable V-Sync

    // 4. Initialize Glad
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }

    // 5. Initialize Engine Components
    Scene scene;
    Physics physics;
    Renderer renderer;
    renderer.init();

    // Load scene if playMode requested
    if (playMode) {
        if (!sceneData.empty()) {
            scene.deserializeFromString(sceneData);
        } else if (!loadSceneFile.empty()) {
            scene.loadFromFile(loadSceneFile);
        }
    }

    // Initialize ScriptEngine state
    ScriptEngine::reset();

    // 6. Setup UI or Player Controller
    Editor* editor = nullptr;
    Player* player = nullptr;

    if (!playMode) {
        // Setup ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");

        editor = new Editor();
    } else {
        player = new Player();
        physics.compileAndLoadAllCppScripts(scene);
    }

    // 7. Main Loop variables
    float lastFrame = 0.0f;

    // 8. Main Loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = static_cast<float>(glfwGetTime());
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        if (playMode) {
            // Standalone Play mode: update physics and render scene
            physics.update(scene, deltaTime);
            player->runFrame(window, renderer, scene, physics, deltaTime);
            
            // Allow exiting playmode with ESC
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                glfwSetWindowShouldClose(window, true);
            }
        } 
        else {
            // Editor Mode
            if (editor->viewport.isPlaying) {
                physics.update(scene, deltaTime);
            }

            // Start ImGui Frame
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGuizmo::BeginFrame();

            bool exportRequested = false;
            editor->draw(renderer, scene, physics, deltaTime, exportRequested);

            if (exportRequested) {
                std::string outPath = "MyGame.exe";
                if (exportToExe(scene, outPath)) {
                    editor->logMessage("Standalone game compiled successfully: " + outPath, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                } else {
                    editor->logMessage("Failed to compile standalone game.", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                }
            }

            // Render ImGui
            ImGui::Render();
            int display_w, display_h;
            glfwGetFramebufferSize(window, &display_w, &display_h);
            glViewport(0, 0, display_w, display_h);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        glfwSwapBuffers(window);
    }

    // 9. Cleanup
    if (!playMode) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        delete editor;
    } else {
        physics.unloadAllCppScripts();
        delete player;
    }

    MeshCache::clear();
    renderer.cleanup();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
