#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include <map>
#include <sstream>

class ScriptEngine {
public:
    // Stateful tracking for moving objects
    static inline std::map<int, bool> pingpongDirection; // true = towards P1, false = towards P2
    static inline bool victoryTriggered = false;
    static inline bool playerNeedsRespawn = false;
    static inline glm::vec3 currentCheckpoint = glm::vec3(0.0f, 6.0f, 0.0f);
    static inline bool hasCheckpoint = false;
    static inline std::string consoleMsg = "";

    static void reset() {
        pingpongDirection.clear();
        victoryTriggered = false;
        playerNeedsRespawn = false;
        currentCheckpoint = glm::vec3(0.0f, 6.0f, 0.0f);
        hasCheckpoint = false;
        consoleMsg = "";
    }

    static bool checkAABBOverlap(const std::shared_ptr<Part>& a, const std::shared_ptr<Part>& b) {
        glm::vec3 aHalf = a->size * 0.5f;
        glm::vec3 aMin = a->position - aHalf;
        glm::vec3 aMax = a->position + aHalf;

        glm::vec3 bHalf = b->size * 0.5f;
        glm::vec3 bMin = b->position - bHalf;
        glm::vec3 bMax = b->position + bHalf;

        return (aMin.x <= bMax.x && aMax.x >= bMin.x) &&
               (aMin.y <= bMax.y && aMax.y >= bMin.y) &&
               (aMin.z <= bMax.z && aMax.z >= bMin.z);
    }

    static void runUpdate(Scene& scene, float dt) {
        std::shared_ptr<Part> player = nullptr;
        for (const auto& part : scene.workspace) {
            if (part->isPlayer) {
                player = part;
                break;
            }
        }

        for (auto& part : scene.workspace) {
            if (part->isPlayer || part->script.empty() || part->scriptLanguage == 1) continue;

            std::stringstream ss(part->script);
            std::string line;
            while (std::getline(ss, line)) {
                // Trim leading/trailing whitespace
                line.erase(0, line.find_first_not_of(" \t\r\n"));
                line.erase(line.find_last_not_of(" \t\r\n") + 1);
                if (line.empty() || line[0] == '#') continue; // Skip comments/empty lines

                std::stringstream lineSS(line);
                std::string command;
                lineSS >> command;

                if (command == "rotate") {
                    float rx = 0.0f, ry = 0.0f, rz = 0.0f;
                    lineSS >> rx >> ry >> rz;
                    part->rotation += glm::vec3(rx, ry, rz) * dt;
                }
                else if (command == "chase") {
                    float speed = 4.0f;
                    lineSS >> speed;
                    if (player) {
                        glm::vec3 dir = player->position - part->position;
                        // Avoid Y axis differences for walking enemies
                        dir.y = 0.0f;
                        if (glm::length(dir) > 0.01f) {
                            glm::vec3 move = glm::normalize(dir) * speed * dt;
                            part->position += move;
                            // Face player
                            float targetYaw = glm::degrees(atan2(-dir.z, dir.x)) + 90.0f;
                            part->rotation.y = targetYaw;
                        }
                    }
                }
                else if (command == "damage") {
                    float amount = 100.0f;
                    lineSS >> amount;
                    if (player && checkAABBOverlap(part, player)) {
                        playerNeedsRespawn = true;
                        consoleMsg = "Player touched " + part->name + " and died! Respawns.";
                    }
                }
                else if (command == "checkpoint") {
                    if (player && checkAABBOverlap(part, player)) {
                        glm::vec3 newCheckpoint = part->position + glm::vec3(0.0f, part->size.y * 0.5f + 1.0f, 0.0f);
                        if (!hasCheckpoint || glm::distance(currentCheckpoint, newCheckpoint) > 0.5f) {
                            currentCheckpoint = newCheckpoint;
                            hasCheckpoint = true;
                            part->color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f); // Change color to green on activation
                            consoleMsg = "Checkpoint activated at " + part->name + "!";
                        }
                    }
                }
                else if (command == "victory") {
                    if (player && checkAABBOverlap(part, player)) {
                        if (!victoryTriggered) {
                            victoryTriggered = true;
                            consoleMsg = "VICTORY! You reached the victory point!";
                        }
                    }
                }
                else if (command == "pingpong") {
                    float x1 = 0, y1 = 0, z1 = 0, x2 = 0, y2 = 0, z2 = 0, speed = 2.0f;
                    if (lineSS >> x1 >> y1 >> z1 >> x2 >> y2 >> z2 >> speed) {
                        glm::vec3 p1(x1, y1, z1);
                        glm::vec3 p2(x2, y2, z2);

                        if (pingpongDirection.find(part->id) == pingpongDirection.end()) {
                            pingpongDirection[part->id] = true;
                        }

                        glm::vec3 target = pingpongDirection[part->id] ? p1 : p2;
                        glm::vec3 diff = target - part->position;
                        float dist = glm::length(diff);

                        if (dist < 0.1f) {
                            // Toggle target
                            pingpongDirection[part->id] = !pingpongDirection[part->id];
                            target = pingpongDirection[part->id] ? p1 : p2;
                            diff = target - part->position;
                            dist = glm::length(diff);
                        }

                        if (dist > 0.0f) {
                            part->position += glm::normalize(diff) * std::min(speed * dt, dist);
                        }
                    }
                }
            }
        }
    }
};
