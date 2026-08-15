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
  src/animation/Skeleton.cpp
  src/animation/AnimationClip.cpp
  src/animation/Pose.cpp
  src/animation/Animator.cpp
  src/animation/SkinInfluence.cpp
  src/animation/SkinningPalette.cpp
  src/animation/ModelAnimationBinding.cpp
  engine/assets/json_mini.cpp
  engine/assets/gltf.cpp
  engine/assets/TangentGenerator.cpp
  engine/assets/HdrLoader.cpp
  engine/render/renderer_gl.cpp
  engine/render/IBL.cpp
)
target_include_directories(stw PRIVATE src engine engine/render engine/assets engine/platform engine/core game game/player game/weapons game/world)
target_link_libraries(stw PRIVATE SDL2::SDL2 OpenGL::GL)
target_compile_options(stw PRIVATE -Wall)

add_executable(cache_builder
  tools/cache_builder.cpp
  src/animation/Skeleton.cpp
  src/animation/AnimationClip.cpp
  src/animation/Pose.cpp
  src/animation/SkinInfluence.cpp
  engine/assets/json_mini.cpp
  engine/assets/gltf.cpp
  engine/assets/TangentGenerator.cpp
)
target_include_directories(cache_builder PRIVATE src engine engine/assets)
target_compile_options(cache_builder PRIVATE -Wall)
