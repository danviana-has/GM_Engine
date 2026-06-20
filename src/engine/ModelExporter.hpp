#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>

class ModelExporter {
private:
    struct LocalVertex {
        glm::vec3 pos;
        glm::vec3 norm;
    };

    // Helper to encode binary data to Base64 (needed for self-contained GLTF)
    static std::string base64Encode(const unsigned char* data, size_t len) {
        static const char s_base64_table[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string out;
        out.reserve(((len + 2) / 3) * 4);
        for (size_t i = 0; i < len; i += 3) {
            size_t val = (data[i] << 16) | 
                         ((i + 1 < len ? data[i + 1] : 0) << 8) | 
                         (i + 2 < len ? data[i + 2] : 0);
            out.push_back(s_base64_table[(val >> 18) & 63]);
            out.push_back(s_base64_table[(val >> 12) & 63]);
            out.push_back(i + 1 < len ? s_base64_table[(val >> 6) & 63] : '=');
            out.push_back(i + 2 < len ? s_base64_table[val & 63] : '=');
        }
        return out;
    }

    // Generate local mesh for Block
    static void generateBlock(std::vector<LocalVertex>& vertices, std::vector<uint32_t>& indices) {
        vertices = {
            // Front (z = 0.5)
            { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            { { 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            { {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            // Back (z = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f} },
            { {-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f} },
            { { 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f} },
            { { 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f} },
            // Top (y = 0.5)
            { {-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f} },
            { {-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f} },
            { { 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f} },
            { { 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f} },
            // Bottom (y = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f} },
            { { 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f} },
            { { 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f} },
            { {-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f} },
            // Left (x = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f} },
            { {-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f} },
            { {-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f} },
            { {-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f} },
            // Right (x = 0.5)
            { { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f} },
            { { 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f} },
            { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f} },
            { { 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f} }
        };

        for (uint32_t i = 0; i < 6; ++i) {
            uint32_t base = i * 4;
            indices.push_back(base + 0);
            indices.push_back(base + 1);
            indices.push_back(base + 2);
            indices.push_back(base + 0);
            indices.push_back(base + 2);
            indices.push_back(base + 3);
        }
    }

    // Generate local mesh for Wedge
    static void generateWedge(std::vector<LocalVertex>& vertices, std::vector<uint32_t>& indices) {
        glm::vec3 normalSlant = glm::normalize(glm::vec3(0.0f, 1.0f, -1.0f));

        // Wedge is 5 faces: bottom, back, slant, left, right
        // We define them vertex by vertex to specify flat normals
        vertices = {
            // Bottom (y = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f} },
            { { 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f} },
            { { 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f} },
            { {-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f} },
            // Back (z = 0.5)
            { {-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            { { 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            { { 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            { {-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f} },
            // Slant (slanted from top-back to bottom-front)
            { {-0.5f,  0.5f,  0.5f}, normalSlant },
            { { 0.5f,  0.5f,  0.5f}, normalSlant },
            { { 0.5f, -0.5f, -0.5f}, normalSlant },
            { {-0.5f, -0.5f, -0.5f}, normalSlant },
            // Left Triangle (x = -0.5)
            { {-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f} },
            { {-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f} },
            { {-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f} },
            // Right Triangle (x = 0.5)
            { { 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f} },
            { { 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f} },
            { { 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f} }
        };

        // Bottom Cap
        indices.push_back(0); indices.push_back(1); indices.push_back(2);
        indices.push_back(0); indices.push_back(2); indices.push_back(3);

        // Back Cap
        indices.push_back(4); indices.push_back(5); indices.push_back(6);
        indices.push_back(4); indices.push_back(6); indices.push_back(7);

        // Slant Cap
        indices.push_back(8); indices.push_back(9); indices.push_back(10);
        indices.push_back(8); indices.push_back(10); indices.push_back(11);

        // Left Triangle
        indices.push_back(12); indices.push_back(13); indices.push_back(14);

        // Right Triangle
        indices.push_back(15); indices.push_back(16); indices.push_back(17);
    }

    // Generate local mesh for Sphere
    static void generateSphere(std::vector<LocalVertex>& vertices, std::vector<uint32_t>& indices) {
        const unsigned int X_SEGMENTS = 16;
        const unsigned int Y_SEGMENTS = 16;

        for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f) * 0.5f;
                float yPos = std::cos(ySegment * 3.14159265f) * 0.5f;
                float zPos = std::sin(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f) * 0.5f;

                glm::vec3 pos(xPos, yPos, zPos);
                glm::vec3 norm = glm::normalize(pos);
                vertices.push_back({ pos, norm });
            }
        }

        for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x < X_SEGMENTS; ++x) {
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x + 1);

                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x + 1);
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
            }
        }
    }

    // Generate local mesh for Cylinder
    static void generateCylinder(std::vector<LocalVertex>& vertices, std::vector<uint32_t>& indices) {
        const int slices = 16;
        float halfHeight = 0.5f;

        // Sides
        for (int i = 0; i <= slices; ++i) {
            float theta = (i * 2.0f * 3.14159265f) / slices;
            float c = std::cos(theta) * 0.5f;
            float s = std::sin(theta) * 0.5f;
            glm::vec3 normal = glm::normalize(glm::vec3(c, 0.0f, s));

            // Top
            vertices.push_back({ {c, halfHeight, s}, normal });
            // Bottom
            vertices.push_back({ {c, -halfHeight, s}, normal });
        }

        for (int i = 0; i < slices; ++i) {
            uint32_t idx = i * 2;
            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 3);

            indices.push_back(idx);
            indices.push_back(idx + 3);
            indices.push_back(idx + 2);
        }

        // Top Cap
        uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({ {0.0f, halfHeight, 0.0f}, {0.0f, 1.0f, 0.0f} });
        for (int i = 0; i <= slices; ++i) {
            float theta = (i * 2.0f * 3.14159265f) / slices;
            float c = std::cos(theta) * 0.5f;
            float s = std::sin(theta) * 0.5f;
            vertices.push_back({ {c, halfHeight, s}, {0.0f, 1.0f, 0.0f} });
        }
        for (int i = 0; i < slices; ++i) {
            indices.push_back(topCenterIdx);
            indices.push_back(topCenterIdx + i + 1);
            indices.push_back(topCenterIdx + i + 2);
        }

        // Bottom Cap
        uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size());
        vertices.push_back({ {0.0f, -halfHeight, 0.0f}, {0.0f, -1.0f, 0.0f} });
        for (int i = 0; i <= slices; ++i) {
            float theta = (i * 2.0f * 3.14159265f) / slices;
            float c = std::cos(theta) * 0.5f;
            float s = std::sin(theta) * 0.5f;
            vertices.push_back({ {c, -halfHeight, s}, {0.0f, -1.0f, 0.0f} });
        }
        for (int i = 0; i < slices; ++i) {
            indices.push_back(bottomCenterIdx);
            indices.push_back(bottomCenterIdx + i + 2);
            indices.push_back(bottomCenterIdx + i + 1);
        }
    }

    // Dispatch helper
    static void generateShape(PartShape shape, std::vector<LocalVertex>& vertices, std::vector<uint32_t>& indices) {
        switch (shape) {
            case PartShape::Block: generateBlock(vertices, indices); break;
            case PartShape::Wedge: generateWedge(vertices, indices); break;
            case PartShape::Sphere: generateSphere(vertices, indices); break;
            case PartShape::Cylinder: generateCylinder(vertices, indices); break;
        }
    }

    static void generateShapeForPart(const std::shared_ptr<Part>& part, std::vector<LocalVertex>& vertices, std::vector<uint32_t>& indices) {
        if (!part->meshPath.empty()) {
            size_t dot = part->meshPath.find_last_of('.');
            if (dot != std::string::npos) {
                std::string ext = part->meshPath.substr(dot + 1);
                for (auto& c : ext) c = tolower(c);
                if (ext == "obj") {
                    std::ifstream file(part->meshPath);
                    if (file.is_open()) {
                        std::vector<glm::vec3> temp_positions;
                        std::vector<glm::vec3> temp_normals;
                        std::map<std::string, uint32_t> uniqueVerts;
                        std::string line;
                        while (std::getline(file, line)) {
                            if (line.empty()) continue;
                            std::stringstream ss(line);
                            std::string type;
                            ss >> type;
                            if (type == "v") {
                                float x, y, z;
                                ss >> x >> y >> z;
                                temp_positions.push_back(glm::vec3(x, y, z));
                            } else if (type == "vn") {
                                float x, y, z;
                                ss >> x >> y >> z;
                                temp_normals.push_back(glm::vec3(x, y, z));
                            } else if (type == "f") {
                                std::vector<std::string> faceTokens;
                                std::string token;
                                while (ss >> token) {
                                    faceTokens.push_back(token);
                                }
                                for (size_t i = 1; i < faceTokens.size() - 1; ++i) {
                                    std::string corners[3] = { faceTokens[0], faceTokens[i], faceTokens[i+1] };
                                    for (int c = 0; c < 3; ++c) {
                                        std::string key = corners[c];
                                        if (uniqueVerts.find(key) != uniqueVerts.end()) {
                                            indices.push_back(uniqueVerts[key]);
                                        } else {
                                            std::stringstream tokenSS(key);
                                            std::string vStr, vtStr, vnStr;
                                            std::getline(tokenSS, vStr, '/');
                                            std::getline(tokenSS, vtStr, '/');
                                            std::getline(tokenSS, vnStr, '/');
                                            int vIdx = !vStr.empty() ? std::stoi(vStr) : 0;
                                            int vnIdx = !vnStr.empty() ? std::stoi(vnStr) : 0;
                                            if (vIdx < 0) vIdx = (int)temp_positions.size() + vIdx + 1;
                                            if (vnIdx < 0) vnIdx = (int)temp_normals.size() + vnIdx + 1;
                                            glm::vec3 pos(0.0f);
                                            if (vIdx > 0 && vIdx <= (int)temp_positions.size()) pos = temp_positions[vIdx - 1];
                                            glm::vec3 norm(0.0f, 1.0f, 0.0f);
                                            if (vnIdx > 0 && vnIdx <= (int)temp_normals.size()) norm = temp_normals[vnIdx - 1];
                                            uint32_t newIdx = (uint32_t)vertices.size();
                                            vertices.push_back({ pos, norm });
                                            uniqueVerts[key] = newIdx;
                                            indices.push_back(newIdx);
                                        }
                                    }
                                }
                            }
                        }
                        return;
                    }
                }
            }
        }
        generateShape(part->shape, vertices, indices);
    }

public:
    // 1. Export scene to OBJ and MTL
    static bool exportToOBJ(const Scene& scene, const std::string& filepath) {
        std::ofstream obj(filepath);
        if (!obj.is_open()) return false;

        // Strip file extension to get base path for companion MTL
        std::string basepath = filepath;
        size_t lastDot = basepath.find_last_of('.');
        if (lastDot != std::string::npos) {
            basepath = basepath.substr(0, lastDot);
        }
        std::string mtlFilename = filepath.substr(filepath.find_last_of("/\\") + 1);
        size_t mtlLastDot = mtlFilename.find_last_of('.');
        if (mtlLastDot != std::string::npos) {
            mtlFilename = mtlFilename.substr(0, mtlLastDot) + ".mtl";
        }

        std::ofstream mtl(basepath + ".mtl");
        if (!mtl.is_open()) return false;

        obj << "# GM Engine exported scene\n";
        obj << "mtllib " << mtlFilename << "\n\n";

        mtl << "# GM Engine material library\n\n";

        uint32_t globalVertexOffset = 1;

        for (const auto& part : scene.workspace) {
            if (part->isPlayer) continue;

            std::vector<LocalVertex> localVerts;
            std::vector<uint32_t> localInds;
            generateShapeForPart(part, localVerts, localInds);

            obj << "o " << part->name << "_" << part->id << "\n";
            obj << "usemtl mat_" << part->id << "\n";

            glm::mat4 modelMatrix = part->getModelMatrix();
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));

            // Write vertices & normals
            for (const auto& lv : localVerts) {
                glm::vec3 worldPos = glm::vec3(modelMatrix * glm::vec4(lv.pos, 1.0f));
                glm::vec3 worldNorm = glm::normalize(normalMatrix * lv.norm);
                
                obj << "v " << worldPos.x << " " << worldPos.y << " " << worldPos.z << "\n";
                obj << "vn " << worldNorm.x << " " << worldNorm.y << " " << worldNorm.z << "\n";
            }

            // Write faces
            for (size_t i = 0; i < localInds.size(); i += 3) {
                uint32_t v1 = localInds[i] + globalVertexOffset;
                uint32_t v2 = localInds[i+1] + globalVertexOffset;
                uint32_t v3 = localInds[i+2] + globalVertexOffset;
                obj << "f " << v1 << "//" << v1 << " " 
                            << v2 << "//" << v2 << " " 
                            << v3 << "//" << v3 << "\n";
            }
            obj << "\n";

            globalVertexOffset += static_cast<uint32_t>(localVerts.size());

            // Write material info
            mtl << "newmtl mat_" << part->id << "\n";
            mtl << "Ka 0.2 0.2 0.2\n";
            mtl << "Kd " << part->color.r << " " << part->color.g << " " << part->color.b << "\n";
            
            // Specular highlights
            float specMultiplier = 0.5f;
            float shininess = 32.0f;
            if (part->material == PartMaterial::Metal) { specMultiplier = 1.2f; shininess = 64.0f; }
            else if (part->material == PartMaterial::Wood) { specMultiplier = 0.1f; shininess = 8.0f; }
            else if (part->material == PartMaterial::Glass) { specMultiplier = 1.5f; shininess = 128.0f; }
            
            mtl << "Ks " << specMultiplier << " " << specMultiplier << " " << specMultiplier << "\n";
            mtl << "Ns " << shininess << "\n";
            mtl << "d " << (1.0f - part->transparency) << "\n";
            mtl << "illum 2\n\n";
        }

        return true;
    }

    // 2. Export scene to GLTF (embedded base64 format for self-containment)
    static bool exportToGLTF(const Scene& scene, const std::string& filepath) {
        // We will generate the 4 geometries: Block, Sphere, Cylinder, Wedge
        // And pack them into a single binary buffer, which is base64-encoded.
        std::vector<char> binBuffer;

        struct MeshMetadata {
            uint32_t vertexBufferOffset;
            uint32_t vertexBufferLength;
            uint32_t indexBufferOffset;
            uint32_t indexBufferLength;
            uint32_t indexCount;
            uint32_t vertexCount;
        };

        std::map<PartShape, MeshMetadata> shapeMetadata;
        PartShape shapesToPack[] = { PartShape::Block, PartShape::Wedge, PartShape::Sphere, PartShape::Cylinder };

        for (PartShape sh : shapesToPack) {
            std::vector<LocalVertex> verts;
            std::vector<uint32_t> inds;
            generateShape(sh, verts, inds);

            MeshMetadata meta;
            meta.indexCount = static_cast<uint32_t>(inds.size());
            meta.vertexCount = static_cast<uint32_t>(verts.size());

            // Pack vertices (glTF expects position then normal)
            // Struct layout: positions (float3), normals (float3)
            meta.vertexBufferOffset = static_cast<uint32_t>(binBuffer.size());
            size_t vertBytes = verts.size() * sizeof(float) * 6;
            meta.vertexBufferLength = static_cast<uint32_t>(vertBytes);

            size_t oldSize = binBuffer.size();
            binBuffer.resize(oldSize + vertBytes);
            float* destFloats = reinterpret_cast<float*>(&binBuffer[oldSize]);
            for (size_t vIdx = 0; vIdx < verts.size(); ++vIdx) {
                destFloats[vIdx * 6 + 0] = verts[vIdx].pos.x;
                destFloats[vIdx * 6 + 1] = verts[vIdx].pos.y;
                destFloats[vIdx * 6 + 2] = verts[vIdx].pos.z;
                destFloats[vIdx * 6 + 3] = verts[vIdx].norm.x;
                destFloats[vIdx * 6 + 4] = verts[vIdx].norm.y;
                destFloats[vIdx * 6 + 5] = verts[vIdx].norm.z;
            }

            // Align buffer to 4 bytes for indices
            while (binBuffer.size() % 4 != 0) {
                binBuffer.push_back(0);
            }

            // Pack indices (uint32)
            meta.indexBufferOffset = static_cast<uint32_t>(binBuffer.size());
            size_t indBytes = inds.size() * sizeof(uint32_t);
            meta.indexBufferLength = static_cast<uint32_t>(indBytes);

            size_t oldIndSize = binBuffer.size();
            binBuffer.resize(oldIndSize + indBytes);
            uint32_t* destInds = reinterpret_cast<uint32_t*>(&binBuffer[oldIndSize]);
            for (size_t i = 0; i < inds.size(); ++i) {
                destInds[i] = inds[i];
            }

            // Align to 4 bytes
            while (binBuffer.size() % 4 != 0) {
                binBuffer.push_back(0);
            }

            shapeMetadata[sh] = meta;
        }

        std::string base64Buf = base64Encode(reinterpret_cast<const unsigned char*>(binBuffer.data()), binBuffer.size());

        // Construct GLTF JSON
        nlohmann::json j;
        j["asset"] = { {"version", "2.0"}, {"generator", "GM Engine Exporter"} };
        j["scene"] = 0;
        j["scenes"] = nlohmann::json::array({ { {"nodes", nlohmann::json::array() } } });

        // Buffer
        nlohmann::json bufferJson;
        bufferJson["byteLength"] = binBuffer.size();
        bufferJson["uri"] = "data:application/octet-stream;base64," + base64Buf;
        j["buffers"] = nlohmann::json::array({ bufferJson });

        // BufferViews & Accessors
        nlohmann::json bufferViews = nlohmann::json::array();
        nlohmann::json accessors = nlohmann::json::array();

        int viewCounter = 0;
        int accCounter = 0;

        // We will define accessors for each shape's positions, normals, and indices
        std::map<PartShape, int> shapePosAccIdx;
        std::map<PartShape, int> shapeNormAccIdx;
        std::map<PartShape, int> shapeIndAccIdx;

        for (PartShape sh : shapesToPack) {
            const auto& meta = shapeMetadata[sh];

            // 1. Vertex Buffer View (interleaved positions and normals)
            nlohmann::json vView;
            vView["buffer"] = 0;
            vView["byteOffset"] = meta.vertexBufferOffset;
            vView["byteLength"] = meta.vertexBufferLength;
            vView["byteStride"] = 24; // 6 floats * 4 bytes
            vView["target"] = 34962; // ARRAY_BUFFER
            bufferViews.push_back(vView);
            int vViewIdx = viewCounter++;

            // 2. Index Buffer View
            nlohmann::json iView;
            iView["buffer"] = 0;
            iView["byteOffset"] = meta.indexBufferOffset;
            iView["byteLength"] = meta.indexBufferLength;
            iView["target"] = 34963; // ELEMENT_ARRAY_BUFFER
            bufferViews.push_back(iView);
            int iViewIdx = viewCounter++;

            // Accessor: POSITION
            nlohmann::json posAcc;
            posAcc["bufferView"] = vViewIdx;
            posAcc["byteOffset"] = 0;
            posAcc["componentType"] = 5126; // FLOAT
            posAcc["count"] = meta.vertexCount;
            posAcc["type"] = "VEC3";
            
            // Min/Max bound calculations
            glm::vec3 minB(99999.0f), maxB(-99999.0f);
            std::vector<LocalVertex> verts;
            std::vector<uint32_t> inds;
            generateShape(sh, verts, inds);
            for (const auto& v : verts) {
                minB = glm::min(minB, v.pos);
                maxB = glm::max(maxB, v.pos);
            }
            posAcc["min"] = { minB.x, minB.y, minB.z };
            posAcc["max"] = { maxB.x, maxB.y, maxB.z };
            accessors.push_back(posAcc);
            shapePosAccIdx[sh] = accCounter++;

            // Accessor: NORMAL
            nlohmann::json normAcc;
            normAcc["bufferView"] = vViewIdx;
            normAcc["byteOffset"] = 12; // starts after float3 position
            normAcc["componentType"] = 5126; // FLOAT
            normAcc["count"] = meta.vertexCount;
            normAcc["type"] = "VEC3";
            accessors.push_back(normAcc);
            shapeNormAccIdx[sh] = accCounter++;

            // Accessor: INDEX
            nlohmann::json indAcc;
            indAcc["bufferView"] = iViewIdx;
            indAcc["byteOffset"] = 0;
            indAcc["componentType"] = 5125; // UNSIGNED_INT
            indAcc["count"] = meta.indexCount;
            indAcc["type"] = "SCALAR";
            accessors.push_back(indAcc);
            shapeIndAccIdx[sh] = accCounter++;
        }

        j["bufferViews"] = bufferViews;
        j["accessors"] = accessors;

        // Materials setup
        nlohmann::json materials = nlohmann::json::array();
        std::map<int, int> partMaterialIdx;
        int matCounter = 0;

        for (const auto& part : scene.workspace) {
            if (part->isPlayer) continue;

            nlohmann::json mat;
            mat["name"] = part->name + "_material";
            
            nlohmann::json pbr;
            pbr["baseColorFactor"] = { part->color.r, part->color.g, part->color.b, part->color.a * (1.0f - part->transparency) };
            
            // Simple PBR approximation based on Roblox materials
            float metallic = 0.0f;
            float roughness = 0.5f;
            if (part->material == PartMaterial::Metal) {
                metallic = 0.8f;
                roughness = 0.2f;
            } else if (part->material == PartMaterial::Wood) {
                metallic = 0.0f;
                roughness = 0.9f;
            } else if (part->material == PartMaterial::Glass) {
                metallic = 0.1f;
                roughness = 0.1f;
            }
            pbr["metallicFactor"] = metallic;
            pbr["roughnessFactor"] = roughness;
            mat["pbrMetallicRoughness"] = pbr;

            // Glass material transparent setting
            if (part->transparency > 0.01f || part->color.a < 0.99f) {
                mat["alphaMode"] = "BLEND";
            }

            materials.push_back(mat);
            partMaterialIdx[part->id] = matCounter++;
        }
        j["materials"] = materials;

        // Meshes (4 meshes correspond to 4 shapes)
        nlohmann::json meshes = nlohmann::json::array();
        std::map<PartShape, int> shapeMeshIdx;
        int meshCounter = 0;

        for (PartShape sh : shapesToPack) {
            nlohmann::json mesh;
            mesh["name"] = ShapeToString(sh) + "_Mesh";
            
            nlohmann::json primitive;
            primitive["attributes"] = {
                {"POSITION", shapePosAccIdx[sh]},
                {"NORMAL", shapeNormAccIdx[sh]}
            };
            primitive["indices"] = shapeIndAccIdx[sh];
            // Material is referenced per-node or in standard primitive. 
            // In glTF, material is at primitive level. So we need a mesh per material, OR primitives with different materials.
            // A mesh primitive in glTF contains geometry. If two nodes use the same geometry but different materials, 
            // we should either define multiple meshes referencing the same accessors (but with different materials),
            // or define a mesh per node. Defining a mesh per node is very easy and cleaner!
            // Let's do that: We define a mesh *for each part*, referencing the shared accessors!
            // That is incredibly clean and standard. Let's rewrite the mesh array.
            shapeMeshIdx[sh] = meshCounter;
        }

        // Let's define meshes: one per part!
        nlohmann::json partMeshes = nlohmann::json::array();
        int partMeshCounter = 0;
        std::map<int, int> partMeshIdMapping;

        for (const auto& part : scene.workspace) {
            if (part->isPlayer) continue;

            nlohmann::json mesh;
            mesh["name"] = part->name + "_Mesh";
            
            nlohmann::json primitive;
            primitive["attributes"] = {
                {"POSITION", shapePosAccIdx[part->shape]},
                {"NORMAL", shapeNormAccIdx[part->shape]}
            };
            primitive["indices"] = shapeIndAccIdx[part->shape];
            primitive["material"] = partMaterialIdx[part->id];
            
            mesh["primitives"] = nlohmann::json::array({ primitive });
            partMeshes.push_back(mesh);
            
            partMeshIdMapping[part->id] = partMeshCounter++;
        }
        j["meshes"] = partMeshes;

        // Nodes & Scene graph
        nlohmann::json nodes = nlohmann::json::array();
        int nodeCounter = 0;

        for (const auto& part : scene.workspace) {
            if (part->isPlayer) continue;

            nlohmann::json node;
            node["name"] = part->name;
            node["mesh"] = partMeshIdMapping[part->id];

            // Translation
            node["translation"] = { part->position.x, part->position.y, part->position.z };

            // Rotation (Euler to Quaternion)
            glm::quat q = glm::quat(glm::vec3(glm::radians(part->rotation.x), 
                                             glm::radians(part->rotation.y), 
                                             glm::radians(part->rotation.z)));
            node["rotation"] = { q.x, q.y, q.z, q.w };

            // Scale
            node["scale"] = { part->size.x, part->size.y, part->size.z };

            nodes.push_back(node);
            j["scenes"][0]["nodes"].push_back(nodeCounter++);
        }
        j["nodes"] = nodes;

        // Write to file
        std::ofstream file(filepath);
        if (!file.is_open()) return false;
        file << j.dump(4);
        return true;
    }
};
