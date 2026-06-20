#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include "Renderer.hpp"
#include "Physics.hpp"
#include <imgui.h>
#include <ImGuizmo.h>

class Camera {
public:
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    float yaw;
    float pitch;
    float speed = 15.0f;
    float sensitivity = 0.15f;

    Camera() {
        position = glm::vec3(0.0f, 10.0f, 25.0f);
        worldUp = glm::vec3(0.0f, 1.0f, 0.0f);
        yaw = -90.0f;
        pitch = -15.0f;
        updateCameraVectors();
    }

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void processKeyboard(const std::string& direction, float deltaTime, bool shiftPressed) {
        float velocity = speed * deltaTime;
        if (shiftPressed) velocity *= 3.0f; // Shift to fly fast

        if (direction == "FORWARD") position += front * velocity;
        if (direction == "BACKWARD") position -= front * velocity;
        if (direction == "LEFT") position -= right * velocity;
        if (direction == "RIGHT") position += right * velocity;
        if (direction == "UP") position += worldUp * velocity;
        if (direction == "DOWN") position -= worldUp * velocity;
    }

    void processMouseMovement(float xOffset, float yOffset) {
        xOffset *= sensitivity;
        yOffset *= sensitivity;

        yaw += xOffset;
        pitch += yOffset;

        if (pitch > 89.0f) pitch = 89.0f;
        if (pitch < -89.0f) pitch = -89.0f;

        updateCameraVectors();
    }

private:
    void updateCameraVectors() {
        glm::vec3 f;
        f.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        f.y = sin(glm::radians(pitch));
        f.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(f);
        
        right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));
    }
};

class Viewport {
public:
    Camera camera;
    int selectedPartId = -1;
    bool isPlaying = false; // Is physics running

    // Gizmo state (ImGuizmo)
    int currentGizmoOperation = ImGuizmo::TRANSLATE;

    // Mouse drag state for camera control
    bool rightMousePressed = false;
    ImVec2 lastMousePos;

    void draw(Renderer& renderer, Scene& scene, Physics& physics, float deltaTime) {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        
        // Get panel size
        ImVec2 size = ImGui::GetContentRegionAvail();
        renderer.resizeViewport(static_cast<int>(size.x), static_cast<int>(size.y));

        // Locate player part
        std::shared_ptr<Part> playerPart = nullptr;
        for (const auto& part : scene.workspace) {
            if (part->isPlayer) {
                playerPart = part;
                break;
            }
        }

        // 1. Third-Person Orbit Camera positioning (before rendering)
        if (isPlaying && playerPart) {
            glm::vec3 target = playerPart->position + glm::vec3(0.0f, 1.5f, 0.0f); // follow head level
            camera.position = target - camera.front * 16.0f; // Stay 16 units behind along view vector
        }

        // Render scene to FBO
        renderer.startRender();
        
        glm::mat4 projection = glm::perspective(glm::radians(45.0f), size.x / size.y, 0.1f, 1000.0f);
        glm::mat4 view = camera.getViewMatrix();
        
        renderer.renderScene(scene, view, projection, selectedPartId);
        
        renderer.endRender();

        // Display texture in ImGui
        ImGui::Image(
            reinterpret_cast<void*>(static_cast<uintptr_t>(renderer.textureColorBuffer)), 
            size, 
            ImVec2(0, 1), 
            ImVec2(1, 0)
        );

        // 3. Draw Roblox-style StarterGui overlays
        ImVec2 imagePos = ImGui::GetItemRectMin();
        ImVec2 imageSize = ImGui::GetItemRectSize();
        ImGui::SetCursorScreenPos(imagePos);
        ImGui::BeginChild("StarterGuiCanvas", imageSize, ImGuiChildFlags_None, 
                          ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav);
        {
            for (const auto& elem : scene.starterGui) {
                if (elem->parentId == -1) {
                    GuiElement::drawElement(scene.starterGui, elem, imagePos.x, imagePos.y, imageSize.x, imageSize.y, isPlaying);
                }
            }
        }
        ImGui::EndChild();

        // Capture Inputs if viewport window is hovered/active
        bool hovered = ImGui::IsWindowHovered();
        ImGuiIO& io = ImGui::GetIO();

        // Right click camera control
        if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            rightMousePressed = true;
            lastMousePos = io.MousePos;
        }
        if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
            rightMousePressed = false;
        }

        if (rightMousePressed) {
            ImVec2 mousePos = io.MousePos;
            float xOffset = mousePos.x - lastMousePos.x;
            float yOffset = lastMousePos.y - mousePos.y; // reversed since y-coordinates go from bottom to top
            camera.processMouseMovement(xOffset, yOffset);
            lastMousePos = mousePos;
        }

        // Handle controls based on play mode state
        if (isPlaying && playerPart) {
            // PLAY MODE: WASD controls the character avatar
            if (!io.WantCaptureKeyboard) {
                glm::vec3 moveDir(0.0f);
                glm::vec3 camForward = glm::normalize(glm::vec3(camera.front.x, 0.0f, camera.front.z));
                glm::vec3 camRight = glm::normalize(glm::vec3(camera.right.x, 0.0f, camera.right.z));

                if (ImGui::IsKeyDown(ImGuiKey_W)) moveDir += camForward;
                if (ImGui::IsKeyDown(ImGuiKey_S)) moveDir -= camForward;
                if (ImGui::IsKeyDown(ImGuiKey_A)) moveDir -= camRight;
                if (ImGui::IsKeyDown(ImGuiKey_D)) moveDir += camRight;

                float speed = 12.0f;
                if (glm::length(moveDir) > 0.0f) {
                    moveDir = glm::normalize(moveDir);
                    playerPart->velocity.x = moveDir.x * speed;
                    playerPart->velocity.z = moveDir.z * speed;

                    // Face character in walking direction
                    float targetYaw = glm::degrees(atan2(-moveDir.z, moveDir.x)) + 90.0f;
                    playerPart->rotation.y = targetYaw;
                }

                // Jump
                if (ImGui::IsKeyPressed(ImGuiKey_Space) && physics.isGrounded(scene, playerPart)) {
                    playerPart->velocity.y = 15.0f;
                }
            }
        } 
        else {
            // EDITOR MODE: WASD controls the free camera
            if (hovered && !io.WantCaptureKeyboard) {
                bool shift = io.KeyShift;
                if (ImGui::IsKeyDown(ImGuiKey_W)) camera.processKeyboard("FORWARD", deltaTime, shift);
                if (ImGui::IsKeyDown(ImGuiKey_S)) camera.processKeyboard("BACKWARD", deltaTime, shift);
                if (ImGui::IsKeyDown(ImGuiKey_A)) camera.processKeyboard("LEFT", deltaTime, shift);
                if (ImGui::IsKeyDown(ImGuiKey_D)) camera.processKeyboard("RIGHT", deltaTime, shift);
                if (ImGui::IsKeyDown(ImGuiKey_E)) camera.processKeyboard("UP", deltaTime, shift);
                if (ImGui::IsKeyDown(ImGuiKey_Q)) camera.processKeyboard("DOWN", deltaTime, shift);

                // Gizmo Hotkeys
                if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
                if (ImGui::IsKeyPressed(ImGuiKey_E)) currentGizmoOperation = ImGuizmo::ROTATE;
                if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::SCALE;
            }

            // Left click selection (only if not dragging gizmo)
            if (hovered && !rightMousePressed && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver()) {
                ImVec2 winPos = ImGui::GetWindowPos();
                ImVec2 localMousePos = ImVec2(io.MousePos.x - winPos.x, io.MousePos.y - winPos.y);
                selectedPartId = castRayToSelectPart(localMousePos, size, scene, view, projection);
            }

            // 2. Draw ImGuizmo color overlay
            if (selectedPartId >= 1) {
                auto part = scene.findPart(selectedPartId);
                if (part && !part->isPlayer) {
                    ImGuizmo::SetDrawlist();
                    ImGuizmo::SetRect(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y, size.x, size.y);

                    glm::mat4 modelMatrix = part->getModelMatrix();
                    glm::mat4 viewMatrix = view;
                    glm::mat4 projMatrix = projection;

                    // Draw colored handle overlay
                    if (ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projMatrix), 
                                            (ImGuizmo::OPERATION)currentGizmoOperation, 
                                            ImGuizmo::LOCAL, glm::value_ptr(modelMatrix))) {
                        // Decompose back to components
                        float translation[3], rotation[3], scale[3];
                        ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(modelMatrix), translation, rotation, scale);

                        part->position = glm::vec3(translation[0], translation[1], translation[2]);
                        part->rotation = glm::vec3(rotation[0], rotation[1], rotation[2]);
                        part->size = glm::vec3(scale[0], scale[1], scale[2]);
                    }
                }
            }
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

private:
    int castRayToSelectPart(ImVec2 localMouse, ImVec2 viewportSize, const Scene& scene, const glm::mat4& view, const glm::mat4& projection) {
        // Calculate normalized device coordinates [-1, 1]
        float x = (2.0f * localMouse.x) / viewportSize.x - 1.0f;
        float y = 1.0f - (2.0f * localMouse.y) / viewportSize.y;

        glm::vec4 rayClip = glm::vec4(x, y, -1.0f, 1.0f);
        glm::vec4 rayEye = glm::inverse(projection) * rayClip;
        rayEye = glm::vec4(rayEye.x, rayEye.y, -1.0f, 0.0f);
        
        glm::vec3 rayWorld = glm::vec3(glm::inverse(view) * rayEye);
        glm::vec3 rayDir = glm::normalize(rayWorld);
        glm::vec3 rayOrigin = camera.position;

        int closestPartId = -1;
        float minT = 999999.0f;

        for (const auto& part : scene.workspace) {
            if (part->isPlayer) continue; // Skip selecting player capsule

            glm::vec3 minBound = part->position - part->size * 0.5f;
            glm::vec3 maxBound = part->position + part->size * 0.5f;

            float t;
            if (rayAABBIntersection(rayOrigin, rayDir, minBound, maxBound, t)) {
                if (t >= 0.0f && t < minT) {
                    minT = t;
                    closestPartId = part->id;
                }
            }
        }

        return closestPartId;
    }

    bool rayAABBIntersection(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& minB, const glm::vec3& maxB, float& t) {
        float tmin = (minB.x - origin.x) / (dir.x + 1e-6f);
        float tmax = (maxB.x - origin.x) / (dir.x + 1e-6f);

        if (tmin > tmax) std::swap(tmin, tmax);

        float tymin = (minB.y - origin.y) / (dir.y + 1e-6f);
        float tymax = (maxB.y - origin.y) / (dir.y + 1e-6f);

        if (tymin > tymax) std::swap(tymin, tymax);

        if ((tmin > tymax) || (tymin > tmax)) return false;

        if (tymin > tmin) tmin = tymin;
        if (tymax < tmax) tmax = tymax;

        float tzmin = (minB.z - origin.z) / (dir.z + 1e-6f);
        float tzmax = (maxB.z - origin.z) / (dir.z + 1e-6f);

        if (tzmin > tzmax) std::swap(tzmin, tzmax);

        if ((tmin > tzmax) || (tzmin > tmax)) return false;

        if (tzmin > tmin) tmin = tzmin;
        if (tzmax < tmax) tmax = tzmax;

        t = tmin;
        return true;
    }
};
