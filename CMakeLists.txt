cmake_minimum_required(VERSION 3.10)
project(GMEngine C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCES
    src/main.cpp
    thirdparty/glad/src/glad.c
    thirdparty/imgui/imgui.cpp
    thirdparty/imgui/imgui_draw.cpp
    thirdparty/imgui/imgui_tables.cpp
    thirdparty/imgui/imgui_widgets.cpp
    thirdparty/imgui/backends/imgui_impl_glfw.cpp
    thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    thirdparty/imguizmo/ImGuizmo.cpp
)

# Create executable
add_executable(GMEngine ${SOURCES})

# Include directories
target_include_directories(GMEngine PRIVATE
    src
    src/engine
    src/editor
    src/player
    thirdparty/glfw/include
    thirdparty/glad/include
    thirdparty/imgui
    thirdparty/imgui/backends
    thirdparty/glm
    thirdparty/json
    thirdparty/imguizmo
)

# GLFW Static library path (built for MinGW 64-bit)
set(GLFW_LIB "${CMAKE_SOURCE_DIR}/thirdparty/glfw/lib-mingw-w64/libglfw3.a")

# Link libraries
# On Windows, we need opengl32, gdi32, shell32 and standard Windows subsystems
target_link_libraries(GMEngine PRIVATE
    ${GLFW_LIB}
    opengl32
    gdi32
    shell32
    uuid
    comdlg32
    ws2_32
    winhttp
)

# Static linking for portability (removes dependency on libgcc, libstdc++, etc. DLLs)
target_link_options(GMEngine PRIVATE -static-libgcc -static-libstdc++ -static)

# Compiler definitions
target_compile_definitions(GMEngine PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

# --- GMPlay MMO Executable Target ---
set(GMPLAY_SOURCES
    src/gmplay_app.cpp
    thirdparty/glad/src/glad.c
    thirdparty/imgui/imgui.cpp
    thirdparty/imgui/imgui_draw.cpp
    thirdparty/imgui/imgui_tables.cpp
    thirdparty/imgui/imgui_widgets.cpp
    thirdparty/imgui/backends/imgui_impl_glfw.cpp
    thirdparty/imgui/backends/imgui_impl_opengl3.cpp
    thirdparty/imguizmo/ImGuizmo.cpp
)

add_executable(GMPlay ${GMPLAY_SOURCES})

target_include_directories(GMPlay PRIVATE
    src
    src/engine
    src/editor
    src/player
    thirdparty/glfw/include
    thirdparty/glad/include
    thirdparty/imgui
    thirdparty/imgui/backends
    thirdparty/glm
    thirdparty/json
    thirdparty/imguizmo
)

target_link_libraries(GMPlay PRIVATE
    ${GLFW_LIB}
    opengl32
    gdi32
    shell32
    uuid
    comdlg32
    ws2_32
)

target_link_options(GMPlay PRIVATE -static-libgcc -static-libstdc++ -static)
target_compile_definitions(GMPlay PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)

# Optimization flag for release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(GMEngine PRIVATE -O3)
    target_compile_options(GMPlay PRIVATE -O3)
endif()
