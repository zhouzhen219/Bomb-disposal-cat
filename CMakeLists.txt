cmake_minimum_required(VERSION 3.28)
project(ExplodingKittens LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ====== 1. 使用项目内相对路径查找 SFML ======
set(SFML_DIR "${CMAKE_SOURCE_DIR}/third_party/SFML-3.0.0/lib/cmake/SFML")
find_package(SFML 3 COMPONENTS Graphics Window System Audio Network REQUIRED)

# ====== 2. 编写目标文件 ======
add_executable(main src/main.cpp)
target_compile_features(main PRIVATE cxx_std_17)

# 优化标志
if(MSVC)
  target_compile_options(main PRIVATE /O2 /W4)
else()
  target_compile_options(main PRIVATE -O2 -Wall -Wextra)
endif()

# 链接 SFML（通过导入的目标，自动关联头文件与库）
target_link_libraries(main PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
    SFML::Audio
    SFML::Network
)

# MinGW 静态链接标准库（减少外部依赖）
if(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")))
    target_link_libraries(main PRIVATE -static-libgcc -static-libstdc++)
endif()

# ====== 3. 复制 SFML DLL 到可执行文件同目录（这样双击就能运行） ======
if(WIN32)
    add_custom_command(TARGET main POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/third_party/SFML-3.0.0/bin"
            "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}"
        COMMENT "Copying SFML DLLs to build/bin/"
    )
endif()

# ====== 以下根据你原有需要保留（测试、安装等） ======
enable_testing()
add_test(NAME SmokeTest COMMAND main WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install(TARGETS main RUNTIME DESTINATION .)
install(DIRECTORY "data/" DESTINATION "data" FILES_MATCHING PATTERN "*")

set(CPACK_PACKAGE_NAME "ExplodingKittens")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "YourName")
include(CPack)