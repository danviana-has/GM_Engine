#pragma once

#include "Part.hpp"
#include "GuiElement.hpp"

class Scene {
private:
    int nextId = 1;

public:
    std::vector<std::shared_ptr<Part>> workspace;
    std::vector<std::shared_ptr<GuiElement>> starterGui;
    int nextGuiId = 1;
    
    // Lighting & Environment settings
    glm::vec3 ambientColor;
    glm::vec3 sunDirection;
    glm::vec3 skyColor;

    Scene() {
        // Defaults matching Roblox Studio's outdoor look
        ambientColor = glm::vec3(0.4f, 0.4f, 0.4f);
        sunDirection = glm::normalize(glm::vec3(0.5f, -1.0f, 0.3f));
        skyColor = glm::vec3(0.53f, 0.81f, 0.92f); // Sky blue
        
        reset();
    }

    void reset() {
        workspace.clear();
        starterGui.clear();
        nextId = 1;
        nextGuiId = 1;
        
        // Add a default Baseplate like Roblox Studio
        auto baseplate = addPart(PartShape::Block);
        baseplate->name = "Baseplate";
        baseplate->position = glm::vec3(0.0f, -1.0f, 0.0f);
        baseplate->size = glm::vec3(100.0f, 2.0f, 100.0f);
        baseplate->color = glm::vec4(0.35f, 0.6f, 0.3f, 1.0f); // Grass green
        baseplate->anchored = true;
        baseplate->canCollide = true;
    }

    std::shared_ptr<Part> addPart(PartShape shape = PartShape::Block) {
        std::string defaultName = "Part";
        switch (shape) {
            case PartShape::Block: defaultName = "Part"; break;
            case PartShape::Sphere: defaultName = "Sphere"; break;
            case PartShape::Cylinder: defaultName = "Cylinder"; break;
            case PartShape::Wedge: defaultName = "Wedge"; break;
        }
        
        auto part = std::make_shared<Part>(nextId++, defaultName);
        part->shape = shape;
        
        // Offset slightly if there are existing parts to avoid exact overlap
        if (!workspace.empty()) {
            part->position = glm::vec3(0.0f, 5.0f, 0.0f);
        }
        
        workspace.push_back(part);
        return part;
    }

    void removePart(int id) {
        workspace.erase(
            std::remove_if(workspace.begin(), workspace.end(), 
                [id](const std::shared_ptr<Part>& p) { return p->id == id; }),
            workspace.end()
        );
    }

    std::shared_ptr<Part> findPart(int id) {
        for (const auto& part : workspace) {
            if (part->id == id) return part;
        }
        return nullptr;
    }

    std::shared_ptr<GuiElement> addGuiElement(GuiElementType type, const std::string& name = "") {
        std::string defaultName = "GuiElement";
        switch (type) {
            case GuiElementType::ScreenGui: defaultName = "ScreenGui"; break;
            case GuiElementType::Frame: defaultName = "Frame"; break;
            case GuiElementType::TextLabel: defaultName = "TextLabel"; break;
            case GuiElementType::TextButton: defaultName = "TextButton"; break;
        }
        std::string finalName = name.empty() ? defaultName : name;
        auto elem = std::make_shared<GuiElement>(nextGuiId++, finalName, type);
        starterGui.push_back(elem);
        return elem;
    }

    void removeGuiElement(int id) {
        starterGui.erase(
            std::remove_if(starterGui.begin(), starterGui.end(), 
                [id](const std::shared_ptr<GuiElement>& g) { return g->id == id; }),
            starterGui.end()
        );
        // Also remove children parented to this element
        starterGui.erase(
            std::remove_if(starterGui.begin(), starterGui.end(), 
                [id](const std::shared_ptr<GuiElement>& g) { return g->parentId == id; }),
            starterGui.end()
        );
    }

    std::shared_ptr<GuiElement> findGuiElement(int id) {
        for (const auto& elem : starterGui) {
            if (elem->id == id) return elem;
        }
        return nullptr;
    }

    std::string serializeToString() const {
        nlohmann::json j;
        
        // Environment settings
        j["ambientColor"] = { ambientColor.r, ambientColor.g, ambientColor.b };
        j["sunDirection"] = { sunDirection.x, sunDirection.y, sunDirection.z };
        j["skyColor"] = { skyColor.r, skyColor.g, skyColor.b };
        
        // Parts array
        nlohmann::json partsJson = nlohmann::json::array();
        for (const auto& part : workspace) {
            if (part->isPlayer) continue; // Do not serialize player avatar
            partsJson.push_back(part->toJson());
        }
        j["workspace"] = partsJson;
        
        // GUI elements array
        nlohmann::json guiJson = nlohmann::json::array();
        for (const auto& elem : starterGui) {
            guiJson.push_back(elem->toJson());
        }
        j["starterGui"] = guiJson;
        
        return j.dump(4);
    }

    bool deserializeFromString(const std::string& data) {
        try {
            nlohmann::json j = nlohmann::json::parse(data);
            
            workspace.clear();
            starterGui.clear();
            
            if (j.contains("ambientColor")) {
                ambientColor = glm::vec3(j["ambientColor"][0], j["ambientColor"][1], j["ambientColor"][2]);
            }
            if (j.contains("sunDirection")) {
                sunDirection = glm::vec3(j["sunDirection"][0], j["sunDirection"][1], j["sunDirection"][2]);
            }
            if (j.contains("skyColor")) {
                skyColor = glm::vec3(j["skyColor"][0], j["skyColor"][1], j["skyColor"][2]);
            }
            
            int maxId = 0;
            if (j.contains("workspace")) {
                for (const auto& partJson : j["workspace"]) {
                    auto part = std::make_shared<Part>(0, "");
                    part->fromJson(partJson);
                    workspace.push_back(part);
                    if (part->id > maxId) {
                        maxId = part->id;
                    }
                }
            }
            nextId = maxId + 1;
            
            int maxGuiId = 0;
            if (j.contains("starterGui")) {
                for (const auto& guiJson : j["starterGui"]) {
                    auto elem = std::make_shared<GuiElement>(0, "", GuiElementType::ScreenGui);
                    elem->fromJson(guiJson);
                    starterGui.push_back(elem);
                    if (elem->id > maxGuiId) {
                        maxGuiId = elem->id;
                    }
                }
            }
            nextGuiId = maxGuiId + 1;
            
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Deserialization error: " << e.what() << std::endl;
            return false;
        }
    }

    bool saveToFile(const std::string& filepath) const {
        std::ofstream file(filepath);
        if (!file.is_open()) return false;
        file << serializeToString();
        return true;
    }

    bool loadFromFile(const std::string& filepath) {
        std::ifstream file(filepath);
        if (!file.is_open()) return false;
        std::stringstream ss;
        ss << file.rdbuf();
        return deserializeFromString(ss.str());
    }
};
