#pragma once

#include "Common.hpp"
#include <windows.h>
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

struct PartState {
    float position[3];
    float rotation[3];
    float size[3];
    float color[4];
    float velocity[3];
    bool anchored;
    bool canCollide;
    bool isGrounded;
    float dt;
};

class CppCompiler {
public:
    static std::string getProjectRoot() {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("/\\");
        std::string exeDir = (lastSlash != std::string::npos) ? exePath.substr(0, lastSlash) : ".";
        
        std::string rawRoot = exeDir;
        // Check if thirdparty folder exists in exeDir
        DWORD attribs = GetFileAttributesA((exeDir + "\\thirdparty").c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
            // Otherwise assume project root is parent directory (build folder case)
            rawRoot = exeDir + "\\..";
        }
        
        // Resolve absolute canonical path (removes double backslashes and relative '..')
        char canonicalPath[MAX_PATH];
        if (GetFullPathNameA(rawRoot.c_str(), MAX_PATH, canonicalPath, NULL) != 0) {
            return std::string(canonicalPath);
        }
        return rawRoot;
    }

public:
    static inline std::map<int, HMODULE> dllHandles;
    static inline std::map<int, void(*)(PartState*)> dllFunctions;

    static void unloadDLL(int partId) {
        if (dllHandles.find(partId) != dllHandles.end()) {
            FreeLibrary(dllHandles[partId]);
            dllHandles.erase(partId);
        }
        if (dllFunctions.find(partId) != dllFunctions.end()) {
            dllFunctions.erase(partId);
        }
    }

    static void unloadAll() {
        for (auto& pair : dllHandles) {
            FreeLibrary(pair.second);
        }
        dllHandles.clear();
        dllFunctions.clear();
    }

    static bool compileScript(int partId, const std::string& code, std::string& outErrors) {
        std::string root = getProjectRoot();
        std::string buildDir = root + "\\build";
        std::string scratchDir = root + "\\build\\scratch";

        // Ensure directories exist using absolute paths
        CreateDirectoryA(buildDir.c_str(), NULL);
        CreateDirectoryA(scratchDir.c_str(), NULL);

        std::string cppPath = scratchDir + "\\Script_" + std::to_string(partId) + ".cpp";
        std::string dllPath = scratchDir + "\\Script_" + std::to_string(partId) + ".dll";
        std::string logPath = scratchDir + "\\compile_" + std::to_string(partId) + ".log";

        // Unload existing DLL before compilation so compiler can overwrite the DLL
        unloadDLL(partId);

        // Prepend struct definitions and includes to the code
        std::ofstream cppFile(cppPath);
        if (!cppFile.is_open()) {
            outErrors = "Failed to create source code file: " + cppPath;
            return false;
        }

        cppFile << "#include <glm/glm.hpp>\n";
        cppFile << "#include <cmath>\n\n";
        cppFile << "struct PartState {\n";
        cppFile << "    float position[3];\n";
        cppFile << "    float rotation[3];\n";
        cppFile << "    float size[3];\n";
        cppFile << "    float color[4];\n";
        cppFile << "    float velocity[3];\n";
        cppFile << "    bool anchored;\n";
        cppFile << "    bool canCollide;\n";
        cppFile << "    bool isGrounded;\n";
        cppFile << "    float dt;\n";
        cppFile << "};\n\n";
        cppFile << code << "\n";
        cppFile.close();

        // Build compilation command using absolute paths
        std::string glmInc = root + "\\thirdparty\\glm";
        // Wrap entire command in outer quotes so cmd.exe doesn't strip inner quotes
        std::string cmd = "\"\"C:\\Users\\Public\\GMEngineTools\\mingw64\\bin\\g++.exe\" -shared -fPIC -O2 -I\"" + glmInc + "\" \"" + cppPath + "\" -o \"" + dllPath + "\" > \"" + logPath + "\" 2>&1\"";

        int result = std::system(cmd.c_str());

        // Read log
        std::ifstream logFile(logPath);
        std::stringstream logSS;
        if (logFile.is_open()) {
            logSS << logFile.rdbuf();
            outErrors = logSS.str();
        }

        if (result != 0) {
            return false;
        }

        // Verify if DLL actually exists
        DWORD attribs = GetFileAttributesA(dllPath.c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES || (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (outErrors.empty()) {
                outErrors = "g++ exited successfully but DLL was not found.";
            }
            return false;
        }

        return true;
    }

    static bool loadDLL(int partId, std::string& outErrors) {
        unloadDLL(partId);

        std::string dllPath = getProjectRoot() + "\\build\\scratch\\Script_" + std::to_string(partId) + ".dll";

        HMODULE handle = LoadLibraryA(dllPath.c_str());
        if (!handle) {
            DWORD error = GetLastError();
            outErrors = "Failed to load compiled script DLL (Error Code: " + std::to_string(error) + ")";
            return false;
        }

        auto func = (void(*)(PartState*))GetProcAddress(handle, "updatePart");
        if (!func) {
            FreeLibrary(handle);
            outErrors = "g++ compiled script DLL loaded, but could not find entry point function:\n"
                        "extern \"C\" __declspec(dllexport) void updatePart(PartState* state)";
            return false;
        }

        dllHandles[partId] = handle;
        dllFunctions[partId] = func;
        return true;
    }

    static void executeScript(int partId, PartState* state) {
        if (dllFunctions.find(partId) != dllFunctions.end()) {
            dllFunctions[partId](state);
        }
    }
};
