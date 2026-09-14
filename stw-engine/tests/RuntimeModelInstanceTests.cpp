#include "animation/AnimationClip.hpp"
#include "animation/Skeleton.hpp"
#include "assets/StwcFormat.hpp"
#include "assets/gltf.h"
#include "render/renderer.h"
#include "runtime/ModelInstance.hpp"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#ifndef STW_TEST_ANIMATED_ASSET
#define STW_TEST_ANIMATED_ASSET "playtest/assets/imported_animation.gltf"
#endif

#ifndef STW_TEST_STATIC_ASSET
#define STW_TEST_STATIC_ASSET "assets/test_scene.gltf"
#endif

namespace stw {
namespace {

static_assert(STWC_VERSION == 3u,
              "T4-A must not change the serialized STWC cache format");

void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

bool Near(float left, float right, float epsilon = 1.0e-4f) {
  return std::fabs(left - right) <= epsilon;
}

bool MatrixNear(const glm::mat4& left,
                const glm::mat4& right,
                float epsilon = 1.0e-4f) {
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      if (!Near(left[column][row], right[column][row], epsilon)) return false;
    }
  }
  return true;
}

StwMesh MakeTriangle(bool skinned) {
  StwMesh mesh;
  mesh.pos = {0.0f, 0.0f, 0.0f,
              1.0f, 0.0f, 0.0f,
              0.0f, 1.0f, 0.0f};
  mesh.nrm = {0.0f, 0.0f, 1.0f,
              0.0f, 0.0f, 1.0f,
              0.0f, 0.0f, 1.0f};
  mesh.uv = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
  mesh.tan = {1.0f, 0.0f, 0.0f, 1.0f,
              1.0f, 0.0f, 0.0f, 1.0f,
              1.0f, 0.0f, 0.0f, 1.0f};
  mesh.idx = {0u, 1u, 2u};
  mesh.bmax[0] = 1.0f;
  mesh.bmax[1] = 1.0f;
  if (skinned) {
    SkinInfluence4 influence;
    influence.joints = {0u, 1u, 0u, 0u};
    influence.weights = glm::vec4(0.5f, 0.5f, 0.0f, 0.0f);
    mesh.skinInfluences.assign(3u, influence);
  }
  return mesh;
}

std::shared_ptr<StwModel> MakeAnimatedAsset() {
  auto model = std::make_shared<StwModel>();
  model->meshes.push_back(MakeTriangle(false));
  model->meshes.push_back(MakeTriangle(true));
  model->meshMat = {-1, 0};
  StwMaterial material;
  material.base[0] = 0.9f;
  material.base[1] = 0.2f;
  model->mats.push_back(material);

  SkeletonBuildInput skeletonInput;
  SkeletonJoint root;
  root.name = "root";
  root.parent = -1;
  skeletonInput.joints.push_back(root);
  SkeletonJoint child;
  child.name = "child";
  child.parent = 0;
  child.bindTranslation = glm::vec3(0.0f, 1.0f, 0.0f);
  child.inverseBindMatrix =
      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
  skeletonInput.joints.push_back(child);

  StwSkin skin;
  skin.name = "test skin";
  skin.jointNodes = {8u, 2u};
  skin.skeletonRootNode = 8;
  std::string error;
  Require(Skeleton::Build(skeletonInput, skin.skeleton, &error), error);
  model->skins.push_back(std::move(skin));
  model->skinnedMeshes.push_back({1u, 0u, 41u});

  AnimationChannel translation;
  translation.targetIndex = 1;
  translation.targetPath = AnimationTarget::Translation;
  translation.interpolation = AnimationInterpolation::Linear;
  translation.keyTimes = {0.0f, 2.0f};
  translation.values = AnimationVec3Values{
      glm::vec3(0.0f, 1.0f, 0.0f),
      glm::vec3(2.0f, 1.0f, 0.0f),
  };
  AnimationClipInput clipInput;
  clipInput.name = "Walk";
  clipInput.channels.push_back(std::move(translation));
  AnimationClip clip;
  Require(AnimationClip::Build(clipInput, clip, &error), error);

  StwAnimation animation;
  animation.name = "Walk";
  animation.sourceAnimationIndex = 0u;
  animation.skinClips.resize(1u);
  animation.skinClips[0] = std::move(clip);
  model->animations.push_back(std::move(animation));
  return model;
}

class FakeRenderer final : public IRenderer {
 public:
  bool Init(void*) override { return true; }
  void BeginFrame() override {}
  void SetViewProj(const float[16]) override {}
  void SetCameraPos(float, float, float) override {}
  void SetLight(const Light&) override {}
  std::uint32_t UploadMesh(const StwMesh&) override { return nextHandle++; }
  void Draw(std::uint32_t mesh,
            const StwMaterial&,
            const float[16]) override {
    ++staticDraws;
    lastMesh = mesh;
  }
  bool DrawWithSkinning(std::uint32_t mesh,
                        const StwMaterial&,
                        const float[16],
                        const SkinningPalette* palette,
                        std::string* error) override {
    if (rejectSkinned) {
      if (error) *error = "deliberate renderer palette rejection";
      return false;
    }
    if (!palette || palette->empty()) {
      if (error) *error = "fake renderer expected a palette";
      return false;
    }
    ++skinnedDraws;
    lastMesh = mesh;
    lastPaletteSize = palette->size();
    if (error) error->clear();
    return true;
  }
  void EndFrame() override {}
  void Shutdown() override {}

  std::uint32_t nextHandle = 0u;
  std::uint32_t lastMesh = kInvalidMeshHandle;
  std::size_t staticDraws = 0u;
  std::size_t skinnedDraws = 0u;
  std::size_t lastPaletteSize = 0u;
  bool rejectSkinned = false;
};

void TestDefaultSetAndRendererPaths() {
  const auto asset = MakeAnimatedAsset();
  std::vector<RuntimeModelInstance> instances;
  std::string error;
  Require(RuntimeModelInstance::CreateDefaultSet(asset, instances, &error), error);
  Require(instances.size() == 2u, "default set did not create two draw instances");

  RuntimeModelInstance* skinned = nullptr;
  RuntimeModelInstance* statik = nullptr;
  for (RuntimeModelInstance& instance : instances) {
    if (instance.skinned()) {
      skinned = &instance;
    } else {
      statik = &instance;
    }
  }
  Require(skinned && statik, "default set did not preserve static/skinned paths");
  Require(skinned->meshIndex() == 1u && skinned->skinIndex() == 0u,
          "runtime guessed mesh or skin indices");
  Require(skinned->sourceNodeIndex() == 41u,
          "runtime lost the source node association");
  Require(skinned->selectedAnimationIndex() == 0u,
          "runtime did not select the first compatible source animation");
  Require(skinned->playbackState() == ModelPlaybackState::Playing,
          "default compatible animation is not playing");
  Require(skinned->palette() && skinned->palette()->size() == 2u,
          "skinned runtime palette was not initialized");
  Require(statik->meshIndex() == 0u, "static mesh mapping changed");
  Require(skinned->asset().get() == statik->asset().get(),
          "instances do not share immutable model data");

  FakeRenderer renderer;
  const std::vector<std::uint32_t> handles{17u, 23u};
  StwMaterial fallback;
  Require(statik->Submit(renderer, handles, fallback, &error), error);
  Require(skinned->Submit(renderer, handles, fallback, &error), error);
  Require(renderer.staticDraws == 1u && renderer.skinnedDraws == 1u,
          "normal submission did not select both renderer paths");
  Require(renderer.lastMesh == 23u && renderer.lastPaletteSize == 2u,
          "skinned submission used the wrong mesh handle or palette");

  renderer.rejectSkinned = true;
  Require(!skinned->Submit(renderer, handles, fallback, &error),
          "renderer palette rejection was ignored");
  Require(error == "deliberate renderer palette rejection",
          "renderer rejection diagnostic was lost");
}

void TestBindPoseFallback() {
  auto asset = MakeAnimatedAsset();
  asset->animations.clear();
  RuntimeModelInstance instance;
  std::string error;
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, instance, &error), error);
  Require(!instance.selectedAnimationIndex(),
          "model without a clip selected a fake animation");
  Require(instance.playbackState() == ModelPlaybackState::BindPose,
          "model without a clip did not remain in bind pose");
  Require(instance.palette() && instance.palette()->size() == 2u,
          "bind-pose model has no valid palette");
  Require(MatrixNear(instance.palette()->matrix(0), glm::mat4(1.0f)) &&
              MatrixNear(instance.palette()->matrix(1), glm::mat4(1.0f)),
          "bind-pose palette is not stable");
  Require(instance.Update(1.0f, &error), error);
  Require(Near(instance.animationTime(), 0.0f),
          "bind-pose update invented animation time");
}

void TestAnimationUpdateAndSelection() {
  const auto asset = MakeAnimatedAsset();
  RuntimeModelInstance instance;
  std::string error;
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, instance, &error), error);
  Require(instance.SetAnimationTime(1.0f, &error), error);
  Require(Near(instance.animationTime(), 1.0f), "animation time was not applied");
  Require(Near(instance.palette()->matrix(1)[3][0], 1.0f),
          "palette did not reflect sampled animation state");
  Require(instance.Update(0.5f, &error), error);
  Require(Near(instance.animationTime(), 1.5f) &&
              Near(instance.palette()->matrix(1)[3][0], 1.5f),
          "normal runtime update did not advance Animator then palette");

  Require(instance.SetPaused(true, &error), error);
  const glm::mat4 pausedPalette = instance.palette()->matrix(1);
  Require(instance.Update(0.25f, &error), error);
  Require(Near(instance.animationTime(), 1.5f) &&
              MatrixNear(instance.palette()->matrix(1), pausedPalette),
          "paused instance changed state");
  Require(instance.SetPaused(false, &error), error);

  const float validTime = instance.animationTime();
  const glm::mat4 validPalette = instance.palette()->matrix(1);
  Require(!instance.SelectAnimation(99u, &error),
          "invalid animation index was accepted");
  Require(Near(instance.animationTime(), validTime) &&
              MatrixNear(instance.palette()->matrix(1), validPalette),
          "invalid selection partially modified the instance");
  Require(!instance.SelectAnimationByName("missing", &error),
          "invalid animation name was accepted");
  Require(Near(instance.animationTime(), validTime) &&
              MatrixNear(instance.palette()->matrix(1), validPalette),
          "invalid named selection partially modified the instance");
  Require(instance.SelectAnimationByName("Walk", &error), error);
  Require(Near(instance.animationTime(), 0.0f),
          "valid named selection did not restart transactionally");
  Require(instance.SelectBindPose(&error), error);
  Require(instance.playbackState() == ModelPlaybackState::BindPose &&
              !instance.selectedAnimationIndex(),
          "bind-pose selection retained animation state");
}

void TestFirstCompatibleAnimationPolicy() {
  auto asset = MakeAnimatedAsset();
  StwAnimation unrelated;
  unrelated.name = "Node-only animation";
  unrelated.sourceAnimationIndex = 0u;
  unrelated.skinClips.resize(asset->skins.size());
  asset->animations[0].sourceAnimationIndex = 1u;
  asset->animations.insert(asset->animations.begin(), std::move(unrelated));

  RuntimeModelInstance instance;
  std::string error;
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, instance, &error), error);
  Require(instance.selectedAnimationIndex() == 1u &&
              instance.animationName() == "Walk",
          "runtime did not choose the first compatible animation by source order");
}

void TestIndependentInstances() {
  const auto asset = MakeAnimatedAsset();
  RuntimeModelInstance first;
  RuntimeModelInstance second;
  std::string error;
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, first, &error), error);
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, second, &error), error);
  Require(first.SetAnimationTime(0.3f, &error), error);
  Require(second.SetAnimationTime(1.7f, &error), error);

  const glm::mat4 secondPalette = second.palette()->matrix(1);
  Require(first.Update(0.2f, &error), error);
  Require(Near(first.animationTime(), 0.5f) &&
              Near(second.animationTime(), 1.7f),
          "instances accidentally share animation time");
  Require(MatrixNear(second.palette()->matrix(1), secondPalette),
          "updating one instance changed another palette");
  Require(first.animator() != second.animator() &&
              first.palette() != second.palette() &&
              &first.animator()->pose() != &second.animator()->pose(),
          "mutable animation state is shared between instances");
  Require(first.asset().get() == second.asset().get(),
          "immutable asset was unnecessarily duplicated");
}

void TestTransactionalFailures() {
  const auto asset = MakeAnimatedAsset();
  RuntimeModelInstance instance;
  std::string error;
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, instance, &error), error);
  Require(instance.SetAnimationTime(0.4f, &error), error);
  const float validTime = instance.animationTime();
  const glm::mat4 validPalette = instance.palette()->matrix(1);
  Require(!instance.Update(-0.1f, &error), "negative delta was accepted");
  Require(!instance.Update(std::numeric_limits<float>::quiet_NaN(), &error),
          "non-finite delta was accepted");
  Require(Near(instance.animationTime(), validTime) &&
              MatrixNear(instance.palette()->matrix(1), validPalette),
          "failed update corrupted the previous pose or palette");

  glm::mat4 invalidTransform(1.0f);
  invalidTransform[0][0] = std::numeric_limits<float>::infinity();
  const glm::mat4 previousTransform = instance.transform();
  Require(!instance.SetTransform(invalidTransform, &error),
          "non-finite instance transform was accepted");
  Require(MatrixNear(instance.transform(), previousTransform),
          "failed transform update modified the instance");

  RuntimeModelInstance output;
  Require(RuntimeModelInstance::CreateStatic(asset, 0u, output, &error), error);
  auto malformed = MakeAnimatedAsset();
  malformed->skinnedMeshes[0].skinIndex = 9u;
  Require(!RuntimeModelInstance::CreateSkinned(malformed, 0u, output, &error),
          "invalid skin reference was accepted");
  Require(output.valid() && !output.skinned() && output.asset().get() == asset.get(),
          "failed creation partially replaced the output instance");

  malformed = MakeAnimatedAsset();
  malformed->animations[0].skinClips.clear();
  RuntimeModelInstance malformedOutput;
  Require(!RuntimeModelInstance::CreateSkinned(malformed, 0u,
                                                malformedOutput, &error),
          "incompatible clip map was accepted");
  Require(!malformedOutput.valid(),
          "failed animation setup partially initialized its output");
}

void TestImportedT2T3Regression() {
  auto asset = std::make_shared<StwModel>();
  std::string error;
  Require(LoadGLTF(STW_TEST_ANIMATED_ASSET, *asset, &error), error);
  Require(asset->skins.size() == 1u && asset->skinnedMeshes.size() == 1u &&
              asset->animations.size() == 1u,
          "T2/T3 imported skin or animation data regressed");
  Require(!asset->meshes[asset->skinnedMeshes[0].meshIndex]
               .skinInfluences.empty(),
          "T2 JOINTS_0/WEIGHTS_0 data was lost");

  RuntimeModelInstance instance;
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, instance, &error), error);
  const glm::mat4 bindPalette = instance.palette()->matrix(1);
  Require(instance.SetAnimationTime(1.0f, &error), error);
  Require(!MatrixNear(instance.palette()->matrix(1), bindPalette),
          "T3 imported clip did not change the production palette");
}

void TestStaticCacheRegression() {
  namespace fs = std::filesystem;
  const auto nonce =
      std::chrono::steady_clock::now().time_since_epoch().count();
  const fs::path directory =
      fs::temp_directory_path() /
      ("stw-runtime-model-tests-" + std::to_string(nonce));
  fs::create_directories(directory);
  struct Cleanup {
    fs::path path;
    ~Cleanup() {
      std::error_code error;
      fs::remove_all(path, error);
    }
  } cleanup{directory};

  const fs::path fixture = directory / "static.gltf";
  fs::copy_file(STW_TEST_STATIC_ASSET, fixture);
  StwModel first;
  StwModel cached;
  std::string error;
  Require(LoadGLTF(fixture.string(), first, &error), error);
  Require(fs::exists(fixture.string() + ".stwc"),
          "static model did not retain its STWC cache path");
  Require(LoadGLTF(fixture.string(), cached, &error), error);
  Require(first.meshes.size() == cached.meshes.size() &&
              cached.skins.empty() && cached.animations.empty(),
          "static cache hit changed the static model contract");

  const auto cachedAsset = std::make_shared<StwModel>(std::move(cached));
  std::vector<RuntimeModelInstance> instances;
  Require(RuntimeModelInstance::CreateDefaultSet(cachedAsset, instances,
                                                  &error),
          error);
  Require(instances.size() == cachedAsset->meshes.size(),
          "static cache model no longer creates static runtime instances");
  for (const RuntimeModelInstance& instance : instances) {
    Require(!instance.skinned(), "static cache model entered the skinned path");
  }
}

}  // namespace
}  // namespace stw

int main() {
  try {
    stw::TestDefaultSetAndRendererPaths();
    stw::TestBindPoseFallback();
    stw::TestAnimationUpdateAndSelection();
    stw::TestFirstCompatibleAnimationPolicy();
    stw::TestIndependentInstances();
    stw::TestTransactionalFailures();
    stw::TestImportedT2T3Regression();
    stw::TestStaticCacheRegression();
  } catch (const std::exception& exception) {
    std::cerr << "RuntimeModelInstanceTests FAILED: " << exception.what()
              << '\n';
    return 1;
  }
  std::cout << "RuntimeModelInstanceTests PASSED\n";
  return 0;
}
