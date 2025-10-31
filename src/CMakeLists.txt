add_executable(Hand
        gl_env.h
        main.cpp
        skeletal_mesh.h
        texture_image.h
        texture_image.cpp
        skybox.h
        skybox.cpp
        tinyexr_impl.cpp)

target_link_libraries(Hand PRIVATE assimp::assimp glew_s glm stb glfw)
target_include_directories(Hand PRIVATE
        ../third_party/glew/include
        ../third_party/tinyexr  # 添加这一行
        #../third_party/miniz
        ${CMAKE_CURRENT_BINARY_DIR})

target_compile_features(Hand PRIVATE cxx_std_11)

configure_file(config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)