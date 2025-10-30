
// ===== 文件概述 =====
// 这个文件实现了简单的纹理图像加载器和渲染器。
// 主要功能：加载各种格式的图像文件，创建OpenGL纹理对象，并提供绑定功能。
// 支持格式：BMP, JPG, PNG, TGA。
// 作者：Yi Kangrui <yikangrui@pku.edu.cn>

// ===== 如何调用你的纹理图片 =====
// 1. 将纹理图片文件放到项目data目录下，例如：data/my_texture.png
// 2. 在代码中加载纹理：
//    TextureImage::Texture &tex = TextureImage::Texture::loadTexture("my_texture", "data/my_texture.png");
//    或者如果文件在data目录且名称匹配：
//    TextureImage::Texture &tex = TextureImage::Texture::loadTexture("my_texture");
//    （会自动尝试添加扩展名）
// 3. 在渲染时绑定纹理：
//    tex.bind(0);  // 绑定到纹理通道0
// 4. 在着色器中使用：
//    uniform sampler2D u_diffuse;  // 在着色器中声明
//    vec4 color = texture(u_diffuse, texCoord);  // 采样纹理

// ===== 文件头和预处理 =====
// #pragma once 确保头文件只被包含一次，避免重复定义。
#pragma once

// ===== 包含必要的头文件 =====
// 标准库头文件，用于输入输出、字符串处理等。
#include <iostream>  // 用于std::cout输出调试信息。

#include <vector>    // 虽然在这个文件中没有直接使用，但可能在其他地方需要。
#include <sstream>   // 字符串流。
#include <stdexcept> // 异常处理。
#include <string>    // 字符串类。
#include <map>       // 映射容器，用于存储纹理名称到纹理对象的映射。

// OpenGL环境头文件，提供GLFW、GLEW等初始化。
#include "gl_env.h"

// STB图像库，用于加载图像文件。
#include <stb_image.h>  // STB库的图像加载功能，支持多种格式。

namespace TextureImage {  // 定义纹理图像处理的命名空间。
    class Texture {  // 纹理类，负责加载和管理单个纹理。
    public:
        typedef std::map<std::string, Texture *> Name2Texture;  // 类型定义：字符串到纹理指针的映射。
        static Name2Texture allTexture;  // 静态成员：存储所有加载的纹理。
        static Texture error;  // 静态成员：错误纹理，当加载失败时返回。

    private:
        bool available;      // 纹理是否可用。
        std::string name;    // 纹理名称。
        std::string filename; // 文件名。
        int width;           // 图像宽度。
        int height;          // 图像高度。
        GLuint tex;          // OpenGL纹理对象ID。

        // ===== 私有构造函数，防止外部直接构造 =====
        // 禁止拷贝构造。
        Texture(const Texture &_copy)
                : Texture() {}

        // 默认构造函数，初始化成员变量。
        Texture()
                : available(false), name(), filename(), width(0), height(0), tex(0) {}

        // 虚析构函数，确保正确清理资源。
        virtual ~Texture() { clear(); }

    public:
        // ===== 公共成员函数 =====
        // clear() 函数：清理纹理资源，重置状态。
        void clear() {
            available = false;  // 标记为不可用。
            name = std::string();  // 清空名称。
            filename = std::string();  // 清空文件名。
            glDeleteTextures(1, &tex);  // 删除OpenGL纹理对象。
            tex = 0;  // 重置纹理ID。
        }

        // testAllSuffix() 函数：尝试不同的文件扩展名，找到存在的文件。
        // 参数：no_suffix_name - 不带扩展名的文件名。
        // 返回：找到的文件名，如果没找到返回空字符串。
        static std::string testAllSuffix(std::string no_suffix_name) {
            const int support_suffix_num = 5;  // 支持的扩展名数量。
            const std::string support_suffix[support_suffix_num] = {  // 支持的图像格式扩展名。
                    ".bmp",  // BMP格式。
                    ".jpg",  // JPEG格式。
                    ".jpeg", // JPEG格式（另一种扩展名）。
                    ".png",  // PNG格式。
                    ".tga"   // TGA格式。
            };

            for (int i = 0; i < support_suffix_num; i++) {  // 循环尝试每个扩展名。
                std::string try_filename = no_suffix_name + support_suffix[i];  // 拼接文件名。
                FILE *ftest = fopen(try_filename.c_str(), "r");  // 尝试打开文件。
                if (ftest) {  // 如果文件存在。
                    fclose(ftest);  // 关闭文件。
                    return try_filename;  // 返回完整文件名。
                }
            }
            return std::string();  // 如果都没找到，返回空字符串。
        }

        // loadTexture() 函数：加载纹理的主要函数。
        // 参数：_name - 纹理名称，_filename - 文件名（可选，如果为空会自动尝试扩展名）。
        // 返回：加载的纹理引用，如果失败返回error。
        static Texture &loadTexture(std::string _name, std::string _filename = std::string()) {
            GLenum gl_error_code = GL_NO_ERROR;  // OpenGL错误代码。
            if ((gl_error_code = glGetError()) != GL_NO_ERROR) {  // 检查之前的OpenGL错误。
                const GLubyte *errString = glewGetErrorString(gl_error_code);  // 获取错误字符串。
                std::cout << "ERROR before loadTexture():" << std::endl;  // 输出错误信息。
                std::cout << errString << std::endl;
            }
            std::cout << _name << "<>" << _filename << std::endl;  // 调试输出：名称和文件名。
            std::cout << "Attempting to load texture from: " << _filename << std::endl;
            if (_filename.empty() || _filename == "") {  // 如果文件名为空。
                _filename = testAllSuffix(_name);  // 尝试添加扩展名。
                if (_filename.empty()) return error;  // 如果没找到文件，返回错误纹理。
            }
            FILE *fi = fopen(_filename.c_str(), "r");  // 再次检查文件是否存在。
            if (fi == NULL) return error;  // 如果文件不存在，返回错误纹理。
            fclose(fi);  // 关闭文件。

            // 尝试插入新纹理到allTexture映射中。
            std::pair<Name2Texture::iterator, bool> insertion =
                    allTexture.insert(Name2Texture::value_type(_name, new Texture()));  // 插入新纹理对象。
            Texture &target = *(insertion.first->second);  // 获取目标纹理引用。
            if (!insertion.second) {  // 如果纹理名称已存在。
                if (target.filename == _filename && target.available) {  // 如果文件名相同且可用。
                    return target;  // 返回现有纹理。
                } else {
                    target.clear();  // 否则清理现有纹理。
                }
            }

            target.name = _name;  // 设置纹理名称。
            target.filename = _filename;  // 设置文件名。

            // 使用STB库加载图像数据。
            stbi_set_flip_vertically_on_load(true);  // 设置图像垂直翻转，因为OpenGL的Y轴方向不同。
            int channels;  // 图像通道数（1=灰度, 3=RGB, 4=RGBA）。
            unsigned char *data =  // 加载图像数据。
                    stbi_load(_filename.c_str(), &target.width, &target.height, &channels, 0);  // 加载图像。
            if (!data) {  // 如果加载失败。
                std::cout << "Failed to load image data for " << _name << std::endl;
                return error;  // 返回错误纹理。
            }
            std::cout << "Loaded texture " << _name << " with " << channels << " channels, size " << target.width << "x" << target.height << std::endl;

            // 根据通道数确定OpenGL格式。
            GLenum format = GL_RGBA;  // 默认RGBA。
            GLenum internalFormat = GL_RGBA;  // 内部格式。
            if (channels == 1) {  // 单通道（灰度）。
                format = GL_RED;
                internalFormat = GL_RED;
            }
            if (channels == 2) {  // 双通道。
                format = GL_RG;
                internalFormat = GL_RG;
            }
            if (channels == 3) {  // 三通道（RGB）。
                format = GL_RGB;
                internalFormat = GL_RGB;
            }

            // 创建OpenGL纹理对象并设置参数。
            glGenTextures(1, &target.tex);  // 生成纹理对象。
            glBindTexture(GL_TEXTURE_2D, target.tex);  // 绑定纹理。
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);  // S轴重复。
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);  // T轴重复。
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);  // 放大过滤：线性。
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);  // 缩小过滤：线性。
            glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, target.width, target.height,  // 上传纹理数据。
                         0, format, GL_UNSIGNED_BYTE, data);  // 内部格式和数据格式根据channels确定。
            glGenerateMipmap(GL_TEXTURE_2D);  // 生成多级渐远纹理。
            glBindTexture(GL_TEXTURE_2D, 0);  // 解绑纹理。

            stbi_image_free(data);  // 释放STB加载的图像数据。

            // 检查OpenGL错误。
            if ((gl_error_code = glGetError()) != GL_NO_ERROR) {  // 如果有错误。
                const GLubyte *errString = glewGetErrorString(gl_error_code);  // 获取错误字符串。
                std::cout << "ERROR in loadTexture() for " << _name << ":" << std::endl;  // 输出错误信息。
                std::cout << "Error code: " << gl_error_code << ", " << errString << std::endl;
                return error;  // 返回错误纹理。
            }

            target.available = true;  // 标记纹理为可用。
            return target;  // 返回加载的纹理。
        }

        // unloadTexture() 函数：卸载纹理，释放内存。
        // 参数：_name - 纹理名称。
        // 返回：是否成功卸载。
        static bool unloadTexture(std::string _name) {
            return allTexture.erase(_name) != 0;  // 从映射中删除，返回是否成功。
        }

        // getTexture() 函数：获取已加载的纹理。
        // 参数：_name - 纹理名称。
        // 返回：纹理引用，如果不存在返回error。
        static Texture &getTexture(const std::string &_name) {
            Name2Texture::iterator find_result = allTexture.find(_name);  // 查找纹理。
            if (find_result == allTexture.end()) return error;  // 如果没找到，返回error。
            return *(find_result->second);  // 返回找到的纹理。
        }

        // bind() 函数：绑定纹理到指定的纹理通道。
        // 参数：textureChannel - 纹理通道（0,1,2...）。
        // 返回：是否成功绑定。
        bool bind(GLenum textureChannel) const {
            if (!available) return false;  // 如果纹理不可用，返回false。
            glActiveTexture(GL_TEXTURE0 + textureChannel);  // 激活纹理通道。
            glBindTexture(GL_TEXTURE_2D, tex);  // 绑定纹理。
            return true;  // 返回true表示成功。
        }
    };  // Texture类结束。

    // ===== 静态成员初始化 =====
    Texture::Name2Texture Texture::allTexture;  // 全局纹理映射初始化。
    Texture Texture::error;  // 错误纹理对象初始化。
}  // TextureImage命名空间结束。
