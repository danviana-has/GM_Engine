#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include "Renderer.hpp"
#include "Viewport.hpp"
#include "Explorer.hpp"
#include "Properties.hpp"
#include "../engine/ModelExporter.hpp"
#include "../engine/ScriptEngine.hpp"
#include <imgui.h>
#ifdef _WIN32
#include <windows.h>
#include <commdlg.h>
#include <winhttp.h>
#endif
#include <thread>
#include <mutex>
#include <vector>

#ifdef _WIN32
inline std::string makeHuggingFaceRequest(const std::string& apiToken, const std::string& model, const std::string& jsonPayload, std::string& outError) {
    std::string responseStr = "";
    HINTERNET hSession = NULL, hConnect = NULL, hRequest = NULL;
    
    // Try with default proxy settings first
    hSession = WinHttpOpen(L"GMEngineAI/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        hSession = WinHttpOpen(L"GMEngineAI/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (!hSession) { outError = "WinHttpOpen falhou."; return ""; }
    
    // Increase timeouts since LLM scene generation can take upwards of a minute to complete
    WinHttpSetTimeouts(hSession, 0, 60000, 30000, 180000);
    
    std::wstring host = L"router.huggingface.co";
    std::wstring path = L"/v1/chat/completions";
    
    hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        hSession = WinHttpOpen(L"GMEngineAI/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            WinHttpSetTimeouts(hSession, 0, 60000, 30000, 180000);
            hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
        }
    }
    if (!hConnect) { outError = "WinHttpConnect falhou."; if (hSession) WinHttpCloseHandle(hSession); return ""; }
    
    hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hRequest) { outError = "WinHttpOpenRequest falhou."; WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
    
    std::wstring headers = L"Authorization: Bearer " + std::wstring(apiToken.begin(), apiToken.end()) + L"\r\nContent-Type: application/json\r\n";
    
    BOOL bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1, (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(), (DWORD)jsonPayload.length(), 0);
    
    // Self-healing: if name resolution failed (12007), recreate session and request bypassing proxy
    if (!bResults && GetLastError() == 12007) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        hRequest = NULL; hConnect = NULL; hSession = NULL;
        
        hSession = WinHttpOpen(L"GMEngineAI/1.0", WINHTTP_ACCESS_TYPE_NO_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
        if (hSession) {
            WinHttpSetTimeouts(hSession, 0, 60000, 30000, 180000);
            hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
            if (hConnect) {
                hRequest = WinHttpOpenRequest(hConnect, L"POST", path.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
                if (hRequest) {
                    bResults = WinHttpSendRequest(hRequest, headers.c_str(), (DWORD)-1, (LPVOID)jsonPayload.c_str(), (DWORD)jsonPayload.length(), (DWORD)jsonPayload.length(), 0);
                }
            }
        }
    }
    
    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }
    
    if (bResults) {
        DWORD dwSize = 0;
        do {
            dwSize = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
            if (dwSize == 0) break;
            
            std::vector<char> tempBuf(dwSize + 1);
            DWORD dwDownloaded = 0;
            if (WinHttpReadData(hRequest, &tempBuf[0], dwSize, &dwDownloaded)) {
                tempBuf[dwDownloaded] = '\0';
                responseStr += std::string(&tempBuf[0], dwDownloaded);
            }
        } while (dwSize > 0);
    } else {
        outError = "Requisicao HTTP falhou (Error code: " + std::to_string(GetLastError()) + ")";
    }
    
    if (hRequest) WinHttpCloseHandle(hRequest);
    if (hConnect) WinHttpCloseHandle(hConnect);
    if (hSession) WinHttpCloseHandle(hSession);
    
    return responseStr;
}

inline std::string cleanModelJson(const std::string& rawResponse) {
    std::string clean = rawResponse;
    
    try {
        auto responseJ = nlohmann::json::parse(rawResponse);
        if (responseJ.contains("choices") && responseJ["choices"].is_array() && !responseJ["choices"].empty()) {
            auto firstChoice = responseJ["choices"][0];
            if (firstChoice.contains("message") && firstChoice["message"].contains("content")) {
                clean = firstChoice["message"]["content"].get<std::string>();
            }
        } else if (responseJ.is_array() && !responseJ.empty()) {
            clean = responseJ[0].value("generated_text", "");
        }
    } catch (...) {}
    
    size_t codeStart = clean.find("```json");
    if (codeStart != std::string::npos) {
        clean = clean.substr(codeStart + 7);
        size_t codeEnd = clean.find("```");
        if (codeEnd != std::string::npos) {
            clean = clean.substr(0, codeEnd);
        }
    } else {
        codeStart = clean.find("```");
        if (codeStart != std::string::npos) {
            clean = clean.substr(codeStart + 3);
            size_t codeEnd = clean.find("```");
            if (codeEnd != std::string::npos) {
                clean = clean.substr(0, codeEnd);
            }
        }
    }
    
    clean.erase(0, clean.find_first_not_of(" \t\r\n"));
    clean.erase(clean.find_last_not_of(" \t\r\n") + 1);
    
    return clean;
}
#endif

class Editor {
public:
    Viewport viewport;
    Explorer explorer;
    Properties properties;

    std::string editorStateBackup = "";
    std::vector<std::pair<std::string, glm::vec4>> consoleLogs;
    char consoleFilter[64] = "";

    bool openExportOBJPopup = false;
    bool openExportGLTFPopup = false;
    bool openImportPopup = false;
    bool openPublishPopup = false;
    
    bool showAiCreatorDialog = false;
    char aiPrompt[512] = "Um parkour facil com 5 plataformas azuis e um cubo neon de vitoria";
    char aiModel[128] = "Qwen/Qwen2.5-72B-Instruct";
    char aiToken[128] = "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";
    std::string aiStatus = "";
    bool aiGenerating = false;
    std::string aiResultJson = "";
    std::string aiErrorMsg = "";
    bool aiResultReady = false;

    Editor() {
        // Add a default starting log
        logMessage("GM Engine v1.0 Editor Initialized.", glm::vec4(0.0f, 0.8f, 1.0f, 1.0f));
        logMessage("OpenGL 3.3 Core Renderer Active.", glm::vec4(0.0f, 1.0f, 0.5f, 1.0f));
    }

    void logMessage(const std::string& message, const glm::vec4& color = glm::vec4(0.9f, 0.9f, 0.9f, 1.0f)) {
        consoleLogs.push_back({ message, color });
        if (consoleLogs.size() > 500) {
            consoleLogs.erase(consoleLogs.begin());
        }
    }

    void applyRobloxDarkTheme() {
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* colors = style.Colors;

        // Rounding
        style.WindowRounding = 4.0f;
        style.ChildRounding = 3.0f;
        style.FrameRounding = 3.0f;
        style.PopupRounding = 3.0f;
        style.ScrollbarRounding = 3.0f;
        style.GrabRounding = 3.0f;
        style.TabRounding = 3.0f;

        // Styling Borders
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 0.0f;
        style.PopupBorderSize = 1.0f;

        // Roblox Studio Dark Palette
        ImVec4 baseBg       = ImVec4(0.14f, 0.14f, 0.15f, 1.0f); // #242426 (Panels background)
        ImVec4 darkerBg     = ImVec4(0.11f, 0.11f, 0.12f, 1.0f); // #1c1c1e (Main window background)
        ImVec4 titleBg      = ImVec4(0.18f, 0.18f, 0.19f, 1.0f); // #2e2e30 (Header / tabs)
        ImVec4 accentBlue   = ImVec4(0.00f, 0.48f, 0.80f, 1.0f); // #007acc (Selected highlights)
        ImVec4 hoverBlue    = ImVec4(0.10f, 0.55f, 0.90f, 1.0f); // Hover highlight
        ImVec4 buttonBg     = ImVec4(0.22f, 0.22f, 0.23f, 1.0f); // #38383b
        ImVec4 buttonHover  = ImVec4(0.28f, 0.28f, 0.30f, 1.0f); // #48484c
        ImVec4 borderCol    = ImVec4(0.08f, 0.08f, 0.09f, 1.0f); // #141417

        colors[ImGuiCol_Text]                   = ImVec4(0.88f, 0.88f, 0.89f, 1.0f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.0f);
        colors[ImGuiCol_WindowBg]               = baseBg;
        colors[ImGuiCol_ChildBg]                = baseBg;
        colors[ImGuiCol_PopupBg]                = darkerBg;
        colors[ImGuiCol_Border]                 = borderCol;
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.0f);
        colors[ImGuiCol_FrameBg]                = darkerBg;
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.25f, 0.25f, 0.27f, 1.0f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.30f, 0.30f, 0.32f, 1.0f);
        colors[ImGuiCol_TitleBg]                = titleBg;
        colors[ImGuiCol_TitleBgActive]          = titleBg;
        colors[ImGuiCol_TitleBgCollapsed]       = titleBg;
        colors[ImGuiCol_MenuBarBg]              = titleBg;
        colors[ImGuiCol_ScrollbarBg]            = darkerBg;
        colors[ImGuiCol_ScrollbarGrab]          = buttonBg;
        colors[ImGuiCol_ScrollbarGrabHovered]   = buttonHover;
        colors[ImGuiCol_ScrollbarGrabActive]    = accentBlue;
        colors[ImGuiCol_CheckMark]              = accentBlue;
        colors[ImGuiCol_SliderGrab]             = accentBlue;
        colors[ImGuiCol_SliderGrabActive]       = hoverBlue;
        colors[ImGuiCol_Button]                 = buttonBg;
        colors[ImGuiCol_ButtonHovered]          = buttonHover;
        colors[ImGuiCol_ButtonActive]           = accentBlue;
        colors[ImGuiCol_Header]                 = ImVec4(0.22f, 0.22f, 0.23f, 0.5f);
        colors[ImGuiCol_HeaderHovered]          = buttonHover;
        colors[ImGuiCol_HeaderActive]           = accentBlue;
        colors[ImGuiCol_Separator]              = borderCol;
        colors[ImGuiCol_SeparatorHovered]       = hoverBlue;
        colors[ImGuiCol_SeparatorActive]        = accentBlue;
        colors[ImGuiCol_ResizeGrip]             = buttonBg;
        colors[ImGuiCol_ResizeGripHovered]      = buttonHover;
        colors[ImGuiCol_ResizeGripActive]       = accentBlue;
        colors[ImGuiCol_Tab]                    = titleBg;
        colors[ImGuiCol_TabHovered]             = buttonHover;
        colors[ImGuiCol_TabActive]              = baseBg;
        colors[ImGuiCol_TabUnfocused]           = titleBg;
        colors[ImGuiCol_TabUnfocusedActive]     = baseBg;
        colors[ImGuiCol_DockingPreview]         = ImVec4(0.00f, 0.48f, 0.80f, 0.4f);
        colors[ImGuiCol_DockingEmptyBg]         = darkerBg;
        colors[ImGuiCol_PlotLines]              = accentBlue;
        colors[ImGuiCol_PlotLinesHovered]       = hoverBlue;
        colors[ImGuiCol_PlotHistogram]          = accentBlue;
        colors[ImGuiCol_PlotHistogramHovered]   = hoverBlue;
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.00f, 0.48f, 0.80f, 0.35f);
        colors[ImGuiCol_DragDropTarget]         = accentBlue;
    }

    void draw(Renderer& renderer, Scene& scene, Physics& physics, float deltaTime, bool& outExportRequested, bool& outPublishRequested, std::string& outPublishTitle, std::string& outPublishDesc) {
        applyRobloxDarkTheme();

        // 1. Fullscreen Dockspace
        ImGuiViewport* imguiViewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(imguiViewport->WorkPos);
        ImGui::SetNextWindowSize(imguiViewport->WorkSize);
        ImGui::SetNextWindowViewport(imguiViewport->ID);
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("GMEngineWorkspace", nullptr, window_flags);
        ImGui::PopStyleVar(2);

        // Menu Bar
        if (ImGui::BeginMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene")) {
                    scene.reset();
                    viewport.selectedPartId = -1;
                    logMessage("Created new scene.");
                }
                if (ImGui::MenuItem("Save Scene (scene.gmg)")) {
                    if (scene.saveToFile("scene.gmg")) {
                        logMessage("Scene saved to scene.gmg successfully.", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                    } else {
                        logMessage("Failed to save scene.", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                    }
                }
                if (ImGui::MenuItem("Open Scene (scene.gmg)")) {
                    if (scene.loadFromFile("scene.gmg")) {
                        viewport.selectedPartId = -1;
                        logMessage("Scene loaded from scene.gmg successfully.", glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
                    } else {
                        logMessage("Failed to load scene from scene.gmg.", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Import 3D Model (.obj/.gltf/.glb)...")) {
                    openImportPopup = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Export Scene as OBJ...")) {
                    openExportOBJPopup = true;
                }
                if (ImGui::MenuItem("Export Scene as GLTF...")) {
                    openExportGLTFPopup = true;
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Export standalone EXE")) {
                    outExportRequested = true;
                }
                if (ImGui::MenuItem("Publish to GM Play...")) {
                    openPublishPopup = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Insert")) {
                if (ImGui::MenuItem("Block")) {
                    auto p = scene.addPart(PartShape::Block);
                    viewport.selectedPartId = p->id;
                    logMessage("Inserted Block.");
                }
                if (ImGui::MenuItem("Sphere")) {
                    auto p = scene.addPart(PartShape::Sphere);
                    viewport.selectedPartId = p->id;
                    logMessage("Inserted Sphere.");
                }
                if (ImGui::MenuItem("Cylinder")) {
                    auto p = scene.addPart(PartShape::Cylinder);
                    viewport.selectedPartId = p->id;
                    logMessage("Inserted Cylinder.");
                }
                if (ImGui::MenuItem("Wedge")) {
                    auto p = scene.addPart(PartShape::Wedge);
                    viewport.selectedPartId = p->id;
                    logMessage("Inserted Wedge.");
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Enemy (Template)")) {
                    auto enemy = scene.addPart(PartShape::Sphere);
                    enemy->name = "Enemy";
                    enemy->color = glm::vec4(1.0f, 0.1f, 0.1f, 1.0f);
                    enemy->anchored = false;
                    enemy->canCollide = true;
                    enemy->script = "chase 4.0\ndamage 100\n";
                    viewport.selectedPartId = enemy->id;
                    logMessage("Inserted Enemy Template.");
                }
                if (ImGui::MenuItem("Checkpoint (Template)")) {
                    auto cp = scene.addPart(PartShape::Cylinder);
                    cp->name = "Checkpoint";
                    cp->color = glm::vec4(0.9f, 0.9f, 0.1f, 1.0f);
                    cp->anchored = true;
                    cp->canCollide = false;
                    cp->size = glm::vec3(3.0f, 0.5f, 3.0f);
                    cp->script = "checkpoint\n";
                    viewport.selectedPartId = cp->id;
                    logMessage("Inserted Checkpoint Template.");
                }
                if (ImGui::MenuItem("Victory Point (Template)")) {
                    auto vic = scene.addPart(PartShape::Block);
                    vic->name = "VictoryPoint";
                    vic->color = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f);
                    vic->material = PartMaterial::Neon;
                    vic->anchored = true;
                    vic->canCollide = false;
                    vic->size = glm::vec3(2.0f, 2.0f, 2.0f);
                    vic->script = "victory\n";
                    viewport.selectedPartId = vic->id;
                    logMessage("Inserted Victory Point Template.");
                }
                if (ImGui::MenuItem("Arma (Template)")) {
                    auto gun = scene.addPart(PartShape::Block);
                    gun->name = "Gun";
                    gun->color = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
                    gun->anchored = true;
                    gun->canCollide = false;
                    gun->position = glm::vec3(0.0f, 5.0f, -5.0f);
                    gun->size = glm::vec3(0.5f, 0.5f, 2.0f);
                    
                    auto bullet = scene.addPart(PartShape::Sphere);
                    bullet->name = "Bullet";
                    bullet->color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
                    bullet->anchored = false;
                    bullet->canCollide = true;
                    bullet->position = glm::vec3(0.0f, 5.0f, -5.0f);
                    bullet->size = glm::vec3(0.6f, 0.6f, 0.6f);
                    bullet->scriptLanguage = 1;
                    bullet->script = 
                        "// C++ Bullet Script\n"
                        "extern \"C\" __declspec(dllexport) void updatePart(PartState* state) {\n"
                        "    // Fly forward along the Z axis\n"
                        "    state->position[2] += 30.0f * state->dt;\n"
                        "    \n"
                        "    // Reset position if it collides or goes too far\n"
                        "    if (state->isColliding || state->position[2] > 60.0f) {\n"
                        "        state->position[0] = 0.0f;\n"
                        "        state->position[1] = 5.0f;\n"
                        "        state->position[2] = -5.0f;\n"
                        "        state->velocity[0] = 0.0f;\n"
                        "        state->velocity[1] = 0.0f;\n"
                        "        state->velocity[2] = 0.0f;\n"
                        "    }\n"
                        "}\n";
                        
                    viewport.selectedPartId = gun->id;
                    logMessage("Inserted Arma (Weapon & Bullet) Template.");
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("IA Criadora")) {
                if (ImGui::MenuItem("Gerar Jogo com IA...")) {
                    showAiCreatorDialog = true;
                }
                ImGui::EndMenu();
            }
            ImGui::EndMenuBar();
        }

        // Enable Docking
        ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
        ImGui::End();

        // 2. Toolbar Panel
        ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            // Insert parts shortcuts
            ImGui::Text("Insert:");
            ImGui::SameLine();
            if (ImGui::Button("Block")) {
                auto p = scene.addPart(PartShape::Block);
                viewport.selectedPartId = p->id;
                logMessage("Inserted Block.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Sphere")) {
                auto p = scene.addPart(PartShape::Sphere);
                viewport.selectedPartId = p->id;
                logMessage("Inserted Sphere.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Cylinder")) {
                auto p = scene.addPart(PartShape::Cylinder);
                viewport.selectedPartId = p->id;
                logMessage("Inserted Cylinder.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Wedge")) {
                auto p = scene.addPart(PartShape::Wedge);
                viewport.selectedPartId = p->id;
                logMessage("Inserted Wedge.");
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::Text("Templates:");
            ImGui::SameLine();
            if (ImGui::Button("Enemy")) {
                auto enemy = scene.addPart(PartShape::Sphere);
                enemy->name = "Enemy";
                enemy->color = glm::vec4(1.0f, 0.1f, 0.1f, 1.0f);
                enemy->anchored = false;
                enemy->canCollide = true;
                enemy->script = "chase 4.0\ndamage 100\n";
                viewport.selectedPartId = enemy->id;
                logMessage("Inserted Enemy Template.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Checkpoint")) {
                auto cp = scene.addPart(PartShape::Cylinder);
                cp->name = "Checkpoint";
                cp->color = glm::vec4(0.9f, 0.9f, 0.1f, 1.0f);
                cp->anchored = true;
                cp->canCollide = false;
                cp->size = glm::vec3(3.0f, 0.5f, 3.0f);
                cp->script = "checkpoint\n";
                viewport.selectedPartId = cp->id;
                logMessage("Inserted Checkpoint Template.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Victory")) {
                auto vic = scene.addPart(PartShape::Block);
                vic->name = "VictoryPoint";
                vic->color = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f);
                vic->material = PartMaterial::Neon;
                vic->anchored = true;
                vic->canCollide = false;
                vic->size = glm::vec3(2.0f, 2.0f, 2.0f);
                vic->script = "victory\n";
                viewport.selectedPartId = vic->id;
                logMessage("Inserted Victory Point Template.");
            }
            ImGui::SameLine();
            if (ImGui::Button("Arma")) {
                auto gun = scene.addPart(PartShape::Block);
                gun->name = "Gun";
                gun->color = glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
                gun->anchored = true;
                gun->canCollide = false;
                gun->position = glm::vec3(0.0f, 5.0f, -5.0f);
                gun->size = glm::vec3(0.5f, 0.5f, 2.0f);
                
                auto bullet = scene.addPart(PartShape::Sphere);
                bullet->name = "Bullet";
                bullet->color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
                bullet->anchored = false;
                bullet->canCollide = true;
                bullet->position = glm::vec3(0.0f, 5.0f, -5.0f);
                bullet->size = glm::vec3(0.6f, 0.6f, 0.6f);
                bullet->scriptLanguage = 1;
                bullet->script = 
                    "// C++ Bullet Script\n"
                    "extern \"C\" __declspec(dllexport) void updatePart(PartState* state) {\n"
                    "    // Fly forward along the Z axis\n"
                    "    state->position[2] += 30.0f * state->dt;\n"
                    "    \n"
                    "    // Reset position if it collides or goes too far\n"
                    "    if (state->isColliding || state->position[2] > 60.0f) {\n"
                    "        state->position[0] = 0.0f;\n"
                    "        state->position[1] = 5.0f;\n"
                    "        state->position[2] = -5.0f;\n"
                    "        state->velocity[0] = 0.0f;\n"
                    "        state->velocity[1] = 0.0f;\n"
                    "        state->velocity[2] = 0.0f;\n"
                    "    }\n"
                    "}\n";
                    
                viewport.selectedPartId = gun->id;
                logMessage("Inserted Arma (Weapon & Bullet) Template.");
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            ImGui::Text("Gizmo:");
            ImGui::SameLine();
            
            bool isTranslate = (viewport.currentGizmoOperation == ImGuizmo::TRANSLATE);
            if (isTranslate) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            if (ImGui::Button("Move")) {
                viewport.currentGizmoOperation = ImGuizmo::TRANSLATE;
            }
            if (isTranslate) ImGui::PopStyleColor();
            
            ImGui::SameLine();
            
            bool isRotate = (viewport.currentGizmoOperation == ImGuizmo::ROTATE);
            if (isRotate) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            if (ImGui::Button("Rotate")) {
                viewport.currentGizmoOperation = ImGuizmo::ROTATE;
            }
            if (isRotate) ImGui::PopStyleColor();
            
            ImGui::SameLine();
            
            bool isScale = (viewport.currentGizmoOperation == ImGuizmo::SCALE);
            if (isScale) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
            if (ImGui::Button("Scale")) {
                viewport.currentGizmoOperation = ImGuizmo::SCALE;
            }
            if (isScale) ImGui::PopStyleColor();

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            // Play/Pause simulation
            if (!viewport.isPlaying) {
                if (ImGui::Button("Play (Physics)")) {
                    viewport.isPlaying = true;
                    // Reset script engine
                    ScriptEngine::reset();

                    // Compile and load all C++ scripts before starting simulation
                    physics.compileAndLoadAllCppScripts(scene);

                    // Backup editor state
                    editorStateBackup = scene.serializeToString();
                    
                    // Spawn player avatar
                    auto playerPart = scene.addPart(PartShape::Block);
                    playerPart->name = "Player";
                    playerPart->size = glm::vec3(1.8f, 4.0f, 1.8f);
                    playerPart->position = glm::vec3(0.0f, 6.0f, 0.0f);
                    playerPart->color = glm::vec4(0.0f, 0.4f, 0.8f, 1.0f); // Blue character
                    playerPart->anchored = false;
                    playerPart->canCollide = true;
                    playerPart->isPlayer = true;
                    
                    logMessage("Physics simulation started. Spawned Player Character.", glm::vec4(1.0f, 0.8f, 0.0f, 1.0f));
                }
            } else {
                if (ImGui::Button("Stop (Reset Scene)")) {
                    viewport.isPlaying = false;
                    // Reset script engine
                    ScriptEngine::reset();

                    // Unload all compiled C++ scripts
                    physics.unloadAllCppScripts();

                    // Restore editor state
                    if (!editorStateBackup.empty()) {
                        scene.deserializeFromString(editorStateBackup);
                        logMessage("Physics simulation stopped. Scene reset to original state.", glm::vec4(1.0f, 0.8f, 0.0f, 1.0f));
                    }
                }
            }

            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();

            if (ImGui::Button("Export Game as EXE")) {
                outExportRequested = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Publish to GM Play")) {
                openPublishPopup = true;
            }
        }
        ImGui::End();

        // 3. Explorer Panel
        explorer.draw(scene, viewport.selectedPartId);

        // 4. Properties Panel
        properties.draw(scene, viewport.selectedPartId);

        // 5. Bottom Console Panel
        ImGui::Begin("Output Console");
        {
            ImGui::SetNextItemWidth(200);
            ImGui::InputText("Filter", consoleFilter, sizeof(consoleFilter));
            ImGui::SameLine();
            if (ImGui::Button("Clear")) {
                consoleLogs.clear();
            }
            ImGui::Separator();

            ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& log : consoleLogs) {
                if (consoleFilter[0] != '\0') {
                    if (log.first.find(consoleFilter) == std::string::npos) {
                        continue;
                    }
                }
                
                ImVec4 c = ImVec4(log.second.r, log.second.g, log.second.b, log.second.a);
                ImGui::TextColored(c, "%s", log.first.c_str());
            }

            // Scroll to bottom if logs are appended
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
                ImGui::SetScrollHereY(1.0f);
            }
            ImGui::EndChild();
        }
        ImGui::End();

        // Poll messages from physics/scripts to show in Editor Console
        if (!ScriptEngine::consoleMsg.empty()) {
            glm::vec4 color(0.9f, 0.9f, 0.9f, 1.0f);
            if (ScriptEngine::consoleMsg.find("VICTORY") != std::string::npos) {
                color = glm::vec4(1.0f, 0.8f, 0.0f, 1.0f); // gold for victory
            } else if (ScriptEngine::consoleMsg.find("died") != std::string::npos || ScriptEngine::consoleMsg.find("abyss") != std::string::npos) {
                color = glm::vec4(1.0f, 0.4f, 0.4f, 1.0f); // soft red for hazard/respawn
            } else if (ScriptEngine::consoleMsg.find("Checkpoint") != std::string::npos) {
                color = glm::vec4(0.2f, 1.0f, 0.5f, 1.0f); // green for checkpoints
            } else if (ScriptEngine::consoleMsg.find("Compiler error") != std::string::npos || 
                       ScriptEngine::consoleMsg.find("Failed to load") != std::string::npos ||
                       ScriptEngine::consoleMsg.find("error:") != std::string::npos) {
                color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f); // bright red for errors
            }
            logMessage(ScriptEngine::consoleMsg, color);
            ScriptEngine::consoleMsg = "";
        }

        // Popups for Exporters
        if (openExportOBJPopup) {
            ImGui::OpenPopup("Export to OBJ");
            openExportOBJPopup = false;
        }
        if (ImGui::BeginPopupModal("Export to OBJ", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char filename[256] = "scene.obj";
            ImGui::Text("Enter output filename (companion .mtl will also be created):");
            ImGui::InputText("Filename", filename, sizeof(filename));
            ImGui::Separator();
            if (ImGui::Button("Export", ImVec2(120, 0))) {
                if (ModelExporter::exportToOBJ(scene, filename)) {
                    logMessage("Scene exported to OBJ successfully: " + std::string(filename), glm::vec4(0.0f, 1.0f, 0.5f, 1.0f));
                } else {
                    logMessage("Failed to export to OBJ.", glm::vec4(1.0f, 0.1f, 0.1f, 1.0f));
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (openExportGLTFPopup) {
            ImGui::OpenPopup("Export to GLTF");
            openExportGLTFPopup = false;
        }
        if (ImGui::BeginPopupModal("Export to GLTF", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char filename[256] = "scene.gltf";
            ImGui::Text("Enter output filename (self-contained glTF including meshes & colors):");
            ImGui::InputText("Filename", filename, sizeof(filename));
            ImGui::Separator();
            if (ImGui::Button("Export", ImVec2(120, 0))) {
                if (ModelExporter::exportToGLTF(scene, filename)) {
                    logMessage("Scene exported to GLTF successfully: " + std::string(filename), glm::vec4(0.0f, 1.0f, 0.5f, 1.0f));
                } else {
                    logMessage("Failed to export to GLTF.", glm::vec4(1.0f, 0.1f, 0.1f, 1.0f));
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (openImportPopup) {
            ImGui::OpenPopup("Import 3D Model");
            openImportPopup = false;
        }
        if (ImGui::BeginPopupModal("Import 3D Model", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char pathBuf[512] = "";
            ImGui::Text("Select your 3D model file (.obj, .gltf, .glb):");
            
            ImGui::InputText("File Path", pathBuf, sizeof(pathBuf));
            ImGui::SameLine();
            if (ImGui::Button("Browse...")) {
#ifdef _WIN32
                OPENFILENAMEA ofn;
                char szFile[260] = { 0 };
                ZeroMemory(&ofn, sizeof(ofn));
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = NULL;
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = sizeof(szFile);
                ofn.lpstrFilter = "3D Models (*.obj;*.gltf;*.glb)\0*.obj;*.gltf;*.glb\0All Files (*.*)\0*.*\0";
                ofn.nFilterIndex = 1;
                ofn.lpstrFileTitle = NULL;
                ofn.nMaxFileTitle = 0;
                ofn.lpstrInitialDir = NULL;
                ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                
                if (GetOpenFileNameA(&ofn) == TRUE) {
                    strcpy(pathBuf, ofn.lpstrFile);
                }
#else
                // Web/Android: user types the path manually
                ImGui::OpenPopup("##web_path_hint");
#endif
            }
            ImGui::Separator();
            if (ImGui::Button("Import", ImVec2(120, 0))) {
                std::string path(pathBuf);
                // Load model into cache to verify
                MeshData mesh = MeshCache::loadMesh(path);
                if (mesh.loaded && mesh.vao != 0) {
                    auto part = scene.addPart(PartShape::Block);
                    // Extract file name
                    size_t slash = path.find_last_of("/\\");
                    part->name = (slash != std::string::npos) ? path.substr(slash + 1) : path;
                    part->meshPath = path;
                    viewport.selectedPartId = part->id;
                    logMessage("Successfully imported 3D model: " + part->name, glm::vec4(0.0f, 1.0f, 0.5f, 1.0f));
                } else {
                    logMessage("Failed to load 3D model. Check path and model format.", glm::vec4(1.0f, 0.1f, 0.1f, 1.0f));
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (openPublishPopup) {
            ImGui::OpenPopup("Publish to GM Play");
            openPublishPopup = false;
        }
        if (ImGui::BeginPopupModal("Publish to GM Play", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            static char titleBuf[128] = "My GM Game";
            static char descBuf[512] = "A fun physics game built in GM Engine!";
            ImGui::InputText("Game Title", titleBuf, sizeof(titleBuf));
            ImGui::InputTextMultiline("Description", descBuf, sizeof(descBuf), ImVec2(350, ImGui::GetTextLineHeight() * 4));
            
            ImGui::Separator();
            if (ImGui::Button("Publish", ImVec2(120, 0))) {
                outPublishTitle = std::string(titleBuf);
                outPublishDesc = std::string(descBuf);
                outPublishRequested = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if (showAiCreatorDialog) {
            ImGui::OpenPopup("IA Criadora - Gerar Cenario");
            showAiCreatorDialog = false;
        }
        if (ImGui::BeginPopupModal("IA Criadora - Gerar Cenario", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextColored(ImVec4(0.1f, 0.6f, 0.9f, 1.0f), "Gerar Jogo com Inteligência Artificial");
            ImGui::Separator();
            ImGui::Spacing();
            
            ImGui::InputText("Prompt do Cenário", aiPrompt, sizeof(aiPrompt));
            ImGui::InputText("Token do Hugging Face", aiToken, sizeof(aiToken));
            ImGui::InputText("Modelo do HF", aiModel, sizeof(aiModel));
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
            
            if (aiGenerating) {
                ImGui::TextColored(ImVec4(0.9f, 0.8f, 0.1f, 1.0f), "Gerando cenário, aguarde por favor...");
            } else {
                if (ImGui::Button("Gerar Cenario", ImVec2(150, 35))) {
                    aiGenerating = true;
                    aiResultReady = false;
                    aiErrorMsg = "";
                    aiResultJson = "";
                    
                    std::string promptStr(aiPrompt);
                    std::string modelStr(aiModel);
                    std::string tokenStr(aiToken);
                    
                    std::thread([this, promptStr, modelStr, tokenStr]() {
                        nlohmann::json payload;
                        payload["model"] = modelStr;
                        payload["messages"] = nlohmann::json::array({
                            {
                                {"role", "user"},
                                {"content", "[System: You are an expert level designer for GM Engine. You only output a single raw JSON scene format. Do NOT include markdown code blocks, backticks, or any other explanations. Output ONLY valid JSON containing the 'ambientColor', 'sunDirection', 'skyColor', and 'workspace' array of parts. Example: {\"ambientColor\": [0.4, 0.4, 0.4], \"sunDirection\": [0.5, -1.0, 0.3], \"skyColor\": [0.5, 0.8, 1.0], \"workspace\": [{\"id\": 1, \"name\": \"Baseplate\", \"shape\": \"Block\", \"material\": \"Plastic\", \"position\": [0, -1, 0], \"size\": [100, 2, 100], \"rotation\": [0, 0, 0], \"color\": [0.35, 0.6, 0.3, 1], \"transparency\": 0, \"reflectance\": 0, \"anchored\": true, \"canCollide\": true}]} ]\n\nGenerate a game scene in GM Engine JSON format based on: " + promptStr}
                            }
                        });
                        payload["max_tokens"] = 1500;
                        
                        std::string err = "";
                        std::string rawResponse = makeHuggingFaceRequest(tokenStr, modelStr, payload.dump(), err);
                        
                        if (!err.empty()) {
                            aiErrorMsg = err;
                        } else {
                            std::string cleaned = cleanModelJson(rawResponse);
                            try {
                                auto verifyJ = nlohmann::json::parse(cleaned);
                                if (verifyJ.contains("workspace")) {
                                    aiResultJson = cleaned;
                                } else {
                                    aiErrorMsg = "Modelo nao retornou um workspace valido de GM Engine.\nResposta:\n" + cleaned.substr(0, 300);
                                }
                            } catch (const std::exception& e) {
                                aiErrorMsg = "Erro ao parsear JSON. Modelo retornou:\n" + cleaned.substr(0, 300);
                            }
                        }
                        aiResultReady = true;
                    }).detach();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancelar", ImVec2(100, 35))) {
                    ImGui::CloseCurrentPopup();
                }
            }
            
            if (!aiErrorMsg.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1, 0.1f, 0.1f, 1), "Erro: %s", aiErrorMsg.c_str());
            }
            
            ImGui::EndPopup();
        }

        // Handle async result loading safely on main thread
        if (aiResultReady) {
            aiGenerating = false;
            aiResultReady = false;
            if (aiErrorMsg.empty() && !aiResultJson.empty()) {
                scene.deserializeFromString(aiResultJson);
                logMessage("IA: Cenario gerado e carregado com sucesso!", glm::vec4(0.0f, 1.0f, 0.5f, 1.0f));
            } else {
                logMessage("IA Erro: " + aiErrorMsg, glm::vec4(1.0f, 0.1f, 0.1f, 1.0f));
            }
        }

        // 6. Viewport Panel
        viewport.isPlaying = viewport.isPlaying; // sync state
        viewport.draw(renderer, scene, physics, deltaTime);
    }
};
