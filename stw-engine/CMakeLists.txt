cmake_minimum_required(VERSION 3.16)
project(stw_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

add_executable(stw
  src/main.cpp
  src/json_mini.cpp
  src/gltf.cpp
  src/renderer_gl.cpp
)
target_include_directories(stw PRIVATE src)
target_link_libraries(stw PRIVATE SDL2::SDL2 OpenGL::GL)
target_compile_options(stw PRIVATE -Wall)
