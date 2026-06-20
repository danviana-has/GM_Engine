#pragma once

#include "Common.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

struct MeshData {
    unsigned int vao = 0;
    unsigned int vbo = 0;
    unsigned int ebo = 0;
    unsigned int indexCount = 0;
    bool loaded = false;
};

class MeshCache {
private:
    // Helper to decode Base64
    static inline std::vector<char> base64Decode(const std::string& in) {
        std::vector<char> out;
        static const int T[] = {
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
            -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,
            -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
            -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1
        };
        int val = 0, valb = -8;
        for (char c : in) {
            if (c == '=') break;
            if (c < 0 || T[(unsigned char)c] == -1) continue;
            val = (val << 6) + T[(unsigned char)c];
            valb += 6;
            if (valb >= 0) {
                out.push_back(char((val >> valb) & 0xFF));
                valb -= 8;
            }
        }
        return out;
    }

    // Helper to read accessor data
    template<typename T>
    static inline std::vector<T> readAccessor(const nlohmann::json& j, int accessorIdx, const std::vector<char>& bufferData) {
        nlohmann::json acc = j["accessors"][accessorIdx];
        int viewIdx = acc["bufferView"];
        nlohmann::json view = j["bufferViews"][viewIdx];
        
        int accOffset = acc.value("byteOffset", 0);
        int viewOffset = view.value("byteOffset", 0);
        int totalOffset = viewOffset + accOffset;
        
        int count = acc["count"];
        int componentType = acc["componentType"];
        std::string type = acc["type"];
        
        int numComponents = 1;
        if (type == "VEC2") numComponents = 2;
        else if (type == "VEC3") numComponents = 3;
        else if (type == "VEC4") numComponents = 4;
        
        int stride = view.value("byteStride", 0);
        if (stride == 0) {
            int compSize = 4; // float / uint32
            if (componentType == 5123) compSize = 2; // ushort
            else if (componentType == 5121) compSize = 1; // ubyte
            stride = numComponents * compSize;
        }
        
        std::vector<T> result;
        result.resize(count * numComponents);
        
        for (int i = 0; i < count; ++i) {
            int elementOffset = totalOffset + i * stride;
            for (int c = 0; c < numComponents; ++c) {
                int offset = elementOffset;
                if (componentType == 5126) { // FLOAT
                    offset += c * sizeof(float);
                    if (offset + sizeof(float) <= bufferData.size()) {
                        float val = *reinterpret_cast<const float*>(&bufferData[offset]);
                        result[i * numComponents + c] = static_cast<T>(val);
                    }
                } else if (componentType == 5125) { // UNSIGNED_INT
                    offset += c * sizeof(uint32_t);
                    if (offset + sizeof(uint32_t) <= bufferData.size()) {
                        uint32_t val = *reinterpret_cast<const uint32_t*>(&bufferData[offset]);
                        result[i * numComponents + c] = static_cast<T>(val);
                    }
                } else if (componentType == 5123) { // UNSIGNED_SHORT
                    offset += c * sizeof(uint16_t);
                    if (offset + sizeof(uint16_t) <= bufferData.size()) {
                        uint16_t val = *reinterpret_cast<const uint16_t*>(&bufferData[offset]);
                        result[i * numComponents + c] = static_cast<T>(val);
                    }
                } else if (componentType == 5121) { // UNSIGNED_BYTE
                    offset += c * sizeof(uint8_t);
                    if (offset + sizeof(uint8_t) <= bufferData.size()) {
                        uint8_t val = *reinterpret_cast<const uint8_t*>(&bufferData[offset]);
                        result[i * numComponents + c] = static_cast<T>(val);
                    }
                }
            }
        }
        return result;
    }

    static inline MeshData loadOBJ(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            std::cerr << "Failed to open OBJ file: " << path << std::endl;
            return {};
        }

        std::vector<glm::vec3> temp_positions;
        std::vector<glm::vec3> temp_normals;
        std::vector<float> vertexBuffer; 
        std::vector<unsigned int> indices;
        std::map<std::string, unsigned int> uniqueVertices;

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
                
                // Polygon Fan Triangulation
                for (size_t i = 1; i < faceTokens.size() - 1; ++i) {
                    std::string corners[3] = { faceTokens[0], faceTokens[i], faceTokens[i+1] };
                    for (int c = 0; c < 3; ++c) {
                        std::string key = corners[c];
                        if (uniqueVertices.find(key) != uniqueVertices.end()) {
                            indices.push_back(uniqueVertices[key]);
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
                            if (vIdx > 0 && vIdx <= (int)temp_positions.size()) {
                                pos = temp_positions[vIdx - 1];
                            }
                            glm::vec3 norm(0.0f, 1.0f, 0.0f);
                            if (vnIdx > 0 && vnIdx <= (int)temp_normals.size()) {
                                norm = temp_normals[vnIdx - 1];
                            }

                            unsigned int newIdx = (unsigned int)(vertexBuffer.size() / 6);
                            vertexBuffer.push_back(pos.x);
                            vertexBuffer.push_back(pos.y);
                            vertexBuffer.push_back(pos.z);
                            vertexBuffer.push_back(norm.x);
                            vertexBuffer.push_back(norm.y);
                            vertexBuffer.push_back(norm.z);

                            uniqueVertices[key] = newIdx;
                            indices.push_back(newIdx);
                        }
                    }
                }
            }
        }

        if (vertexBuffer.empty()) return {};

        MeshData mesh;
        mesh.indexCount = (unsigned int)indices.size();

        glGenVertexArrays(1, &mesh.vao);
        glGenBuffers(1, &mesh.vbo);
        glGenBuffers(1, &mesh.ebo);

        glBindVertexArray(mesh.vao);

        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(float), &vertexBuffer[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        mesh.loaded = true;
        return mesh;
    }

    static inline MeshData loadGLTFOrGLB(const std::string& path, bool isBinary) {
        nlohmann::json j;
        std::vector<char> bufferData;

        if (isBinary) {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) return {};

            uint32_t magic, version, length;
            file.read(reinterpret_cast<char*>(&magic), 4);
            file.read(reinterpret_cast<char*>(&version), 4);
            file.read(reinterpret_cast<char*>(&length), 4);

            if (magic != 0x46546C67) return {}; // "glTF"

            uint32_t chunkLength, chunkType;
            file.read(reinterpret_cast<char*>(&chunkLength), 4);
            file.read(reinterpret_cast<char*>(&chunkType), 4);
            if (chunkType != 0x4E4F534A) return {}; // "JSON"

            std::string jsonStr(chunkLength, '\0');
            file.read(&jsonStr[0], chunkLength);
            j = nlohmann::json::parse(jsonStr);

            if (file.peek() != EOF) {
                file.read(reinterpret_cast<char*>(&chunkLength), 4);
                file.read(reinterpret_cast<char*>(&chunkType), 4);
                if (chunkType == 0x004E4942) { // "BIN"
                    bufferData.resize(chunkLength);
                    file.read(bufferData.data(), chunkLength);
                }
            }
        } else {
            std::ifstream file(path);
            if (!file.is_open()) return {};
            file >> j;

            if (j.contains("buffers") && !j["buffers"].empty()) {
                nlohmann::json buf = j["buffers"][0];
                std::string uri = buf.value("uri", "");
                if (uri.rfind("data:", 0) == 0) {
                    size_t comma = uri.find(',');
                    if (comma != std::string::npos) {
                        std::string b64 = uri.substr(comma + 1);
                        bufferData = base64Decode(b64);
                    }
                } else if (!uri.empty()) {
                    std::string dir = path.substr(0, path.find_last_of("/\\") + 1);
                    std::ifstream binFile(dir + uri, std::ios::binary);
                    if (binFile.is_open()) {
                        binFile.seekg(0, std::ios::end);
                        size_t size = binFile.tellg();
                        binFile.seekg(0, std::ios::beg);
                        bufferData.resize(size);
                        binFile.read(bufferData.data(), size);
                    }
                }
            }
        }

        if (bufferData.empty() || !j.contains("meshes") || j["meshes"].empty()) {
            return {};
        }

        nlohmann::json mesh = j["meshes"][0];
        nlohmann::json primitive = mesh["primitives"][0];
        nlohmann::json attributes = primitive["attributes"];

        if (!attributes.contains("POSITION")) return {};

        int posIdx = attributes["POSITION"];
        int normIdx = attributes.value("NORMAL", -1);
        int indIdx = primitive.value("indices", -1);

        std::vector<float> posData = readAccessor<float>(j, posIdx, bufferData);
        std::vector<float> normData;
        if (normIdx != -1) {
            normData = readAccessor<float>(j, normIdx, bufferData);
        }
        std::vector<unsigned int> indData;
        if (indIdx != -1) {
            indData = readAccessor<unsigned int>(j, indIdx, bufferData);
        }

        std::vector<float> vertexBuffer;
        vertexBuffer.reserve(posData.size() + (normData.empty() ? posData.size() : normData.size()));

        size_t vertCount = posData.size() / 3;
        for (size_t i = 0; i < vertCount; ++i) {
            vertexBuffer.push_back(posData[i * 3 + 0]);
            vertexBuffer.push_back(posData[i * 3 + 1]);
            vertexBuffer.push_back(posData[i * 3 + 2]);

            if (normData.size() >= (i + 1) * 3) {
                vertexBuffer.push_back(normData[i * 3 + 0]);
                vertexBuffer.push_back(normData[i * 3 + 1]);
                vertexBuffer.push_back(normData[i * 3 + 2]);
            } else {
                vertexBuffer.push_back(0.0f);
                vertexBuffer.push_back(1.0f);
                vertexBuffer.push_back(0.0f);
            }
        }

        MeshData meshOut;
        meshOut.indexCount = static_cast<unsigned int>(indData.size());

        glGenVertexArrays(1, &meshOut.vao);
        glGenBuffers(1, &meshOut.vbo);
        glGenBuffers(1, &meshOut.ebo);

        glBindVertexArray(meshOut.vao);

        glBindBuffer(GL_ARRAY_BUFFER, meshOut.vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexBuffer.size() * sizeof(float), &vertexBuffer[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, meshOut.ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indData.size() * sizeof(unsigned int), &indData[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
        meshOut.loaded = true;
        return meshOut;
    }

public:
    static inline std::map<std::string, MeshData> cache;

    static inline MeshData getMesh(const std::string& path) {
        if (cache.find(path) != cache.end()) {
            return cache[path];
        }
        MeshData data = loadMesh(path);
        cache[path] = data;
        return data;
    }

    static inline MeshData loadMesh(const std::string& path) {
        size_t dot = path.find_last_of('.');
        if (dot == std::string::npos) return {};
        std::string ext = path.substr(dot + 1);
        for (auto& c : ext) c = tolower(c);
        
        if (ext == "obj") {
            return loadOBJ(path);
        } else if (ext == "gltf") {
            return loadGLTFOrGLB(path, false);
        } else if (ext == "glb") {
            return loadGLTFOrGLB(path, true);
        }
        return {};
    }

    static inline void clear() {
        for (auto& pair : cache) {
            if (pair.second.vao) glDeleteVertexArrays(1, &pair.second.vao);
            if (pair.second.vbo) glDeleteBuffers(1, &pair.second.vbo);
            if (pair.second.ebo) glDeleteBuffers(1, &pair.second.ebo);
        }
        cache.clear();
    }
};
