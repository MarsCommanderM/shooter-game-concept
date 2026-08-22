#include "game/GameRuntime.hpp"

#include "Events.hpp"
#include "FpsController.hpp"
#include "IBL.hpp"
#include "TangentGenerator.hpp"
#include "Targets.hpp"
#include "Weapons.hpp"
#include "camera.h"
#include "game/CharacterAnimationController.hpp"
#include "game/GameAudio.hpp"
#include "game/GameplayPresentation.hpp"
#include "gltf.h"
#include "renderer.h"
#include "runtime/ModelInstance.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#ifndef STW_RUNTIME_ACCEPTANCE_ASSET
#define STW_RUNTIME_ACCEPTANCE_ASSET \
  "playtest/assets/gameplay_animation_states.gltf"
#endif

namespace stw {
namespace {

enum class GamePhase {
  Menu,
  Playing,
  Paused,
};

struct RemoteGameInput {
  float strafe = 0.0f;
  float forward = 0.0f;
  float lookX = 0.0f;
  float lookY = 0.0f;
  bool fire = false;
  bool sprint = false;
  unsigned long long lastInputSequence = 0;
  unsigned long long lastCommandSequence = 0;
  std::chrono::steady_clock::time_point lastInputAt{};
};

struct RenderObject {
  std::uint32_t mesh = 0;
  StwMaterial material;
  std::array<float, 16> transform{};
};

struct RuntimeModelSet {
  std::shared_ptr<const StwModel> asset;
  std::vector<std::uint32_t> meshHandles;
  std::vector<RuntimeModelInstance> instances;

  bool loaded() const noexcept {
    return asset && !meshHandles.empty() && !instances.empty();
  }
};

const char* PhaseName(GamePhase phase) {
  switch (phase) {
    case GamePhase::Menu:
      return "MENU";
    case GamePhase::Playing:
      return "PLAYING";
    case GamePhase::Paused:
      return "PAUSED";
  }
  return "UNKNOWN";
}

const char* QualityName(GameVisualQuality quality) {
  return quality == GameVisualQuality::HQ ? "HQ" : "STANDARD";
}

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool LoadRuntimeModelSet(const std::string& path,
                         IRenderer& renderer,
                         RuntimeModelSet& out,
                         std::string* error) {
  if (error) error->clear();
  auto candidateAsset = std::make_shared<StwModel>();
  if (!LoadGLTF(path, *candidateAsset, error)) return false;

  std::vector<RuntimeModelInstance> candidateInstances;
  if (!RuntimeModelInstance::CreateDefaultSet(candidateAsset,
                                               candidateInstances, error)) {
    return false;
  }

  std::vector<std::uint32_t> candidateHandles;
  candidateHandles.reserve(candidateAsset->meshes.size());
  for (const StwMesh& mesh : candidateAsset->meshes) {
    std::uint32_t handle = kInvalidMeshHandle;
    if (!renderer.UploadMeshChecked(mesh, handle, error)) return false;
    candidateHandles.push_back(handle);
  }

  RuntimeModelSet candidate;
  candidate.asset = std::move(candidateAsset);
  candidate.meshHandles = std::move(candidateHandles);
  candidate.instances = std::move(candidateInstances);
  out = std::move(candidate);
  return true;
}

bool ConfigureRuntimeAcceptanceInstances(
    RuntimeModelSet& model,
    CharacterAnimationController& outController,
    std::size_t& outControlledInstance,
    std::string* error) {
  if (error) error->clear();
  if (!model.asset || model.asset->skinnedMeshes.empty()) {
    return Fail(error,
                "runtime acceptance asset has no skinned mesh binding");
  }

  std::vector<RuntimeModelInstance> candidate;
  candidate.reserve(4u);
  for (std::size_t index = 0; index < 4u; ++index) {
    RuntimeModelInstance instance;
    if (!RuntimeModelInstance::CreateSkinned(model.asset, 0u, instance,
                                             error)) {
      return false;
    }
    candidate.push_back(std::move(instance));
  }

  if (!candidate[0].SelectBindPose(error) ||
      !candidate[2].SetAnimationTime(0.3f, error) ||
      !candidate[3].SetAnimationTime(1.7f, error)) {
    return false;
  }

  const std::array<float, 4> positions{-6.0f, 2.2f, 6.0f, 9.0f};
  const std::array<float, 4> depths{-10.0f, -7.5f, -10.0f, -14.0f};
  const std::array<float, 4> scales{1.55f, 2.05f, 1.55f, 1.45f};
  for (std::size_t index = 0; index < candidate.size(); ++index) {
    const glm::mat4 transform =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(positions[index], 0.08f, depths[index])) *
        glm::scale(glm::mat4(1.0f), glm::vec3(scales[index]));
    if (!candidate[index].SetTransform(transform, error)) return false;
  }

  CharacterAnimationController candidateController;
  if (!CharacterAnimationController::Create(candidate[1],
                                             candidateController, error)) {
    return false;
  }

  model.instances = std::move(candidate);
  outController = std::move(candidateController);
  outControlledInstance = 1u;
  return true;
}

bool UpdateRuntimeModels(RuntimeModelSet& model,
                         float deltaSeconds,
                         std::string* error) {
  if (error) error->clear();
  bool succeeded = true;
  std::string firstError;
  for (RuntimeModelInstance& instance : model.instances) {
    std::string instanceError;
    if (!instance.Update(deltaSeconds, &instanceError)) {
      if (firstError.empty()) {
        firstError = "model node " +
            std::to_string(instance.sourceNodeIndex().value_or(0u)) +
            ": " + instanceError;
      }
      succeeded = false;
    }
  }
  if (!succeeded && error) *error = firstError;
  return succeeded;
}

bool ResetRuntimeAnimations(RuntimeModelSet& model, std::string* error) {
  if (error) error->clear();
  for (RuntimeModelInstance& instance : model.instances) {
    if (!instance.selectedAnimationIndex()) continue;
    if (!instance.SetAnimationTime(0.0f, error) ||
        !instance.SetPaused(false, error)) {
      return false;
    }
  }
  return true;
}

bool SubmitRuntimeModels(const RuntimeModelSet& model,
                         IRenderer& renderer,
                         const StwMaterial& fallbackMaterial,
                         std::string* error) {
  if (error) error->clear();
  bool succeeded = true;
  std::string firstError;
  for (const RuntimeModelInstance& instance : model.instances) {
    std::string instanceError;
    if (!instance.Submit(renderer, model.meshHandles, fallbackMaterial,
                         &instanceError)) {
      if (firstError.empty()) {
        firstError = "model node " +
            std::to_string(instance.sourceNodeIndex().value_or(0u)) +
            ": " + instanceError;
      }
      succeeded = false;
    }
  }
  if (!succeeded && error) *error = firstError;
  return succeeded;
}

std::string JsonEscape(const std::string& input) {
  std::ostringstream output;
  for (const unsigned char character : input) {
    switch (character) {
      case '"': output << "\\\""; break;
      case '\\': output << "\\\\"; break;
      case '\n': output << "\\n"; break;
      case '\r': output << "\\r"; break;
      case '\t': output << "\\t"; break;
      default:
        if (character < 0x20u) {
          output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                 << static_cast<int>(character) << std::dec << std::setfill(' ');
        } else {
          output << static_cast<char>(character);
        }
        break;
    }
  }
  return output.str();
}

bool WriteTextAtomic(const std::filesystem::path& path,
                     const std::string& contents,
                     std::string* error) {
  const std::filesystem::path temporary = path.string() + ".tmp";
  {
    std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
    if (!file) return Fail(error, "failed to open remote output temporary file");
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.flush();
    if (!file) return Fail(error, "failed to write remote output temporary file");
  }
  if (std::rename(temporary.string().c_str(), path.string().c_str()) != 0) {
    return Fail(error, "failed to publish remote output atomically");
  }
  return true;
}

bool IsFiniteUnit(float value) {
  return std::isfinite(static_cast<double>(value)) && value >= -1.0f &&
         value <= 1.0f;
}

bool PollRemoteInput(const std::filesystem::path& directory,
                     RemoteGameInput& input,
                     std::string& lastError) {
  const std::filesystem::path inputPath = directory / "input.txt";
  std::error_code fileError;
  if (!std::filesystem::exists(inputPath, fileError)) return true;
  if (fileError) return Fail(&lastError, "failed to inspect remote input file");
  const std::uintmax_t size = std::filesystem::file_size(inputPath, fileError);
  if (fileError || size > 512u) {
    return Fail(&lastError, "remote input file is invalid or too large");
  }

  std::ifstream file(inputPath, std::ios::binary);
  if (!file) return Fail(&lastError, "failed to read remote input file");
  const std::string payload((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
  std::istringstream stream(payload);
  unsigned long long sequence = 0;
  std::string kind;
  float strafe = 0.0f;
  float forward = 0.0f;
  float lookX = 0.0f;
  float lookY = 0.0f;
  int fire = 0;
  int sprint = 0;
  std::string trailing;
  if (!(stream >> sequence >> kind >> strafe >> forward >> lookX >> lookY >>
        fire >> sprint) ||
      (stream >> trailing) || kind != "input" ||
      !IsFiniteUnit(strafe) || !IsFiniteUnit(forward) ||
      !IsFiniteUnit(lookX) || !IsFiniteUnit(lookY) ||
      (fire != 0 && fire != 1) || (sprint != 0 && sprint != 1)) {
    return Fail(&lastError, "remote input contains invalid values");
  }
  if (sequence <= input.lastInputSequence) return true;
  input.lastInputSequence = sequence;
  input.strafe = strafe;
  input.forward = forward;
  input.lookX = lookX;
  input.lookY = lookY;
  input.fire = fire != 0;
  input.sprint = sprint != 0;
  input.lastInputAt = std::chrono::steady_clock::now();
  lastError.clear();
  return true;
}

bool PollRemoteCommand(const std::filesystem::path& directory,
                       RemoteGameInput& input,
                       GamePhase& phase,
                       bool& running,
                       bool& resetRequested,
                       bool& cycleWeaponRequested,
                       bool& reloadRequested,
                       std::string& lastError) {
  const std::filesystem::path commandPath = directory / "command.txt";
  std::error_code fileError;
  if (!std::filesystem::exists(commandPath, fileError)) return true;
  if (fileError) return Fail(&lastError, "failed to inspect remote command file");
  const std::uintmax_t size = std::filesystem::file_size(commandPath, fileError);
  if (fileError || size > 512u) {
    return Fail(&lastError, "remote command file is invalid or too large");
  }
  std::ifstream file(commandPath, std::ios::binary);
  if (!file) return Fail(&lastError, "failed to read remote command file");
  const std::string payload((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
  std::istringstream stream(payload);
  unsigned long long sequence = 0;
  std::string command;
  std::string trailing;
  if (!(stream >> sequence >> command) || sequence <= input.lastCommandSequence) {
    return true;
  }
  if (stream >> trailing) return Fail(&lastError, "remote command has trailing data");
  input.lastCommandSequence = sequence;

  if (command == "game_start" || command == "play") {
    phase = GamePhase::Playing;
  } else if (command == "game_pause" || command == "pause") {
    phase = phase == GamePhase::Paused ? GamePhase::Playing : GamePhase::Paused;
  } else if (command == "game_reset" || command == "reset") {
    resetRequested = true;
    phase = GamePhase::Playing;
  } else if (command == "game_weapon") {
    cycleWeaponRequested = true;
  } else if (command == "game_reload") {
    reloadRequested = true;
  } else if (command == "stop") {
    running = false;
  } else {
    return Fail(&lastError, "remote game command is not allowlisted");
  }
  lastError.clear();
  return true;
}

bool WriteRemoteStatus(const std::filesystem::path& directory,
                       GamePhase phase,
                       const FpsController& controller,
                       const WeaponSystem& weapon,
                       const TargetWorld& world,
                       bool haveModel,
                       const CharacterAnimationController* animationController,
                       const RuntimeModelInstance* animationInstance,
                       bool animatedModelLoaded,
                       bool audioAvailable,
                       GameVisualQuality quality,
                       double fps,
                       double frameMilliseconds,
                       std::uint64_t frameNumber,
                       const std::string& lastError,
                       std::string* error) {
  std::size_t aliveTargets = 0;
  for (const Target& target : world.targets()) {
    if (target.alive) ++aliveTargets;
  }
  std::ostringstream json;
  json << std::fixed << std::setprecision(3)
       << "{\n"
       << "  \"playtest\": \"game\",\n"
       << "  \"runtime\": \"STW native OpenGL\",\n"
       << "  \"state\": \"" << PhaseName(phase) << "\",\n"
       << "  \"scene\": \""
       << (phase == GamePhase::Menu ? "main-menu" : "training-map")
       << "\",\n"
       << "  \"map\": \"training-map\",\n"
       << "  \"fps\": " << fps << ",\n"
       << "  \"frameTimeMs\": " << frameMilliseconds << ",\n"
       << "  \"frameNumber\": " << frameNumber << ",\n"
       << "  \"weapon\": \"" << JsonEscape(weapon.spec().name) << "\",\n"
       << "  \"ammoMagazine\": " << weapon.magazineAmmo() << ",\n"
       << "  \"ammoReserve\": " << weapon.reserveAmmo() << ",\n"
       << "  \"reloading\": " << (weapon.isReloading() ? "true" : "false")
       << ",\n"
       << "  \"reloadProgress\": " << weapon.reloadProgress() << ",\n"
       << "  \"quality\": \"" << QualityName(quality) << "\",\n"
       << "  \"audio\": \"" << (audioAvailable ? "native" : "unavailable")
       << "\",\n"
       << "  \"aliveTargets\": " << aliveTargets << ",\n"
       << "  \"modelLoaded\": " << (haveModel ? "true" : "false") << ",\n"
       << "  \"animatedModelLoaded\": "
       << (animatedModelLoaded ? "true" : "false") << ",\n"
       << "  \"animationState\": \""
       << (animationController
               ? CharacterAnimationStateName(animationController->state())
               : "Unavailable")
       << "\",\n"
       << "  \"clip\": \""
       << JsonEscape(animationInstance ? animationInstance->animationName()
                                       : std::string{})
       << "\",\n"
       << "  \"animationTime\": "
       << (animationInstance ? animationInstance->animationTime() : 0.0f)
       << ",\n"
       << "  \"player\": {\"x\": " << controller.pos.x
       << ", \"y\": " << controller.pos.y
       << ", \"z\": " << controller.pos.z << "},\n"
       << "  \"lastError\": \"" << JsonEscape(lastError) << "\"\n"
       << "}\n";
  return WriteTextAtomic(directory / "status.json", json.str(), error);
}

std::array<unsigned char, 7> Glyph(char character) {
  switch (character) {
    case 'A': return {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11};
    case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f};
    case 'P': return {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10};
    case 'S': return {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e};
    case 'T': return {0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
    case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a};
    case 'Y': return {0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04};
    default: return {0, 0, 0, 0, 0, 0, 0};
  }
}

void DrawPixelText(IRenderer& renderer,
                   std::uint32_t cube,
                   const StwMaterial& material,
                   const std::string& text,
                   float baselineY,
                   float depth,
                   float pixelSize) {
  const float advance = pixelSize * 6.0f;
  const float startX = -0.5f * advance * static_cast<float>(text.size());
  for (std::size_t letter = 0; letter < text.size(); ++letter) {
    const auto rows = Glyph(text[letter]);
    for (int row = 0; row < 7; ++row) {
      for (int column = 0; column < 5; ++column) {
        if ((rows[static_cast<std::size_t>(row)] & (1u << (4 - column))) == 0u) {
          continue;
        }
        const glm::vec3 position(
            startX + static_cast<float>(letter) * advance +
                static_cast<float>(column) * pixelSize,
            baselineY + static_cast<float>(6 - row) * pixelSize, depth);
        const glm::mat4 transform =
            glm::translate(glm::mat4(1.0f), position) *
            glm::scale(glm::mat4(1.0f),
                       glm::vec3(pixelSize * 0.42f, pixelSize * 0.42f,
                                 pixelSize * 0.16f));
        renderer.Draw(cube, material, glm::value_ptr(transform));
      }
    }
  }
}

StwMaterial MakeMaterial(float red,
                         float green,
                         float blue,
                         float metallic,
                         float roughness) {
  StwMaterial material;
  material.base[0] = red;
  material.base[1] = green;
  material.base[2] = blue;
  material.metallic = metallic;
  material.roughness = roughness;
  return material;
}

void DrawWorldBox(IRenderer& renderer,
                  std::uint32_t cube,
                  const StwMaterial& material,
                  const glm::vec3& center,
                  const glm::vec3& size) {
  const glm::mat4 transform =
      glm::translate(glm::mat4(1.0f), center) *
      glm::scale(glm::mat4(1.0f), size);
  renderer.Draw(cube, material, glm::value_ptr(transform));
}

void SubmitTrainingArena(IRenderer& renderer,
                         std::uint32_t cube,
                         bool hq) {
  const StwMaterial wall = hq
      ? MakeMaterial(0.28f, 0.34f, 0.39f, 0.18f, 0.72f)
      : MakeMaterial(0.14f, 0.17f, 0.18f, 0.08f, 0.82f);
  const StwMaterial structure =
      MakeMaterial(0.11f, 0.15f, 0.18f, 0.64f, 0.36f);
  const StwMaterial accent =
      MakeMaterial(0.03f, 0.64f, 0.82f, 0.20f, 0.34f);
  const StwMaterial lane =
      MakeMaterial(0.66f, 0.72f, 0.68f, 0.06f, 0.78f);

  DrawWorldBox(renderer, cube, wall, glm::vec3(-12.0f, 1.55f, -10.0f),
               glm::vec3(0.35f, 3.1f, 32.0f));
  DrawWorldBox(renderer, cube, wall, glm::vec3(12.0f, 1.55f, -10.0f),
               glm::vec3(0.35f, 3.1f, 32.0f));
  DrawWorldBox(renderer, cube, wall, glm::vec3(0.0f, 2.0f, -26.0f),
               glm::vec3(24.0f, 4.0f, 0.45f));

  for (int laneIndex = -2; laneIndex <= 2; ++laneIndex) {
    const float x = static_cast<float>(laneIndex) * 3.2f;
    DrawWorldBox(renderer, cube, lane, glm::vec3(x, 0.018f, -14.0f),
                 glm::vec3(0.055f, 0.025f, 23.0f));
  }
  DrawWorldBox(renderer, cube, accent, glm::vec3(0.0f, 0.025f, -5.0f),
               glm::vec3(10.5f, 0.035f, 0.10f));
  DrawWorldBox(renderer, cube, accent, glm::vec3(0.0f, 0.025f, -23.8f),
               glm::vec3(10.5f, 0.035f, 0.10f));

  DrawWorldBox(renderer, cube, structure, glm::vec3(-9.0f, 0.72f, -8.0f),
               glm::vec3(2.0f, 1.44f, 2.0f));
  DrawWorldBox(renderer, cube, structure, glm::vec3(9.0f, 0.72f, -8.0f),
               glm::vec3(2.0f, 1.44f, 2.0f));
  DrawWorldBox(renderer, cube, structure, glm::vec3(-9.5f, 1.25f, -18.0f),
               glm::vec3(1.0f, 2.5f, 3.0f));
  DrawWorldBox(renderer, cube, structure, glm::vec3(9.5f, 1.25f, -18.0f),
               glm::vec3(1.0f, 2.5f, 3.0f));

  const StwMaterial platform =
      MakeMaterial(0.18f, 0.22f, 0.25f, 0.45f, 0.48f);
  DrawWorldBox(renderer, cube, platform, glm::vec3(2.2f, 0.08f, -7.5f),
               glm::vec3(2.2f, 0.16f, 1.4f));
}

void ConfigureIbl(IRenderer& renderer, bool hq) {
  constexpr int width = 256;
  constexpr int height = 128;
  std::vector<float> pixels(width * height * 3);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float u = static_cast<float>(x) / static_cast<float>(width - 1);
      const float v = static_cast<float>(y) / static_cast<float>(height - 1);
      const float phi = u * 6.28318f;
      const float theta = v * 3.14159f;
      const glm::vec3 direction(std::cos(phi) * std::sin(theta),
                                std::cos(theta),
                                std::sin(phi) * std::sin(theta));
      const float horizon = glm::clamp(direction.y, -1.0f, 1.0f);
      const glm::vec3 low = hq ? glm::vec3(0.18f, 0.22f, 0.25f)
                               : glm::vec3(0.10f, 0.08f, 0.06f);
      const glm::vec3 high = hq ? glm::vec3(0.38f, 0.62f, 0.92f)
                                : glm::vec3(0.08f, 0.12f, 0.20f);
      glm::vec3 color = glm::mix(
          low, high,
          std::pow(glm::clamp(horizon * 1.25f + 0.35f, 0.0f, 1.0f), 0.7f));
      const glm::vec3 sun = glm::normalize(glm::vec3(-0.50f, 0.55f, 0.42f));
      const float sunDot = std::max(glm::dot(direction, sun), 0.0f);
      color += (hq ? glm::vec3(1.0f, 0.84f, 0.62f)
                   : glm::vec3(1.0f, 0.55f, 0.22f)) *
               (std::pow(sunDot, 500.0f) * (hq ? 18.0f : 12.0f) +
                std::pow(sunDot, 7.0f) * (hq ? 0.75f : 0.35f));
      const std::size_t offset =
          static_cast<std::size_t>((y * width + x) * 3);
      pixels[offset] = color.r;
      pixels[offset + 1] = color.g;
      pixels[offset + 2] = color.b;
    }
  }
  renderer.SetIBL(BuildIBLFromEquirect(pixels.data(), width, height));
}

void ConfigureNormalMap(IRenderer& renderer) {
  constexpr int size = 128;
  std::vector<unsigned char> pixels(size * size * 4);
  const auto heightAt = [](int x, int y) {
    const float gridX = std::fmod(static_cast<float>(x), 32.0f) - 16.0f;
    const float gridY = std::fmod(static_cast<float>(y), 32.0f) - 16.0f;
    return std::max(0.0f, 1.0f - std::sqrt(gridX * gridX + gridY * gridY) / 10.0f);
  };
  for (int y = 0; y < size; ++y) {
    for (int x = 0; x < size; ++x) {
      const float dx = heightAt(x + 1, y) - heightAt(x - 1, y);
      const float dy = heightAt(x, y + 1) - heightAt(x, y - 1);
      const glm::vec3 normal =
          glm::normalize(glm::vec3(-dx * 2.0f, -dy * 2.0f, 1.0f));
      const std::size_t offset =
          static_cast<std::size_t>((y * size + x) * 4);
      pixels[offset] = static_cast<unsigned char>((normal.x * 0.5f + 0.5f) * 255.0f);
      pixels[offset + 1] = static_cast<unsigned char>((normal.y * 0.5f + 0.5f) * 255.0f);
      pixels[offset + 2] = static_cast<unsigned char>((normal.z * 0.5f + 0.5f) * 255.0f);
      pixels[offset + 3] = 255u;
    }
  }
  renderer.SetNormalTexture(renderer.UploadTextureRGBA(pixels.data(), size, size));
}

}  // namespace

int RunGameRuntime(const GameRuntimeOptions& options) {
  std::filesystem::path remoteDirectory;
  if (!options.remoteDirectory.empty()) {
    remoteDirectory = options.remoteDirectory;
    std::error_code directoryError;
    if (!std::filesystem::is_directory(remoteDirectory, directoryError) ||
        directoryError) {
      std::cerr << "GAME ERROR: remote directory must already exist: "
                << remoteDirectory << '\n';
      return 1;
    }
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "GAME ERROR: SDL video initialization failed: "
              << SDL_GetError() << '\n';
    return 1;
  }
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

  const Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
      (options.hiddenWindow ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
  SDL_Window* window = SDL_CreateWindow(
      "SAVE THE WORLD // STW-ENGINE", SDL_WINDOWPOS_CENTERED,
      SDL_WINDOWPOS_CENTERED, 1280, 720, windowFlags);
  if (!window) {
    std::cerr << "GAME ERROR: OpenGL window creation failed: "
              << SDL_GetError() << '\n';
    SDL_Quit();
    return 1;
  }

  std::unique_ptr<IRenderer> renderer(CreateGLRenderer());
  if (!renderer || !renderer->Init(window)) {
    std::cerr << "GAME ERROR: real STW OpenGL renderer initialization failed\n";
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  RuntimeModelSet model;
  std::string modelLoadError;
  const bool haveModel =
      LoadRuntimeModelSet(options.assetPath, *renderer, model, &modelLoadError);
  if (!haveModel) {
    std::cerr << "GAME MODEL WARNING: " << modelLoadError << '\n';
  } else {
    for (std::size_t index = 0; index < model.instances.size(); ++index) {
      const glm::mat4 transform =
          glm::translate(glm::mat4(1.0f),
                         glm::vec3(-8.0f + static_cast<float>(index) * 1.6f,
                                   0.8f, -13.0f)) *
          glm::scale(glm::mat4(1.0f), glm::vec3(1.6f));
      std::string transformError;
      if (!model.instances[index].SetTransform(transform, &transformError)) {
        modelLoadError = transformError;
        std::cerr << "GAME MODEL WARNING: " << transformError << '\n';
      }
    }
  }

  RuntimeModelSet animationAcceptanceModel;
  CharacterAnimationController playerAnimationController;
  std::size_t playerAnimationInstance = 0u;
  std::string acceptanceLoadError;
  const bool haveAnimationAcceptance =
      LoadRuntimeModelSet(STW_RUNTIME_ACCEPTANCE_ASSET, *renderer,
                          animationAcceptanceModel, &acceptanceLoadError) &&
      ConfigureRuntimeAcceptanceInstances(animationAcceptanceModel,
                                          playerAnimationController,
                                          playerAnimationInstance,
                                          &acceptanceLoadError);
  if (!haveAnimationAcceptance) {
    std::cerr << "GAME ANIMATION WARNING: " << acceptanceLoadError << '\n';
  }
  const bool hq = options.visualQuality == GameVisualQuality::HQ;
  ConfigureIbl(*renderer, hq);
  renderer->SetClearColor(hq ? 0.16f : 0.035f,
                          hq ? 0.30f : 0.055f,
                          hq ? 0.48f : 0.075f);

  StwMesh cube = MakeCubeMesh();
  cube.tan.resize(cube.pos.size() / 3u * 4u);
  GenerateTangentsArrays(cube.pos.data(), cube.nrm.data(), cube.uv.data(),
                         cube.idx.data(), cube.idx.size(), cube.pos.size() / 3u,
                         cube.tan.data());
  StwMesh ground = MakeGroundMesh(30.0f);
  ground.tan.resize(ground.pos.size() / 3u * 4u);
  GenerateTangentsArrays(ground.pos.data(), ground.nrm.data(), ground.uv.data(),
                         ground.idx.data(), ground.idx.size(),
                         ground.pos.size() / 3u, ground.tan.data());
  const std::uint32_t groundHandle = renderer->UploadMesh(ground);
  const std::uint32_t cubeHandle = renderer->UploadMesh(cube);
  ConfigureNormalMap(*renderer);

  std::vector<RenderObject> fallbackCrates;
  for (int index = 0; index < 3; ++index) {
    RenderObject object;
    object.mesh = cubeHandle;
    object.material = MakeMaterial(0.14f, 0.22f, 0.26f, 0.52f, 0.42f);
    const glm::mat4 transform =
        glm::translate(glm::mat4(1.0f),
                       glm::vec3(-8.0f + static_cast<float>(index) * 1.25f,
                                 0.65f, -13.0f)) *
        glm::scale(glm::mat4(1.0f), glm::vec3(1.2f));
    std::memcpy(object.transform.data(), glm::value_ptr(transform),
                sizeof(float) * 16u);
    fallbackCrates.push_back(object);
  }

  StwMaterial groundMaterial;
  groundMaterial.base[0] = hq ? 0.22f : 0.10f;
  groundMaterial.base[1] = hq ? 0.26f : 0.13f;
  groundMaterial.base[2] = hq ? 0.27f : 0.14f;
  groundMaterial.roughness = 0.82f;
  groundMaterial.metallic = 0.05f;
  groundMaterial.hasNormal = true;
  groundMaterial.normalScale = 0.38f;
  StwMaterial fallbackMaterial;
  fallbackMaterial.base[0] = 0.1f;
  fallbackMaterial.base[1] = 0.55f;
  fallbackMaterial.base[2] = 0.16f;
  fallbackMaterial.roughness = 0.45f;
  fallbackMaterial.metallic = 0.6f;
  StwMaterial menuTitleMaterial;
  menuTitleMaterial.base[0] = 0.05f;
  menuTitleMaterial.base[1] = 0.75f;
  menuTitleMaterial.base[2] = 0.95f;
  menuTitleMaterial.metallic = 0.55f;
  menuTitleMaterial.roughness = 0.25f;
  StwMaterial menuActionMaterial;
  menuActionMaterial.base[0] = 1.0f;
  menuActionMaterial.base[1] = 0.45f;
  menuActionMaterial.base[2] = 0.08f;
  menuActionMaterial.metallic = 0.15f;
  menuActionMaterial.roughness = 0.35f;

  Camera camera;
  FpsController controller;
  WeaponSystem weapon;
  GameplayPresentation presentation;
  GameAudio audio;
  std::string audioError;
  if (!audio.Initialize(&audioError)) {
    std::cerr << "GAME AUDIO WARNING: " << audioError << '\n';
  }
  TargetWorld targetWorld(5);
  FrameEvents events;
  FpsInput frameInput;
  RemoteGameInput remoteInput;
  GamePhase phase = options.autoStart ? GamePhase::Playing : GamePhase::Menu;
  bool localFire = false;
  bool running = true;
  bool failed = false;
  bool cliCaptureComplete = options.capturePath.empty();
  std::string lastError = !modelLoadError.empty()
      ? modelLoadError
      : acceptanceLoadError;
  std::uint64_t frameNumber = 0;
  double simulatedSeconds = 0.0;
  double remoteCaptureAccumulator = 1.0;
  double statusAccumulator = 1.0;
  double fpsAccumulator = 0.0;
  std::uint64_t fpsFrames = 0;
  double displayedFps = 0.0;
  const Uint64 performanceFrequency = SDL_GetPerformanceFrequency();
  Uint64 previousCounter = SDL_GetPerformanceCounter();

  std::cout << "STW GAME RUNTIME: real menu and training map ["
            << QualityName(options.visualQuality) << "]\n"
            << "Controls: Enter/click start, WASD/mouse move/look, left fire, "
               "Q weapon, E reload, R reset, Esc quit\n";

  while (running) {
    bool resetRequested = false;
    bool cycleWeaponRequested = false;
    bool reloadRequested = false;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      } else if (!options.noInput && event.type == SDL_KEYDOWN &&
                 event.key.repeat == 0) {
        if (event.key.keysym.sym == SDLK_ESCAPE) {
          running = false;
        } else if (phase == GamePhase::Menu &&
                   (event.key.keysym.sym == SDLK_RETURN ||
                    event.key.keysym.sym == SDLK_SPACE)) {
          phase = GamePhase::Playing;
          SDL_SetRelativeMouseMode(SDL_TRUE);
        } else if (event.key.keysym.sym == SDLK_q) {
          cycleWeaponRequested = true;
        } else if (event.key.keysym.sym == SDLK_e) {
          reloadRequested = true;
        } else if (event.key.keysym.sym == SDLK_r) {
          resetRequested = true;
          phase = GamePhase::Playing;
        } else if (event.key.keysym.sym == SDLK_p) {
          phase = phase == GamePhase::Paused ? GamePhase::Playing
                                             : GamePhase::Paused;
        }
      } else if (!options.noInput && event.type == SDL_MOUSEBUTTONDOWN) {
        if (phase == GamePhase::Menu) phase = GamePhase::Playing;
        localFire = true;
        SDL_SetRelativeMouseMode(SDL_TRUE);
      } else if (!options.noInput && event.type == SDL_MOUSEBUTTONUP) {
        localFire = false;
      } else if (!options.noInput && event.type == SDL_MOUSEMOTION &&
                 SDL_GetRelativeMouseMode()) {
        frameInput.lookDx += static_cast<float>(event.motion.xrel);
        frameInput.lookDy += static_cast<float>(event.motion.yrel);
      } else if (event.type == SDL_WINDOWEVENT &&
                 event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        int width = 0;
        int height = 0;
        SDL_GL_GetDrawableSize(window, &width, &height);
        glViewport(0, 0, width, height);
      }
    }
    if (!running) break;

    const Uint64 currentCounter = SDL_GetPerformanceCounter();
    const double realDelta = performanceFrequency == 0u
        ? 0.0
        : static_cast<double>(currentCounter - previousCounter) /
              static_cast<double>(performanceFrequency);
    previousCounter = currentCounter;
    const float deltaSeconds = options.noInput
        ? 1.0f / 60.0f
        : static_cast<float>(std::clamp(realDelta, 0.0, 0.05));

    if (!remoteDirectory.empty()) {
      PollRemoteInput(remoteDirectory, remoteInput, lastError);
      PollRemoteCommand(remoteDirectory, remoteInput, phase, running,
                        resetRequested, cycleWeaponRequested, reloadRequested,
                        lastError);
    }
    if (!running) break;
    if (remoteInput.lastInputAt.time_since_epoch().count() != 0 &&
        std::chrono::steady_clock::now() - remoteInput.lastInputAt >
            std::chrono::milliseconds(350)) {
      remoteInput.strafe = 0.0f;
      remoteInput.forward = 0.0f;
      remoteInput.lookX = 0.0f;
      remoteInput.lookY = 0.0f;
      remoteInput.fire = false;
      remoteInput.sprint = false;
    }

    if (resetRequested) {
      controller = FpsController{};
      weapon = WeaponSystem{};
      presentation.Reset();
      targetWorld = TargetWorld(5);
      std::string resetError;
      if (haveModel && !ResetRuntimeAnimations(model, &resetError)) {
        lastError = resetError;
        if (options.noInput) failed = true;
      }
      resetError.clear();
      if (haveAnimationAcceptance &&
          !ResetRuntimeAnimations(animationAcceptanceModel, &resetError)) {
        lastError = resetError;
        if (options.noInput) failed = true;
      }
      resetError.clear();
      if (haveAnimationAcceptance &&
          (playerAnimationInstance >= animationAcceptanceModel.instances.size() ||
           !playerAnimationController.Reset(
               animationAcceptanceModel.instances[playerAnimationInstance],
               &resetError))) {
        lastError = resetError.empty()
            ? "gameplay animation instance mapping is invalid"
            : resetError;
        if (options.noInput) failed = true;
      }
    }
    if (cycleWeaponRequested) {
      weapon.cycle();
      presentation.OnWeaponSwitched();
      audio.PlayWeaponSwitch();
    }
    if (reloadRequested && weapon.startReload()) {
      presentation.OnReloadStarted();
      audio.PlayReload();
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const float localForward = options.noInput ? 0.0f :
        (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) -
        (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
    const float localStrafe = options.noInput ? 0.0f :
        (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) -
        (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
    frameInput.fwd = std::clamp(localForward + remoteInput.forward, -1.0f, 1.0f);
    frameInput.strafe = std::clamp(localStrafe + remoteInput.strafe, -1.0f, 1.0f);
    frameInput.sprint = (!options.noInput && keys[SDL_SCANCODE_LSHIFT]) ||
                        remoteInput.sprint;
    frameInput.lookDx += remoteInput.lookX * 12.0f;
    frameInput.lookDy += remoteInput.lookY * 12.0f;
    remoteInput.lookX = 0.0f;
    remoteInput.lookY = 0.0f;

    events.clear();
    if (phase == GamePhase::Playing) {
      bool firedThisFrame = false;
      controller.update(frameInput, deltaSeconds);
      weapon.update(deltaSeconds);
      const float movementMagnitude = std::min(
          1.0f, std::sqrt(frameInput.fwd * frameInput.fwd +
                          frameInput.strafe * frameInput.strafe));
      presentation.Update(deltaSeconds, movementMagnitude);
      if (const auto shot = weapon.tryFire(localFire || remoteInput.fire,
                                           controller.eye(),
                                           controller.forward(), events)) {
        firedThisFrame = true;
        std::optional<glm::vec3> shotHitPoint;
        if (const auto hit = targetWorld.hitscan(
                shot->origin, shot->dir, weapon.spec().range)) {
          shotHitPoint = hit->second;
          events.hits.push_back({hit->second, hit->first});
          targetWorld.applyDamage(hit->first, weapon.spec().damage, events);
          audio.PlayHit();
        }
        presentation.OnWeaponFired(*shot, weapon.spec().range, shotHitPoint);
        audio.PlayFire(shot->weapon);
      }
      targetWorld.update(deltaSeconds, events);

      std::string animationError;
      bool gameplayAnimationFailed = false;
      if (haveAnimationAcceptance) {
        if (playerAnimationInstance >=
            animationAcceptanceModel.instances.size()) {
          animationError = "gameplay animation instance mapping is invalid";
          gameplayAnimationFailed = true;
        } else {
          const float movementMagnitudeSquared =
              frameInput.fwd * frameInput.fwd +
              frameInput.strafe * frameInput.strafe;
          const bool moving = movementMagnitudeSquared > 1.0e-6f;
          CharacterAnimationInput animationInput;
          animationInput.moving = moving;
          animationInput.fireTriggered = firedThisFrame;
          if (!playerAnimationController.Update(
                  animationInput,
                  animationAcceptanceModel.instances[playerAnimationInstance],
                  &animationError)) {
            // The instance keeps its previous valid Animator/Pose/palette.
            gameplayAnimationFailed = true;
          } else if (!playerAnimationController.lastDiagnostic().empty()) {
            animationError = playerAnimationController.lastDiagnostic();
          }
        }
        if (!animationError.empty()) {
          lastError = animationError;
          if (options.noInput && gameplayAnimationFailed) failed = true;
        }
      }
      animationError.clear();
      if (haveModel &&
          !UpdateRuntimeModels(model, deltaSeconds, &animationError)) {
        lastError = animationError;
        if (options.noInput) failed = true;
      }
      animationError.clear();
      if (haveAnimationAcceptance &&
          !UpdateRuntimeModels(animationAcceptanceModel, deltaSeconds,
                               &animationError)) {
        lastError = animationError;
        if (options.noInput) failed = true;
      }
    }
    frameInput.lookDx = 0.0f;
    frameInput.lookDy = 0.0f;

    renderer->BeginFrame();
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    camera.aspect = drawableHeight > 0
        ? static_cast<float>(drawableWidth) / static_cast<float>(drawableHeight)
        : 1.0f;
    Light light;
    if (phase == GamePhase::Menu) {
      renderer->SetClearColor(0.008f, 0.018f, 0.028f);
      camera.pos = glm::vec3(0.0f, 1.4f, 14.0f);
      camera.yaw = 0.0f;
      camera.pitch = 0.0f;
      light.dir[0] = -0.4f;
      light.dir[1] = -0.55f;
      light.dir[2] = -0.7f;
      light.strength = 5.0f;
      renderer->SetViewProj(glm::value_ptr(camera.viewProj()));
      renderer->SetCameraPos(camera.pos.x, camera.pos.y, camera.pos.z);
      renderer->SetLight(light);
      DrawPixelText(*renderer, cubeHandle, menuTitleMaterial, "STW", 1.4f,
                    -8.0f, 0.72f);
      DrawPixelText(*renderer, cubeHandle, menuActionMaterial, "PLAY", -3.2f,
                    -8.0f, 0.32f);
    } else {
      renderer->SetClearColor(hq ? 0.16f : 0.035f,
                              hq ? 0.30f : 0.055f,
                              hq ? 0.48f : 0.075f);
      camera.pos = controller.eye();
      camera.yaw = controller.yaw;
      camera.pitch = controller.pitch;
      if (hq) {
        // The visible face of the imported animation mannequin points +Z;
        // this sun vector lights it and the arena instead of relying on a
        // debug ambient override.
        light.dir[0] = -0.38f;
        light.dir[1] = -0.78f;
        light.dir[2] = -0.55f;
        light.color[0] = 1.0f;
        light.color[1] = 0.94f;
        light.color[2] = 0.84f;
        light.ambient[0] = 0.20f;
        light.ambient[1] = 0.24f;
        light.ambient[2] = 0.28f;
        light.strength = 4.35f + presentation.muzzleIntensity() * 1.25f;
      }
      renderer->SetViewProj(glm::value_ptr(camera.viewProj()));
      renderer->SetCameraPos(camera.pos.x, camera.pos.y, camera.pos.z);
      renderer->SetLight(light);
      const glm::mat4 identity(1.0f);
      renderer->Draw(groundHandle, groundMaterial, glm::value_ptr(identity));
      SubmitTrainingArena(*renderer, cubeHandle, hq);

      if (haveModel && model.loaded()) {
        std::string drawError;
        if (!SubmitRuntimeModels(model, *renderer, fallbackMaterial,
                                 &drawError)) {
          lastError = drawError;
          if (options.noInput) failed = true;
        }
      } else {
        for (const RenderObject& object : fallbackCrates) {
          renderer->Draw(object.mesh, object.material, object.transform.data());
        }
      }

      if (haveAnimationAcceptance && animationAcceptanceModel.loaded()) {
        std::string drawError;
        if (!SubmitRuntimeModels(animationAcceptanceModel, *renderer,
                                 fallbackMaterial, &drawError)) {
          lastError = drawError;
          if (options.noInput) failed = true;
        }
      }

      for (const Target& target : targetWorld.targets()) {
        SubmitTrainingTarget(*renderer, cubeHandle, target);
      }
      presentation.SubmitTransientEffects(*renderer, cubeHandle);
      presentation.SubmitFirstPersonWeapon(*renderer, cubeHandle, controller,
                                           weapon);
      presentation.SubmitCrosshair(*renderer, cubeHandle, controller);
    }

    simulatedSeconds += deltaSeconds;
    remoteCaptureAccumulator += deltaSeconds;
    statusAccumulator += deltaSeconds;
    fpsAccumulator += options.noInput ? deltaSeconds : realDelta;
    ++fpsFrames;
    const bool finalFrameByCount = options.frameLimit > 0 &&
        frameNumber + 1u >= static_cast<std::uint64_t>(options.frameLimit);
    const bool finalFrameByDuration = options.durationSeconds > 0.0f &&
        simulatedSeconds >= static_cast<double>(options.durationSeconds);
    const bool oneShotCapture = !options.capturePath.empty() &&
        options.frameLimit == 0 && options.durationSeconds == 0.0f &&
        frameNumber == 0u;

    std::string requestedCapture;
    bool captureIsCli = false;
    if (!cliCaptureComplete &&
        (finalFrameByCount || finalFrameByDuration || oneShotCapture)) {
      requestedCapture = options.capturePath;
      captureIsCli = true;
    } else if (!remoteDirectory.empty() &&
               remoteCaptureAccumulator >=
                   1.0 / static_cast<double>(options.streamFramesPerSecond)) {
      requestedCapture = (remoteDirectory / "frame.png").string();
      remoteCaptureAccumulator = 0.0;
    }

    bool captureQueued = false;
    if (!requestedCapture.empty()) {
      std::string captureError;
      captureQueued = renderer->RequestFrameCapture(requestedCapture,
                                                    &captureError);
      if (!captureQueued) {
        lastError = captureError;
        if (captureIsCli) failed = true;
      }
    }
    renderer->EndFrame();
    if (captureQueued) {
      std::string captureError;
      if (!renderer->ConsumeFrameCaptureResult(&captureError)) {
        lastError = captureError;
        if (captureIsCli) failed = true;
      } else if (captureIsCli) {
        cliCaptureComplete = true;
      }
    }

    ++frameNumber;
    if (fpsAccumulator >= 0.5) {
      displayedFps = fpsAccumulator > 0.0
          ? static_cast<double>(fpsFrames) / fpsAccumulator
          : 0.0;
      fpsAccumulator = 0.0;
      fpsFrames = 0;
    }
    if (!remoteDirectory.empty() && statusAccumulator >= 0.1) {
      std::string statusError;
      const RuntimeModelInstance* gameplayAnimationInstance =
          haveAnimationAcceptance &&
                  playerAnimationInstance <
                      animationAcceptanceModel.instances.size()
              ? &animationAcceptanceModel.instances[playerAnimationInstance]
              : nullptr;
      if (!WriteRemoteStatus(remoteDirectory, phase, controller, weapon,
                             targetWorld, haveModel,
                             haveAnimationAcceptance
                                 ? &playerAnimationController
                                 : nullptr,
                             gameplayAnimationInstance,
                             haveAnimationAcceptance, audio.available(),
                             options.visualQuality, displayedFps,
                             static_cast<double>(deltaSeconds) * 1000.0,
                             frameNumber, lastError, &statusError)) {
        lastError = statusError;
      }
      statusAccumulator = 0.0;
    }
    if (failed || finalFrameByCount || finalFrameByDuration || oneShotCapture) {
      running = false;
    }
    if (!options.noInput && running) SDL_Delay(1);
  }

  if (!remoteDirectory.empty()) {
    std::string statusError;
    const RuntimeModelInstance* gameplayAnimationInstance =
        haveAnimationAcceptance &&
                playerAnimationInstance <
                    animationAcceptanceModel.instances.size()
            ? &animationAcceptanceModel.instances[playerAnimationInstance]
            : nullptr;
    WriteRemoteStatus(remoteDirectory, phase, controller, weapon, targetWorld,
                      haveModel,
                      haveAnimationAcceptance ? &playerAnimationController
                                              : nullptr,
                      gameplayAnimationInstance, haveAnimationAcceptance,
                      audio.available(), options.visualQuality,
                      displayedFps, 0.0,
                      frameNumber, lastError,
                      &statusError);
  }
  audio.Shutdown();
  renderer->Shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (!cliCaptureComplete) {
    std::cerr << "GAME ERROR: requested capture was not completed\n";
    failed = true;
  }
  if (failed) {
    std::cerr << "GAME FAILED: "
              << (lastError.empty() ? "runtime error" : lastError) << '\n';
    return 1;
  }
  std::cout << "GAME COMPLETE: frames=" << frameNumber << '\n';
  return 0;
}

}  // namespace stw
