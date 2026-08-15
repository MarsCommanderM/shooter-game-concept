#pragma once

#include <cstddef>
#include <memory>
#include <string>

namespace stw {

class IRenderer;

struct PlaytestOptions {
  std::string name;
  int frameLimit = 0;
  float durationSeconds = 0.0f;
  bool noInput = false;
  bool hiddenWindow = false;
  std::string capturePath;
  std::string remoteDirectory;
  int streamFramesPerSecond = 10;
};

class IPlaytestScene {
 public:
  virtual ~IPlaytestScene() = default;

  virtual const char* name() const noexcept = 0;
  virtual bool Initialize(IRenderer& renderer,
                          std::string* error = nullptr) = 0;
  virtual bool SelectAnimation(bool resetTime,
                               std::string* error = nullptr) = 0;
  virtual bool SelectBindPose(std::string* error = nullptr) = 0;
  virtual bool Update(float deltaSeconds,
                      std::string* error = nullptr) = 0;
  virtual bool Draw(IRenderer& renderer,
                    std::string* error = nullptr) const = 0;

  virtual const std::string& clipName() const noexcept = 0;
  virtual float animationTime() const noexcept = 0;
  virtual std::size_t jointCount() const noexcept = 0;
  virtual std::size_t paletteSize() const noexcept = 0;
  virtual bool looping() const noexcept = 0;
};

std::unique_ptr<IPlaytestScene> CreateSkinningPlaytest();
std::unique_ptr<IPlaytestScene> CreateAnimationPlaytest();

int RunPlaytestRuntime(const PlaytestOptions& options,
                       std::unique_ptr<IPlaytestScene> scene);

}  // namespace stw
