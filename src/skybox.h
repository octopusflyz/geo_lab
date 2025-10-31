#pragma once

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>

#include "texture_image.h"

namespace Skybox {
    class SkyboxRenderer {
    public:
        SkyboxRenderer();
        ~SkyboxRenderer();

        bool initialize(const std::string& hdrTexturePath);
        void render(const glm::mat4& view, const glm::mat4& projection);

    private:
        GLuint shaderProgram;
        GLuint VAO, VBO, EBO;
        TextureImage::Texture* hdrTexture;

        const char* vertexShaderSource = R"(
            #version 330 core
            layout(location = 0) in vec3 in_position;

            out vec3 texCoord;

            uniform mat4 u_view;
            uniform mat4 u_projection;

            void main() {
                texCoord = in_position;
                
                // 移除视图矩阵的平移部分，让天空盒跟随相机旋转但不移动
                mat4 viewWithoutTranslation = mat4(mat3(u_view));
                vec4 pos = u_projection * viewWithoutTranslation * vec4(in_position, 1.0);
                
                gl_Position = pos.xyww; // 使用 w 作为深度值
            }
        )";

        const char* fragmentShaderSource = R"(
            #version 330 core
            in vec3 texCoord;

            out vec4 out_color;

            uniform sampler2D u_hdrTexture;

            const float PI = 3.14159265359;

            void main() {
                vec3 dir = normalize(texCoord);
                
                // 正确的等距柱状投影坐标计算
                float phi = atan(dir.z, dir.x);
                float theta = asin(dir.y);
                
                float u = 0.5 + 0.5 * phi / PI;  // phi从[-PI, PI]映射到[0, 1]
                float v = 0.5 - theta / PI;       // theta从[-PI/2, PI/2]映射到[0, 1]
                // 确保坐标在有效范围内
                u = clamp(u, 0.0, 1.0);
                v = clamp(v, 0.0, 1.0);

                out_color = texture(u_hdrTexture, vec2(u, v));
            }
        )";

        // Cube vertices for skybox (positions only, no normals/texcoords needed)
        const std::vector<GLfloat> vertices = {
            // Front face
            -1.0f,  1.0f, -1.0f,
            -1.0f, -1.0f, -1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            // Back face
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
            // Left face
            -1.0f,  1.0f,  1.0f,
            -1.0f, -1.0f,  1.0f,
            -1.0f, -1.0f, -1.0f,
            -1.0f,  1.0f, -1.0f,
            // Right face
             1.0f,  1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f,
             1.0f,  1.0f, -1.0f,
            // Top face
            -1.0f,  1.0f, -1.0f,
            -1.0f,  1.0f,  1.0f,
             1.0f,  1.0f,  1.0f,
             1.0f,  1.0f, -1.0f,
            // Bottom face
            -1.0f, -1.0f, -1.0f,
            -1.0f, -1.0f,  1.0f,
             1.0f, -1.0f,  1.0f,
             1.0f, -1.0f, -1.0f
        };

        const std::vector<GLuint> indices = {
            // Front face
            0, 1, 2, 2, 3, 0,
            // Back face
            4, 5, 6, 6, 7, 4,
            // Left face
            8, 9, 10, 10, 11, 8,
            // Right face
            12, 13, 14, 14, 15, 12,
            // Top face
            16, 17, 18, 18, 19, 16,
            // Bottom face
            20, 21, 22, 22, 23, 20
        };
    };
}
