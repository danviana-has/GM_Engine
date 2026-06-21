#pragma once

#include "Common.hpp"
#include <map>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

// PartState is used by both platforms
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
    bool isTouchingPlayer;
    bool playerNeedsRespawn;
    bool isColliding;
};

// ============================================================
//  Windows Desktop build — full DLL hot-reload compiler
// ============================================================
#ifdef _WIN32

#include <windows.h>

class CppCompiler {
public:
    static std::string getProjectRoot() {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        std::string exePath(path);
        size_t lastSlash = exePath.find_last_of("/\\");
        std::string exeDir = (lastSlash != std::string::npos) ? exePath.substr(0, lastSlash) : ".";

        std::string rawRoot = exeDir;
        DWORD attribs = GetFileAttributesA((exeDir + "\\thirdparty").c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES || !(attribs & FILE_ATTRIBUTE_DIRECTORY)) {
            rawRoot = exeDir + "\\..";
        }

        char canonicalPath[MAX_PATH];
        if (GetFullPathNameA(rawRoot.c_str(), MAX_PATH, canonicalPath, NULL) != 0) {
            return std::string(canonicalPath);
        }
        return rawRoot;
    }

    static inline std::map<int, HMODULE> dllHandles;
    static inline std::map<int, void(*)(PartState*)> dllFunctions;

    static void unloadDLL(int partId) {
        if (dllHandles.find(partId) != dllHandles.end()) {
            FreeLibrary(dllHandles[partId]);
            dllHandles.erase(partId);
        }
        dllFunctions.erase(partId);
    }

    static void unloadAll() {
        for (auto& pair : dllHandles) FreeLibrary(pair.second);
        dllHandles.clear();
        dllFunctions.clear();
    }

    static bool compileScript(int partId, const std::string& code, std::string& outErrors) {
        std::string root      = getProjectRoot();
        std::string buildDir  = root + "\\build";
        std::string scratchDir = root + "\\build\\scratch";

        CreateDirectoryA(buildDir.c_str(), NULL);
        CreateDirectoryA(scratchDir.c_str(), NULL);

        std::string cppPath = scratchDir + "\\Script_" + std::to_string(partId) + ".cpp";
        std::string dllPath = scratchDir + "\\Script_" + std::to_string(partId) + ".dll";
        std::string logPath = scratchDir + "\\compile_" + std::to_string(partId) + ".log";

        unloadDLL(partId);

        std::ofstream cppFile(cppPath);
        if (!cppFile.is_open()) { outErrors = "Failed to create source file: " + cppPath; return false; }

        cppFile << "#include <glm/glm.hpp>\n#include <cmath>\n\n";
        cppFile << "struct PartState {\n";
        cppFile << "    float position[3]; float rotation[3]; float size[3]; float color[4];\n";
        cppFile << "    float velocity[3]; bool anchored; bool canCollide; bool isGrounded; float dt;\n";
        cppFile << "    bool isTouchingPlayer; bool playerNeedsRespawn; bool isColliding;\n};\n\n";
        cppFile << code << "\n";
        cppFile.close();

        std::string glmInc = root + "\\thirdparty\\glm";
        std::string cmd = "\"\"C:\\Users\\Public\\GMEngineTools\\mingw64\\bin\\g++.exe\" -shared -fPIC -O2 -I\""
                          + glmInc + "\" \"" + cppPath + "\" -o \"" + dllPath + "\" > \"" + logPath + "\" 2>&1\"";

        int result = std::system(cmd.c_str());

        std::ifstream logFile(logPath);
        if (logFile.is_open()) { std::stringstream ss; ss << logFile.rdbuf(); outErrors = ss.str(); }

        if (result != 0) return false;

        DWORD attribs = GetFileAttributesA(dllPath.c_str());
        if (attribs == INVALID_FILE_ATTRIBUTES || (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
            if (outErrors.empty()) outErrors = "g++ exited OK but DLL not found.";
            return false;
        }
        return true;
    }

    static bool loadDLL(int partId, std::string& outErrors) {
        unloadDLL(partId);
        std::string dllPath = getProjectRoot() + "\\build\\scratch\\Script_" + std::to_string(partId) + ".dll";

        HMODULE handle = LoadLibraryA(dllPath.c_str());
        if (!handle) {
            outErrors = "Failed to load DLL (Error: " + std::to_string(GetLastError()) + ")";
            return false;
        }

        auto func = (void(*)(PartState*))GetProcAddress(handle, "updatePart");
        if (!func) {
            FreeLibrary(handle);
            outErrors = "DLL loaded but 'updatePart' entry point not found.";
            return false;
        }

        dllHandles[partId] = handle;
        dllFunctions[partId] = func;
        return true;
    }

    static void executeScript(int partId, PartState* state) {
        if (dllFunctions.find(partId) != dllFunctions.end())
            dllFunctions[partId](state);
    }
};

// ============================================================
//  Emscripten / Web / Android build — stub (no DLL support)
// ============================================================
#else // !_WIN32

class CppCompiler {
public:
    static std::string getProjectRoot() { return "/engine"; }

    static inline std::map<int, void(*)(PartState*)> dllFunctions; // always empty

    static void unloadDLL(int) {}
    static void unloadAll()    {}

    static bool compileScript(int, const std::string&, std::string& outErrors) {
        outErrors = "[Web Build] C++ script compilation is not available in the Android/Web version.\n"
                    "Use the Text Script mode instead.";
        return false;
    }

    static bool loadDLL(int, std::string& outErrors) {
        outErrors = "[Web Build] DLL loading not available.";
        return false;
    }

    static void executeScript(int, PartState*) {}
};

#endif // _WIN32
