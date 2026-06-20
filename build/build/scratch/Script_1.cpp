#include <glm/glm.hpp>
#include <cmath>

struct PartState {
    float position[3];
    float rotation[3];
    float size[3];
    float color[4];
    float velocity[3];
    bool anchored;
    bool canCollide;
    bool isGrounded;
    float dt;
};

// C++ game script compiled with MinGW GCC
extern "C" __declspec(dllexport) void updatePart(PartState* state) {
    // state->dt is the delta time
    // state->position[0,1,2] matches X,Y,Z coordinates
    // state->rotation[0,1,2] matches Euler angles in degrees
    
    // Example: continuous Y-axis rotation
    state->rotation[1] += 45.0f * state->dt;
}

