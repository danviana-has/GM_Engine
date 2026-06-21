#include <glm/glm.hpp>
#include <cmath>

struct PartState {
    float position[3]; float rotation[3]; float size[3]; float color[4];
    float velocity[3]; bool anchored; bool canCollide; bool isGrounded; float dt;
    bool isTouchingPlayer; bool playerNeedsRespawn; bool isColliding;
};

// C++ Bullet Script
extern "C" __declspec(dllexport) void updatePart(PartState* state) {
    // Fly forward along the Z axis
    state->position[2] += 30.0f * state->dt;
    
    // Reset position if it collides or goes too far
    if (state->isColliding || state->position[2] > 60.0f) {
        state->position[0] = 0.0f;
        state->position[1] = 5.0f;
        state->position[2] = -5.0f;
        state->velocity[0] = 0.0f;
        state->velocity[1] = 0.0f;
        state->velocity[2] = 0.0f;
    }
}

