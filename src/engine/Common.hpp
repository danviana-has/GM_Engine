#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <map>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

// nlohmann/json
#include <nlohmann/json.hpp>

enum class PartShape {
    Block = 0,
    Sphere,
    Cylinder,
    Wedge
};

enum class PartMaterial {
    Plastic = 0,
    Wood,
    Metal,
    Glass,
    Neon
};

inline std::string ShapeToString(PartShape shape) {
    switch (shape) {
        case PartShape::Block: return "Block";
        case PartShape::Sphere: return "Sphere";
        case PartShape::Cylinder: return "Cylinder";
        case PartShape::Wedge: return "Wedge";
    }
    return "Block";
}

inline PartShape StringToShape(const std::string& str) {
    if (str == "Sphere") return PartShape::Sphere;
    if (str == "Cylinder") return PartShape::Cylinder;
    if (str == "Wedge") return PartShape::Wedge;
    return PartShape::Block;
}

inline std::string MaterialToString(PartMaterial mat) {
    switch (mat) {
        case PartMaterial::Plastic: return "Plastic";
        case PartMaterial::Wood: return "Wood";
        case PartMaterial::Metal: return "Metal";
        case PartMaterial::Glass: return "Glass";
        case PartMaterial::Neon: return "Neon";
    }
    return "Plastic";
}

inline PartMaterial StringToMaterial(const std::string& str) {
    if (str == "Wood") return PartMaterial::Wood;
    if (str == "Metal") return PartMaterial::Metal;
    if (str == "Glass") return PartMaterial::Glass;
    if (str == "Neon") return PartMaterial::Neon;
    return PartMaterial::Plastic;
}
