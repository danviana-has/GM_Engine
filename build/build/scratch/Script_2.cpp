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

#include <cmath>

extern "C" __declspec(dllexport) void updatePart(PartState* state) {
    // 1. Rotaciona o objeto no eixo Y em 90 graus por segundo
    state->rotation[1] += 90.0f * state->dt;
    if (state->rotation[1] > 360.0f) {
        state->rotation[1] -= 360.0f;
    }

    // 2. Faz o objeto flutuar para cima e para baixo usando o tempo decorrido
    // Guardamos o tempo total acumulado em uma variável estática
    static float totalTime = 0.0f;
    totalTime += state->dt;

    // Amplitude de 1.5 unidades para cima/baixo, frequência suave
    state->position[1] += std::sin(totalTime * 3.0f) * 0.02f;
}
