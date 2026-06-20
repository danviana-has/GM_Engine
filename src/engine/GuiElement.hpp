#pragma once

#include "Common.hpp"
#include <imgui.h>
#include <sstream>
#include "ScriptState.hpp"

enum class GuiElementType {
    ScreenGui = 0,
    Frame,
    TextLabel,
    TextButton
};

class GuiElement {
public:
    int id;
    std::string name;
    GuiElementType type;
    int parentId; // -1 if child of ScreenGui directly or if it is ScreenGui itself
    
    // Position & Size (Scale, Offset)
    float posXScale = 0.0f;
    float posXOffset = 10.0f;
    float posYScale = 0.0f;
    float posYOffset = 10.0f;
    
    float sizeXScale = 0.0f;
    float sizeXOffset = 100.0f;
    float sizeYScale = 0.0f;
    float sizeYOffset = 100.0f;
    
    glm::vec4 backgroundColor = glm::vec4(0.3f, 0.3f, 0.3f, 0.8f);
    glm::vec4 textColor = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
    std::string text = "Text";
    bool visible = true;
    
    // Script command to execute when clicked (only for TextButton)
    std::string script = ""; 
    
    GuiElement(int guiId, const std::string& guiName, GuiElementType guiType)
        : id(guiId), name(guiName), type(guiType), parentId(-1) {}
        
    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["type"] = static_cast<int>(type);
        j["parentId"] = parentId;
        j["pos"] = { posXScale, posXOffset, posYScale, posYOffset };
        j["size"] = { sizeXScale, sizeXOffset, sizeYScale, sizeYOffset };
        j["backgroundColor"] = { backgroundColor.r, backgroundColor.g, backgroundColor.b, backgroundColor.a };
        j["textColor"] = { textColor.r, textColor.g, textColor.b, textColor.a };
        j["text"] = text;
        j["visible"] = visible;
        j["script"] = script;
        return j;
    }
    
    void fromJson(const nlohmann::json& j) {
        if (j.contains("id")) id = j["id"];
        if (j.contains("name")) name = j["name"];
        if (j.contains("type")) type = static_cast<GuiElementType>(j["type"]);
        if (j.contains("parentId")) parentId = j["parentId"];
        if (j.contains("pos")) {
            posXScale = j["pos"][0]; posXOffset = j["pos"][1];
            posYScale = j["pos"][2]; posYOffset = j["pos"][3];
        }
        if (j.contains("size")) {
            sizeXScale = j["size"][0]; sizeXOffset = j["size"][1];
            sizeYScale = j["size"][2]; sizeYOffset = j["size"][3];
        }
        if (j.contains("backgroundColor")) {
            backgroundColor = glm::vec4(j["backgroundColor"][0], j["backgroundColor"][1], j["backgroundColor"][2], j["backgroundColor"][3]);
        }
        if (j.contains("textColor")) {
            textColor = glm::vec4(j["textColor"][0], j["textColor"][1], j["textColor"][2], j["textColor"][3]);
        }
        if (j.contains("text")) text = j["text"];
        if (j.contains("visible")) visible = j["visible"];
        if (j.contains("script")) script = j["script"];
    }

    static inline std::string TypeToString(GuiElementType t) {
        switch (t) {
            case GuiElementType::ScreenGui: return "ScreenGui";
            case GuiElementType::Frame: return "Frame";
            case GuiElementType::TextLabel: return "TextLabel";
            case GuiElementType::TextButton: return "TextButton";
        }
        return "ScreenGui";
    }

    static void drawElement(const std::vector<std::shared_ptr<GuiElement>>& starterGui, std::shared_ptr<GuiElement> elem, float parentX, float parentY, float parentW, float parentH, bool isPlaying) {
        if (!elem->visible) return;

        float absW = elem->sizeXScale * parentW + elem->sizeXOffset;
        float absH = elem->sizeYScale * parentH + elem->sizeYOffset;
        float absX = parentX + elem->posXScale * parentW + elem->posXOffset;
        float absY = parentY + elem->posYScale * parentH + elem->posYOffset;

        // Draw background for Frame and TextLabel
        if (elem->type == GuiElementType::Frame || elem->type == GuiElementType::TextLabel) {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImVec2 pMin(absX, absY);
            ImVec2 pMax(absX + absW, absY + absH);
            ImU32 bgCol = ImGui::ColorConvertFloat4ToU32(ImVec4(elem->backgroundColor.r, elem->backgroundColor.g, elem->backgroundColor.b, elem->backgroundColor.a));
            drawList->AddRectFilled(pMin, pMax, bgCol);

            // Draw text for label
            if (elem->type == GuiElementType::TextLabel) {
                ImVec2 textSize = ImGui::CalcTextSize(elem->text.c_str());
                ImVec2 textPos(absX + (absW - textSize.x) * 0.5f, absY + (absH - textSize.y) * 0.5f);
                ImU32 txtCol = ImGui::ColorConvertFloat4ToU32(ImVec4(elem->textColor.r, elem->textColor.g, elem->textColor.b, elem->textColor.a));
                drawList->AddText(textPos, txtCol, elem->text.c_str());
            }
        }
        else if (elem->type == GuiElementType::TextButton) {
            // Draw interactive button as its own window so it only absorbs inputs locally
            ImGui::SetNextWindowPos(ImVec2(absX, absY));
            ImGui::SetNextWindowSize(ImVec2(absW, absH));
            std::string winName = "##gui_btn_" + std::to_string(elem->id);
            
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            ImGui::Begin(winName.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings);
            
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(elem->backgroundColor.r, elem->backgroundColor.g, elem->backgroundColor.b, elem->backgroundColor.a));
            // Slight color variation on hover/active
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(std::min(elem->backgroundColor.r * 1.15f, 1.0f), std::min(elem->backgroundColor.g * 1.15f, 1.0f), std::min(elem->backgroundColor.b * 1.15f, 1.0f), elem->backgroundColor.a));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(elem->backgroundColor.r * 0.85f, elem->backgroundColor.g * 0.85f, elem->backgroundColor.b * 0.85f, elem->backgroundColor.a));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(elem->textColor.r, elem->textColor.g, elem->textColor.b, elem->textColor.a));
            
            if (ImGui::Button(elem->text.c_str(), ImVec2(absW, absH))) {
                if (isPlaying && !elem->script.empty()) {
                    std::stringstream ss(elem->script);
                    std::string cmd;
                    ss >> cmd;
                    if (cmd == "print") {
                        std::string msg;
                        std::getline(ss, msg);
                        if (!msg.empty() && msg[0] == ' ') msg.erase(0, 1);
                        ScriptState::consoleMsg = "[GUI] " + msg;
                    } else if (cmd == "teleport") {
                        float tx = 0.0f, ty = 0.0f, tz = 0.0f;
                        if (ss >> tx >> ty >> tz) {
                            ScriptState::consoleMsg = "[GUI] Teleporting player to: (" + std::to_string((int)tx) + ", " + std::to_string((int)ty) + ", " + std::to_string((int)tz) + ")";
                            ScriptState::playerNeedsRespawn = true;
                            ScriptState::currentCheckpoint = glm::vec3(tx, ty, tz);
                        }
                    }
                }
            }
            
            ImGui::PopStyleColor(4);
            ImGui::End();
            ImGui::PopStyleVar(2);
        }

        // Draw children
        for (const auto& child : starterGui) {
            if (child->parentId == elem->id) {
                drawElement(starterGui, child, absX, absY, absW, absH, isPlaying);
            }
        }
    }
};
