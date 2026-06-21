#pragma once

#include "Scene.hpp"
#include "ScriptEngine.hpp"
#include "CppCompiler.hpp"

class Physics {
public:
    float gravity = -35.0f; // Roblox-style quick gravity
    float airResistance = 0.99f; // Dampening to stabilize physics

    bool isGrounded(const Scene& scene, const std::shared_ptr<Part>& part) {
        if (part->anchored) return true;
        
        glm::vec3 halfSize = part->size * 0.5f;
        glm::vec3 minBound = part->position - halfSize;
        glm::vec3 maxBound = part->position + halfSize;
        
        // Define a small detection box directly below the part
        glm::vec3 checkMin = glm::vec3(minBound.x + 0.05f, minBound.y - 0.15f, minBound.z + 0.05f);
        glm::vec3 checkMax = glm::vec3(maxBound.x - 0.05f, minBound.y + 0.01f, maxBound.z - 0.05f);
        
        for (const auto& other : scene.workspace) {
            if (other->id == part->id || !other->canCollide) continue;
            
            glm::vec3 oHalfSize = other->size * 0.5f;
            glm::vec3 oMin = other->position - oHalfSize;
            glm::vec3 oMax = other->position + oHalfSize;
            
            bool overlapX = (checkMin.x <= oMax.x && checkMax.x >= oMin.x);
            bool overlapY = (checkMin.y <= oMax.y && checkMax.y >= oMin.y);
            bool overlapZ = (checkMin.z <= oMax.z && checkMax.z >= oMin.z);
            
            if (overlapX && overlapY && overlapZ) {
                return true;
            }
        }
        return false;
    }

    void compileAndLoadAllCppScripts(Scene& scene) {
        CppCompiler::unloadAll();
        for (auto& part : scene.workspace) {
            if (part->isPlayer || part->script.empty() || part->scriptLanguage != 1) continue;
            
            std::string errors;
            ScriptEngine::consoleMsg = "[C++] Compiling script for part: " + part->name + "...";
            if (CppCompiler::compileScript(part->id, part->script, errors)) {
                if (CppCompiler::loadDLL(part->id, errors)) {
                    ScriptEngine::consoleMsg = "[C++] Script for " + part->name + " successfully compiled and loaded!";
                } else {
                    ScriptEngine::consoleMsg = "[C++] Failed to load script DLL for " + part->name + ":\n" + errors;
                }
            } else {
                ScriptEngine::consoleMsg = "[C++] Compiler error in part " + part->name + ":\n" + errors;
            }
        }
    }

    void unloadAllCppScripts() {
        CppCompiler::unloadAll();
    }

    void update(Scene& scene, float dt) {
        // Cap dt to prevent huge steps when lagging
        if (dt > 0.1f) dt = 0.1f;

        // Find player part first
        std::shared_ptr<Part> playerPart = nullptr;
        for (const auto& part : scene.workspace) {
            if (part->isPlayer) {
                playerPart = part;
                break;
            }
        }

        // Initialize checkpoint to player's starting spawn point if it hasn't been set yet
        if (playerPart && !ScriptEngine::hasCheckpoint) {
            ScriptEngine::currentCheckpoint = playerPart->position;
            ScriptEngine::hasCheckpoint = true;
        }

        // 0. Run C++ scripting updates
        for (auto& part : scene.workspace) {
            if (part->isPlayer || part->script.empty() || part->scriptLanguage != 1) continue;
            
            if (CppCompiler::dllFunctions.find(part->id) != CppCompiler::dllFunctions.end()) {
                PartState state;
                state.position[0] = part->position.x;
                state.position[1] = part->position.y;
                state.position[2] = part->position.z;
                
                state.rotation[0] = part->rotation.x;
                state.rotation[1] = part->rotation.y;
                state.rotation[2] = part->rotation.z;
                
                state.size[0] = part->size.x;
                state.size[1] = part->size.y;
                state.size[2] = part->size.z;
                
                state.color[0] = part->color.r;
                state.color[1] = part->color.g;
                state.color[2] = part->color.b;
                state.color[3] = part->color.a;
                
                state.velocity[0] = part->velocity.x;
                state.velocity[1] = part->velocity.y;
                state.velocity[2] = part->velocity.z;
                
                state.anchored = part->anchored;
                state.canCollide = part->canCollide;
                state.isGrounded = isGrounded(scene, part);
                state.dt = dt;
                
                state.isTouchingPlayer = false;
                if (playerPart && ScriptEngine::checkAABBOverlap(part, playerPart)) {
                    state.isTouchingPlayer = true;
                }
                state.playerNeedsRespawn = ScriptEngine::playerNeedsRespawn;
                
                state.isColliding = false;
                for (const auto& other : scene.workspace) {
                    if (other->id != part->id && other->canCollide && ScriptEngine::checkAABBOverlap(part, other)) {
                        state.isColliding = true;
                        break;
                    }
                }
                
                CppCompiler::executeScript(part->id, &state);
                
                part->position = glm::vec3(state.position[0], state.position[1], state.position[2]);
                part->rotation = glm::vec3(state.rotation[0], state.rotation[1], state.rotation[2]);
                part->size = glm::vec3(state.size[0], state.size[1], state.size[2]);
                part->color = glm::vec4(state.color[0], state.color[1], state.color[2], state.color[3]);
                part->velocity = glm::vec3(state.velocity[0], state.velocity[1], state.velocity[2]);
                part->anchored = state.anchored;
                part->canCollide = state.canCollide;
                
                if (state.playerNeedsRespawn) {
                    ScriptEngine::playerNeedsRespawn = true;
                }
            }
        }

        // Run scripting engine updates (for text scripts)
        ScriptEngine::runUpdate(scene, dt);

        // 1. Apply gravity and update position of unanchored parts
        for (auto& part : scene.workspace) {
            if (part->anchored) {
                part->velocity = glm::vec3(0.0f);
                continue;
            }
            
            // Apply gravity
            part->velocity.y += gravity * dt;
            
            // Apply air resistance / snap friction for players
            if (part->isPlayer) {
                // Snape horizontal movement to avoid sliding on floor
                part->velocity.x *= std::pow(0.80f, dt * 60.0f);
                part->velocity.z *= std::pow(0.80f, dt * 60.0f);
                part->velocity.y *= std::pow(airResistance, dt * 60.0f);
            } else {
                part->velocity *= std::pow(airResistance, dt * 60.0f);
            }
            
            // Move part
            part->position += part->velocity * dt;
        }

        // 2. Handle collisions (AABB vs AABB)
        const int iterations = 3;
        for (int iter = 0; iter < iterations; ++iter) {
            resolveCollisions(scene);
        }

        // 3. Check for falling into the abyss or script-triggered respawn
        if (playerPart) {
            if (playerPart->position.y < -30.0f || ScriptEngine::playerNeedsRespawn) {
                playerPart->position = ScriptEngine::currentCheckpoint;
                playerPart->velocity = glm::vec3(0.0f);
                ScriptEngine::playerNeedsRespawn = false;
                if (playerPart->position.y < -30.0f) {
                    ScriptEngine::consoleMsg = "Player fell in the abyss! Respawning at checkpoint.";
                }
            }
        }
    }

private:
    struct AABB {
        glm::vec3 minBound;
        glm::vec3 maxBound;
    };

    AABB getAABB(const std::shared_ptr<Part>& part) {
        AABB box;
        // In this simple engine, we assume axis-aligned boxes for collision
        glm::vec3 halfSize = part->size * 0.5f;
        box.minBound = part->position - halfSize;
        box.maxBound = part->position + halfSize;
        return box;
    }

    void resolveCollisions(Scene& scene) {
        size_t numParts = scene.workspace.size();
        for (size_t i = 0; i < numParts; ++i) {
            auto& partA = scene.workspace[i];
            if (!partA->canCollide) continue;

            for (size_t j = i + 1; j < numParts; ++j) {
                auto& partB = scene.workspace[j];
                if (!partB->canCollide) continue;

                // At least one part must be unanchored to move
                if (partA->anchored && partB->anchored) continue;

                AABB a = getAABB(partA);
                AABB b = getAABB(partB);

                // Check intersection
                bool overlapX = (a.minBound.x <= b.maxBound.x && a.maxBound.x >= b.minBound.x);
                bool overlapY = (a.minBound.y <= b.maxBound.y && a.maxBound.y >= b.minBound.y);
                bool overlapZ = (a.minBound.z <= b.maxBound.z && a.maxBound.z >= b.minBound.z);

                if (overlapX && overlapY && overlapZ) {
                    // If one of the parts is a Bullet, apply custom physics
                    if (partA->name == "Bullet" || partB->name == "Bullet") {
                        auto bullet = (partA->name == "Bullet") ? partA : partB;
                        auto target = (partA->name == "Bullet") ? partB : partA;
                        
                        if (target->isPlayer) {
                            ScriptEngine::playerNeedsRespawn = true;
                            ScriptEngine::consoleMsg = "Player was hit by bullet and died! Respawns.";
                        } else if (!target->anchored) {
                            // Make it fly away!
                            glm::vec3 pushDirection = glm::normalize(target->position - bullet->position);
                            // Avoid NaN
                            if (glm::length(pushDirection) < 0.01f) pushDirection = glm::vec3(0, 1, 0);
                            target->velocity = pushDirection * 45.0f; // Fly away!
                        }
                    }

                    // Collision detected! Calculate overlaps on all axes
                    float dx1 = b.maxBound.x - a.minBound.x;
                    float dx2 = a.maxBound.x - b.minBound.x;
                    float dy1 = b.maxBound.y - a.minBound.y;
                    float dy2 = a.maxBound.y - b.minBound.y;
                    float dz1 = b.maxBound.z - a.minBound.z;
                    float dz2 = a.maxBound.z - b.minBound.z;

                    float minOverlap = 99999.0f;
                    glm::vec3 normal(0.0f);

                    if (dx1 < minOverlap) { minOverlap = dx1; normal = glm::vec3(1, 0, 0); }
                    if (dx2 < minOverlap) { minOverlap = dx2; normal = glm::vec3(-1, 0, 0); }
                    if (dy1 < minOverlap) { minOverlap = dy1; normal = glm::vec3(0, 1, 0); }
                    if (dy2 < minOverlap) { minOverlap = dy2; normal = glm::vec3(0, -1, 0); }
                    if (dz1 < minOverlap) { minOverlap = dz1; normal = glm::vec3(0, 0, 1); }
                    if (dz2 < minOverlap) { minOverlap = dz2; normal = glm::vec3(0, 0, -1); }

                    // Resolve collision: push apart along normal by minOverlap
                    if (partA->anchored) {
                        // Move B
                        partB->position -= normal * minOverlap;
                        // Zero velocity along collision normal for B (moving towards A means dot > 0)
                        if (glm::dot(partB->velocity, normal) > 0) {
                            partB->velocity -= glm::dot(partB->velocity, normal) * normal;
                        }
                    } else if (partB->anchored) {
                        // Move A (in opposite direction of normal)
                        partA->position += normal * minOverlap;
                        // Zero velocity along collision normal for A (moving towards B means dot < 0)
                        if (glm::dot(partA->velocity, normal) < 0) {
                            partA->velocity -= glm::dot(partA->velocity, normal) * normal;
                        }
                    } else {
                        // Both unanchored: share the push
                        partA->position += normal * (minOverlap * 0.5f);
                        partB->position -= normal * (minOverlap * 0.5f);

                        // Relative velocity resolution
                        glm::vec3 relVel = partB->velocity - partA->velocity;
                        float velAlongNormal = glm::dot(relVel, normal);
                        if (velAlongNormal > 0) {
                            // Simple inelastic response (no bounce to keep stack stable)
                            float impulse = velAlongNormal;
                            partA->velocity += impulse * 0.5f * normal;
                            partB->velocity -= impulse * 0.5f * normal;
                        }
                    }
                }
            }
        }
    }
};
