#include "game/PlayerCamera.hpp"
#include "game/RemoteInputProtocol.hpp"

#include "Events.hpp"
#include "Weapons.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

#include <glm/glm.hpp>

namespace stw {
namespace {

void Require(bool condition, const std::string& message) {
  if (!condition) throw std::runtime_error(message);
}

bool Near(float left, float right, float epsilon = 1.0e-5f) {
  return std::fabs(left - right) <= epsilon;
}

bool VectorNear(const glm::vec3& left,
                const glm::vec3& right,
                float epsilon = 1.0e-5f) {
  return Near(left.x, right.x, epsilon) &&
      Near(left.y, right.y, epsilon) && Near(left.z, right.z, epsilon);
}

bool MatrixNear(const glm::mat4& left,
                const glm::mat4& right,
                float epsilon = 1.0e-5f) {
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      if (!Near(left[column][row], right[column][row], epsilon)) return false;
    }
  }
  return true;
}

FpsInput ToFpsInput(const RemoteInputSample& sample) {
  FpsInput input;
  input.fwd = sample.forward;
  input.strafe = sample.strafe;
  input.sprint = sample.sprint;
  input.lookDx = sample.lookX * kRemoteLookMouseCountsPerUnit;
  input.lookDy = sample.lookY * kRemoteLookMouseCountsPerUnit;
  return input;
}

void TestNeutralInputKeepsPlayerAndRenderedViewStable() {
  FpsController controller;
  Camera camera;
  SyncCameraFromFpsController(controller, camera);
  const glm::vec3 position = controller.pos;
  const float yaw = controller.yaw;
  const float pitch = controller.pitch;
  const glm::mat4 viewProjection = camera.viewProj();

  controller.update(FpsInput{}, 1.0f / 60.0f);
  SyncCameraFromFpsController(controller, camera);
  Require(VectorNear(controller.pos, position) && Near(controller.yaw, yaw) &&
              Near(controller.pitch, pitch),
          "neutral input changed authoritative FPS state");
  Require(MatrixNear(camera.viewProj(), viewProjection),
          "neutral input changed the view matrix consumed by rendering");
}

void TestForwardInputChangesPlayerAndRenderedView() {
  FpsController controller;
  Camera camera;
  SyncCameraFromFpsController(controller, camera);
  const glm::vec3 initialPosition = controller.pos;
  const glm::mat4 initialView = camera.viewProj();
  FpsInput input;
  input.fwd = 1.0f;
  for (int frame = 0; frame < 60; ++frame) {
    controller.update(input, 1.0f / 60.0f);
  }
  SyncCameraFromFpsController(controller, camera);
  Require(controller.pos.z < initialPosition.z - 5.0f,
          "forward input did not move the native player in facing direction");
  Require(!MatrixNear(camera.viewProj(), initialView),
          "player movement did not reach the rendered camera view path");
}

void TestCumulativeLookSurvivesSkippedInputFiles() {
  RemoteInputCursor cursor;
  RemoteInputSample sample;
  bool updated = false;
  std::string error;
  Require(DecodeRemoteInputPayload(
              "4 input_v2 0 0 0.75 -0.5 0 0\n", cursor, sample,
              updated, &error), error);
  Require(updated && Near(sample.lookX, 0.75f) && Near(sample.lookY, -0.5f),
          "first observed cumulative packet did not recover skipped look");
  Require(DecodeRemoteInputPayload(
              "7 input_v2 0 0 1.25 -0.25 0 0\n", cursor, sample,
              updated, &error), error);
  Require(updated && Near(sample.lookX, 0.5f) && Near(sample.lookY, 0.25f),
          "later cumulative packet did not recover all intervening look");

  const RemoteInputSample previous = sample;
  Require(DecodeRemoteInputPayload(
              "6 input_v2 0 0 20 20 1 1\n", cursor, sample,
              updated, &error), error);
  Require(!updated && Near(sample.lookX, previous.lookX) &&
              Near(sample.lookY, previous.lookY),
          "stale input sequence modified the consumed input sample");
}

void TestLookChangesOrientationAndRenderedView() {
  RemoteInputCursor cursor;
  RemoteInputSample sample;
  bool updated = false;
  std::string error;
  Require(DecodeRemoteInputPayload(
              "1 input_v2 0 0 0.5 -0.25 0 0\n", cursor, sample,
              updated, &error), error);
  Require(updated, "valid look packet was not marked updated");

  FpsController controller;
  Camera camera;
  SyncCameraFromFpsController(controller, camera);
  const glm::mat4 initialView = camera.viewProj();
  controller.update(ToFpsInput(sample), 1.0f / 60.0f);
  SyncCameraFromFpsController(controller, camera);
  Require(!Near(controller.yaw, 0.0f) && !Near(controller.pitch, 0.0f),
          "remote look did not change native yaw and pitch");
  Require(Near(camera.yaw, controller.yaw) &&
              Near(camera.pitch, controller.pitch) &&
              !MatrixNear(camera.viewProj(), initialView),
          "remote look changed status but not the camera used for rendering");
}

void TestMoveLookFireRemainSimultaneous() {
  RemoteInputSample sample;
  sample.forward = 1.0f;
  sample.strafe = 0.4f;
  sample.lookX = 0.35f;
  sample.lookY = -0.2f;
  sample.fire = true;

  FpsController controller;
  Camera camera;
  SyncCameraFromFpsController(controller, camera);
  const glm::vec3 initialPosition = controller.pos;
  const glm::mat4 initialView = camera.viewProj();
  controller.update(ToFpsInput(sample), 0.1f);
  SyncCameraFromFpsController(controller, camera);

  WeaponSystem weapon;
  FrameEvents events;
  const auto shot = weapon.tryFire(sample.fire, controller.eye(),
                                   controller.forward(), events);
  Require(!VectorNear(controller.pos, initialPosition) &&
              !Near(controller.yaw, 0.0f) &&
              !MatrixNear(camera.viewProj(), initialView),
          "simultaneous movement and look did not update native camera state");
  Require(shot.has_value() && events.fired.size() == 1u,
          "valid fire was lost while native movement and look were active");

  const glm::vec3 releasedPosition = controller.pos;
  const float releasedYaw = controller.yaw;
  controller.update(FpsInput{}, 0.1f);
  Require(VectorNear(controller.pos, releasedPosition) &&
              Near(controller.yaw, releasedYaw) && !controller.sprinting,
          "released input left movement, look, or sprint stuck");
}

void TestInvalidPacketIsTransactional() {
  RemoteInputCursor cursor;
  RemoteInputSample sample;
  bool updated = false;
  std::string error;
  Require(DecodeRemoteInputPayload(
              "1 input_v2 0 0 0.25 0 0 0\n", cursor, sample,
              updated, &error), error);
  const RemoteInputCursor previousCursor = cursor;
  const RemoteInputSample previousSample = sample;
  Require(!DecodeRemoteInputPayload(
              "2 input_v2 0 0 nan 0 1 0\n", cursor, sample,
              updated, &error),
          "non-finite internal look total was accepted");
  Require(cursor.lastSequence == previousCursor.lastSequence &&
              Near(static_cast<float>(cursor.lastLookTotalX),
                   static_cast<float>(previousCursor.lastLookTotalX)) &&
              Near(sample.lookX, previousSample.lookX),
          "invalid remote packet partially modified valid input state");
}

}  // namespace
}  // namespace stw

int main() {
  try {
    stw::TestNeutralInputKeepsPlayerAndRenderedViewStable();
    stw::TestForwardInputChangesPlayerAndRenderedView();
    stw::TestCumulativeLookSurvivesSkippedInputFiles();
    stw::TestLookChangesOrientationAndRenderedView();
    stw::TestMoveLookFireRemainSimultaneous();
    stw::TestInvalidPacketIsTransactional();
  } catch (const std::exception& error) {
    std::cerr << "PlayerInputRuntimeTests FAILED: " << error.what() << '\n';
    return 1;
  }
  std::cout << "PlayerInputRuntimeTests PASSED\n";
  return 0;
}
