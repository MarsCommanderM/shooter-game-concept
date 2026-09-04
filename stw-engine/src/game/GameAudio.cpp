#include "GameAudio.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace stw {
namespace {

constexpr float kPi = 3.14159265358979323846f;

float DeterministicNoise(std::uint32_t& state) {
  state = state * 1664525u + 1013904223u;
  return static_cast<float>((state >> 8u) & 0xffffu) / 32767.5f - 1.0f;
}

std::vector<float> BuildFireCue(int sampleRate, int weaponIndex) {
  const std::array<float, 5> durations{0.075f, 0.085f, 0.060f, 0.095f,
                                       0.145f};
  const std::array<float, 5> bodyFrequencies{118.0f, 96.0f, 145.0f, 82.0f,
                                             62.0f};
  const std::size_t count = static_cast<std::size_t>(
      static_cast<float>(sampleRate) * durations[weaponIndex]);
  std::vector<float> samples(count);
  std::uint32_t noiseState = 0x5f3759dfu +
      static_cast<std::uint32_t>(weaponIndex) * 7919u;
  for (std::size_t index = 0; index < count; ++index) {
    const float time = static_cast<float>(index) / sampleRate;
    const float envelope = std::exp(-34.0f * time);
    const float body = std::sin(2.0f * kPi * bodyFrequencies[weaponIndex] * time);
    const float crack = DeterministicNoise(noiseState);
    samples[index] = std::clamp((0.34f * body + 0.66f * crack) *
                                    envelope * 0.48f,
                                -0.85f, 0.85f);
  }
  return samples;
}

std::vector<float> BuildMechanicalCue(int sampleRate,
                                      float duration,
                                      const std::vector<float>& clickTimes) {
  const std::size_t count = static_cast<std::size_t>(sampleRate * duration);
  std::vector<float> samples(count, 0.0f);
  std::uint32_t noiseState = 0x9e3779b9u;
  for (const float clickTime : clickTimes) {
    const std::size_t start = static_cast<std::size_t>(sampleRate * clickTime);
    const std::size_t clickLength = static_cast<std::size_t>(sampleRate * 0.035f);
    for (std::size_t offset = 0;
         offset < clickLength && start + offset < samples.size(); ++offset) {
      const float time = static_cast<float>(offset) / sampleRate;
      const float envelope = std::exp(-105.0f * time);
      const float tone = std::sin(2.0f * kPi * 540.0f * time);
      samples[start + offset] +=
          (tone * 0.55f + DeterministicNoise(noiseState) * 0.45f) *
          envelope * 0.34f;
    }
  }
  return samples;
}

std::vector<float> BuildHitCue(int sampleRate) {
  const std::size_t count = static_cast<std::size_t>(sampleRate * 0.075f);
  std::vector<float> samples(count);
  for (std::size_t index = 0; index < count; ++index) {
    const float time = static_cast<float>(index) / sampleRate;
    const float envelope = std::exp(-48.0f * time);
    samples[index] = (std::sin(2.0f * kPi * 1100.0f * time) * 0.65f +
                      std::sin(2.0f * kPi * 1650.0f * time) * 0.25f) *
                     envelope * 0.35f;
  }
  return samples;
}

}  // namespace

GameAudio::~GameAudio() { Shutdown(); }

bool GameAudio::Initialize(std::string* error) {
  if (error) error->clear();
  if (available()) return true;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
    if (error) *error = SDL_GetError();
    return false;
  }

  SDL_AudioSpec desired{};
  desired.freq = 48000;
  desired.format = AUDIO_F32SYS;
  desired.channels = 1;
  desired.samples = 512;
  SDL_AudioSpec obtained{};
  const SDL_AudioDeviceID candidate = SDL_OpenAudioDevice(
      nullptr, 0, &desired, &obtained, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (candidate == 0u || obtained.format != AUDIO_F32SYS ||
      obtained.channels != 1 || obtained.freq <= 0) {
    if (candidate != 0u) SDL_CloseAudioDevice(candidate);
    if (error) *error = candidate == 0u
        ? SDL_GetError()
        : "native audio device returned an unsupported format";
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return false;
  }

  device_ = candidate;
  sampleRate_ = obtained.freq;
  for (std::size_t index = 0; index < fireCues_.size(); ++index) {
    fireCues_[index] = BuildFireCue(sampleRate_, static_cast<int>(index));
  }
  reloadCue_ = BuildMechanicalCue(sampleRate_, 0.34f,
                                   {0.0f, 0.15f, 0.29f});
  switchCue_ = BuildMechanicalCue(sampleRate_, 0.09f, {0.0f, 0.045f});
  hitCue_ = BuildHitCue(sampleRate_);
  SDL_PauseAudioDevice(device_, 0);
  return true;
}

void GameAudio::Queue(const std::vector<float>& samples) noexcept {
  if (!available() || samples.empty() ||
      samples.size() > std::numeric_limits<std::uint32_t>::max() /
                           sizeof(float)) {
    return;
  }
  const std::uint32_t maximumQueuedBytes = static_cast<std::uint32_t>(
      std::max(sampleRate_, 1) * sizeof(float) / 3);
  if (SDL_GetQueuedAudioSize(device_) > maximumQueuedBytes) {
    SDL_ClearQueuedAudio(device_);
  }
  SDL_QueueAudio(device_, samples.data(),
                 static_cast<std::uint32_t>(samples.size() * sizeof(float)));
}

void GameAudio::PlayFire(int weaponIndex) noexcept {
  if (weaponIndex < 0 ||
      static_cast<std::size_t>(weaponIndex) >= fireCues_.size()) {
    return;
  }
  Queue(fireCues_[static_cast<std::size_t>(weaponIndex)]);
}

void GameAudio::PlayReload() noexcept { Queue(reloadCue_); }

void GameAudio::PlayWeaponSwitch() noexcept { Queue(switchCue_); }

void GameAudio::PlayHit() noexcept { Queue(hitCue_); }

void GameAudio::Shutdown() noexcept {
  if (device_ != 0u) {
    SDL_ClearQueuedAudio(device_);
    SDL_CloseAudioDevice(device_);
    device_ = 0u;
  }
  sampleRate_ = 0;
  for (std::vector<float>& cue : fireCues_) cue.clear();
  reloadCue_.clear();
  switchCue_.clear();
  hitCue_.clear();
  if ((SDL_WasInit(SDL_INIT_AUDIO) & SDL_INIT_AUDIO) != 0u) {
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
}

}  // namespace stw
