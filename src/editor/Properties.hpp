#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include <imgui.h>
#include <windows.h>
#include <commdlg.h>

class Properties {
public:
    void draw(Scene& scene, int selectedId) {
        ImGui::Begin("Properties");

        if (selectedId == -1) {
            ImGui::Text("Select an object to view its properties.");
        } 
        else if (selectedId == -99) {
            // Lighting properties
            ImGui::TextDisabled("Lighting Properties");
            ImGui::Separator();
            
            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::ColorEdit3("Ambient Color", glm::value_ptr(scene.ambientColor));
                ImGui::ColorEdit3("Sky Color", glm::value_ptr(scene.skyColor));
            }
            
            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Sun Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
                float sunDir[3] = { scene.sunDirection.x, scene.sunDirection.y, scene.sunDirection.z };
                if (ImGui::DragFloat3("Sun Direction", sunDir, 0.01f, -1.0f, 1.0f)) {
                    scene.sunDirection = glm::normalize(glm::vec3(sunDir[0], sunDir[1], sunDir[2]));
                }
            }
        } 
        else {
            // Part properties
            auto part = scene.findPart(selectedId);
            if (!part) {
                ImGui::Text("Part not found.");
                ImGui::End();
                return;
            }

            ImGui::TextDisabled("Part (ID: %d)", part->id);
            ImGui::Separator();

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
                // Name
                char nameBuf[128];
                strcpy(nameBuf, part->name.c_str());
                if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf))) {
                    part->name = std::string(nameBuf);
                }

                // Shape (read-only in properties, change in editor/explorer)
                std::string shapeStr = ShapeToString(part->shape);
                ImGui::LabelText("Shape", "%s", shapeStr.c_str());

                // Mesh Path (for custom models)
                char meshBuf[512];
                strcpy(meshBuf, part->meshPath.c_str());
                if (ImGui::InputText("Mesh Path (.obj/.gltf/.glb)", meshBuf, sizeof(meshBuf))) {
                    part->meshPath = std::string(meshBuf);
                }
                ImGui::SameLine();
                if (ImGui::Button("Browse...")) {
                    OPENFILENAMEA ofn;
                    char szFile[260] = { 0 };
                    ZeroMemory(&ofn, sizeof(ofn));
                    ofn.lStructSize = sizeof(ofn);
                    ofn.hwndOwner = NULL;
                    ofn.lpstrFile = szFile;
                    ofn.nMaxFile = sizeof(szFile);
                    ofn.lpstrFilter = "3D Models (*.obj;*.gltf;*.glb)\0*.obj;*.gltf;*.glb\0All Files (*.*)\0*.*\0";
                    ofn.nFilterIndex = 1;
                    ofn.lpstrFileTitle = NULL;
                    ofn.nMaxFileTitle = 0;
                    ofn.lpstrInitialDir = NULL;
                    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
                    
                    if (GetOpenFileNameA(&ofn) == TRUE) {
                        part->meshPath = std::string(ofn.lpstrFile);
                    }
                }

                // Material selection
                const char* materials[] = { "Plastic", "Wood", "Metal", "Glass", "Neon" };
                int currentMat = static_cast<int>(part->material);
                if (ImGui::Combo("Material", &currentMat, materials, IM_ARRAYSIZE(materials))) {
                    part->material = static_cast<PartMaterial>(currentMat);
                }

                // Color Picker
                ImGui::ColorEdit4("Color", glm::value_ptr(part->color));

                // Transparency
                ImGui::SliderFloat("Transparency", &part->transparency, 0.0f, 1.0f);

                // Reflectance
                ImGui::SliderFloat("Reflectance", &part->reflectance, 0.0f, 1.0f);
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Position", glm::value_ptr(part->position), 0.1f);
                
                // Warn about zero/negative sizes
                if (ImGui::DragFloat3("Size", glm::value_ptr(part->size), 0.1f, 0.1f, 100.0f)) {
                    part->size.x = std::max(part->size.x, 0.05f);
                    part->size.y = std::max(part->size.y, 0.05f);
                    part->size.z = std::max(part->size.z, 0.05f);
                }
                
                ImGui::DragFloat3("Rotation", glm::value_ptr(part->rotation), 1.0f, -360.0f, 360.0f);
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Checkbox("Anchored", &part->anchored);
                ImGui::Checkbox("CanCollide", &part->canCollide);
                
                // Show physical velocity (read-only for information)
                ImGui::LabelText("Velocity", "(%.2f, %.2f, %.2f)", part->velocity.x, part->velocity.y, part->velocity.z);
            }

            ImGui::Spacing();
            if (ImGui::CollapsingHeader("Scripting", ImGuiTreeNodeFlags_DefaultOpen)) {
                const char* languages[] = { "Text Commands", "C++ (MinGW compiler)" };
                int currentLang = part->scriptLanguage;
                if (ImGui::Combo("Script Language", &currentLang, languages, IM_ARRAYSIZE(languages))) {
                    part->scriptLanguage = currentLang;
                }

                if (part->scriptLanguage == 1 && part->script.empty()) {
                    if (ImGui::Button("Load C++ Script Template")) {
                        part->script = 
                            "// C++ game script compiled with MinGW GCC\n"
                            "extern \"C\" __declspec(dllexport) void updatePart(PartState* state) {\n"
                            "    // state->dt is the delta time\n"
                            "    // state->position[0,1,2] matches X,Y,Z coordinates\n"
                            "    // state->rotation[0,1,2] matches Euler angles in degrees\n"
                            "    \n"
                            "    // Example: continuous Y-axis rotation\n"
                            "    state->rotation[1] += 45.0f * state->dt;\n"
                            "}\n";
                    }
                }

                char scriptBuf[2048];
                strncpy(scriptBuf, part->script.c_str(), sizeof(scriptBuf));
                scriptBuf[sizeof(scriptBuf)-1] = '\0';
                
                if (ImGui::InputTextMultiline("Script Code", scriptBuf, sizeof(scriptBuf), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 8))) {
                    part->script = std::string(scriptBuf);
                }
                
                if (part->scriptLanguage == 0) {
                    ImGui::TextDisabled("Commands:");
                    ImGui::TextDisabled("  chase [speed]   | damage [amount]");
                    ImGui::TextDisabled("  checkpoint      | victory");
                    ImGui::TextDisabled("  rotate [rx ry rz]");
                    ImGui::TextDisabled("  pingpong [x1 y1 z1 x2 y2 z2 speed]");
                } else {
                    ImGui::TextDisabled("C++ compilation compiles to DLL at play.");
                    ImGui::TextDisabled("Errors will print to output console.");
                }
            }
        }

        ImGui::End();
    }
};
