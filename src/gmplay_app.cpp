#include "Common.hpp"
#include "Scene.hpp"
#include "Renderer.hpp"
#include "Physics.hpp"
#include "player/Player.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nlohmann/json.hpp>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <sstream>
#include <map>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")

// --- Network Structures ---
struct RemotePlayerState {
    int id;
    std::string username;
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 size;
    glm::vec4 color;
    int accessoryType;
    glm::vec4 accessoryColor;
};

// --- Global State ---
enum class AppState {
    Browser,
    Customizer,
    HostSetup,
    JoinSetup,
    Playing
};

AppState currentAppState = AppState::Browser;
std::string selectedGameTitle = "";
std::string selectedGamePath = "";
std::string selectedGameId = "";

// --- Customization Fields ---
char myUsername[64] = "AvatarMaster";
glm::vec4 myColor = glm::vec4(0.1f, 0.6f, 0.9f, 1.0f);
float myScale = 1.0f;
int myAccessoryType = 1; // 1 = Hat, 2 = Crown, 3 = Horns
glm::vec4 myAccessoryColor = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);

// --- Networking Variables ---
SOCKET serverSocket = INVALID_SOCKET;
SOCKET clientSocket = INVALID_SOCKET;
bool isHost = false;
bool isConnected = false;
int myClientId = -1;

std::mutex netMutex;
std::map<int, RemotePlayerState> remotePlayers;
std::vector<std::string> chatLog;
char chatInput[128] = "";
bool chatActive = false;

// --- Multiplayer Server ---
struct ServerClientInfo {
    SOCKET socket;
    int id;
    std::string username;
};

std::mutex serverMutex;
std::vector<ServerClientInfo> serverClients;
SOCKET serverListenSocket = INVALID_SOCKET;
bool serverThreadRunning = false;
int serverNextPlayerId = 1;

void broadcastServerMessage(const std::string& msg, SOCKET excludeSocket = INVALID_SOCKET) {
    std::lock_guard<std::mutex> lock(serverMutex);
    std::string framed = msg + "\n";
    for (const auto& client : serverClients) {
        if (client.socket != excludeSocket) {
            send(client.socket, framed.c_str(), (int)framed.length(), 0);
        }
    }
}

void serverClientHandler(SOCKET clientSocket, int playerId) {
    char buffer[4096];
    std::string accumulated = "";
    
    // Send welcome packet
    nlohmann::json welcome;
    welcome["type"] = "welcome";
    welcome["id"] = playerId;
    std::string welcomeStr = welcome.dump() + "\n";
    send(clientSocket, welcomeStr.c_str(), (int)welcomeStr.length(), 0);
    
    while (serverThreadRunning) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) break;
        
        buffer[bytes] = '\0';
        accumulated += buffer;
        
        size_t newline;
        while ((newline = accumulated.find('\n')) != std::string::npos) {
            std::string line = accumulated.substr(0, newline);
            accumulated.erase(0, newline + 1);
            if (line.empty()) continue;
            
            try {
                auto j = nlohmann::json::parse(line);
                std::string type = j.value("type", "");
                
                if (type == "join") {
                    j["id"] = playerId;
                    {
                        std::lock_guard<std::mutex> lock(serverMutex);
                        for (auto& c : serverClients) {
                            if (c.id == playerId) {
                                c.username = j.value("username", "Guest");
                                break;
                            }
                        }
                    }
                    // Broadcast join to everyone
                    broadcastServerMessage(j.dump(), clientSocket);
                    
                    // Chat system notification
                    nlohmann::json joinChat;
                    joinChat["type"] = "chat";
                    joinChat["sender"] = "SYSTEM";
                    joinChat["message"] = j.value("username", "Guest") + " entrou no servidor!";
                    broadcastServerMessage(joinChat.dump());
                }
                else if (type == "update") {
                    j["id"] = playerId;
                    broadcastServerMessage(j.dump(), clientSocket);
                }
                else if (type == "chat") {
                    broadcastServerMessage(j.dump());
                }
            } catch (...) {}
        }
    }
    
    // Disconnection cleanup
    closesocket(clientSocket);
    std::string leftName = "Guest";
    {
        std::lock_guard<std::mutex> lock(serverMutex);
        auto it = std::find_if(serverClients.begin(), serverClients.end(), [playerId](const ServerClientInfo& c) { return c.id == playerId; });
        if (it != serverClients.end()) {
            leftName = it->username;
            serverClients.erase(it);
        }
    }
    
    nlohmann::json leave;
    leave["type"] = "leave";
    leave["id"] = playerId;
    broadcastServerMessage(leave.dump());
    
    nlohmann::json leaveChat;
    leaveChat["type"] = "chat";
    leaveChat["sender"] = "SYSTEM";
    leaveChat["message"] = leftName + " saiu do servidor.";
    broadcastServerMessage(leaveChat.dump());
}

void startMultiplayerServer(int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return;
    
    struct addrinfo hints, *result = NULL;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    
    std::string portStr = std::to_string(port);
    if (getaddrinfo(NULL, portStr.c_str(), &hints, &result) != 0) {
        WSACleanup();
        return;
    }
    
    serverListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (serverListenSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        WSACleanup();
        return;
    }
    
    if (bind(serverListenSocket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(serverListenSocket);
        WSACleanup();
        return;
    }
    
    freeaddrinfo(result);
    
    if (listen(serverListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(serverListenSocket);
        WSACleanup();
        return;
    }
    
    serverThreadRunning = true;
    serverNextPlayerId = 1;
    
    while (serverThreadRunning) {
        SOCKET clientSock = accept(serverListenSocket, NULL, NULL);
        if (clientSock == INVALID_SOCKET) break;
        
        int pId = serverNextPlayerId++;
        {
            std::lock_guard<std::mutex> lock(serverMutex);
            ServerClientInfo info;
            info.socket = clientSock;
            info.id = pId;
            info.username = "Guest";
            serverClients.push_back(info);
        }
        
        std::thread(serverClientHandler, clientSock, pId).detach();
    }
    
    closesocket(serverListenSocket);
    WSACleanup();
}

void stopServer() {
    serverThreadRunning = false;
    if (serverListenSocket != INVALID_SOCKET) {
        closesocket(serverListenSocket);
        serverListenSocket = INVALID_SOCKET;
    }
    std::lock_guard<std::mutex> lock(serverMutex);
    for (auto& c : serverClients) {
        closesocket(c.socket);
    }
    serverClients.clear();
}

// --- Multiplayer Client ---
void clientReceiveThread() {
    char buffer[4096];
    std::string accumulated = "";
    
    while (isConnected) {
        int bytes = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            isConnected = false;
            break;
        }
        buffer[bytes] = '\0';
        accumulated += buffer;
        
        size_t newline;
        while ((newline = accumulated.find('\n')) != std::string::npos) {
            std::string line = accumulated.substr(0, newline);
            accumulated.erase(0, newline + 1);
            if (line.empty()) continue;
            
            try {
                auto j = nlohmann::json::parse(line);
                std::string type = j.value("type", "");
                
                if (type == "welcome") {
                    myClientId = j.value("id", -1);
                }
                else if (type == "chat") {
                    std::lock_guard<std::mutex> lock(netMutex);
                    std::string sender = j.value("sender", "");
                    std::string msg = j.value("message", "");
                    chatLog.push_back(sender + ": " + msg);
                }
                else if (type == "update") {
                    int pId = j.value("id", -1);
                    if (pId != myClientId) {
                        RemotePlayerState state;
                        state.id = pId;
                        state.username = j.value("username", "Guest");
                        
                        auto pos = j["position"];
                        state.position = glm::vec3(pos[0], pos[1], pos[2]);
                        
                        auto rot = j["rotation"];
                        state.rotation = glm::vec3(rot[0], rot[1], rot[2]);
                        
                        auto size = j["size"];
                        state.size = glm::vec3(size[0], size[1], size[2]);
                        
                        auto col = j["color"];
                        state.color = glm::vec4(col[0], col[1], col[2], col[3]);
                        
                        state.accessoryType = j.value("accessoryType", 0);
                        
                        auto accCol = j["accessoryColor"];
                        state.accessoryColor = glm::vec4(accCol[0], accCol[1], accCol[2], accCol[3]);
                        
                        std::lock_guard<std::mutex> lock(netMutex);
                        remotePlayers[pId] = state;
                    }
                }
                else if (type == "leave") {
                    int pId = j.value("id", -1);
                    std::lock_guard<std::mutex> lock(netMutex);
                    remotePlayers.erase(pId);
                }
            } catch (...) {}
        }
    }
    
    closesocket(clientSocket);
    clientSocket = INVALID_SOCKET;
    isConnected = false;
}

bool connectToMultiplayerServer(const std::string& ip, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;
    
    clientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (clientSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }
    
    sockaddr_in clientService;
    clientService.sin_family = AF_INET;
    inet_pton(AF_INET, ip.c_str(), &clientService.sin_addr);
    clientService.sin_port = htons(port);
    
    if (connect(clientSocket, (SOCKADDR*)&clientService, sizeof(clientService)) == SOCKET_ERROR) {
        closesocket(clientSocket);
        WSACleanup();
        return false;
    }
    
    isConnected = true;
    std::thread(clientReceiveThread).detach();
    
    // Send join packet
    nlohmann::json join;
    join["type"] = "join";
    join["username"] = std::string(myUsername);
    join["color"] = { myColor.r, myColor.g, myColor.b, myColor.a };
    join["size"] = { myScale * 1.8f, myScale * 4.0f, myScale * 1.8f };
    join["accessoryType"] = myAccessoryType;
    join["accessoryColor"] = { myAccessoryColor.r, myAccessoryColor.g, myAccessoryColor.b, myAccessoryColor.a };
    
    std::string joinStr = join.dump() + "\n";
    send(clientSocket, joinStr.c_str(), (int)joinStr.length(), 0);
    
    return true;
}

void sendClientUpdate(const glm::vec3& pos, const glm::vec3& rot) {
    if (!isConnected) return;
    
    nlohmann::json packet;
    packet["type"] = "update";
    packet["username"] = std::string(myUsername);
    packet["position"] = { pos.x, pos.y, pos.z };
    packet["rotation"] = { rot.x, rot.y, rot.z };
    packet["size"] = { myScale * 1.8f, myScale * 4.0f, myScale * 1.8f };
    packet["color"] = { myColor.r, myColor.g, myColor.b, myColor.a };
    packet["accessoryType"] = myAccessoryType;
    packet["accessoryColor"] = { myAccessoryColor.r, myAccessoryColor.g, myAccessoryColor.b, myAccessoryColor.a };
    
    std::string pktStr = packet.dump() + "\n";
    send(clientSocket, pktStr.c_str(), (int)pktStr.length(), 0);
}

void sendClientChatMessage(const std::string& msg) {
    if (!isConnected) return;
    
    nlohmann::json packet;
    packet["type"] = "chat";
    packet["sender"] = std::string(myUsername);
    packet["message"] = msg;
    
    std::string pktStr = packet.dump() + "\n";
    send(clientSocket, pktStr.c_str(), (int)pktStr.length(), 0);
}

void disconnectClient() {
    isConnected = false;
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
    std::lock_guard<std::mutex> lock(netMutex);
    remotePlayers.clear();
    chatLog.clear();
}

// --- Map Loader Helper ---
std::string checkEmbeddedSceneInExe(const std::string& exePath) {
    std::ifstream file(exePath, std::ios::binary);
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

void createDefaultObbyScene(Scene& scene) {
    scene.reset();
    
    // Add some jumping blocks
    for (int i = 0; i < 5; ++i) {
        auto step = scene.addPart(PartShape::Block);
        step->name = "Step " + std::to_string(i + 1);
        step->position = glm::vec3(0.0f, 2.0f + i * 2.0f, -10.0f - i * 8.0f);
        step->size = glm::vec3(4.0f, 0.8f, 4.0f);
        step->color = glm::vec4(0.2f, 0.6f, 0.9f, 1.0f);
        step->anchored = true;
    }
    
    // Add a checkpoint
    auto cp = scene.addPart(PartShape::Cylinder);
    cp->name = "Checkpoint";
    cp->position = glm::vec3(0.0f, 11.0f, -50.0f);
    cp->size = glm::vec3(4.0f, 0.5f, 4.0f);
    cp->color = glm::vec4(0.9f, 0.9f, 0.1f, 1.0f);
    cp->anchored = true;
    cp->canCollide = false;
    cp->script = "checkpoint\n";
    
    // Add some more blocks moving sideways (pingpong)
    auto pp = scene.addPart(PartShape::Block);
    pp->name = "PingPongStep";
    pp->position = glm::vec3(0.0f, 13.0f, -65.0f);
    pp->size = glm::vec3(4.0f, 0.8f, 4.0f);
    pp->color = glm::vec4(0.9f, 0.3f, 0.3f, 1.0f);
    pp->anchored = true;
    pp->script = "pingpong -8 13 -65 8 13 -65 4.0\n";
    
    // Add a victory point
    auto victory = scene.addPart(PartShape::Block);
    victory->name = "VictoryPoint";
    victory->position = glm::vec3(0.0f, 15.0f, -80.0f);
    victory->size = glm::vec3(3.0f, 3.0f, 3.0f);
    victory->color = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f);
    victory->material = PartMaterial::Neon;
    victory->anchored = true;
    victory->canCollide = false;
    victory->script = "victory\n";
}

void syncNetworkPlayers(Scene& scene) {
    std::lock_guard<std::mutex> lock(netMutex);
    
    // 1. Remove parts for players who left
    for (auto it = scene.workspace.begin(); it != scene.workspace.end(); ) {
        auto part = *it;
        if (part->isPlayer && part->name.rfind("Player_", 0) == 0) {
            try {
                int pId = std::stoi(part->name.substr(7));
                if (remotePlayers.find(pId) == remotePlayers.end()) {
                    it = scene.workspace.erase(it);
                    continue;
                }
            } catch (...) {}
        }
        ++it;
    }
    
    // 2. Add or update parts for active players
    for (const auto& pair : remotePlayers) {
        int pId = pair.first;
        const auto& state = pair.second;
        
        std::shared_ptr<Part> netPart = nullptr;
        std::string partName = "Player_" + std::to_string(pId);
        
        for (const auto& part : scene.workspace) {
            if (part->isPlayer && part->name == partName) {
                netPart = part;
                break;
            }
        }
        
        if (!netPart) {
            netPart = scene.addPart(PartShape::Block);
            netPart->isPlayer = true;
            netPart->name = partName;
            netPart->anchored = true; 
            netPart->canCollide = true;
        }
        
        // Update properties
        netPart->position = state.position;
        netPart->rotation = state.rotation;
        netPart->size = state.size;
        netPart->color = state.color;
        netPart->username = state.username;
        netPart->accessoryType = state.accessoryType;
        netPart->accessoryColor = state.accessoryColor;
    }
}

// --- Style setup ---
void applyRichDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.PopupRounding = 6.0f;
    
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text]                   = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
    colors[ImGuiCol_WindowBg]               = ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg]                = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    colors[ImGuiCol_PopupBg]                = ImVec4(0.11f, 0.11f, 0.13f, 0.92f);
    colors[ImGuiCol_Border]                 = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]                = ImVec4(0.15f, 0.15f, 0.17f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_FrameBgActive]          = ImVec4(0.25f, 0.25f, 0.27f, 1.00f);
    colors[ImGuiCol_TitleBg]                = ImVec4(0.07f, 0.07f, 0.08f, 1.00f);
    colors[ImGuiCol_TitleBgActive]          = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgCollapsed]      = ImVec4(0.07f, 0.07f, 0.08f, 1.00f);
    colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.16f, 1.00f);
    colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
    colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.35f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.40f, 0.40f, 0.44f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.55f, 1.00f);
    colors[ImGuiCol_CheckMark]              = ImVec4(0.10f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrab]             = ImVec4(0.10f, 0.60f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.20f, 0.70f, 1.00f, 1.00f);
    colors[ImGuiCol_Button]                 = ImVec4(0.12f, 0.40f, 0.75f, 1.00f);
    colors[ImGuiCol_ButtonHovered]          = ImVec4(0.18f, 0.50f, 0.85f, 1.00f);
    colors[ImGuiCol_ButtonActive]           = ImVec4(0.10f, 0.35f, 0.68f, 1.00f);
    colors[ImGuiCol_Header]                 = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
    colors[ImGuiCol_HeaderHovered]          = ImVec4(0.25f, 0.25f, 0.28f, 1.00f);
    colors[ImGuiCol_HeaderActive]           = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_Separator]              = ImVec4(0.18f, 0.18f, 0.20f, 1.00f);
}

// --- MAIN ENTRY POINT ---
int main(int argc, char* argv[]) {
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    GLFWwindow* window = glfwCreateWindow(1280, 720, "GM Play - MMO Platform", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return -1;
    }
    
    // Setup Engine Components
    Scene scene;
    Physics physics;
    Renderer renderer;
    renderer.init();
    
    Player player;
    
    // Initialize ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    
    applyRichDarkTheme();
    
    // Load Games List
    nlohmann::json gamesList = nlohmann::json::array();
    std::string dbJsonPath = CppCompiler::getProjectRoot() + "/gmplay/games.json";
    std::ifstream file(dbJsonPath);
    if (file.is_open()) {
        try {
            file >> gamesList;
        } catch (...) {}
        file.close();
    }
    
    float lastFrameTime = 0.0f;
    char joinIp[64] = "127.0.0.1";
    int joinPort = 7777;
    int hostPort = 7777;
    
    while (!glfwWindowShouldClose(window)) {
        float currentFrameTime = static_cast<float>(glfwGetTime());
        float dt = currentFrameTime - lastFrameTime;
        lastFrameTime = currentFrameTime;
        
        glfwPollEvents();
        
        // Network Sync Player positions & spawn networked avatars
        if (currentAppState == AppState::Playing) {
            syncNetworkPlayers(scene);
            
            // Local Physics Update
            physics.update(scene, dt);
            player.runFrame(window, renderer, scene, physics, dt);
            
            // Send position updates to server
            std::shared_ptr<Part> localChar = nullptr;
            for (auto& part : scene.workspace) {
                if (part->isPlayer && part->name == "Player") {
                    localChar = part;
                    break;
                }
            }
            if (localChar) {
                // Apply custom scale
                localChar->size = glm::vec3(myScale * 1.8f, myScale * 4.0f, myScale * 1.8f);
                localChar->color = myColor;
                localChar->username = std::string(myUsername);
                localChar->accessoryType = myAccessoryType;
                localChar->accessoryColor = myAccessoryColor;
                
                sendClientUpdate(localChar->position, localChar->rotation);
            }
        } else {
            // Background preview rendering for Customizer
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            glViewport(0, 0, width, height);
            glClearColor(0.08f, 0.08f, 0.09f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        }
        
        // ImGui drawing
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        
        if (currentAppState == AppState::Browser) {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
            ImGui::Begin("GM Play Browser", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_MenuBar);
            
            // Menu bar
            if (ImGui::BeginMenuBar()) {
                ImGui::Text("GM PLAY - Plataforma de Jogos MMO C++");
                ImGui::EndMenuBar();
            }
            
            ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.9f, 1.0f), "Escolha um jogo dos usuarios para jogar online!");
            ImGui::Separator();
            
            // Show games list
            if (gamesList.empty()) {
                ImGui::Text("Nenhum jogo encontrado em gmplay/games.json.");
            } else {
                ImGui::Spacing();
                ImGui::Columns(3, "GameGrid", false);
                for (size_t i = 0; i < gamesList.size(); ++i) {
                    auto game = gamesList[i];
                    std::string title = game.value("title", "Sem Nome");
                    std::string desc = game.value("description", "");
                    std::string bin = game.value("binary", "#");
                    std::string date = game.value("date", "");
                    std::string size = game.value("fileSize", "");
                    
                    ImGui::BeginChild((std::string("Card_") + std::to_string(i)).c_str(), ImVec2(-10, 160), true);
                    ImGui::TextColored(ImVec4(1, 1, 1, 1), "%s", title.c_str());
                    ImGui::TextDisabled("Data: %s | Tamanho: %s", date.c_str(), size.c_str());
                    ImGui::Separator();
                    ImGui::TextWrapped("%s", desc.c_str());
                    ImGui::Spacing();
                    
                    if (ImGui::Button((std::string("Jogar MMO##") + std::to_string(i)).c_str(), ImVec2(-1, 30))) {
                        selectedGameTitle = title;
                        selectedGamePath = bin;
                        selectedGameId = game.value("id", "default");
                        currentAppState = AppState::Customizer;
                    }
                    ImGui::EndChild();
                    ImGui::NextColumn();
                }
                ImGui::Columns(1);
            }
            ImGui::End();
        }
        else if (currentAppState == AppState::Customizer) {
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2((float)width, (float)height));
            ImGui::Begin("Avatar Customizer", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoResize);
            
            ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.9f, 1.0f), "CUSTOMIZE SEU PERSONAGEM (MMO)");
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Columns(2, "CustomizerColumns", false);
            
            // Left Column: Inputs
            ImGui::InputText("Nome do Jogador", myUsername, sizeof(myUsername));
            ImGui::ColorEdit4("Cor do Corpo", glm::value_ptr(myColor));
            ImGui::SliderFloat("Escala do Personagem", &myScale, 0.5f, 2.0f);
            
            const char* accs[] = { "Nenhum", "Chapeu Esferico", "Coroa de Metal", "Chifres de Wedge" };
            ImGui::Combo("Acessorio de Cabeca", &myAccessoryType, accs, IM_ARRAYSIZE(accs));
            if (myAccessoryType > 0) {
                ImGui::ColorEdit4("Cor do Acessorio", glm::value_ptr(myAccessoryColor));
            }
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::Text("Modo Multiplayer:");
            if (ImGui::Button("Hospedar Servidor (Host)", ImVec2(200, 40))) {
                currentAppState = AppState::HostSetup;
            }
            ImGui::SameLine();
            if (ImGui::Button("Conectar a Servidor (Join)", ImVec2(200, 40))) {
                currentAppState = AppState::JoinSetup;
            }
            
            ImGui::Spacing();
            if (ImGui::Button("< Voltar para Navegador", ImVec2(200, 30))) {
                currentAppState = AppState::Browser;
            }
            
            ImGui::NextColumn();
            
            // Right Column: 3D Preview (simulate with custom window)
            ImGui::Text("Preview do Personagem:");
            ImGui::BeginChild("PreviewWindow", ImVec2(350, 350), true);
            ImGui::Text("Visualizacao 3D:");
            ImGui::TextDisabled("Avatar: %s", myUsername);
            ImGui::TextDisabled("Escala: %.2f", myScale);
            ImGui::TextDisabled("Corpo: (%.1f, %.1f, %.1f)", myColor.r, myColor.g, myColor.b);
            if (myAccessoryType > 0) {
                ImGui::TextDisabled("Acessorio: %s", accs[myAccessoryType]);
            }
            ImGui::EndChild();
            
            ImGui::Columns(1);
            ImGui::End();
        }
        else if (currentAppState == AppState::HostSetup) {
            ImGui::SetNextWindowPos(ImVec2(width * 0.25f, height * 0.25f));
            ImGui::SetNextWindowSize(ImVec2(width * 0.5f, height * 0.5f));
            ImGui::Begin("Setup Server Host", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            
            ImGui::Text("Configuracao do Servidor:");
            ImGui::InputInt("Porta TCP", &hostPort);
            
            ImGui::Spacing();
            if (ImGui::Button("Iniciar e Jogar", ImVec2(200, 45))) {
                isHost = true;
                std::thread(startMultiplayerServer, hostPort).detach();
                // Wait briefly for server startup
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                
                if (connectToMultiplayerServer("127.0.0.1", hostPort)) {
                    // Load selected scene
                    if (selectedGamePath == "#") {
                        createDefaultObbyScene(scene);
                    } else {
                        // Read path relative to projects root
                        std::string exeAbsPath = CppCompiler::getProjectRoot() + "/gmplay/" + selectedGamePath;
                        std::string sceneStr = checkEmbeddedSceneInExe(exeAbsPath);
                        if (!sceneStr.empty()) {
                            scene.deserializeFromString(sceneStr);
                        } else {
                            createDefaultObbyScene(scene);
                        }
                    }
                    player.initPlayerCharacter(scene);
                    currentAppState = AppState::Playing;
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancelar", ImVec2(100, 45))) {
                currentAppState = AppState::Customizer;
            }
            
            ImGui::End();
        }
        else if (currentAppState == AppState::JoinSetup) {
            ImGui::SetNextWindowPos(ImVec2(width * 0.25f, height * 0.25f));
            ImGui::SetNextWindowSize(ImVec2(width * 0.5f, height * 0.5f));
            ImGui::Begin("Connect to Server", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
            
            ImGui::Text("Insira os dados do Host:");
            ImGui::InputText("IP do Servidor", joinIp, sizeof(joinIp));
            ImGui::InputInt("Porta TCP", &joinPort);
            
            ImGui::Spacing();
            if (ImGui::Button("Conectar", ImVec2(200, 45))) {
                isHost = false;
                if (connectToMultiplayerServer(std::string(joinIp), joinPort)) {
                    // Load selected scene
                    if (selectedGamePath == "#") {
                        createDefaultObbyScene(scene);
                    } else {
                        std::string exeAbsPath = CppCompiler::getProjectRoot() + "/gmplay/" + selectedGamePath;
                        std::string sceneStr = checkEmbeddedSceneInExe(exeAbsPath);
                        if (!sceneStr.empty()) {
                            scene.deserializeFromString(sceneStr);
                        } else {
                            createDefaultObbyScene(scene);
                        }
                    }
                    player.initPlayerCharacter(scene);
                    currentAppState = AppState::Playing;
                }
            }
            
            ImGui::SameLine();
            if (ImGui::Button("Cancelar", ImVec2(100, 45))) {
                currentAppState = AppState::Customizer;
            }
            
            ImGui::End();
        }
        else if (currentAppState == AppState::Playing) {
            // HUD Overlay for MMO Chat and active player list
            ImGui::SetNextWindowPos(ImVec2(10, 10));
            ImGui::SetNextWindowSize(ImVec2(350, 80));
            ImGui::Begin("HUD_Info", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "MULTIJOGADOR ATIVO");
            ImGui::Text("Jogo: %s", selectedGameTitle.c_str());
            ImGui::Text("ID Client: %d | Username: %s", myClientId, myUsername);
            ImGui::End();
            
            // Esc Menu Button
            ImGui::SetNextWindowPos(ImVec2((float)width - 120.0f, 10.0f));
            ImGui::SetNextWindowSize(ImVec2(110.0f, 40.0f));
            ImGui::Begin("EscapeMenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav);
            if (ImGui::Button("Desconectar", ImVec2(100, 30))) {
                disconnectClient();
                if (isHost) stopServer();
                currentAppState = AppState::Browser;
            }
            ImGui::End();
            
            // Global Chat panel
            ImGui::SetNextWindowPos(ImVec2(10, (float)height - 240.0f));
            ImGui::SetNextWindowSize(ImVec2(400, 220));
            ImGui::Begin("MMO Global Chat", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysVerticalScrollbar);
            
            ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.9f, 1.0f), "CHAT GLOBAL DO JOGO");
            ImGui::Separator();
            
            ImGui::BeginChild("ChatHistory", ImVec2(0, 140), false);
            {
                std::lock_guard<std::mutex> lock(netMutex);
                for (const auto& logMsg : chatLog) {
                    if (logMsg.rfind("SYSTEM:", 0) == 0) {
                        ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.1f, 1.0f), "%s", logMsg.c_str());
                    } else {
                        ImGui::TextWrapped("%s", logMsg.c_str());
                    }
                }
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
            
            // Chat Send input
            ImGui::PushItemWidth(300.0f);
            bool reclaimFocus = false;
            if (ImGui::InputText("##chatinput", chatInput, sizeof(chatInput), ImGuiInputTextFlags_EnterReturnsTrue)) {
                std::string chatStr(chatInput);
                if (!chatStr.empty()) {
                    sendClientChatMessage(chatStr);
                    chatInput[0] = '\0';
                }
                reclaimFocus = true;
            }
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Enviar") || reclaimFocus) {
                if (reclaimFocus) {
                    ImGui::SetKeyboardFocusHere(-1); // keep focus
                }
            }
            ImGui::End();
        }
        
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        glfwSwapBuffers(window);
    }
    
    // Cleanup
    disconnectClient();
    if (isHost) stopServer();
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
