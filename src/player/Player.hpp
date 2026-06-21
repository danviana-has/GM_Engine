#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include "Renderer.hpp"
#include "Physics.hpp"
#include "../editor/Viewport.hpp" // Reuse Camera
#include <GLFW/glfw3.h>

class Player {
public:
    Camera camera;
    bool rightMousePressed = false;
    double lastMouseX = 0, lastMouseY = 0;

    Player() {
        camera.position = glm::vec3(0.0f, 10.0f, 25.0f);
        camera.pitch = -15.0f;
    }

    void initPlayerCharacter(Scene& scene) {
        // Ensure character exists
        for (const auto& part : scene.workspace) {
            if (part->isPlayer) return;
        }

        auto playerPart = scene.addPart(PartShape::Block);
        playerPart->name = "Player";
        playerPart->size = glm::vec3(1.8f, 4.0f, 1.8f);
        playerPart->position = glm::vec3(0.0f, 6.0f, 0.0f);
        playerPart->color = glm::vec4(0.0f, 0.4f, 0.8f, 1.0f); // Blue character
        playerPart->anchored = false;
        playerPart->canCollide = true;
        playerPart->isPlayer = true;
    }

    void handleInput(GLFWwindow* window, Scene& scene, Physics& physics, float deltaTime) {
        std::shared_ptr<Part> playerPart = nullptr;
        for (const auto& part : scene.workspace) {
            if (part->isPlayer) {
                playerPart = part;
                break;
            }
        }

        if (!playerPart) return;

        // Toggle camera rotation with right click drag
        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            double mouseX, mouseY;
            glfwGetCursorPos(window, &mouseX, &mouseY);
            if (!rightMousePressed) {
                rightMousePressed = true;
            } else {
                float xOffset = static_cast<float>(mouseX - lastMouseX);
                float yOffset = static_cast<float>(lastMouseY - mouseY);
                camera.processMouseMovement(xOffset, yOffset);
            }
            lastMouseX = mouseX;
            lastMouseY = mouseY;
        } else {
            rightMousePressed = false;
        }

        // WASD controls the character avatar relative to camera angle
        glm::vec3 moveDir(0.0f);
        glm::vec3 camForward = glm::normalize(glm::vec3(camera.front.x, 0.0f, camera.front.z));
        glm::vec3 camRight = glm::normalize(glm::vec3(camera.right.x, 0.0f, camera.right.z));

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) moveDir += camForward;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) moveDir -= camForward;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) moveDir -= camRight;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) moveDir += camRight;

        float speed = 12.0f;
        if (glm::length(moveDir) > 0.0f) {
            moveDir = glm::normalize(moveDir);
            playerPart->velocity.x = moveDir.x * speed;
            playerPart->velocity.z = moveDir.z * speed;

            // Face walking direction
            float targetYaw = glm::degrees(atan2(-moveDir.z, moveDir.x)) + 90.0f;
            playerPart->rotation.y = targetYaw;
        }

        // Jump
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS && physics.isGrounded(scene, playerPart)) {
            playerPart->velocity.y = 26.0f;
        }
    }

    void runFrame(GLFWwindow* window, Renderer& renderer, Scene& scene, Physics& physics, float deltaTime) {
        // Ensure avatar is active
        initPlayerCharacter(scene);

        // Process inputs
        handleInput(window, scene, physics, deltaTime);

        // Position orbit camera behind character
        std::shared_ptr<Part> playerPart = nullptr;
        for (const auto& part : scene.workspace) {
            if (part->isPlayer) {
                playerPart = part;
                break;
            }
        }
        if (playerPart) {
            glm::vec3 target = playerPart->position + glm::vec3(0.0f, 1.5f, 0.0f); // head level
            camera.position = target - camera.front * 16.0f; // Stay 16 units behind along view vector
        }

        // Render scene directly to screen (no FBO)
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        glViewport(0, 0, width, height);

        // Deep sky clear color
        glClearColor(scene.skyColor.r, scene.skyColor.g, scene.skyColor.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);

        glm::mat4 projection = glm::perspective(glm::radians(45.0f), static_cast<float>(width) / static_cast<float>(height), 0.1f, 1000.0f);
        glm::mat4 view = camera.getViewMatrix();

        renderer.renderScene(scene, view, projection);
    }
};
