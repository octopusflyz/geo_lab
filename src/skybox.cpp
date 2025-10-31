#include "skybox.h"
#include <iostream>


namespace Skybox {
    SkyboxRenderer::SkyboxRenderer()
        : shaderProgram(0), VAO(0), VBO(0), EBO(0), hdrTexture(nullptr) {
    }

    SkyboxRenderer::~SkyboxRenderer() {
        if (shaderProgram) {
            glDeleteProgram(shaderProgram);
        }
        if (VAO) {
            glDeleteVertexArrays(1, &VAO);
        }
        if (VBO) {
            glDeleteBuffers(1, &VBO);
        }
        if (EBO) {
            glDeleteBuffers(1, &EBO);
        }
    }

    bool SkyboxRenderer::initialize(const std::string& hdrTexturePath) {
        // Load HDR texture
        hdrTexture = &TextureImage::Texture::loadHDRTexture("skybox_hdr", hdrTexturePath);
        if (!hdrTexture->bind(0)) {
            std::cout << "Failed to load HDR texture for skybox" << std::endl;
            return false;
        }

        // Create and compile shaders
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
        glCompileShader(vertexShader);

        GLint success;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
            std::cout << "Vertex shader compilation failed: " << infoLog << std::endl;
            return false;
        }

        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
        glCompileShader(fragmentShader);

        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
            std::cout << "Fragment shader compilation failed: " << infoLog << std::endl;
            return false;
        }

        // Link shaders
        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vertexShader);
        glAttachShader(shaderProgram, fragmentShader);
        glLinkProgram(shaderProgram);

        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
            std::cout << "Shader program linking failed: " << infoLog << std::endl;
            return false;
        }

        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);

        // Set up VAO, VBO, EBO
        glGenVertexArrays(1, &VAO);
        glGenBuffers(1, &VBO);
        glGenBuffers(1, &EBO);

        glBindVertexArray(VAO);

        glBindBuffer(GL_ARRAY_BUFFER, VBO);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(GLfloat), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);

        return true;
    }

//     void SkyboxRenderer::render(const glm::mat4& view, const glm::mat4& projection) {
//         if (!shaderProgram || !hdrTexture) return;

//         // Remove translation from view matrix for skybox
//         glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));

//         glUseProgram(shaderProgram);

//         // Set uniforms
//         glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
//         glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_projection"), 1, GL_FALSE, glm::value_ptr(projection));

//         // Bind HDR texture
//         hdrTexture->bind(0);
//         glUniform1i(glGetUniformLocation(shaderProgram, "u_hdrTexture"), 0);

//         // Render skybox
//         glBindVertexArray(VAO);
//         glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
//         glBindVertexArray(0);

//         glUseProgram(0);
//     }
// }

    void SkyboxRenderer::render(const glm::mat4& view, const glm::mat4& projection) {
        if (!shaderProgram) return;

        // 设置深度测试为GL_LEQUAL
        glDepthFunc(GL_LEQUAL);
        
        // Remove translation from view matrix for skybox
        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));

        glUseProgram(shaderProgram);

        // Set uniforms
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_view"), 1, GL_FALSE, glm::value_ptr(viewNoTranslation));
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "u_projection"), 1, GL_FALSE, glm::value_ptr(projection));

        // 绑定纹理 - 使用正确的纹理名称
        TextureImage::Texture::getTexture("skybox_hdr").bind(0);
        glUniform1i(glGetUniformLocation(shaderProgram, "u_hdrTexture"), 0);

        // Render skybox
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

        glUseProgram(0);
        
        // 恢复深度测试
        glDepthFunc(GL_LESS);
    }

}