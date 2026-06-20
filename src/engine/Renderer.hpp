#pragma once

#include "Common.hpp"
#include "Scene.hpp"
#include "MeshImporter.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>

class Renderer {
public:
    unsigned int shaderProgram = 0;
    unsigned int gridShaderProgram = 0;

    // FBO for rendering to ImGui window
    unsigned int fbo = 0;
    unsigned int textureColorBuffer = 0;
    unsigned int rbo = 0;
    int viewportWidth = 800;
    int viewportHeight = 600;

    // Vertex arrays and buffers for shapes
    unsigned int cubeVAO = 0, cubeVBO = 0;
    unsigned int sphereVAO = 0, sphereVBO = 0, sphereEBO = 0;
    int sphereIndexCount = 0;
    unsigned int cylinderVAO = 0, cylinderVBO = 0, cylinderEBO = 0;
    int cylinderIndexCount = 0;
    unsigned int wedgeVAO = 0, wedgeVBO = 0;
    
    // Grid VAO/VBO
    unsigned int gridVAO = 0, gridVBO = 0;
    int gridVertexCount = 0;

    Renderer() {}

    ~Renderer() {
        cleanup();
    }

    void init() {
        // Compile Shaders
        compileMainShader();
        compileGridShader();

        // Setup Shape Geometries
        setupCube();
        setupSphere();
        setupCylinder();
        setupWedge();
        setupGrid();

        // Setup Framebuffer
        setupFramebuffer(viewportWidth, viewportHeight);
    }

    void cleanup() {
        if (shaderProgram) glDeleteProgram(shaderProgram);
        if (gridShaderProgram) glDeleteProgram(gridShaderProgram);
        if (cubeVAO) { glDeleteVertexArrays(1, &cubeVAO); glDeleteBuffers(1, &cubeVBO); }
        if (sphereVAO) { glDeleteVertexArrays(1, &sphereVAO); glDeleteBuffers(1, &sphereVBO); glDeleteBuffers(1, &sphereEBO); }
        if (cylinderVAO) { glDeleteVertexArrays(1, &cylinderVAO); glDeleteBuffers(1, &cylinderVBO); glDeleteBuffers(1, &cylinderEBO); }
        if (wedgeVAO) { glDeleteVertexArrays(1, &wedgeVAO); glDeleteBuffers(1, &wedgeVBO); }
        if (gridVAO) { glDeleteVertexArrays(1, &gridVAO); glDeleteBuffers(1, &gridVBO); }
        if (fbo) { glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &textureColorBuffer); glDeleteRenderbuffers(1, &rbo); }
    }

    void resizeViewport(int width, int height) {
        if (width <= 0 || height <= 0) return;
        if (width == viewportWidth && height == viewportHeight) return;

        viewportWidth = width;
        viewportHeight = height;

        // Re-setup FBO
        setupFramebuffer(width, height);
    }

    void startRender() {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, viewportWidth, viewportHeight);
        
        // Deep blue/gray clear color
        glClearColor(0.12f, 0.12f, 0.13f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        
        // Transparency support
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    void endRender() {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void renderScene(const Scene& scene, const glm::mat4& view, const glm::mat4& projection, int selectedPartId = -1) {
        // 1. Render Grid Ground
        glUseProgram(gridShaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(gridShaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(gridShaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniform3fv(glGetUniformLocation(gridShaderProgram, "uGridColor"), 1, glm::value_ptr(glm::vec3(0.25f, 0.25f, 0.26f)));

        glBindVertexArray(gridVAO);
        glDrawArrays(GL_LINES, 0, gridVertexCount);
        glBindVertexArray(0);

        // 2. Render Parts
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uView"), 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uProjection"), 1, GL_FALSE, glm::value_ptr(projection));
        
        // Light uniforms
        glUniform3fv(glGetUniformLocation(shaderProgram, "uAmbientColor"), 1, glm::value_ptr(scene.ambientColor));
        glUniform3fv(glGetUniformLocation(shaderProgram, "uSunDirection"), 1, glm::value_ptr(scene.sunDirection));
        
        for (const auto& part : scene.workspace) {
            if (part->isPlayer) {
                // Render detailed humanoid character model with swing walking animation
                float speed = glm::length(glm::vec2(part->velocity.x, part->velocity.z));
                float swingAngle = 0.0f;
                if (speed > 0.5f) {
                    float time = static_cast<float>(glfwGetTime());
                    swingAngle = std::sin(time * 12.0f) * 35.0f;
                }
                
                glm::mat4 baseModel = glm::mat4(1.0f);
                baseModel = glm::translate(baseModel, part->position);
                baseModel = glm::rotate(baseModel, glm::radians(part->rotation.x), glm::vec3(1, 0, 0));
                baseModel = glm::rotate(baseModel, glm::radians(part->rotation.y), glm::vec3(0, 1, 0));
                baseModel = glm::rotate(baseModel, glm::radians(part->rotation.z), glm::vec3(0, 0, 1));
                
                // Colors
                glm::vec4 skinColor = glm::vec4(1.0f, 0.8f, 0.65f, 1.0f);
                glm::vec4 torsoColor = part->color; // Shirt color matches player's core color
                glm::vec4 pantsColor = glm::vec4(0.12f, 0.22f, 0.44f, 1.0f); // Blue jeans
                glm::vec4 eyeColor = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f); // Black eyes
                glm::vec4 mouthColor = glm::vec4(0.6f, 0.2f, 0.2f, 1.0f); // Red mouth
                
                auto drawCharBlock = [&](const glm::mat4& mat, const glm::vec4& col, PartMaterial matType) {
                    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(mat));
                    glUniform4fv(glGetUniformLocation(shaderProgram, "uColor"), 1, glm::value_ptr(col));
                    glUniform1i(glGetUniformLocation(shaderProgram, "uMaterialType"), static_cast<int>(matType));
                    glUniform1f(glGetUniformLocation(shaderProgram, "uReflectance"), 0.0f);
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                };
                
                // 1. Torso
                glm::mat4 torsoMat = glm::translate(baseModel, glm::vec3(0.0f, 0.1f, 0.0f));
                torsoMat = glm::scale(torsoMat, glm::vec3(0.9f, 1.6f, 0.5f));
                drawCharBlock(torsoMat, torsoColor, part->material);
                
                // 2. Head
                glm::mat4 headMat = glm::translate(baseModel, glm::vec3(0.0f, 1.35f, 0.0f));
                headMat = glm::scale(headMat, glm::vec3(0.8f, 0.8f, 0.8f));
                drawCharBlock(headMat, skinColor, PartMaterial::Plastic);
                
                // 3. Eyes
                glm::mat4 leftEye = glm::translate(baseModel, glm::vec3(-0.18f, 1.45f, -0.41f));
                leftEye = glm::scale(leftEye, glm::vec3(0.1f, 0.1f, 0.02f));
                drawCharBlock(leftEye, eyeColor, PartMaterial::Plastic);
                
                glm::mat4 rightEye = glm::translate(baseModel, glm::vec3(0.18f, 1.45f, -0.41f));
                rightEye = glm::scale(rightEye, glm::vec3(0.1f, 0.1f, 0.02f));
                drawCharBlock(rightEye, eyeColor, PartMaterial::Plastic);
                
                // 4. Mouth
                glm::mat4 mouth = glm::translate(baseModel, glm::vec3(0.0f, 1.2f, -0.41f));
                mouth = glm::scale(mouth, glm::vec3(0.2f, 0.08f, 0.02f));
                drawCharBlock(mouth, mouthColor, PartMaterial::Plastic);
                
                // 5. Left Leg
                glm::mat4 leftLeg = glm::translate(baseModel, glm::vec3(-0.25f, -0.7f, 0.0f));
                leftLeg = glm::rotate(leftLeg, glm::radians(swingAngle), glm::vec3(1, 0, 0));
                leftLeg = glm::translate(leftLeg, glm::vec3(0.0f, -0.65f, 0.0f));
                leftLeg = glm::scale(leftLeg, glm::vec3(0.4f, 1.3f, 0.4f));
                drawCharBlock(leftLeg, pantsColor, PartMaterial::Plastic);
                
                // 6. Right Leg
                glm::mat4 rightLeg = glm::translate(baseModel, glm::vec3(0.25f, -0.7f, 0.0f));
                rightLeg = glm::rotate(rightLeg, glm::radians(-swingAngle), glm::vec3(1, 0, 0));
                rightLeg = glm::translate(rightLeg, glm::vec3(0.0f, -0.65f, 0.0f));
                rightLeg = glm::scale(rightLeg, glm::vec3(0.4f, 1.3f, 0.4f));
                drawCharBlock(rightLeg, pantsColor, PartMaterial::Plastic);
                
                // 7. Left Arm
                glm::mat4 leftArm = glm::translate(baseModel, glm::vec3(-0.65f, 0.8f, 0.0f));
                leftArm = glm::rotate(leftArm, glm::radians(-swingAngle), glm::vec3(1, 0, 0));
                leftArm = glm::translate(leftArm, glm::vec3(0.0f, -0.65f, 0.0f));
                leftArm = glm::scale(leftArm, glm::vec3(0.35f, 1.3f, 0.35f));
                drawCharBlock(leftArm, skinColor, PartMaterial::Plastic);
                
                // 8. Right Arm
                glm::mat4 rightArm = glm::translate(baseModel, glm::vec3(0.65f, 0.8f, 0.0f));
                rightArm = glm::rotate(rightArm, glm::radians(swingAngle), glm::vec3(1, 0, 0));
                rightArm = glm::translate(rightArm, glm::vec3(0.0f, -0.65f, 0.0f));
                rightArm = glm::scale(rightArm, glm::vec3(0.35f, 1.3f, 0.35f));
                drawCharBlock(rightArm, skinColor, PartMaterial::Plastic);
                
                continue;
            }

            glm::mat4 model = part->getModelMatrix();
            glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uModel"), 1, GL_FALSE, glm::value_ptr(model));
            
            // Selection highlight effect (make part slightly brighter or outline it)
            glm::vec4 color = part->color;
            if (part->id == selectedPartId) {
                // Flash/highlight selected part
                color.r = std::min(color.r + 0.15f, 1.0f);
                color.g = std::min(color.g + 0.15f, 1.0f);
                color.b = std::min(color.b + 0.25f, 1.0f); // Tint blue
            }
            color.a = 1.0f - part->transparency;
            
            glUniform4fv(glGetUniformLocation(shaderProgram, "uColor"), 1, glm::value_ptr(color));
            glUniform1i(glGetUniformLocation(shaderProgram, "uMaterialType"), static_cast<int>(part->material));
            glUniform1f(glGetUniformLocation(shaderProgram, "uReflectance"), part->reflectance);
            
            // Draw shape
            if (!part->meshPath.empty()) {
                MeshData customMesh = MeshCache::getMesh(part->meshPath);
                if (customMesh.loaded && customMesh.vao != 0) {
                    glBindVertexArray(customMesh.vao);
                    glDrawElements(GL_TRIANGLES, customMesh.indexCount, GL_UNSIGNED_INT, 0);
                } else {
                    glBindVertexArray(cubeVAO);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            } else {
                switch (part->shape) {
                    case PartShape::Block:
                        glBindVertexArray(cubeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                        break;
                    case PartShape::Sphere:
                        glBindVertexArray(sphereVAO);
                        glDrawElements(GL_TRIANGLES, sphereIndexCount, GL_UNSIGNED_INT, 0);
                        break;
                    case PartShape::Cylinder:
                        glBindVertexArray(cylinderVAO);
                        glDrawElements(GL_TRIANGLES, cylinderIndexCount, GL_UNSIGNED_INT, 0);
                        break;
                    case PartShape::Wedge:
                        glBindVertexArray(wedgeVAO);
                        glDrawArrays(GL_TRIANGLES, 0, 36);
                        break;
                }
            }
        }
        glBindVertexArray(0);
    }

private:
    void setupFramebuffer(int width, int height) {
        if (fbo) { glDeleteFramebuffers(1, &fbo); glDeleteTextures(1, &textureColorBuffer); glDeleteRenderbuffers(1, &rbo); }

        glGenFramebuffers(1, &fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);

        // Texture color buffer
        glGenTextures(1, &textureColorBuffer);
        glBindTexture(GL_TEXTURE_2D, textureColorBuffer);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureColorBuffer, 0);

        // Depth & Stencil RBO
        glGenRenderbuffers(1, &rbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            std::cerr << "ERROR::FRAMEBUFFER:: Framebuffer is not complete!" << std::endl;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void compileMainShader() {
        const char* vertexShaderSource = R"glsl(
            #version 330 core
            layout (location = 0) in vec3 aPos;
            layout (location = 1) in vec3 aNormal;

            out vec3 FragPos;
            out vec3 Normal;

            uniform mat4 uModel;
            uniform mat4 uView;
            uniform mat4 uProjection;

            void main() {
                FragPos = vec3(uModel * vec4(aPos, 1.0));
                // Normal matrix to handle non-uniform scaling
                Normal = mat3(transpose(inverse(uModel))) * aNormal;
                
                gl_Position = uProjection * uView * vec4(FragPos, 1.0);
            }
        )glsl";

        const char* fragmentShaderSource = R"glsl(
            #version 330 core
            out vec4 FragColor;

            in vec3 FragPos;
            in vec3 Normal;

            uniform vec4 uColor;
            uniform int uMaterialType; // 0: Plastic, 1: Wood, 2: Metal, 3: Glass, 4: Neon
            uniform float uReflectance;
            uniform vec3 uAmbientColor;
            uniform vec3 uSunDirection; // Direct lighting direction (must be normalized, pointing down)

            void main() {
                vec3 norm = normalize(Normal);
                vec3 lightDir = normalize(-uSunDirection); // Direction from frag to sun
                
                // Emissive neon material (glows, ignores light calculation)
                if (uMaterialType == 4) {
                    FragColor = uColor;
                    return;
                }

                // Diffuse lighting
                float diff = max(dot(norm, lightDir), 0.0);
                vec3 diffuse = diff * vec3(1.0, 0.95, 0.9); // Sun light color (slightly warm)

                // Ambient light
                vec3 ambient = uAmbientColor;

                // Specular reflection based on material
                vec3 viewDir = normalize(vec3(0.0) - FragPos); // Approximate camera position in view space or simple world direction
                vec3 reflectDir = reflect(-lightDir, norm);
                
                float shininess = 32.0;
                float specMultiplier = 0.5;
                if (uMaterialType == 2) { // Metal
                    shininess = 64.0;
                    specMultiplier = 1.2;
                } else if (uMaterialType == 1) { // Wood
                    shininess = 8.0;
                    specMultiplier = 0.1;
                } else if (uMaterialType == 3) { // Glass
                    shininess = 128.0;
                    specMultiplier = 1.5;
                }

                float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
                vec3 specular = specMultiplier * spec * vec3(1.0, 1.0, 1.0) * (uReflectance + 0.1);

                // Combine results
                vec3 lighting = (ambient + diffuse) * uColor.rgb + specular;
                
                FragColor = vec4(lighting, uColor.a);
            }
        )glsl";

        // Vertex Shader
        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        checkShaderCompile(vertexShader, "VERTEX");

        // Fragment Shader
        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        checkShaderCompile(fragmentShader, "FRAGMENT");

        // Program
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);
        checkShaderLink(shaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void compileGridShader() {
        const char* vertexShaderSource = R"glsl(
            #version 330 core
            layout (location = 0) in vec3 aPos;

            uniform mat4 uView;
            uniform mat4 uProjection;

            void main() {
                gl_Position = uProjection * uView * vec4(aPos, 1.0);
            }
        )glsl";

        const char* fragmentShaderSource = R"glsl(
            #version 330 core
            out vec4 FragColor;
            uniform vec3 uGridColor;

            void main() {
                FragColor = vec4(uGridColor, 0.4);
            }
        )glsl";

        unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
        glCompileShader(vertexShader);
        checkShaderCompile(vertexShader, "GRID_VERTEX");

        unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
        glCompileShader(fragmentShader);
        checkShaderCompile(fragmentShader, "GRID_FRAGMENT");

        gridShaderProgram = glCreateProgram();
        glAttachShader(gridShaderProgram, vertexShader);
        glAttachShader(gridShaderProgram, fragmentShader);
        glLinkProgram(gridShaderProgram);
        checkShaderLink(gridShaderProgram);

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
    }

    void checkShaderCompile(unsigned int shader, std::string type) {
        int success;
        char infoLog[1024];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, 1024, NULL, infoLog);
            std::cerr << "ERROR::SHADER_COMPILATION_ERROR of type: " << type << "\n" << infoLog << "\n" << std::endl;
        }
    }

    void checkShaderLink(unsigned int program) {
        int success;
        char infoLog[1024];
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(program, 1024, NULL, infoLog);
            std::cerr << "ERROR::PROGRAM_LINKING_ERROR\n" << infoLog << "\n" << std::endl;
        }
    }

    void setupCube() {
        float vertices[] = {
            // Position           // Normal
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,

             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f
        };

        glGenVertexArrays(1, &cubeVAO);
        glGenBuffers(1, &cubeVBO);

        glBindVertexArray(cubeVAO);

        glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // position attribute
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        // normal attribute
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void setupSphere() {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        const unsigned int X_SEGMENTS = 24;
        const unsigned int Y_SEGMENTS = 24;
        for (unsigned int x = 0; x <= X_SEGMENTS; ++x) {
            for (unsigned int y = 0; y <= Y_SEGMENTS; ++y) {
                float xSegment = (float)x / (float)X_SEGMENTS;
                float ySegment = (float)y / (float)Y_SEGMENTS;
                float xPos = std::cos(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f) * 0.5f;
                float yPos = std::cos(ySegment * 3.14159265f) * 0.5f;
                float zPos = std::sin(xSegment * 2.0f * 3.14159265f) * std::sin(ySegment * 3.14159265f) * 0.5f;

                // Position
                vertices.push_back(xPos);
                vertices.push_back(yPos);
                vertices.push_back(zPos);

                // Normal (for sphere, it's just normalized position direction)
                glm::vec3 norm = glm::normalize(glm::vec3(xPos, yPos, zPos));
                vertices.push_back(norm.x);
                vertices.push_back(norm.y);
                vertices.push_back(norm.z);
            }
        }

        bool oddRow = false;
        for (unsigned int y = 0; y < Y_SEGMENTS; ++y) {
            for (unsigned int x = 0; x < X_SEGMENTS; ++x) {
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x + 1);

                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x);
                indices.push_back(y * (X_SEGMENTS + 1) + x + 1);
                indices.push_back((y + 1) * (X_SEGMENTS + 1) + x + 1);
            }
        }
        sphereIndexCount = static_cast<int>(indices.size());

        glGenVertexArrays(1, &sphereVAO);
        glGenBuffers(1, &sphereVBO);
        glGenBuffers(1, &sphereEBO);

        glBindVertexArray(sphereVAO);

        glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void setupCylinder() {
        std::vector<float> vertices;
        std::vector<unsigned int> indices;

        const int slices = 24;
        float halfHeight = 0.5f;

        // Generate cylinder side vertices
        for (int i = 0; i <= slices; ++i) {
            float theta = (i * 2.0f * 3.14159265f) / slices;
            float c = std::cos(theta) * 0.5f;
            float s = std::sin(theta) * 0.5f;
            
            // Side normals are radial vectors
            glm::vec3 normal = glm::normalize(glm::vec3(c, 0.0f, s));

            // Top vertex
            vertices.push_back(c); vertices.push_back(halfHeight); vertices.push_back(s);
            vertices.push_back(normal.x); vertices.push_back(0.0f); vertices.push_back(normal.z);

            // Bottom vertex
            vertices.push_back(c); vertices.push_back(-halfHeight); vertices.push_back(s);
            vertices.push_back(normal.x); vertices.push_back(0.0f); vertices.push_back(normal.z);
        }

        // Indices for cylinder sides
        for (int i = 0; i < slices; ++i) {
            unsigned int idx = i * 2;
            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 3);

            indices.push_back(idx);
            indices.push_back(idx + 3);
            indices.push_back(idx + 2);
        }

        // We can add top cap and bottom cap
        unsigned int capBaseIdx = static_cast<unsigned int>(vertices.size() / 6);
        
        // Top Cap center
        vertices.push_back(0.0f); vertices.push_back(halfHeight); vertices.push_back(0.0f);
        vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);

        // Top Cap ring
        for (int i = 0; i <= slices; ++i) {
            float theta = (i * 2.0f * 3.14159265f) / slices;
            float c = std::cos(theta) * 0.5f;
            float s = std::sin(theta) * 0.5f;
            vertices.push_back(c); vertices.push_back(halfHeight); vertices.push_back(s);
            vertices.push_back(0.0f); vertices.push_back(1.0f); vertices.push_back(0.0f);
        }
        // Top cap indices
        for (int i = 0; i < slices; ++i) {
            indices.push_back(capBaseIdx);
            indices.push_back(capBaseIdx + i + 1);
            indices.push_back(capBaseIdx + i + 2);
        }

        unsigned int bottomCapBaseIdx = static_cast<unsigned int>(vertices.size() / 6);
        
        // Bottom Cap center
        vertices.push_back(0.0f); vertices.push_back(-halfHeight); vertices.push_back(0.0f);
        vertices.push_back(0.0f); vertices.push_back(-1.0f); vertices.push_back(0.0f);

        // Bottom Cap ring
        for (int i = 0; i <= slices; ++i) {
            float theta = (i * 2.0f * 3.14159265f) / slices;
            float c = std::cos(theta) * 0.5f;
            float s = std::sin(theta) * 0.5f;
            vertices.push_back(c); vertices.push_back(-halfHeight); vertices.push_back(s);
            vertices.push_back(0.0f); vertices.push_back(-1.0f); vertices.push_back(0.0f);
        }
        // Bottom cap indices
        for (int i = 0; i < slices; ++i) {
            indices.push_back(bottomCapBaseIdx);
            indices.push_back(bottomCapBaseIdx + i + 2);
            indices.push_back(bottomCapBaseIdx + i + 1);
        }

        cylinderIndexCount = static_cast<int>(indices.size());

        glGenVertexArrays(1, &cylinderVAO);
        glGenBuffers(1, &cylinderVBO);
        glGenBuffers(1, &cylinderEBO);

        glBindVertexArray(cylinderVAO);

        glBindBuffer(GL_ARRAY_BUFFER, cylinderVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cylinderEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void setupWedge() {
        // Wedge: right triangle prism. Slanted from top-back to bottom-front.
        // Bounds from -0.5 to 0.5
        // Slanted surface normal: normal points up and forward
        glm::vec3 normalSlant = glm::normalize(glm::vec3(0.0f, 1.0f, -1.0f));

        float vertices[] = {
            // Positions          // Normals
            // Bottom face (y = -0.5)
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,

            // Back face (z = 0.5)
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f,  1.0f,

            // Slanted Face (from (x, 0.5, 0.5) to (x, -0.5, -0.5))
            -0.5f,  0.5f,  0.5f,  normalSlant.x, normalSlant.y, normalSlant.z,
             0.5f,  0.5f,  0.5f,  normalSlant.x, normalSlant.y, normalSlant.z,
             0.5f, -0.5f, -0.5f,  normalSlant.x, normalSlant.y, normalSlant.z,
             0.5f, -0.5f, -0.5f,  normalSlant.x, normalSlant.y, normalSlant.z,
            -0.5f, -0.5f, -0.5f,  normalSlant.x, normalSlant.y, normalSlant.z,
            -0.5f,  0.5f,  0.5f,  normalSlant.x, normalSlant.y, normalSlant.z,

            // Left triangle face (x = -0.5)
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, // triangle 2 is empty since it's a triangle face
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f, // filler vertices to maintain 36 count
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,

            // Right triangle face (x = 0.5)
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, // filler
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f, // filler
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f
        };

        glGenVertexArrays(1, &wedgeVAO);
        glGenBuffers(1, &wedgeVBO);

        glBindVertexArray(wedgeVAO);

        glBindBuffer(GL_ARRAY_BUFFER, wedgeVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);

        glBindVertexArray(0);
    }

    void setupGrid() {
        std::vector<float> vertices;
        int gridRange = 100;
        int gridStep = 4; // grid line every 4 units like Roblox

        for (int i = -gridRange; i <= gridRange; i += gridStep) {
            // Lines parallel to Z axis
            vertices.push_back(static_cast<float>(i)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(-gridRange));
            vertices.push_back(static_cast<float>(i)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(gridRange));

            // Lines parallel to X axis
            vertices.push_back(static_cast<float>(-gridRange)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(i));
            vertices.push_back(static_cast<float>(gridRange)); vertices.push_back(0.0f); vertices.push_back(static_cast<float>(i));
        }
        gridVertexCount = static_cast<int>(vertices.size() / 3);

        glGenVertexArrays(1, &gridVAO);
        glGenBuffers(1, &gridVBO);

        glBindVertexArray(gridVAO);
        glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }
};
