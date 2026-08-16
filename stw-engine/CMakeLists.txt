cmake_minimum_required(VERSION 3.16)
project(stw_engine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(ZLIB QUIET)

add_executable(stw
  src/main.cpp
  src/game/GameRuntime.cpp
  src/animation/Skeleton.cpp
  src/animation/AnimationClip.cpp
  src/animation/Pose.cpp
  src/animation/Animator.cpp
  src/animation/SkinInfluence.cpp
  src/animation/SkinningPalette.cpp
  src/animation/ModelAnimationBinding.cpp
  src/playtest/PlaytestRegistry.cpp
  src/playtest/PlaytestRuntime.cpp
  src/playtest/SkinningPlaytest.cpp
  src/playtest/AnimationPlaytest.cpp
  engine/assets/json_mini.cpp
  engine/assets/gltf.cpp
  engine/assets/TangentGenerator.cpp
  engine/assets/HdrLoader.cpp
  engine/render/FrameCapture.cpp
  engine/render/renderer_gl.cpp
  engine/render/IBL.cpp
)
target_include_directories(stw PRIVATE src engine engine/render engine/assets engine/platform engine/core game game/player game/weapons game/world)
target_compile_definitions(stw PRIVATE
  STW_PLAYTEST_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/playtest/assets"
)
target_link_libraries(stw PRIVATE SDL2::SDL2 OpenGL::GL)
if(ZLIB_FOUND)
  target_link_libraries(stw PRIVATE ZLIB::ZLIB)
  target_compile_definitions(stw PRIVATE STW_HAVE_ZLIB=1)
endif()
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
