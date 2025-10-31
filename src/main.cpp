// Hand Example

#define DIFFUSE_TEXTURE_MAPPING  // 如果定义了这个宏，会启用纹理映射，使用手部纹理贴图。如果没有自己的纹理，可以注释掉。

#include "gl_env.h"  // 包含OpenGL环境设置的头文件，提供GLFW、GLEW等初始化。

#include <cstdlib>  // 标准库，用于exit等。
#include <cstdio>   // 标准库，用于printf等。
#include <config.h> // 配置文件，可能包含数据目录等。

#ifndef M_PI
#define M_PI (3.1415926535897932)  // 定义PI常量，如果没有定义的话。
#endif

#include <iostream>  // 标准输入输出流。
#include <string>    // 字符串处理。
#include <sstream>   // 字符串流解析。

#include "skeletal_mesh.h"  // 骨骼网格相关的头文件，处理模型加载和渲染。

#include <glm/gtc/matrix_transform.hpp>  // GLM库的矩阵变换头文件，用于旋转、平移等变换。
#include <glm/gtc/quaternion.hpp>  // GLM库的四元数头文件，用于四元数操作。

#include "texture_image.h"  // 纹理图像加载器头文件，用于加载和绑定纹理。
#include "skybox.h"  // 天空盒渲染器头文件。

namespace SkeletalAnimation {  // 定义一个命名空间，包含骨骼动画相关的着色器代码。
    const char *vertex_shader_330 =  // 顶点着色器代码，使用GLSL 3.30版本。
            "#version 330 core\n"
            "const int MAX_BONES = 100;\n"  // 最大骨骼数量。
            "uniform mat4 u_bone_transf[MAX_BONES];\n"  // 骨骼变换矩阵的uniform变量，从CPU传递过来。
            "uniform mat4 u_mvp;\n"  // 模型视图投影矩阵。
            "layout(location = 0) in vec3 in_position;\n"  // 输入顶点位置。
            "layout(location = 1) in vec2 in_texcoord;\n"  // 输入纹理坐标。
            "layout(location = 2) in vec3 in_normal;\n"  // 输入法线。
            "layout(location = 3) in ivec4 in_bone_index;\n"  // 输入骨骼索引，每个顶点最多影响4个骨骼。
            "layout(location = 4) in vec4 in_bone_weight;\n"  // 输入骨骼权重。
            "out vec2 pass_texcoord;\n"  // 输出纹理坐标到片段着色器。
            "void main() {\n"
            "    float adjust_factor = 0.0;\n"  // 调整因子，用于归一化权重。
            "    for (int i = 0; i < 4; i++) adjust_factor += in_bone_weight[i] * 0.25;\n"  // 计算调整因子。
            "    mat4 bone_transform = mat4(1.0);\n"  // 初始化骨骼变换矩阵为单位矩阵。
            "    if (adjust_factor > 1e-3) {\n"  // 如果调整因子大于阈值，表示顶点受骨骼影响。
            "        bone_transform -= bone_transform;\n"  // 重置为零矩阵。
            "        for (int i = 0; i < 4; i++)\n"  // 循环4个骨骼。
            "            bone_transform += u_bone_transf[in_bone_index[i]] * in_bone_weight[i] / adjust_factor;\n"  // 根据权重累加变换矩阵。
            "	 }\n"
            "    gl_Position = u_mvp * bone_transform * vec4(in_position, 1.0);\n"  // 计算最终顶点位置。
            "    pass_texcoord = in_texcoord;\n"  // 传递纹理坐标。
            "}\n";

    const char *fragment_shader_330 =  // 片段着色器代码。
            "#version 330 core\n"
            "uniform sampler2D u_basecolor;\n"  // 基础颜色纹理采样器。
            "uniform sampler2D u_normal;\n"  // 法线纹理采样器。
            "uniform sampler2D u_metallic;\n"  // 金属度纹理采样器。
            "uniform sampler2D u_roughness;\n"  // 粗糙度纹理采样器。
            "uniform sampler2D u_ao;\n"  // AO纹理采样器。
            "uniform int texture_mode;\n"  // 纹理模式：1=使用纹理，0=使用漫反射通道。
            "in vec2 pass_texcoord;\n"  // 从顶点着色器接收的纹理坐标。
            "out vec4 out_color;\n"  // 输出颜色。
            "void main() {\n"
            #ifdef DIFFUSE_TEXTURE_MAPPING  // 如果启用了纹理映射。
            "    if (texture_mode == 1) {\n"
            "        vec3 basecolor = texture(u_basecolor, pass_texcoord).xyz;\n"  // 采样基础颜色。
            "        float ao = texture(u_ao, pass_texcoord).r;\n"  // 采样AO（使用红色通道）。
            "        out_color = vec4(basecolor * ao, 1.0);\n"  // 叠加基础颜色和AO。
            "    } else {\n"
            "        out_color = vec4(pass_texcoord, 0.0, 1.0);\n"  // 使用纹理坐标作为颜色，像origin.cpp一样。
            "    }\n"
            #else
            "    out_color = vec4(pass_texcoord, 0.0, 1.0);\n"  // 否则，使用纹理坐标作为颜色（调试用）。
            #endif
            "}\n";
}

static void error_callback(int error, const char *description) {  // GLFW错误回调函数，当GLFW发生错误时调用。
    fprintf(stderr, "Error: %s\n", description);  // 输出错误信息到标准错误流。
}

static int current_action = 10;  // 当前默认动作
static int current_tex = 0;  // 当前纹理：0=mano-hand-cyborg, 1=hand-sculpture, 2=no texture
static bool input_mode = false;  // 是否进入输入模式

// Camera control variables
static glm::vec3 camera_eye = glm::vec3(30.0f, 5.0f, 10.0f);  // 相机位置 (Camera position)
static glm::vec3 camera_center = glm::vec3(0.0f, 5.0f, 0.0f);  // 注视点 (moved further down) (Look at point, moved further down)
static glm::vec3 camera_up = glm::vec3(0.0f, 1.0f, 0.0f);  // 上方向向量 (Up vector)
static bool mouse_dragging = false;  // 是否正在拖拽鼠标 (Is mouse dragging)
static double last_mouse_x = 0.0, last_mouse_y = 0.0;  // 上次鼠标位置 (Last mouse position)
static float camera_distance = glm::length(camera_eye - camera_center);  // 相机距离中心点的距离 (Distance from center)
static float camera_yaw = atan2(camera_eye.x - camera_center.x, camera_eye.z - camera_center.z);  // 水平旋转角度 (Horizontal rotation angle)
static float camera_pitch = asin((camera_eye.y - camera_center.y) / camera_distance);  // 垂直旋转角度 (Vertical rotation angle)
static bool camera_locked = true;  // 是否锁定相机位置 (Lock camera position)
static float camera_max_distance = 50.0f;  // 最大相机距离 (Maximum camera distance)
static glm::quat camera_orientation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // 相机方向四元数 (Camera orientation quaternion)

// Camera interpolation variables
static glm::vec3 camera_pos_A = glm::vec3(30.0f, 5.0f, 10.0f);  // Position A
static glm::quat camera_ori_A = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Orientation A
static glm::vec3 camera_pos_B = glm::vec3(-30.0f, 5.0f, 10.0f);  // Position B
static glm::quat camera_ori_B = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);  // Orientation B
static bool is_interpolating = false;  // 是否正在插值 (Is interpolating)
static float interpolation_time = 0.0f;  // 插值时间 (0.0 to 1.0)
static float interpolation_duration = 2.0f;  // 插值持续时间 (seconds)
static bool interpolate_from_A_to_B = true;  // 插值方向 (true: A to B, false: B to A)

// Camera setting modes
enum CameraMode {
    NORMAL_MODE,    // 正常模式 (Normal mode)
    SET_POINT_A,    // 设置A点模式 (Set point A mode)
    SET_POINT_B     // 设置B点模式 (Set point B mode)
};
static CameraMode camera_mode = NORMAL_MODE;  // 当前相机模式 (Current camera mode)

// 初始化相机方向四元数为初始 yaw 和 pitch 对应的四元数。需要计算初始方向向量，然后用 glm::quatLookAt 或类似方法设置四元数
// Initialize camera orientation quaternion based on initial yaw and pitch
static void initialize_camera_orientation() {
    // Calculate initial direction vector from yaw and pitch
    glm::vec3 direction;
    direction.x = cos(camera_pitch) * sin(camera_yaw);
    direction.y = sin(camera_pitch);
    direction.z = cos(camera_pitch) * cos(camera_yaw);
    direction = glm::normalize(direction);

    // Calculate right and up vectors
    glm::vec3 right = glm::normalize(glm::cross(direction, camera_up));
    glm::vec3 up = glm::normalize(glm::cross(right, direction));

    // Create quaternion from the rotation matrix
    glm::mat3 rotation_matrix(right, up, -direction);  // Note: -direction for look-at
    camera_orientation = glm::quat_cast(rotation_matrix);
}

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {  // 键盘回调函数，处理键盘输入。
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)  // 如果按下ESC键。
        glfwSetWindowShouldClose(window, GLFW_TRUE);  // 设置窗口关闭标志。
    if (key == GLFW_KEY_0 && action == GLFW_PRESS)  // 按0键
        current_action = 0;
    if (key == GLFW_KEY_1 && action == GLFW_PRESS)  // 按1键
        current_action = 1;
    if (key == GLFW_KEY_2 && action == GLFW_PRESS)  // 按2键
        current_action = 2;
    if (key == GLFW_KEY_3 && action == GLFW_PRESS)  // 按3键
        current_action = 3;
    if (key == GLFW_KEY_4 && action == GLFW_PRESS)  // 按4键
        current_action = 4;
    if (key == GLFW_KEY_5 && action == GLFW_PRESS)  // 按5键
        current_action = 5;
    if (key == GLFW_KEY_6 && action == GLFW_PRESS)  // 按6键
        current_action = 6;
    if (key == GLFW_KEY_7 && action == GLFW_PRESS)  // 按7键
        current_action = 7;
    if (key == GLFW_KEY_8 && action == GLFW_PRESS)  // 按8键
        current_action = 8;
    if (key == GLFW_KEY_9 && action == GLFW_PRESS)  // 按9键
        current_action = 9;
    if (key == GLFW_KEY_Q && action == GLFW_PRESS)  // 按Q键切换纹理
        current_tex = (current_tex + 1) % 3;
    if (key == GLFW_KEY_A && action == GLFW_PRESS)  // 按A键进入输入模式
        input_mode = true;
    if (key == GLFW_KEY_W && action == GLFW_PRESS)  // 按W键挥手动作
        current_action = 11;
    if (key == GLFW_KEY_L && action == GLFW_PRESS)  // 按L键锁定/解锁相机
        camera_locked = !camera_locked;
    if (key == GLFW_KEY_F1 && action == GLFW_PRESS) {  // 按F1键设置A点
        camera_mode = SET_POINT_A;
        std::cout << "Set Point A mode: Click to set camera position A" << std::endl;
    }
    if (key == GLFW_KEY_F2 && action == GLFW_PRESS) {  // 按F2键设置B点
        camera_mode = SET_POINT_B;
        std::cout << "Set Point B mode: Click to set camera position B" << std::endl;
    }
    if (key == GLFW_KEY_F3 && action == GLFW_PRESS) {  // 按F3键回到正常模式
        camera_mode = NORMAL_MODE;
        std::cout << "Normal mode" << std::endl;
    }
    if (key == GLFW_KEY_I && action == GLFW_PRESS && camera_mode == NORMAL_MODE) {  // 按I键开始插值A到B
        if (!is_interpolating) {
            is_interpolating = true;
            interpolation_time = 0.0f;
            interpolate_from_A_to_B = true;
            std::cout << "Starting interpolation from A to B" << std::endl;
        }
    }
    if (key == GLFW_KEY_O && action == GLFW_PRESS && camera_mode == NORMAL_MODE) {  // 按O键开始插值B到A
        if (!is_interpolating) {
            is_interpolating = true;
            interpolation_time = 0.0f;
            interpolate_from_A_to_B = false;
            std::cout << "Starting interpolation from B to A" << std::endl;
        }
    }
    // 这里可以添加更多键盘处理逻辑，比如切换动作或调整参数。
}

static void mouse_button_callback(GLFWwindow *window, int button, int action, int mods) {  // 鼠标按钮回调函数。
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            if (camera_mode == SET_POINT_A) {
                // Set current camera position and orientation as point A
                camera_pos_A = camera_eye;
                camera_ori_A = camera_orientation;
                std::cout << "Point A set at position: (" << camera_eye.x << ", " << camera_eye.y << ", " << camera_eye.z << ")" << std::endl;
                camera_mode = NORMAL_MODE;
            } else if (camera_mode == SET_POINT_B) {
                // Set current camera position and orientation as point B
                camera_pos_B = camera_eye;
                camera_ori_B = camera_orientation;
                std::cout << "Point B set at position: (" << camera_eye.x << ", " << camera_eye.y << ", " << camera_eye.z << ")" << std::endl;
                camera_mode = NORMAL_MODE;
            } else {
                // Normal camera control
                mouse_dragging = true;
                glfwGetCursorPos(window, &last_mouse_x, &last_mouse_y);
                // Disable cursor to prevent accidental zoom
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
        } else if (action == GLFW_RELEASE) {
            mouse_dragging = false;
            // Re-enable cursor
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

static void cursor_position_callback(GLFWwindow *window, double xpos, double ypos) {  // 鼠标位置回调函数。
    if (mouse_dragging && !camera_locked) {
        double dx = xpos - last_mouse_x;
        double dy = ypos - last_mouse_y;

        // Calculate rotation quaternions for yaw and pitch
        float sensitivity = 0.005f;
        glm::quat yaw_rotation = glm::angleAxis((float)dx * sensitivity, glm::vec3(0.0f, 1.0f, 0.0f));  // Yaw around world up
        glm::quat pitch_rotation = glm::angleAxis((float)dy * sensitivity, glm::vec3(1.0f, 0.0f, 0.0f));  // Pitch around local right

        // Update camera orientation quaternion
        camera_orientation = yaw_rotation * camera_orientation * pitch_rotation;

        // Normalize to prevent drift
        camera_orientation = glm::normalize(camera_orientation);

        // Update yaw and pitch for compatibility (optional, but useful for clamping)
        glm::vec3 forward = camera_orientation * glm::vec3(0.0f, 0.0f, -1.0f);
        camera_yaw = atan2(forward.x, forward.z);
        camera_pitch = asin(forward.y);

        // Clamp pitch to avoid flipping
        if (camera_pitch > M_PI / 2.0f - 0.1f) camera_pitch = M_PI / 2.0f - 0.1f;
        if (camera_pitch < -M_PI / 2.0f + 0.1f) camera_pitch = -M_PI / 2.0f + 0.1f;

        last_mouse_x = xpos;
        last_mouse_y = ypos;

        // Update camera position based on quaternion
        glm::vec3 direction = camera_orientation * glm::vec3(0.0f, 0.0f, -1.0f);  // Forward direction
        camera_eye = camera_center - direction * camera_distance;  // Note: -direction because camera looks towards center
    }
}

static void scroll_callback(GLFWwindow *window, double xoffset, double yoffset) {  // 鼠标滚轮回调函数。
    if (!camera_locked) {
        camera_distance -= yoffset * 0.5f;  // Zoom in/out
        if (camera_distance < 1.0f) camera_distance = 1.0f;  // Minimum distance
        if (camera_distance > camera_max_distance) camera_distance = camera_max_distance;  // Maximum distance

        // Update camera position based on quaternion
        glm::vec3 direction = camera_orientation * glm::vec3(0.0f, 0.0f, -1.0f);  // Forward direction
        camera_eye = camera_center - direction * camera_distance;  // Note: -direction because camera looks towards center
    }
}

// 复原函数：让所有手指伸直
static void resetFingers(SkeletalMesh::SkeletonModifier &modifier) {
    // 食指
    modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
    modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
    modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
    // 中指
    modifier["middle_proximal_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
    modifier["middle_intermediate_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
    modifier["middle_distal_phalange"] = glm::identity<glm::mat4>();
    // 无名指
    modifier["ring_proximal_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
    modifier["ring_intermediate_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
    modifier["ring_distal_phalange"] = glm::identity<glm::mat4>();
    // 小指
    modifier["pinky_proximal_phalange"] = glm::identity<glm::mat4>();  // 小指伸直
    modifier["pinky_intermediate_phalange"] = glm::identity<glm::mat4>();  // 小指伸直
    modifier["pinky_distal_phalange"] = glm::identity<glm::mat4>();
    // 大拇指
    modifier["thumb_proximal_phalange"] = glm::identity<glm::mat4>();  // 大拇指伸直
    modifier["thumb_intermediate_phalange"] = glm::identity<glm::mat4>();  // 大拇指伸直
    modifier["thumb_distal_phalange"] = glm::identity<glm::mat4>();
}

int main(int argc, char *argv[]) {  // 主函数，程序入口。
    GLFWwindow *window;  // GLFW窗口指针。
    GLuint vertex_shader, fragment_shader, program;  // OpenGL着色器和程序对象。

    // ===== 初始化 GLFW 和窗口 =====
    glfwSetErrorCallback(error_callback);  // 设置GLFW错误回调。

    if (!glfwInit())  // 初始化GLFW库。
        exit(EXIT_FAILURE);  // 如果失败，退出程序。

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);  // 设置OpenGL上下文版本为3.3。
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 使用核心模式。
#ifdef __APPLE__ // for macos
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);  // MacOS需要向前兼容。
#endif

    window = glfwCreateWindow(800, 800, "OpenGL output", NULL, NULL);  // 创建800x800的窗口。
    if (!window) {  // 如果创建失败。
        glfwTerminate();  // 终止GLFW。
        exit(EXIT_FAILURE);
    }

    glfwSetKeyCallback(window, key_callback);  // 设置键盘回调函数。
    glfwSetMouseButtonCallback(window, mouse_button_callback);  // 设置鼠标按钮回调函数。
    glfwSetCursorPosCallback(window, cursor_position_callback);  // 设置鼠标位置回调函数。
    glfwSetScrollCallback(window, scroll_callback);  // 设置鼠标滚轮回调函数。

    // Initialize camera orientation quaternion
    initialize_camera_orientation();

    // Initialize A and B points with current camera state
    camera_pos_A = camera_eye;
    camera_ori_A = camera_orientation;
    camera_pos_B = camera_eye + glm::vec3(-60.0f, 0.0f, 0.0f);  // B point is 60 units left of A
    camera_ori_B = camera_orientation;  // Same orientation for now

    glfwMakeContextCurrent(window);  // 将窗口设置为当前上下文。
    glfwSwapInterval(0);  // 设置垂直同步为0，即不限制帧率。

    if (glewInit() != GLEW_OK)  // 初始化GLEW库。
        exit(EXIT_FAILURE);  // 如果失败，退出。

    // ===== 加载和编译着色器 =====

    vertex_shader = glCreateShader(GL_VERTEX_SHADER);  // 创建顶点着色器对象。
    glShaderSource(vertex_shader, 1, &SkeletalAnimation::vertex_shader_330, NULL);  // 设置着色器源码。
    glCompileShader(vertex_shader);  // 编译顶点着色器。

    fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);  // 创建片段着色器对象。
    glShaderSource(fragment_shader, 1, &SkeletalAnimation::fragment_shader_330, NULL);  // 设置着色器源码。
    glCompileShader(fragment_shader);  // 编译片段着色器。

    program = glCreateProgram();  // 创建着色器程序对象。
    glAttachShader(program, vertex_shader);  // 附加顶点着色器。
    glAttachShader(program, fragment_shader);  // 附加片段着色器。
    glLinkProgram(program);  // 链接程序。

    int linkStatus;  // 链接状态。
    if (glGetProgramiv(program, GL_LINK_STATUS, &linkStatus), linkStatus == GL_FALSE)  // 检查链接是否成功。
        std::cout << "Error occured in glLinkProgram()" << std::endl;  // 如果失败，输出错误。

    // ===== 加载模型 =====
    // 你可以在这里切换加载不同的模型文件来测试纹理
    // 当前加载的是原始的Hand.fbxmano-hand-cyborg
    //SkeletalMesh::Scene &sr = SkeletalMesh::Scene::loadScene("Mano_Hand_Cyborg", DATA_DIR"/Mano_Hand_Cyborg.fbx");  // 加载手部模型场景，从FBX文件中读取。
    // 当前加载的是原始的Hand.fbx
    SkeletalMesh::Scene &sr = SkeletalMesh::Scene::loadScene("Hand", DATA_DIR"/Hand.fbx");  // 加载原始手部模型场景，从FBX文件中读取。

    if (&sr == &SkeletalMesh::Scene::error)  // 如果加载失败。
        std::cout << "Error occured in loadMesh()" << std::endl;  // 输出错误信息。
    else {
        sr.printBoneNames();  // 打印模型中的所有骨骼名称，用于调试。
    }

    // ===== 加载纹理 =====
    // 加载mano-hand-cyborg的各种纹理 (纹理0)
    TextureImage::Texture &manoBaseColorTex = TextureImage::Texture::loadTexture("mano_basecolor", DATA_DIR"/ManoHand_Cyborg_BaseColor.jpeg");
    TextureImage::Texture &manoMetallicTex = TextureImage::Texture::loadTexture("mano_metallic", DATA_DIR"/ManoHand_Cyborg_Metallic.jpeg");
    TextureImage::Texture &manoNormalTex = TextureImage::Texture::loadTexture("mano_normal", DATA_DIR"/ManoHand_Cyborg_Normal.jpeg");

    TextureImage::Texture &manoRoughnessTex = TextureImage::Texture::loadTexture("mano_roughness", DATA_DIR"/ManoHand_Cyborg_Roughness.jpg");
    TextureImage::Texture &manoAoTex = TextureImage::Texture::loadTexture("mano_ao", DATA_DIR"/ManoHand_Cyborg_ao.jpeg");

    // 加载hand-sculpture的各种纹理 (纹理1)
    TextureImage::Texture &handBaseColorTex = TextureImage::Texture::loadTexture("hand_basecolor", DATA_DIR"/hand-sculpture/textures/hand_albedo.jpg");
    TextureImage::Texture &handNormalTex = TextureImage::Texture::loadTexture("hand_normal", DATA_DIR"/hand-sculpture/textures/hand_normal.jpg");
    TextureImage::Texture &handMetallicTex = TextureImage::Texture::loadTexture("hand_metallic", DATA_DIR"/hand-sculpture/textures/hand_metallic.jpg");
    TextureImage::Texture &handRoughnessTex = TextureImage::Texture::loadTexture("hand_roughness", DATA_DIR"/hand-sculpture/textures/hand_roughness.jpg");
    TextureImage::Texture &handAoTex = TextureImage::Texture::loadTexture("hand_ao", DATA_DIR"/hand-sculpture/textures/hand_ao.jpg");

    // ===== 初始化天空盒 =====
    Skybox::SkyboxRenderer skyboxRenderer;
    if (!skyboxRenderer.initialize(DATA_DIR"/table_mountain_2_puresky_4k.exr")) {
        std::cout << "Failed to initialize skybox" << std::endl;
    }


    sr.setShaderInput(program, "in_position", "in_texcoord", "in_normal", "in_bone_index", "in_bone_weight");  // 设置着色器输入属性，与模型数据对应。

    float passed_time;  // 经过的时间，用于动画。
    float last_time = 0.0f;  // 上次时间，用于计算帧间隔。
    SkeletalMesh::SkeletonModifier modifier;  // 骨骼修改器，用于修改骨骼变换。

    glEnable(GL_DEPTH_TEST);  // 启用深度测试，确保正确渲染3D场景。

    // ===== 主渲染循环 =====
    while (!glfwWindowShouldClose(window)) {  // 主渲染循环，直到窗口关闭。
        passed_time = (float) glfwGetTime();  // 获取从程序启动以来经过的时间，用于动画。
        float delta_time = passed_time - last_time;  // 计算帧间隔时间。
        last_time = passed_time;  // 更新上次时间。

        // ===== 交互输入两个数字 =====
        if (input_mode) {
            std::cout << "Please enter two numbers within 10 (separated by space, e.g. 3 4): " << std::flush;
            int a, b;
            if (std::cin >> a >> b) {
                int result = a + b;
                if (result >= 1 && result <= 9) {
                    std::cout << "Calculation result: " << a << " + " << b << " = " << result << ", will display the corresponding gesture." << std::endl;
                    current_action = result;  // 设置当前动作为结果数字。
                    input_mode = false;  // 退出输入模式
                    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // 忽略剩余输入
                } else {
                    std::cout << "Result " << result << " is not in the range 1-9, please re-enter." << std::endl;
                }
            } else {
                std::cout << "Input error, please enter two integers." << std::endl;
                std::cin.clear();  // 清除错误状态
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');  // 忽略剩余输入
            }
        }

        // ===== 更新相机插值 =====
        if (is_interpolating) {
            interpolation_time += delta_time / interpolation_duration;  // Update interpolation time based on frame delta
            if (interpolation_time >= 1.0f) {
                interpolation_time = 1.0f;
                is_interpolating = false;
                std::cout << "Interpolation completed" << std::endl;
            }

            // Interpolate position and orientation
            if (interpolate_from_A_to_B) {
                camera_eye = glm::mix(camera_pos_A, camera_pos_B, interpolation_time);
                camera_orientation = glm::slerp(camera_ori_A, camera_ori_B, interpolation_time);
            } else {
                camera_eye = glm::mix(camera_pos_B, camera_pos_A, interpolation_time);
                camera_orientation = glm::slerp(camera_ori_B, camera_ori_A, interpolation_time);
            }

            // Update yaw and pitch for compatibility
            glm::vec3 forward = camera_orientation * glm::vec3(0.0f, 0.0f, -1.0f);
            camera_yaw = atan2(forward.x, forward.z);
            camera_pitch = asin(forward.y);
        }

        // ===== 更新动画状态 =====
        // --- You may edit below ---  // 以下是作业需要修改的地方，实现手的运动。

        // 先复原所有手指
        resetFingers(modifier);

        // Example: Rotate the hand  // 示例：旋转整个手。
        // * turn around every 4 seconds  // 每4秒转一圈。
        float metacarpals_angle = passed_time * (M_PI / 4.0f);  // 计算旋转角度，passed_time * (PI/4) 意味着每4秒转90度，但由于是连续的，每秒转PI/4弧度，即每4秒转2PI。
        // * target = metacarpals  // 目标是手掌部分（metacarpals）。
        // * rotation axis = (1, 0, 0)  // 旋转轴是X轴。
        modifier["metacarpals"] = glm::rotate(glm::identity<glm::mat4>(), metacarpals_angle, glm::fvec3(1.0, 0.0, 0.0));  // 设置手掌的变换矩阵为绕X轴旋转。

        /**********************************************************************************\
        *
        * To animate fingers, modify modifier["HAND_SECTION"] each frame,  // 要让手指动起来，每帧修改 modifier["手的部分名称"]。
        * where HAND_SECTION can only be one of the bone names in the Hand's Hierarchy.  // HAND_SECTION 只能是手的层次结构中的骨骼名称之一。
        *
        * A virtual hand's structure is like this: (slightly DIFFERENT from the real world)  // 虚拟手的手指结构（与现实略有不同）：
        *    5432 1
        *    ....        1 = thumb           . = fingertip  // 1=大拇指，. = 指尖
        *    |||| .      2 = index finger    | = distal phalange  // 2=食指，| = 远端指节
        *    $$$$ |      3 = middle finger   $ = intermediate phalange  // 3=中指，$ = 中间指节
        *    #### $      4 = ring finger     # = proximal phalange  // 4=无名指，# = 近端指节
        *    OOOO#       5 = pinky           O = metacarpals  // 5=小指，O = 手掌
        *     OOO
        * (Hand in the real world -> https://en.wikipedia.org/wiki/Hand)  // （现实中的手请参考维基百科）
        *
        * From the structure we can infer the Hand's Hierarchy:  // 从结构可以推断出手的层次：
        *	- metacarpals  // 手掌
        *		- thumb_proximal_phalange  // 大拇指近端指节
        *			- thumb_intermediate_phalange  // 大拇指中间指节
        *				- thumb_distal_phalange  // 大拇指远端指节
        *					- thumb_fingertip  // 大拇指尖
        *		- index_proximal_phalange  // 食指...
        *			- index_intermediate_phalange
        *				- index_distal_phalange
        *					- index_fingertip
        *		- middle_proximal_phalange  // 中指...
        *			- middle_intermediate_phalange
        *				- middle_distal_phalange
        *					- middle_fingertip
        *		- ring_proximal_phalange  // 无名指...
        *			- ring_intermediate_phalange
        *				- ring_distal_phalange
        *					- ring_fingertip
        *		- pinky_proximal_phalange  // 小指...
        *			- pinky_intermediate_phalange
        *				- pinky_distal_phalange
        *					- pinky_fingertip
        *
        * Notice that modifier["HAND_SECTION"] is a local transformation matrix,  // 注意 modifier["手的部分"] 是一个局部变换矩阵，
        * where (1, 0, 0) is the bone's direction, and apparently (0, 1, 0) / (0, 0, 1)  // 其中 (1,0,0) 是骨骼的方向，(0,1,0) 和 (0,0,1) 垂直于骨骼。
        * is perpendicular to the bone.  // 特别是 (0,0,1) 是近端关节的主要旋转轴。
        * Particularly, (0, 0, 1) is the rotation axis of the nearer joint.  // 手指第一指节也可沿 (0,1,0) 小范围转动。
        *
        \**********************************************************************************/
        
        


        float angle_90 = M_PI / 2.0f;  // 90度
        float angle_72 = M_PI / 2.5f;  // 75度
        float angle_60 = M_PI / 3.0f;  // 60度
        float angle_45 = M_PI / 4.0f;  // 45度
        float angle_30 = M_PI / 6.0f;  // 30度
        float angle_10 = M_PI / 18.0f;  // 10度弯曲角度

        
        // ===== 让手指依次动起来 =====
        // 如果当前动作是依次弯曲（默认），
        if (current_action == 10) {
            // 使用时间延迟，让每个手指在不同时间开始弯曲，实现依次动作
            float period = 2.4f;  // 每个手指的弯曲周期
            float delay_step = 0.5f;  // 每个手指之间的延迟时间（秒）
            // 食指（最先动）
            float time_index = passed_time;
            float time_in_period_index = fmod(time_index, period);  // 当前周期内的时间，使用fmod取模
            // * angle: 0 -> PI/3 -> 0  // 角度：0 -> PI/3 -> 0，即从0到60度再回到0。
            float angle_index = abs(time_in_period_index / (period * 0.5f) - 1.0f) * (M_PI / 3.0);  // 计算角度，使用abs函数使之先增后减
            modifier["index_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_index, glm::fvec3(0.0, 0.0, 1.0));

            // 中指（延迟0.5秒）
            //逻辑：当程序刚启动时（passed_time < 0.5），time_middle 会是负数，中指不动。
            //当程序运行超过0.5秒时（passed_time >= 0.5），time_middle 变成正数，中指开始动。
            float time_middle = passed_time - delay_step;
            if (time_middle > 0) {
                float time_in_period_middle = fmod(time_middle, period);
                float angle_middle = abs(time_in_period_middle / (period * 0.5f) - 1.0f) * (M_PI / 3.0);
                modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_middle, glm::fvec3(0.0, 0.0, 1.0));
            }

            // 无名指（延迟1秒）
            float time_ring = passed_time - 2 * delay_step;
            if (time_ring > 0) {
                float time_in_period_ring = fmod(time_ring, period);
                float angle_ring = abs(time_in_period_ring / (period * 0.5f) - 1.0f) * (M_PI / 3.0);
                modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_ring, glm::fvec3(0.0, 0.0, 1.0));
            }

            // 小指（延迟1.5秒）
            float time_pinky = passed_time - 3 * delay_step;
            if (time_pinky > 0) {
                float time_in_period_pinky = fmod(time_pinky, period);
                float angle_pinky = abs(time_in_period_pinky / (period * 0.5f) - 1.0f) * (M_PI / 3.0);
                modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_pinky, glm::fvec3(0.0, 0.0, 1.0));
            }

            // 大拇指（延迟2秒，可能用不同的轴）
            float time_thumb = passed_time - 4 * delay_step;
            if (time_thumb > 0) {
                float time_in_period_thumb = fmod(time_thumb, period);
                float angle_thumb = abs(time_in_period_thumb / (period * 0.5f) - 1.0f) * (M_PI / 3.0);
                modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_thumb, glm::fvec3(0.0, 1.0, 0.0));  // 大拇指用Y轴
            }
        }

        // ===== 比心动作 =====
        // 如果当前动作是比心（按0键切换），覆盖上面的动画
        if (current_action == 0) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));  // 大拇指弯曲30度
            modifier["thumb_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_10, glm::fvec3(0.0, 0.0, 1.0));
            //食指
            modifier["index_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_60, glm::fvec3(0.0, 0.0, 1.0));  // 食指弯曲90度
            modifier["index_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_45, glm::fvec3(0.0, 0.0, 1.0));  // 食指中间指节弯曲90度
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));  // 中指弯曲90度
            modifier["middle_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //无名指
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));    // 无名指弯曲90度
            modifier["ring_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //小指
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));   // 小指弯曲90度
            modifier["pinky_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));

        }
        // ===== 比数字1 =====
        if (current_action == 1) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_45, glm::fvec3(0.0, 0.0, 1.0));
            modifier["thumb_distal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));  // 大拇指弯曲60度
            modifier["thumb_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            //食指
            modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));  // 中指弯曲90度
            modifier["middle_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //无名指
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));    // 无名指弯曲90度
            modifier["ring_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //小指
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));   // 小指弯曲90度
            modifier["pinky_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));

        }
        // ===== 比数字2 =====
        if (current_action == 2) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_45, glm::fvec3(0.0, 0.0, 1.0));
            modifier["thumb_distal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));  // 大拇指弯曲60度
            modifier["thumb_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            //食指
            modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            modifier["middle_intermediate_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            //无名指
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));    // 无名指弯曲90度
            modifier["ring_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //小指
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));   // 小指弯曲90度
            modifier["pinky_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));

        }
        // ===== 比数字3 =====
        if (current_action == 3) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_45, glm::fvec3(0.0, 0.0, 1.0));
            modifier["thumb_distal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));  // 大拇指弯曲60度
            modifier["thumb_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            //食指
            modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            modifier["middle_intermediate_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            //无名指
            modifier["ring_proximal_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
            modifier["ring_intermediate_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
            //小指
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));   // 小指弯曲90度
            modifier["pinky_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));

        }
        // ===== 比数字4 =====
        if (current_action == 4) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_45, glm::fvec3(0.0, 0.0, 1.0));
            modifier["thumb_distal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));  // 大拇指弯曲60度
            modifier["thumb_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
             //食指
            modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            modifier["middle_intermediate_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            //无名指
            modifier["ring_proximal_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
            modifier["ring_intermediate_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
            //小指
            modifier["pinky_proximal_phalange"] = glm::identity<glm::mat4>();  // 小指伸直
            modifier["pinky_intermediate_phalange"] = glm::identity<glm::mat4>();  // 小指伸直

        }
        // ===== 比数字5 =====
        if (current_action == 5) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            modifier["thumb_intermediate_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            modifier["thumb_distal_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
             //食指
            modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            modifier["middle_intermediate_phalange"] = glm::identity<glm::mat4>();  // 中指伸直
            //无名指
            modifier["ring_proximal_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
            modifier["ring_intermediate_phalange"] = glm::identity<glm::mat4>();  // 无名指伸直
            //小指
            modifier["pinky_proximal_phalange"] = glm::identity<glm::mat4>();  // 小指伸直
            modifier["pinky_intermediate_phalange"] = glm::identity<glm::mat4>();  // 小指伸直

        }
        // ===== 比数字6 =====
        if (current_action == 6) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            modifier["thumb_intermediate_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            modifier["thumb_distal_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            //食指
            modifier["index_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));  // 食指弯曲90度
            modifier["index_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));  // 中指弯曲90度
            modifier["middle_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //无名指
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));    // 无名指弯曲90度
            modifier["ring_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //小指
            modifier["pinky_proximal_phalange"] = glm::identity<glm::mat4>();  // 小指伸直
            modifier["pinky_intermediate_phalange"] = glm::identity<glm::mat4>();  // 小指伸直
        }
        // ===== 比数字7 =====
        if (current_action == 7) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_10, glm::fvec3(0.0, 0.0, 1.0));
            modifier["thumb_distal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));  // 大拇指弯曲60度
            modifier["thumb_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            //食指
            modifier["index_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_72, glm::fvec3(0.0, 0.0, 1.0));  // 食指弯曲90度
            modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_72, glm::fvec3(0.0, 0.0, 1.0));  // 中指弯曲90度
            modifier["middle_intermediate_phalange"] = glm::identity<glm::mat4>();
            //无名指
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));    // 无名指弯曲90度
            modifier["ring_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //小指
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));   // 小指弯曲90度
            modifier["pinky_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));

        }
        // ===== 比数字8 =====
        if (current_action == 8) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            modifier["thumb_intermediate_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            modifier["thumb_distal_phalange"] = glm::identity<glm::mat4>();  // 拇指伸直
            //食指
            modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_intermediate_phalange"] = glm::identity<glm::mat4>();  // 食指伸直
            modifier["index_distal_phalange"] = glm::identity<glm::mat4>();
            //中指
            modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));  // 中指弯曲90度
            modifier["middle_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //无名指
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));    // 无名指弯曲90度
            modifier["ring_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //小指
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));   // 小指弯曲90度
            modifier["pinky_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));

        }
        // ===== 比数字9 =====
        if (current_action == 9) {
            //拇指
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_45, glm::fvec3(0.0, 0.0, 1.0));
            modifier["thumb_distal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));  // 大拇指弯曲60度
            modifier["thumb_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            //食指
            modifier["index_proximal_phalange"] = glm::identity<glm::mat4>();
            modifier["index_distal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));  // 食指弯曲60度
            modifier["index_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_60, glm::fvec3(0.0, 0.0, 1.0));
            //中指
            modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));  // 中指弯曲90度
            modifier["middle_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //无名指
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));    // 无名指弯曲90度
            modifier["ring_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));
            //小指
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));   // 小指弯曲90度
            modifier["pinky_intermediate_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_90, glm::fvec3(0.0, 0.0, 1.0));

        }

        // ===== 挥手动作 =====
        if (current_action == 11) {
            // 使用时间让手挥动
            float wave_time = passed_time * 2.0f;  // 挥手速度
            float wave_angle = sin(wave_time) * M_PI / 3.0f;  // 左右摆动角度

            // 整个手掌左右摆动，手掌面向屏幕朝外
            modifier["metacarpals"] = glm::rotate(glm::identity<glm::mat4>(), wave_angle, glm::fvec3(0.0, 1.0, 0.0));

            // 手指稍微弯曲，模拟自然挥手姿势
            modifier["index_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            modifier["middle_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            modifier["ring_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            modifier["pinky_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_30, glm::fvec3(0.0, 0.0, 1.0));
            modifier["thumb_proximal_phalange"] = glm::rotate(glm::identity<glm::mat4>(), angle_45, glm::fvec3(0.0, 0.0, 1.0));
        }
        // --- You may edit above ---  // 以上是需要修改的地方。

        // ===== 渲染准备 =====
        float ratio;  // 窗口宽高比。
        int width, height;  // 窗口宽度和高度。

        glfwGetFramebufferSize(window, &width, &height);  // 获取帧缓冲区大小。
        ratio = width / (float) height;  // 计算宽高比。

        glClearColor(0.0, 0.0, 0.0, 1.0);  // 设置清屏颜色为黑色（天空盒背景）。

        glViewport(0, 0, width, height);  // 设置视口。
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);  // 清空颜色和深度缓冲区。

        // ===== 渲染天空盒 =====
        // 禁用深度写入，渲染天空盒
        glDepthMask(GL_FALSE);
        glm::mat4 view_matrix = glm::lookAt(camera_eye, camera_center, camera_up);  // 计算视图矩阵。
        glm::mat4 projection_matrix = glm::perspective(glm::radians(45.0f), ratio, 0.1f, 100.0f);  // 计算投影矩阵。
        skyboxRenderer.render(view_matrix, projection_matrix);
        glDepthMask(GL_TRUE);  // 重新启用深度写入

        // ===== 设置着色器和矩阵 =====

        glUseProgram(program);  // 使用着色器程序。
        // glm::fmat4 mvp = glm::ortho(-12.5f * ratio, 12.5f * ratio, -5.f, 20.f, -20.f, 20.f)  // 设置正交投影矩阵。
        //                  *
        //                  glm::lookAt(glm::fvec3(.0f, .0f, -1.f), glm::fvec3(.0f, .0f, .0f), glm::fvec3(.0f, 1.f, .0f));  // 设置观察矩阵，从(0,0,-1)看向(0,0,0)，上方向Y轴。
        glm::fmat4 mvp = projection_matrix  // 设置透视投影矩阵。
                         *
                         view_matrix;  // 使用之前计算的视图矩阵。
        glUniformMatrix4fv(glGetUniformLocation(program, "u_mvp"), 1, GL_FALSE, (const GLfloat *) &mvp);  // 传递MVP矩阵到着色器。

        // ===== 绑定纹理 =====
        // 根据当前纹理选择绑定纹理
        if (current_tex == 0) {  // 纹理0: mano-hand-cyborg
            if (manoBaseColorTex.bind(0)) {
                glUniform1i(glGetUniformLocation(program, "u_basecolor"), 0);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_basecolor"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (manoNormalTex.bind(1)) {
                glUniform1i(glGetUniformLocation(program, "u_normal"), 1);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_normal"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (manoMetallicTex.bind(2)) {
                glUniform1i(glGetUniformLocation(program, "u_metallic"), 2);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_metallic"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (manoRoughnessTex.bind(3)) {
                glUniform1i(glGetUniformLocation(program, "u_roughness"), 3);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_roughness"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (manoAoTex.bind(4)) {
                glUniform1i(glGetUniformLocation(program, "u_ao"), 4);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_ao"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            glUniform1i(glGetUniformLocation(program, "texture_mode"), 1);  // 设置为使用纹理
        } else if (current_tex == 1) {  // 纹理1: hand-sculpture
            if (handBaseColorTex.bind(0)) {
                glUniform1i(glGetUniformLocation(program, "u_basecolor"), 0);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_basecolor"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (handNormalTex.bind(1)) {
                glUniform1i(glGetUniformLocation(program, "u_normal"), 1);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_normal"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (handMetallicTex.bind(2)) {
                glUniform1i(glGetUniformLocation(program, "u_metallic"), 2);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_metallic"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (handRoughnessTex.bind(3)) {
                glUniform1i(glGetUniformLocation(program, "u_roughness"), 3);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_roughness"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            if (handAoTex.bind(4)) {
                glUniform1i(glGetUniformLocation(program, "u_ao"), 4);
            } else {
                glUniform1i(glGetUniformLocation(program, "u_ao"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            }
            glUniform1i(glGetUniformLocation(program, "texture_mode"), 1);  // 设置为使用纹理
        } else if (current_tex == 2) {  // 纹理2: no texture
            // 解绑所有纹理，使用默认的漫反射通道
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE2);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE3);
            glBindTexture(GL_TEXTURE_2D, 0);
            glActiveTexture(GL_TEXTURE4);
            glBindTexture(GL_TEXTURE_2D, 0);
            glUniform1i(glGetUniformLocation(program, "u_basecolor"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            glUniform1i(glGetUniformLocation(program, "u_normal"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            glUniform1i(glGetUniformLocation(program, "u_metallic"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            glUniform1i(glGetUniformLocation(program, "u_roughness"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            glUniform1i(glGetUniformLocation(program, "u_ao"), SCENE_RESOURCE_SHADER_DIFFUSE_CHANNEL);
            glUniform1i(glGetUniformLocation(program, "texture_mode"), 0);  // 设置为使用漫反射通道
        }

        SkeletalMesh::Scene::SkeletonTransf bonesTransf;  // 骨骼变换数组。
        sr.getSkeletonTransform(bonesTransf, modifier);  // 根据modifier获取骨骼变换。
        if (!bonesTransf.empty())  // 如果有变换。
            glUniformMatrix4fv(glGetUniformLocation(program, "u_bone_transf"), bonesTransf.size(), GL_FALSE,  // 传递骨骼变换到着色器。
                               (float *) bonesTransf.data());
        sr.render();  // 渲染场景。

        glfwSwapBuffers(window);  // 交换缓冲区。
        glfwPollEvents();  // 处理事件。
    }  // 循环结束。

    // ===== 清理资源 =====
    // SkeletalMesh::Scene::unloadScene("Hand");  // 卸载原始手部场景。
    SkeletalMesh::Scene::unloadScene("ManoHand");  // 卸载mano-hand-cyborg场景。
    TextureImage::Texture::unloadTexture("mano_basecolor");  // 卸载纹理。
    TextureImage::Texture::unloadTexture("mano_normal");
    TextureImage::Texture::unloadTexture("mano_metallic");
    TextureImage::Texture::unloadTexture("mano_roughness");
    TextureImage::Texture::unloadTexture("mano_ao");
    TextureImage::Texture::unloadTexture("hand_basecolor");
    TextureImage::Texture::unloadTexture("hand_normal");
    TextureImage::Texture::unloadTexture("hand_metallic");
    TextureImage::Texture::unloadTexture("hand_roughness");
    TextureImage::Texture::unloadTexture("hand_ao");

    glfwDestroyWindow(window);  // 销毁窗口。

    glfwTerminate();  // 终止GLFW。
    exit(EXIT_SUCCESS);  // 退出程序。
}
