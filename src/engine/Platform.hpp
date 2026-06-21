#pragma once
// Platform.hpp — Abstração de plataforma para Windows (Desktop) e Emscripten (Web/Android)
// Centraliza todas as chamadas Win32 e POSIX para compilação cruzada.

#include <string>
#include <fstream>
#include <sstream>

#ifdef __EMSCRIPTEN__
  #include <emscripten.h>
  #include <emscripten/html5.h>
  #include <sys/stat.h>
  #include <unistd.h>
#else
  #include <windows.h>
  #include <commdlg.h>
  #include <shlobj.h>
#endif

namespace Platform {

    // ── Caminho do executável / raiz do projeto ──────────────────────────────
    inline std::string getExecutablePath() {
#ifdef __EMSCRIPTEN__
        return "/engine";
#else
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        return std::string(path);
#endif
    }

    inline std::string getProjectRoot() {
#ifdef __EMSCRIPTEN__
        return "/engine";
#else
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
        char canonical[MAX_PATH];
        if (GetFullPathNameA(rawRoot.c_str(), MAX_PATH, canonical, NULL) != 0)
            return std::string(canonical);
        return rawRoot;
#endif
    }

    // ── Criação de diretórios ────────────────────────────────────────────────
    inline bool createDirectory(const std::string& path) {
#ifdef __EMSCRIPTEN__
        return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
#else
        return CreateDirectoryA(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
#endif
    }

    // ── Verificação de existência de arquivo/diretório ───────────────────────
    inline bool pathExists(const std::string& path) {
#ifdef __EMSCRIPTEN__
        struct stat st;
        return stat(path.c_str(), &st) == 0;
#else
        DWORD attr = GetFileAttributesA(path.c_str());
        return attr != INVALID_FILE_ATTRIBUTES;
#endif
    }

    // ── Diálogo de abertura de arquivo ───────────────────────────────────────
    // Em Emscripten o diálogo nativo não está disponível; retorna "" para
    // o editor mostrar um campo de texto manual.
    inline std::string openFileDialog(const std::string& title,
                                      const std::string& filter = "All Files\0*.*\0") {
#ifdef __EMSCRIPTEN__
        (void)title; (void)filter;
        return ""; // Web: input type=file via JS é tratado separadamente
#else
        char filename[MAX_PATH] = {0};
        OPENFILENAMEA ofn = {};
        ofn.lStructSize = sizeof(ofn);
        ofn.lpstrFilter = filter.c_str();
        ofn.lpstrFile   = filename;
        ofn.nMaxFile    = MAX_PATH;
        ofn.lpstrTitle  = title.c_str();
        ofn.Flags       = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
        if (GetOpenFileNameA(&ofn)) return std::string(filename);
        return "";
#endif
    }

    // ── Executar comando de sistema ──────────────────────────────────────────
    // Em Emscripten comandos de sistema não estão disponíveis.
    inline int runCommand(const std::string& cmd) {
#ifdef __EMSCRIPTEN__
        (void)cmd;
        return -1; // Não disponível no browser
#else
        return std::system(cmd.c_str());
#endif
    }

    // ── Download de arquivo (web only) ───────────────────────────────────────
    // Permite baixar conteúdo como arquivo no browser.
    inline void triggerDownload(const std::string& filename, const std::string& content) {
#ifdef __EMSCRIPTEN__
        std::string js = "var blob = new Blob([" + std::to_string(content.size()) +
                         " bytes], {type:'application/octet-stream'});"
                         "var a = document.createElement('a');"
                         "a.download='" + filename + "';"
                         "a.href=URL.createObjectURL(blob);"
                         "a.click();";
        // Usa emscripten_run_script para executar JS
        emscripten_run_script(js.c_str());
#else
        // No desktop, simplesmente escrevemos o arquivo
        std::ofstream f(filename, std::ios::binary);
        f << content;
#endif
    }

} // namespace Platform
