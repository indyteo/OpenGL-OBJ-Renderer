cmake_minimum_required(VERSION 3.24)
project(Projet)

set(CMAKE_CXX_STANDARD 14)

find_package(OpenGL REQUIRED)
link_directories(${OPENGL_gl_LIBRARY})

include_directories(../libs/glfw/include)
include_directories(../libs/glew/include)

link_directories(../libs/glfw/lib-mingw-w64)
link_directories(../libs/glew/lib/Release/x64)

set(glm_DIR ../libs/glm/cmake/glm)
find_package(glm REQUIRED)
include_directories(../libs/glm)

include_directories(../common)
add_executable(Projet main.cpp ../common/GLShader.cpp)

target_link_libraries(Projet glfw3 ${OPENGL_gl_LIBRARY} glew32 glm::glm)
