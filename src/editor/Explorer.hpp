#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include <imgui.h>
#include <functional>

class Explorer {
public:
    char renameBuffer[128] = "";
    int renamingPartId = -1;

    void draw(Scene& scene, int& selectedId) {
        ImGui::Begin("Explorer");

        // Hierarchy root
        if (ImGui::TreeNodeEx("Game", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            // Workspace Node
            bool workspaceOpen = ImGui::TreeNodeEx("Workspace", ImGuiTreeNodeFlags_DefaultOpen);
            
            // Workspace Context Menu (right click workspace to insert parts)
            if (ImGui::BeginPopupContextItem("WorkspaceContextMenu")) {
                if (ImGui::MenuItem("Insert Block")) {
                    auto p = scene.addPart(PartShape::Block);
                    selectedId = p->id;
                }
                if (ImGui::MenuItem("Insert Sphere")) {
                    auto p = scene.addPart(PartShape::Sphere);
                    selectedId = p->id;
                }
                if (ImGui::MenuItem("Insert Cylinder")) {
                    auto p = scene.addPart(PartShape::Cylinder);
                    selectedId = p->id;
                }
                if (ImGui::MenuItem("Insert Wedge")) {
                    auto p = scene.addPart(PartShape::Wedge);
                    selectedId = p->id;
                }
                ImGui::EndPopup();
            }

            if (workspaceOpen) {
                // List all parts
                for (auto& part : scene.workspace) {
                    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    if (part->id == selectedId) {
                        nodeFlags |= ImGuiTreeNodeFlags_Selected;
                    }

                    // Display name or rename input field
                    if (renamingPartId == part->id) {
                        ImGui::SetNextItemWidth(120.0f);
                        if (ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
                            part->name = std::string(renameBuffer);
                            renamingPartId = -1;
                        }
                        if (ImGui::IsItemDeactivated() && !ImGui::IsItemActivated()) {
                            // If clicked away, cancel renaming
                            renamingPartId = -1;
                        }
                    } else {
                        // Render standard tree node
                        ImGui::TreeNodeEx((void*)(intptr_t)part->id, nodeFlags, "%s", part->name.c_str());
                        
                        // Select on click
                        if (ImGui::IsItemClicked()) {
                            selectedId = part->id;
                        }

                        // Context menu for each part
                        std::string popupId = "PartContextMenu_" + std::to_string(part->id);
                        if (ImGui::BeginPopupContextItem(popupId.c_str())) {
                            selectedId = part->id;

                            if (ImGui::MenuItem("Rename")) {
                                renamingPartId = part->id;
                                strcpy(renameBuffer, part->name.c_str());
                            }
                            if (ImGui::MenuItem("Duplicate")) {
                                auto dup = scene.addPart(part->shape);
                                dup->name = part->name + " (Copy)";
                                dup->shape = part->shape;
                                dup->material = part->material;
                                dup->position = part->position + glm::vec3(0.0f, part->size.y + 1.0f, 0.0f);
                                dup->size = part->size;
                                dup->rotation = part->rotation;
                                dup->color = part->color;
                                dup->transparency = part->transparency;
                                dup->reflectance = part->reflectance;
                                dup->anchored = part->anchored;
                                dup->canCollide = part->canCollide;
                                selectedId = dup->id;
                            }
                            if (ImGui::MenuItem("Delete")) {
                                scene.removePart(part->id);
                                if (selectedId == part->id) {
                                    selectedId = -1;
                                }
                            }
                            ImGui::EndPopup();
                        }
                    }
                }
                ImGui::TreePop();
            }

            // Lighting Node
            if (ImGui::TreeNodeEx("Lighting", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen)) {
                if (ImGui::IsItemClicked()) {
                    selectedId = -99; // Special ID for Lighting properties
                }
            }

            // StarterGui Node
            bool guiOpen = ImGui::TreeNodeEx("StarterGui", ImGuiTreeNodeFlags_DefaultOpen);
            
            // Context menu on StarterGui root node (right click to insert ScreenGui)
            if (ImGui::BeginPopupContextItem("StarterGuiContextMenu")) {
                if (ImGui::MenuItem("Insert ScreenGui")) {
                    auto g = scene.addGuiElement(GuiElementType::ScreenGui);
                    selectedId = -100 - g->id;
                }
                ImGui::EndPopup();
            }
            
            if (guiOpen) {
                // List GUI elements hierarchically
                // Helper to render GUI element node recursively
                std::function<void(std::shared_ptr<GuiElement>)> renderGuiNode = [&](std::shared_ptr<GuiElement> elem) {
                    bool hasChildren = false;
                    for (const auto& g : scene.starterGui) {
                        if (g->parentId == elem->id) {
                            hasChildren = true;
                            break;
                        }
                    }
                    
                    ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
                    if (!hasChildren) {
                        nodeFlags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                    }
                    if ((-100 - elem->id) == selectedId) {
                        nodeFlags |= ImGuiTreeNodeFlags_Selected;
                    }
                    
                    bool nodeOpen = ImGui::TreeNodeEx((void*)(intptr_t)(-100 - elem->id), nodeFlags, "%s", elem->name.c_str());
                    
                    if (ImGui::IsItemClicked()) {
                        selectedId = -100 - elem->id;
                    }
                    
                    // Context Menu for each GuiElement
                    std::string popupId = "GuiContextMenu_" + std::to_string(elem->id);
                    if (ImGui::BeginPopupContextItem(popupId.c_str())) {
                        selectedId = -100 - elem->id;
                        
                        if (elem->type == GuiElementType::ScreenGui) {
                            if (ImGui::MenuItem("Insert Frame")) {
                                auto g = scene.addGuiElement(GuiElementType::Frame);
                                g->parentId = elem->id;
                                selectedId = -100 - g->id;
                            }
                            if (ImGui::MenuItem("Insert TextLabel")) {
                                auto g = scene.addGuiElement(GuiElementType::TextLabel);
                                g->parentId = elem->id;
                                selectedId = -100 - g->id;
                            }
                            if (ImGui::MenuItem("Insert TextButton")) {
                                auto g = scene.addGuiElement(GuiElementType::TextButton);
                                g->parentId = elem->id;
                                selectedId = -100 - g->id;
                            }
                        } else if (elem->type == GuiElementType::Frame) {
                            if (ImGui::MenuItem("Insert TextLabel")) {
                                auto g = scene.addGuiElement(GuiElementType::TextLabel);
                                g->parentId = elem->id;
                                selectedId = -100 - g->id;
                            }
                            if (ImGui::MenuItem("Insert TextButton")) {
                                auto g = scene.addGuiElement(GuiElementType::TextButton);
                                g->parentId = elem->id;
                                selectedId = -100 - g->id;
                            }
                        }
                        
                        if (ImGui::MenuItem("Delete")) {
                            scene.removeGuiElement(elem->id);
                            if (selectedId == (-100 - elem->id)) {
                                selectedId = -1;
                            }
                        }
                        ImGui::EndPopup();
                    }
                    
                    if (hasChildren && nodeOpen) {
                        for (const auto& g : scene.starterGui) {
                            if (g->parentId == elem->id) {
                                renderGuiNode(g);
                            }
                        }
                        ImGui::TreePop();
                    }
                };
                
                // Print all top-level GUI elements
                for (const auto& g : scene.starterGui) {
                    if (g->parentId == -1) {
                        renderGuiNode(g);
                    }
                }
                
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        ImGui::End();
    }
};
