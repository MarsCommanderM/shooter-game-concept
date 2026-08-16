#pragma once

#include <string>

namespace stw {

// Options around the real game runtime. Normal desktop startup and the
// browser-backed playtest both enter RunGameRuntime; gameplay is never
// reimplemented in the bridge or web client.
struct GameRuntimeOptions {
  std::string assetPath = "assets/test_scene.gltf";
  int frameLimit = 0;
  float durationSeconds = 0.0f;
  bool noInput = false;
  bool hiddenWindow = false;
  bool autoStart = false;
  std::string capturePath;
  std::string remoteDirectory;
  int streamFramesPerSecond = 10;
};

int RunGameRuntime(const GameRuntimeOptions& options);

}  // namespace stw
