//#include "Image.h"
#include "mesh.h"
#include "texture.h"
#include "camera.h"
#include "shader_uniforms.h"
// Always include window first (because it includes glfw, which includes GL which needs to be included AFTER glew).
// Can't wait for modules to fix this stuff...
#include <framework/disable_all_warnings.h>
DISABLE_WARNINGS_PUSH()
#include <glad/glad.h>
// Include glad before glfw3
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <imgui/imgui.h>
DISABLE_WARNINGS_POP()
#include <framework/shader.h>
#include <framework/window.h>
#include <functional>
#include <iostream>
#include <optional>
#include <vector>

struct PointLight {
    glm::vec3 position;
    glm::vec3 color;
};

Mesh mergeMeshes(const std::vector<Mesh>& meshes)
{
    Mesh merged;
    for (const Mesh& mesh : meshes) {
        glm::uint offset = static_cast<glm::uint>(merged.vertices.size());

        merged.vertices.insert(merged.vertices.end(), mesh.vertices.begin(), mesh.vertices.end());

        for (const glm::uvec3& tri : mesh.triangles)
            merged.triangles.push_back(tri + glm::uvec3(offset));
    }

    if (!meshes.empty())
        merged.material = meshes[0].material; // a GPUMesh holds only one material
    return merged;
}

Mesh makeFloorMesh(float size, float y) {
    Mesh mesh;
    constexpr unsigned int cells = 100;
    for (unsigned int z = 0; z <= cells; ++z) {
        for (unsigned int x = 0; x <= cells; ++x) {
            Vertex vertex{};
            vertex.texCoord = glm::vec2(float(x) / float(cells), float(z) / float(cells));
            vertex.position = glm::vec3((vertex.texCoord.x * 2.0f - 1.0f) * size,
                y, (vertex.texCoord.y * 2.0f - 1.0f) * size);
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
            mesh.vertices.push_back(vertex);
        }
    }
    for (unsigned int z = 0; z < cells; ++z) {
        for (unsigned int x = 0; x < cells; ++x) {
            const unsigned int a = z * (cells + 1) + x;
            const unsigned int b = a + 1;
            const unsigned int c = a + cells + 1;
            const unsigned int d = c + 1;
            mesh.triangles.emplace_back(a, c, b);
            mesh.triangles.emplace_back(b, c, d);
        }
    }
    mesh.material.kd = glm::vec3(0.6f);
    return mesh;
}

GLuint createDepthTexture(int width, int height) {
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex;
}

GLuint createShadowFramebuffer(GLuint depthTexture) {
    GLuint framebuffer;
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return framebuffer;
}

class Application {
public:
    Application()
        : m_window("Final Project", glm::ivec2(1024, 1024), OpenGLVersion::GL41)
        , m_texture(RESOURCE_ROOT "resources/checkerboard.png")
        , m_camera(&m_window, glm::vec3(1.5f, 1.0f, 1.0f), -glm::vec3(1.5f, 1.0f, 1.0f))
    {
        m_window.registerKeyCallback([this](int key, int scancode, int action, int mods) {
            if (action == GLFW_PRESS)
                onKeyPressed(key, mods);
            else if (action == GLFW_RELEASE)
                onKeyReleased(key, mods);
        });
        m_window.registerMouseMoveCallback(std::bind(&Application::onMouseMove, this, std::placeholders::_1));
        m_window.registerMouseButtonCallback([this](int button, int action, int mods) {
            if (action == GLFW_PRESS)
                onMouseClicked(button, mods);
            else if (action == GLFW_RELEASE)
                onMouseReleased(button, mods);
        });

        std::vector<Mesh> cpumeshes = loadMesh(RESOURCE_ROOT "resources/dragon.obj");
        m_meshes.clear();
        m_meshes.emplace_back(mergeMeshes(cpumeshes));
        m_waterMesh.emplace(makeFloorMesh(10.0f, -0.5f));

        m_texShadow = createDepthTexture(1024, 1024);
        m_framebuffer = createShadowFramebuffer(m_texShadow);

        try {
            ShaderBuilder defaultBuilder;
            defaultBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shader_vert.glsl");
            defaultBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shader_frag.glsl");
            m_defaultShader = defaultBuilder.build();

            ShaderBuilder shadowBuilder;
            shadowBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/shadow_vert.glsl");
            shadowBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/shadow_frag.glsl");
            m_shadowShader = shadowBuilder.build();

            ShaderBuilder waterBuilder;
            waterBuilder.addStage(GL_VERTEX_SHADER, RESOURCE_ROOT "shaders/water_vert.glsl");
            waterBuilder.addStage(GL_FRAGMENT_SHADER, RESOURCE_ROOT "shaders/water_frag.glsl");
            m_waterShader = waterBuilder.build();
            // Any new shaders can be added below in similar fashion.
            // ==> Don't forget to reconfigure CMake when you do!
            //     Visual Studio: PROJECT => Generate Cache for ComputerGraphics
            //     VS Code: ctrl + shift + p => CMake: Configure => enter
            // ....
        } catch (ShaderLoadingException e) {
            std::cerr << e.what() << std::endl;
        }
    }

    void update()
    {
        while (!m_window.shouldClose()) {
            // This is your game loop
            // Put your real-time logic and rendering in here
            m_window.updateInput();
            m_camera.updateInput();

            renderGui();

            // Clear the screen
            glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            const glm::ivec2 framebufferSize = m_window.getFrameBufferSize();
            if (framebufferSize.x <= 0 || framebufferSize.y <= 0)
                continue;
            m_projectionMatrix = glm::perspective(glm::radians(80.0f),
                float(framebufferSize.x) / float(framebufferSize.y), 0.1f, 30.0f);
            const glm::mat4 mvpMatrix = m_projectionMatrix * m_camera.viewMatrix() * m_modelMatrix;
            // Normals should be transformed differently than positions (ignoring translations + dealing with scaling):
            // https://paroj.github.io/gltut/Illumination/Tut09%20Normal%20Transformation.html
            const glm::mat3 normalModelMatrix = glm::inverseTranspose(glm::mat3(m_modelMatrix));

            glm::mat4 lightProj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 25.0f);

            glm::mat4 lightView = glm::lookAt(
                m_light.position,                              // eye: the light
                m_light.position + glm::vec3(0.0f, -1.0f, 0.0f), // target: straight down
                glm::vec3(0.0f, 0.0f, -1.0f)           // up vector
            );

            glm::mat4 lightMVP = lightProj * lightView;

            // render shadow
            glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);

            glClearDepth(1.0);
            glClear(GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glViewport(0, 0, 1024, 1024);

            m_shadowShader.bind();
            setUniform(m_shadowShader, "mvp", lightMVP * m_modelMatrix);
            m_meshes[0].draw(m_shadowShader);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, framebufferSize.x, framebufferSize.y);

            for (GPUMesh& mesh : m_meshes) {
                // render main
                m_defaultShader.bind();
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("mvpMatrix"), 1, GL_FALSE, glm::value_ptr(mvpMatrix));
                //Uncomment this line when you use the modelMatrix (or fragmentPosition)
                glUniformMatrix4fv(m_defaultShader.getUniformLocation("modelMatrix"), 1, GL_FALSE, glm::value_ptr(m_modelMatrix));
                glUniformMatrix3fv(m_defaultShader.getUniformLocation("normalModelMatrix"), 1, GL_FALSE, glm::value_ptr(normalModelMatrix));
                glUniform3fv(m_defaultShader.getUniformLocation("lightPos"), 1, glm::value_ptr(m_light.position));
                glUniform3fv(m_defaultShader.getUniformLocation("lightColor"), 1, glm::value_ptr(m_light.color));
                setUniform(m_defaultShader, "lightMVP", lightMVP);
                glActiveTexture(GL_TEXTURE1);
                glBindTexture(GL_TEXTURE_2D, m_texShadow);
                setUniform(m_defaultShader, "texShadow", 1);

                if (mesh.hasTextureCoords()) {
                    m_texture.bind(GL_TEXTURE0);
                    glUniform1i(m_defaultShader.getUniformLocation("colorMap"), 0);
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_TRUE);
                    glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), GL_FALSE);
                } else {
                    glUniform1i(m_defaultShader.getUniformLocation("hasTexCoords"), GL_FALSE);
                    glUniform1i(m_defaultShader.getUniformLocation("useMaterial"), m_useMaterial);
                }
                mesh.draw(m_defaultShader);
            }

            m_waterShader.bind();
            setUniform(m_waterShader, "mvp", mvpMatrix);
            setUniform(m_waterShader, "uTime", float(glfwGetTime()));
            setUniform(m_waterShader, "waveSpeed", &waveSpeed);
            setUniform(m_waterShader, "waveStrength", &waveStrength);
            m_waterMesh->draw(m_waterShader);

            // Processes input and swaps the window buffer
            m_window.swapBuffers();
        }
    }

    // In here you can handle key presses
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyPressed(int key, int mods)
    {
        std::cout << "Key pressed: " << key << std::endl;
    }

    // In here you can handle key releases
    // key - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__keys.html
    // mods - Any modifier keys pressed, like shift or control
    void onKeyReleased(int key, int mods)
    {
        std::cout << "Key released: " << key << std::endl;
    }

    // If the mouse is moved this function will be called with the x, y screen-coordinates of the mouse
    void onMouseMove(const glm::dvec2& cursorPos)
    {
        std::cout << "Mouse at position: " << cursorPos.x << " " << cursorPos.y << std::endl;
    }

    // If one of the mouse buttons is pressed this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseClicked(int button, int mods)
    {
        std::cout << "Pressed mouse button: " << button << std::endl;
    }

    // If one of the mouse buttons is released this function will be called
    // button - Integer that corresponds to numbers in https://www.glfw.org/docs/latest/group__buttons.html
    // mods - Any modifier buttons pressed
    void onMouseReleased(int button, int mods)
    {
        std::cout << "Released mouse button: " << button << std::endl;
    }

private:
    void renderGui()
    {
        ImGui::Begin("Window");
        ImGui::Checkbox("Use material if no texture", &m_useMaterial);
        ImGui::DragFloat("Light x", &m_light.position.x);
        ImGui::DragFloat("Wave Speed", &waveSpeed, 0.05f, 0.0f, 2.0f);
        ImGui::DragFloat("Wave Strength", &waveStrength, 0.01f, 0.0f, 2.0f);
        ImGui::End();
    }

    Window m_window;

    // Shader for default rendering and for depth rendering
    Shader m_defaultShader;
    Shader m_shadowShader;
    Shader m_waterShader;

    GLuint m_texShadow;
    GLuint m_framebuffer;

    std::vector<GPUMesh> m_meshes;
    std::optional<GPUMesh> m_waterMesh;
    Texture m_texture;
    Camera m_camera;
    PointLight m_light{ glm::vec3(0.0, 5.0, 3.0), glm::vec3(1.0, 1.0, 1.0) };
    bool m_useMaterial { true };

    float waveStrength { 1.0f };
    float waveSpeed { 1.0f };

    // Projection and view matrices for you to fill in and use
    glm::mat4 m_projectionMatrix = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 30.0f);
    glm::mat4 m_modelMatrix { 1.0f };
};

int main()
{
    Application app;
    app.update();

    return 0;
}
