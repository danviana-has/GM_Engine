#pragma once
#include <map>
#include <string>
#include <glm/glm.hpp>

class ScriptState {
public:
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
};
