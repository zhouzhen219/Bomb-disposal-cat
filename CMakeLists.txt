cmake_minimum_required(VERSION 3.28)
project(CMakeSFMLProject LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 手动指定 SFML 的路径（使用你本地已下载的 SFML 3.0.0）
set(SFML_INCLUDE_DIR "D:/a_computer/SFML/SFML-3.0.0-windows-gcc-14.2.0-mingw-64-bit/SFML-3.0.0/include")
set(SFML_LIB_DIR "D:/a_computer/SFML/SFML-3.0.0-windows-gcc-14.2.0-mingw-64-bit/SFML-3.0.0/lib")

# 添加头文件搜索路径
include_directories(${SFML_INCLUDE_DIR})

# 添加库文件搜索路径
link_directories(${SFML_LIB_DIR})

add_executable(main src/main.cpp)
target_compile_features(main PRIVATE cxx_std_17)

if(WIN32)
  add_custom_command(TARGET main PRE_LINK
    COMMAND cmd /c "taskkill /IM main.exe /F >nul 2>nul & exit /b 0"
    VERBATIM
  )
endif()

# 添加优化标志
if(MSVC)
  target_compile_options(main PRIVATE /O2 /W4)
else()
  target_compile_options(main PRIVATE -O2 -Wall -Wextra)
endif()

# 链接 SFML 库（注意：SFML 3.0 的库文件名通常不带 -d 或 -s 后缀，这里以动态库为例）
target_link_libraries(main PRIVATE
    sfml-graphics
    sfml-window
    sfml-system
    sfml-audio
    sfml-network
)

# 静态链接 libstdc++ 和 libgcc（MinGW 运行时）
if(WIN32 AND (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")))
  target_link_libraries(main PRIVATE
      -static-libgcc
      -static-libstdc++
  )
endif()

enable_testing()
add_test(NAME SmokeTest COMMAND main WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
install(TARGETS main RUNTIME DESTINATION .)
install(DIRECTORY "data/" DESTINATION "data" FILES_MATCHING PATTERN "*")

set(CPACK_PACKAGE_NAME "CMakeSFMLTest")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "HDU")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "CMake-SFML-Test")
set(CPACK_PACKAGE_CONTACT "contact@example.com")
include(CPack)