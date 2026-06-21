#include <glm/glm.hpp>
#include <cmath>

struct PartState {
    float position[3]; float rotation[3]; float size[3]; float color[4];
    float velocity[3]; bool anchored; bool canCollide; bool isGrounded; float dt;
    bool isTouchingPlayer; bool playerNeedsRespawn; bool isColliding;
};

// C++ Touch-Kill Script: respawn player on contact
extern "C" __declspec(dllexport) void updatePart(PartState* state) {
    // if this object touches the player, respawn player at last checkpoint
    if (state->isTouchingPlayer) {
        state->playerNeedsRespawn = true;
    }
}

