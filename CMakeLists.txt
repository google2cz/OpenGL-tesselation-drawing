cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(OpenGL-tesselation-drawing)
set(CMAKE_CXX_STANDARD 14)			# apt g++ ignores c++17

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)

include_directories(${GLEW_INCLUDE_DIRS} ${OPENGL_INCLUDE_DIR})

add_executable(main main.cpp)
target_link_libraries(main ${GLEW_LIBRARIES} ${OPENGL_LIBRARIES} ${glfw3_LIBRARIES} glfw)
