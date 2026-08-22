#include "game/CharacterAnimationController.hpp"

#include "animation/AnimationClip.hpp"
#include "assets/StwcFormat.hpp"
#include "assets/gltf.h"
#include "runtime/ModelInstance.hpp"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace stw {
namespace {

static_assert(STWC_VERSION == 3u,
              "T4-B must not change the serialized STWC cache format");

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

glm::vec3 SkinVertex(const StwMesh& mesh,
                     std::size_t vertexIndex,
                     const SkinningPalette& palette) {
  const glm::vec4 source(mesh.pos[vertexIndex * 3u],
                         mesh.pos[vertexIndex * 3u + 1u],
                         mesh.pos[vertexIndex * 3u + 2u], 1.0f);
  const SkinInfluence4& influence = mesh.skinInfluences[vertexIndex];
  const float weights[4] = {
      influence.weights.x,
      influence.weights.y,
      influence.weights.z,
      influence.weights.w,
  };
  glm::vec4 result(0.0f);
  for (std::size_t component = 0; component < 4u; ++component) {
    if (weights[component] <= 0.0f) continue;
    result += weights[component] *
              (palette.matrix(influence.joints[component]) * source);
  }
  return glm::vec3(result);
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

AnimationClip MakeClip(const std::string& name,
                       float duration,
                       float angleRadians) {
  AnimationChannel rotation;
  rotation.targetIndex = 1;
  rotation.targetPath = AnimationTarget::Rotation;
  rotation.interpolation = AnimationInterpolation::Linear;
  rotation.keyTimes = {0.0f, duration};
  rotation.values = AnimationQuatValues{
      glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
      glm::angleAxis(angleRadians, glm::vec3(0.0f, 0.0f, 1.0f)),
  };
  AnimationClipInput input;
  input.name = name;
  input.channels.push_back(std::move(rotation));
  AnimationClip clip;
  std::string error;
  Require(AnimationClip::Build(input, clip, &error), error);
  return clip;
}

void AddAnimation(StwModel& model,
                  const std::string& name,
                  float duration,
                  float angleRadians) {
  StwAnimation animation;
  animation.name = name;
  animation.sourceAnimationIndex =
      static_cast<std::uint32_t>(model.animations.size());
  animation.skinClips.resize(model.skins.size());
  animation.skinClips[0] = MakeClip(name, duration, angleRadians);
  model.animations.push_back(std::move(animation));
}

std::shared_ptr<StwModel> MakeAsset(bool includeIdle = true,
                                    bool includeMove = true,
                                    bool includeFire = true) {
  auto model = std::make_shared<StwModel>();
  model->meshes.push_back(MakeTriangle(false));
  model->meshes.push_back(MakeTriangle(true));
  model->meshMat = {-1, -1};

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
  skin.name = "character skin";
  skin.jointNodes = {8u, 2u};
  skin.skeletonRootNode = 8;
  std::string error;
  Require(Skeleton::Build(skeletonInput, skin.skeleton, &error), error);
  model->skins.push_back(std::move(skin));
  model->skinnedMeshes.push_back({1u, 0u, 41u});

  // Deliberately vary spelling so semantic, case-insensitive mapping is tested.
  if (includeIdle) AddAnimation(*model, "IDLE", 2.0f, 0.08f);
  if (includeMove) AddAnimation(*model, "Player Walk Cycle", 1.0f, 0.7f);
  if (includeFire) AddAnimation(*model, "attack", 0.25f, -1.1f);
  return model;
}

RuntimeModelInstance MakeInstance(const std::shared_ptr<StwModel>& asset) {
  RuntimeModelInstance instance;
  std::string error;
  Require(RuntimeModelInstance::CreateSkinned(asset, 0u, instance, &error),
          error);
  return instance;
}

CharacterAnimationController MakeController(RuntimeModelInstance& instance) {
  CharacterAnimationController controller;
  std::string error;
  Require(CharacterAnimationController::Create(instance, controller, &error),
          error);
  return controller;
}

void TestIdleMoveAndSameStateStability() {
  const auto asset = MakeAsset();
  RuntimeModelInstance instance = MakeInstance(asset);
  CharacterAnimationController controller = MakeController(instance);
  std::string error;

  Require(controller.state() == CharacterAnimationState::Idle,
          "no movement did not select Idle");
  Require(controller.Update({false, false}, instance, &error), error);
  Require(instance.selectedAnimationIndex() == controller.clips().idle &&
              instance.looping(),
          "Idle did not select its looping clip");

  Require(controller.Update({true, false}, instance, &error), error);
  Require(controller.state() == CharacterAnimationState::Move &&
              instance.selectedAnimationIndex() == controller.clips().move &&
              instance.looping(),
          "movement did not select its looping Move clip");
  Require(instance.Update(0.35f, &error), error);
  const float moveTime = instance.animationTime();
  const glm::mat4 movePalette = instance.palette()->matrix(1);
  Require(controller.Update({true, false}, instance, &error), error);
  Require(Near(instance.animationTime(), moveTime) &&
              MatrixNear(instance.palette()->matrix(1), movePalette),
          "repeated Move state restarted or resampled the clip");

  Require(controller.Update({false, false}, instance, &error), error);
  Require(controller.state() == CharacterAnimationState::Idle &&
              instance.selectedAnimationIndex() == controller.clips().idle &&
              instance.looping() && Near(instance.animationTime(), 0.0f),
          "Move to Idle transition failed");
}

void TestFirePriorityAndCompletion() {
  const auto asset = MakeAsset();
  RuntimeModelInstance instance = MakeInstance(asset);
  CharacterAnimationController controller = MakeController(instance);
  std::string error;

  Require(controller.Update({true, false}, instance, &error), error);
  Require(controller.Update({true, true}, instance, &error), error);
  Require(controller.state() == CharacterAnimationState::Fire &&
              instance.selectedAnimationIndex() == controller.clips().fire &&
              !instance.looping(),
          "Fire did not override moving locomotion as a one-shot");
  Require(instance.Update(0.1f, &error), error);
  const float fireTime = instance.animationTime();
  Require(controller.Update({true, false}, instance, &error), error);
  Require(Near(instance.animationTime(), fireTime),
          "Fire restarted without a new gameplay fire event");
  Require(controller.Update({true, true}, instance, &error), error);
  Require(Near(instance.animationTime(), 0.0f),
          "a new gameplay fire event did not restart the Fire one-shot");
  Require(instance.Update(instance.animationDuration(), &error), error);
  Require(controller.Update({true, false}, instance, &error), error);
  Require(controller.state() == CharacterAnimationState::Move &&
              instance.selectedAnimationIndex() == controller.clips().move,
          "completed Fire did not return to Move");

  Require(controller.Update({false, true}, instance, &error), error);
  Require(controller.state() == CharacterAnimationState::Fire,
          "stationary Fire did not override locomotion");
  Require(instance.Update(instance.animationDuration(), &error), error);
  Require(controller.Update({false, false}, instance, &error), error);
  Require(controller.state() == CharacterAnimationState::Idle &&
              instance.selectedAnimationIndex() == controller.clips().idle,
          "completed stationary Fire did not return to Idle");
}

void TestMissingClipPolicies() {
  {
    const auto asset = MakeAsset(true, true, false);
    RuntimeModelInstance instance = MakeInstance(asset);
    CharacterAnimationController controller = MakeController(instance);
    std::string error;
    Require(controller.Update({true, false}, instance, &error), error);
    Require(instance.Update(0.2f, &error), error);
    const std::optional<std::size_t> selected =
        instance.selectedAnimationIndex();
    const float time = instance.animationTime();
    const glm::mat4 palette = instance.palette()->matrix(1);
    Require(controller.Update({true, true}, instance, &error), error);
    Require(controller.state() == CharacterAnimationState::Move &&
                instance.selectedAnimationIndex() == selected &&
                Near(instance.animationTime(), time) &&
                MatrixNear(instance.palette()->matrix(1), palette) &&
                !controller.lastDiagnostic().empty(),
            "missing Fire clip did not retain deterministic Move state");
  }
  {
    const auto asset = MakeAsset(true, false, true);
    RuntimeModelInstance instance = MakeInstance(asset);
    CharacterAnimationController controller = MakeController(instance);
    std::string error;
    Require(instance.Update(0.2f, &error), error);
    const std::optional<std::size_t> selected =
        instance.selectedAnimationIndex();
    const float time = instance.animationTime();
    const glm::mat4 palette = instance.palette()->matrix(1);
    Require(controller.Update({true, false}, instance, &error), error);
    Require(controller.state() == CharacterAnimationState::Move &&
                instance.selectedAnimationIndex() == selected &&
                Near(instance.animationTime(), time) &&
                MatrixNear(instance.palette()->matrix(1), palette) &&
                !controller.lastDiagnostic().empty(),
            "missing Move clip corrupted the previous valid animation state");
  }
}

void TestIndependentInstances() {
  const auto asset = MakeAsset();
  RuntimeModelInstance idle = MakeInstance(asset);
  RuntimeModelInstance moving = MakeInstance(asset);
  CharacterAnimationController idleController = MakeController(idle);
  CharacterAnimationController moveController = MakeController(moving);
  std::string error;

  Require(moveController.Update({true, false}, moving, &error), error);
  Require(moving.Update(0.4f, &error), error);
  Require(idleController.state() == CharacterAnimationState::Idle &&
              moveController.state() == CharacterAnimationState::Move &&
              idle.selectedAnimationIndex() != moving.selectedAnimationIndex() &&
              Near(idle.animationTime(), 0.0f) &&
              Near(moving.animationTime(), 0.4f) &&
              idle.animator() != moving.animator() &&
              idle.palette() != moving.palette(),
          "two gameplay entities shared mutable animation state");
}

void TestTransactionalTransitionFailure() {
  const auto asset = MakeAsset();
  RuntimeModelInstance instance = MakeInstance(asset);
  CharacterAnimationController controller = MakeController(instance);
  std::string error;
  Require(instance.Update(0.2f, &error), error);
  const std::optional<std::size_t> selected = instance.selectedAnimationIndex();
  const float time = instance.animationTime();
  const glm::mat4 palette = instance.palette()->matrix(1);
  const CharacterAnimationState state = controller.state();

  const std::size_t moveIndex = *controller.clips().move;
  asset->animations[moveIndex].skinClips[0].reset();
  Require(!controller.Update({true, false}, instance, &error),
          "invalid compatible animation transition was accepted");
  Require(controller.state() == state &&
              instance.selectedAnimationIndex() == selected &&
              Near(instance.animationTime(), time) &&
              MatrixNear(instance.palette()->matrix(1), palette),
          "failed transition corrupted controller, pose, or palette state");
}

void TestStaticRegression() {
  const auto asset = MakeAsset();
  RuntimeModelInstance instance;
  std::string error;
  Require(RuntimeModelInstance::CreateStatic(asset, 0u, instance, &error),
          error);
  CharacterAnimationController controller;
  Require(!CharacterAnimationController::Create(instance, controller, &error),
          "static model accepted animated gameplay state");
  Require(instance.valid() && !instance.skinned() && !instance.animator() &&
              !instance.palette() && !instance.selectedAnimationIndex(),
          "static model allocated or retained animation runtime state");
  Require(instance.Update(1.0f, &error), error);
}

void TestImportedGameplayFixture() {
  auto asset = std::make_shared<StwModel>();
  std::string error;
  Require(LoadGLTF(STW_TEST_GAMEPLAY_ASSET, *asset, &error), error);
  Require(asset->skins.size() == 1u && asset->skinnedMeshes.size() == 1u &&
              asset->animations.size() == 3u,
          "gameplay fixture did not import one skin and three animations");
  RuntimeModelInstance instance = MakeInstance(asset);
  CharacterAnimationController controller = MakeController(instance);
  Require(controller.clips().idle && controller.clips().move &&
              controller.clips().fire,
          "imported Idle/Move/Fire clips were not resolved");
  Require(controller.Update({true, true}, instance, &error), error);
  Require(controller.state() == CharacterAnimationState::Fire &&
              instance.animationName() == "Fire" && !instance.looping(),
          "imported fixture did not drive the production controller");
}

void TestImportedGameplayFixtureDeformsGeometry() {
  auto asset = std::make_shared<StwModel>();
  std::string error;
  Require(LoadGLTF(STW_TEST_GAMEPLAY_ASSET, *asset, &error), error);
  RuntimeModelInstance instance = MakeInstance(asset);
  CharacterAnimationController controller = MakeController(instance);
  Require(controller.Update({true, false}, instance, &error), error);
  Require(instance.SetAnimationTime(1.0f, &error), error);

  const StwMesh& mesh = asset->meshes[instance.meshIndex()];
  Require(!mesh.skinInfluences.empty() &&
              mesh.skinInfluences.size() == mesh.pos.size() / 3u &&
              instance.palette(),
          "gameplay fixture has no renderable skin influence data");
  std::vector<glm::vec3> moveVertices;
  moveVertices.reserve(mesh.skinInfluences.size());
  bool moveDeformsMesh = false;
  for (std::size_t vertex = 0; vertex < mesh.skinInfluences.size(); ++vertex) {
    const glm::vec3 source(mesh.pos[vertex * 3u], mesh.pos[vertex * 3u + 1u],
                           mesh.pos[vertex * 3u + 2u]);
    const glm::vec3 deformed = SkinVertex(mesh, vertex, *instance.palette());
    moveVertices.push_back(deformed);
    if (glm::length(deformed - source) > 0.05f) moveDeformsMesh = true;
  }
  Require(moveDeformsMesh,
          "imported Move palette does not visibly deform fixture geometry");

  Require(controller.Update({true, true}, instance, &error), error);
  Require(instance.SetAnimationTime(0.12f, &error), error);
  bool fireDiffersFromMove = false;
  for (std::size_t vertex = 0; vertex < mesh.skinInfluences.size(); ++vertex) {
    const glm::vec3 deformed = SkinVertex(mesh, vertex, *instance.palette());
    if (glm::length(deformed - moveVertices[vertex]) > 0.05f) {
      fireDiffersFromMove = true;
      break;
    }
  }
  Require(fireDiffersFromMove,
          "imported Fire palette is not visually distinct from Move");
}

}  // namespace
}  // namespace stw

int main() {
  try {
    stw::TestIdleMoveAndSameStateStability();
    stw::TestFirePriorityAndCompletion();
    stw::TestMissingClipPolicies();
    stw::TestIndependentInstances();
    stw::TestTransactionalTransitionFailure();
    stw::TestStaticRegression();
    stw::TestImportedGameplayFixture();
    stw::TestImportedGameplayFixtureDeformsGeometry();
  } catch (const std::exception& exception) {
    std::cerr << "GameplayAnimationStateTests FAILED: " << exception.what()
              << '\n';
    return 1;
  }
  std::cout << "GameplayAnimationStateTests PASSED\n";
  return 0;
}
