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
    // 1. Definição das coordenadas dos pontos A e B
    // Exemplo: Movendo de X=0 até X=10 (Y e Z ficam parados em 0)
    float ax = 0.0f, ay = 0.0f, az = 0.0f;
    float bx = 10.0f, by = 0.0f, bz = 0.0f;
    
    float velocidade = 3.0f; 
    float margemErro = 0.1f; 
    
    // 2. Controla o sentido do movimento (mantém o valor entre os frames)
    static bool indoParaB = true;
    
    // 3. Define qual é o destino atual
    float alvoX = indoParaB ? bx : ax;
    float alvoY = indoParaB ? by : ay;
    float alvoZ = indoParaB ? bz : az;
    
    // 4. Calcula a distância até o alvo usando Pitágoras 3D
    float dx = alvoX - state->position[0];
    float dy = alvoY - state->position[1];
    float dz = alvoZ - state->position[2];
    float distancia = sqrtf(dx*dx + dy*dy + dz*dz);
    
    // 5. Se chegou perto do destino, inverte a direção para o próximo frame
    if (distancia <= margemErro) {
        indoParaB = !indoParaB;
        return; // Sai da função para recalcular o novo alvo no próximo frame
    }
    
    // 6. Normaliza a direção (impede velocidade infinita ou desaceleração)
    if (distancia > 0.0f) {
        dx /= distancia;
        dy /= distancia;
        dz /= distancia;
    }
    
    // 7. Aplica o movimento nos eixos X, Y e Z da sua struct
    state->position[0] += dx * velocidade * state->dt;
    state->position[1] += dy * velocidade * state->dt;
    state->position[2] += dz * velocidade * state->dt;
}

