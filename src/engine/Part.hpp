#pragma once

#include "Common.hpp"

class Part {
public:
    int id;
    std::string name;
    PartShape shape;
    PartMaterial material;
    
    glm::vec3 position;
    glm::vec3 size;
    glm::vec3 rotation; // Euler angles in degrees
    
    glm::vec4 color; // RGBA
    float transparency;
    float reflectance;
    
    bool anchored;
    bool canCollide;
    bool isPlayer; // True if this part represents a playable character avatar
    std::string script; // Custom scripting behavior
    std::string meshPath; // File path to imported 3D mesh (.obj, .gltf, .glb)
    int scriptLanguage; // 0 = Text Commands, 1 = C++
    
    std::string username;
    int accessoryType; // 0 = None, 1 = Hat (Sphere), 2 = Crown (Cylinder), 3 = Horns (Wedge)
    glm::vec4 accessoryColor;
    
    // Physics variables (only active if not anchored)
    glm::vec3 velocity;

    Part(int partId, const std::string& partName) 
        : id(partId), name(partName), shape(PartShape::Block), material(PartMaterial::Plastic),
          position(0.0f, 2.0f, 0.0f), size(2.0f, 2.0f, 2.0f), rotation(0.0f, 0.0f, 0.0f),
          color(0.7f, 0.7f, 0.7f, 1.0f), transparency(0.0f), reflectance(0.0f),
          anchored(true), canCollide(true), isPlayer(false), script(""), meshPath(""), scriptLanguage(0), velocity(0.0f, 0.0f, 0.0f),
          username(""), accessoryType(0), accessoryColor(1.0f, 1.0f, 0.0f, 1.0f) {}

    nlohmann::json toJson() const {
        nlohmann::json j;
        j["id"] = id;
        j["name"] = name;
        j["shape"] = ShapeToString(shape);
        j["material"] = MaterialToString(material);
        j["position"] = { position.x, position.y, position.z };
        j["size"] = { size.x, size.y, size.z };
        j["rotation"] = { rotation.x, rotation.y, rotation.z };
        j["color"] = { color.r, color.g, color.b, color.a };
        j["transparency"] = transparency;
        j["reflectance"] = reflectance;
        j["anchored"] = anchored;
        j["canCollide"] = canCollide;
        j["script"] = script;
        j["meshPath"] = meshPath;
        j["scriptLanguage"] = scriptLanguage;
        j["username"] = username;
        j["accessoryType"] = accessoryType;
        j["accessoryColor"] = { accessoryColor.r, accessoryColor.g, accessoryColor.b, accessoryColor.a };
        return j;
    }

    void fromJson(const nlohmann::json& j) {
        if (j.contains("id")) id = j["id"];
        if (j.contains("name")) name = j["name"];
        if (j.contains("shape")) shape = StringToShape(j["shape"]);
        if (j.contains("material")) material = StringToMaterial(j["material"]);
        
        if (j.contains("position")) {
            position = glm::vec3(j["position"][0], j["position"][1], j["position"][2]);
        }
        if (j.contains("size")) {
            size = glm::vec3(j["size"][0], j["size"][1], j["size"][2]);
        }
        if (j.contains("rotation")) {
            rotation = glm::vec3(j["rotation"][0], j["rotation"][1], j["rotation"][2]);
        }
        if (j.contains("color")) {
            color = glm::vec4(j["color"][0], j["color"][1], j["color"][2], j["color"][3]);
        }
        if (j.contains("transparency")) transparency = j["transparency"];
        if (j.contains("reflectance")) reflectance = j["reflectance"];
        if (j.contains("anchored")) anchored = j["anchored"];
        if (j.contains("canCollide")) canCollide = j["canCollide"];
        if (j.contains("script")) script = j["script"];
        if (j.contains("meshPath")) meshPath = j["meshPath"];
        if (j.contains("scriptLanguage")) scriptLanguage = j["scriptLanguage"];
        if (j.contains("username")) username = j["username"];
        if (j.contains("accessoryType")) accessoryType = j["accessoryType"];
        if (j.contains("accessoryColor")) {
            accessoryColor = glm::vec4(j["accessoryColor"][0], j["accessoryColor"][1], j["accessoryColor"][2], j["accessoryColor"][3]);
        }
    }

    glm::mat4 getModelMatrix() const {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, position);
        
        // Apply Euler rotations
        model = glm::rotate(model, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        model = glm::rotate(model, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        model = glm::rotate(model, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        
        model = glm::scale(model, size);
        return model;
    }
};
