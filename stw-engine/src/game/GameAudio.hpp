#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace stw {

// Minimal native, client-only one-shot audio. All cues are generated at
// initialization, so gameplay events never load assets or synthesize samples
// in the frame loop. Failure to acquire an audio device is non-fatal.
class GameAudio {
 public:
  GameAudio() = default;
  GameAudio(const GameAudio&) = delete;
  GameAudio& operator=(const GameAudio&) = delete;
  ~GameAudio();

  bool Initialize(std::string* error = nullptr);
  void PlayFire(int weaponIndex) noexcept;
  void PlayReload() noexcept;
  void PlayWeaponSwitch() noexcept;
  void PlayHit() noexcept;
  void Shutdown() noexcept;

  bool available() const noexcept { return device_ != 0u; }

 private:
  void Queue(const std::vector<float>& samples) noexcept;

  std::uint32_t device_ = 0u;
  int sampleRate_ = 0;
  std::array<std::vector<float>, 5> fireCues_;
  std::vector<float> reloadCue_;
  std::vector<float> switchCue_;
  std::vector<float> hitCue_;
};

}  // namespace stw
