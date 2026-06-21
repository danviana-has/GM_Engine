#include "engine/Common.hpp"
#include "engine/Scene.hpp"
#include "engine/Physics.hpp"
#include "engine/Renderer.hpp"
#include "engine/ScriptEngine.hpp"
#include "engine/Platform.hpp"
#include "editor/Editor.hpp"
#include "player/Player.hpp"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <ImGuizmo.h>
#include <iomanip>
#include <cctype>

std::string checkEmbeddedScene() {
#ifdef _WIN32
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
#else
    return "";
#endif
}

#ifdef _WIN32
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

bool publishToGMPlay(const Scene& scene, const std::string& title, const std::string& description, std::string& outMessage) {
    std::string root = CppCompiler::getProjectRoot();
    std::string gmplayDir = root + "\\gmplay";
    std::string gamesDir = root + "\\gmplay\\games";
    
    // Ensure directories exist using absolute canonical paths
    CreateDirectoryA(gmplayDir.c_str(), NULL);
    CreateDirectoryA(gamesDir.c_str(), NULL);

    // Sanitize title for filename
    std::string safeTitle = "";
    for (char c : title) {
        if (std::isalnum(c)) {
            safeTitle += std::tolower(c);
        } else if (c == ' ') {
            safeTitle += '_';
        }
    }
    if (safeTitle.empty()) safeTitle = "game";
    
    std::string binaryRelativePath = "games/" + safeTitle + ".exe";
    std::string binaryFullPath = gamesDir + "\\" + safeTitle + ".exe";
    std::string dbJsonPath = gmplayDir + "\\games.json";
    std::string dbJsPath = gmplayDir + "\\games.js";

    // Duplicate current exe and append scene payload
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    
    std::ifstream src(path, std::ios::binary);
    if (!src.is_open()) { outMessage = "Failed to read engine executable."; return false; }
    
    std::ofstream dst(binaryFullPath, std::ios::binary);
    if (!dst.is_open()) { outMessage = "Failed to write game binary to: " + binaryFullPath; return false; }
    
    dst << src.rdbuf();
    
    dst << "//GM_" << "SCENE_START//";
    dst << scene.serializeToString();
    dst.close();
    src.close();

    // Update games.json registry
    nlohmann::json gamesList = nlohmann::json::array();
    std::ifstream dbIn(dbJsonPath);
    if (dbIn.is_open()) {
        try {
            dbIn >> gamesList;
        } catch (...) {
            // start fresh if corrupted
        }
        dbIn.close();
    }

    bool found = false;
    for (auto& entry : gamesList) {
        if (entry.contains("title") && entry["title"] == title) {
            entry["description"] = description;
            entry["binary"] = binaryRelativePath;
            
            std::ifstream test(binaryFullPath, std::ios::binary | std::ios::ate);
            double sizeMB = test.is_open() ? (double)test.tellg() / (1024.0 * 1024.0) : 5.1;
            std::stringstream ss;
            ss << std::fixed << std::setprecision(1) << sizeMB << " MB";
            entry["fileSize"] = ss.str();
            found = true;
            break;
        }
    }

    if (!found) {
        nlohmann::json newEntry;
        newEntry["id"] = "game_" + std::to_string(std::hash<std::string>{}(title) % 1000000);
        newEntry["title"] = title;
        newEntry["description"] = description;
        newEntry["binary"] = binaryRelativePath;
        newEntry["date"] = "2026-06-20";
        
        std::ifstream test(binaryFullPath, std::ios::binary | std::ios::ate);
        double sizeMB = test.is_open() ? (double)test.tellg() / (1024.0 * 1024.0) : 5.1;
        std::stringstream ss;
        ss << std::fixed << std::setprecision(1) << sizeMB << " MB";
        newEntry["fileSize"] = ss.str();
        
        gamesList.push_back(newEntry);
    }

    // Write back games.json
    std::ofstream dbOut(dbJsonPath);
    if (!dbOut.is_open()) { outMessage = "Failed to write games.json."; return false; }
    dbOut << gamesList.dump(4);
    dbOut.close();

    // Export games.js containing the global variable for browser compatibility (bypasses CORS)
    std::ofstream jsOut(dbJsPath);
    if (jsOut.is_open()) {
        jsOut << "var gmPlayGames = " << gamesList.dump(4) << ";\n";
        jsOut.close();
    }

    outMessage = "Game '" + title + "' published locally to gmplay/games/" + safeTitle + ".exe\n";

    // --- Auto git push to https://github.com/danviana-has/GM-Play (GitHub Pages) ---
    const std::string GMPLAY_REMOTE = "https://github.com/danviana-has/GM-Play.git";
    std::string logPath = root + "\\build\\git_push.log";

    // Helper: run a git command inside gmplayDir, capture combined stdout+stderr
    auto runGitCmd = [&](const std::string& args) -> std::pair<int, std::string> {
        std::string cmd = "\"\"git\" -C \"" + gmplayDir + "\" " + args + " > \"" + logPath + "\" 2>&1\"";
        int ret = std::system(cmd.c_str());
        std::ifstream lf(logPath);
        std::stringstream ls;
        if (lf.is_open()) ls << lf.rdbuf();
        return { ret, ls.str() };
    };

    // 1. Init repo inside gmplay/ if not already a git repo
    std::string gitDirPath = gmplayDir + "\\.git";
    DWORD gitDirAttr = GetFileAttributesA(gitDirPath.c_str());
    if (gitDirAttr == INVALID_FILE_ATTRIBUTES) {
        auto [initRet, initOut] = runGitCmd("init -b main");
        if (initRet != 0) {
            outMessage += "[git] Warning: git init failed.\n" + initOut;
            return true;
        }
        outMessage += "[git] Initialized new git repo in gmplay/\n";
    }

    // 2. Ensure remote 'origin' points to the correct GM-Play repo
    {
        std::string checkCmd = "\"\"git\" -C \"" + gmplayDir + "\" remote get-url origin > \"" + logPath + "\" 2>&1\"";
        int checkRet = std::system(checkCmd.c_str());
        if (checkRet != 0) {
            runGitCmd("remote add origin " + GMPLAY_REMOTE);
            outMessage += "[git] Remote 'origin' set to " + GMPLAY_REMOTE + "\n";
        } else {
            runGitCmd("remote set-url origin " + GMPLAY_REMOTE);
        }
    }

    // 3. Stage all files in gmplay/
    auto [addRet, addOut] = runGitCmd("add .");
    if (addRet != 0) {
        outMessage += "[git] Warning: git add failed.\n" + addOut;
        return true;
    }

    // 4. Commit
    std::string commitMsg = "GM Play: Publish game - " + title;
    auto [commitRet, commitOut] = runGitCmd("commit -m \"" + commitMsg + "\"");
    if (commitRet != 0 && commitOut.find("nothing to commit") == std::string::npos) {
        outMessage += "[git] Warning: git commit failed.\n" + commitOut;
        return true;
    }

    // 5. Push to origin main (--force because engine is the single source of truth)
    auto [pushRet, pushOut] = runGitCmd("push --force --set-upstream origin main");
    if (pushRet != 0) {
        outMessage += "[git] Warning: git push failed. Run manually:\n";
        outMessage += "  cd gmplay && git push --force --set-upstream origin main\n";
        outMessage += pushOut;
    } else {
        outMessage += "[git] Pushed! GitHub Pages: https://danviana-has.github.io/GM-Play\n";
        outMessage += "[git] Site updates in ~1 minute.\n";
    }

    return true;
}
#endif

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

    // Setup ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    if (!playMode) {
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    }

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    if (!playMode) {
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
            
            // Render HUD overlay
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(width, height));
            ImGui::Begin("StandaloneHUD", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
            
            for (const auto& elem : scene.starterGui) {
                if (elem->parentId == -1) {
                    GuiElement::drawElement(scene.starterGui, elem, 0.0f, 0.0f, (float)width, (float)height, true);
                }
            }
            
            ImGui::End();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            
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
            bool publishRequested = false;
            std::string publishTitle = "";
            std::string publishDescription = "";
            
            editor->draw(renderer, scene, physics, deltaTime, exportRequested, publishRequested, publishTitle, publishDescription);

            if (exportRequested) {
#ifdef _WIN32
                std::string outPath = "MyGame.exe";
                if (exportToExe(scene, outPath)) {
                    editor->logMessage("Standalone game compiled successfully: " + outPath, glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                } else {
                    editor->logMessage("Failed to compile standalone game.", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                }
#else
                std::string sceneStr = scene.serializeToString();
                Platform::triggerDownload("scene.gmg", sceneStr);
                editor->logMessage("Scene file downloaded! Load it using Open Scene.", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
#endif
            }

            if (publishRequested) {
#ifdef _WIN32
                std::string publishMsg;
                if (publishToGMPlay(scene, publishTitle, publishDescription, publishMsg)) {
                    // Log each line separately for readability
                    std::istringstream msgStream(publishMsg);
                    std::string line;
                    while (std::getline(msgStream, line)) {
                        if (!line.empty()) {
                            bool isGit   = line.rfind("[git]", 0) == 0;
                            bool isWarn  = line.find("Warning") != std::string::npos || line.find("failed") != std::string::npos;
                            glm::vec4 col = isGit
                                ? (isWarn ? glm::vec4(1.0f,0.6f,0.0f,1.0f) : glm::vec4(0.0f,1.0f,0.5f,1.0f))
                                : glm::vec4(0.0f,1.0f,0.5f,1.0f);
                            editor->logMessage(line, col);
                        }
                    }
                } else {
                    editor->logMessage("Failed to publish game to GM Play: " + publishMsg, glm::vec4(1.0f, 0.1f, 0.1f, 1.0f));
                }
#else
                editor->logMessage("Publishing is only supported on the Desktop version.", glm::vec4(1.0f, 0.5f, 0.0f, 1.0f));
#endif
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
    // Cleanup ImGui
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (!playMode) {
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
