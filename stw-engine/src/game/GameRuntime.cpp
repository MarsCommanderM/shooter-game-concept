#include "game/GameRuntime.hpp"

#include "Events.hpp"
#include "FpsController.hpp"
#include "IBL.hpp"
#include "TangentGenerator.hpp"
#include "Targets.hpp"
#include "Weapons.hpp"
#include "camera.h"
#include "game/CharacterAnimationController.hpp"
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

  struct AcceptancePlacement {
    glm::vec3 position;
    float uniformScale;
  };
  // Keep the gameplay-driven instance centered in the initial FPS view. The
  // other instances remain in the same world and continue to exercise bind
  // pose and independent playback, but no longer compete for the small mobile
  // viewport. Uniform model scale preserves the current skin direction-matrix
  // contract in the renderer.
  const std::array<AcceptancePlacement, 4> placements{{
      {glm::vec3(-6.5f, 0.0f, -12.0f), 1.6f},
      {glm::vec3(0.0f, 0.3f, -8.0f), 2.0f},
      {glm::vec3(5.5f, 0.0f, -12.0f), 1.6f},
      {glm::vec3(9.0f, 0.0f, -16.0f), 1.6f},
  }};
  for (std::size_t index = 0; index < candidate.size(); ++index) {
    const glm::mat4 transform =
        glm::translate(glm::mat4(1.0f), placements[index].position) *
        glm::scale(glm::mat4(1.0f),
                   glm::vec3(placements[index].uniformScale));
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
       << "  \"aliveTargets\": " << aliveTargets << ",\n"
       << "  \"modelLoaded\": " << (haveModel ? "true" : "false") << ",\n"
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

void ConfigureIbl(IRenderer& renderer) {
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
      glm::vec3 color = glm::mix(
          glm::vec3(0.55f, 0.22f, 0.08f), glm::vec3(0.02f, 0.05f, 0.14f),
          std::pow(glm::clamp(horizon * 1.4f + 0.2f, 0.0f, 1.0f), 0.6f));
      const glm::vec3 sun = glm::normalize(glm::vec3(-0.55f, 0.22f, -0.35f));
      const float sunDot = std::max(glm::dot(direction, sun), 0.0f);
      color += glm::vec3(1.0f, 0.45f, 0.15f) *
               (std::pow(sunDot, 600.0f) * 30.0f +
                std::pow(sunDot, 6.0f) * 0.5f);
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

RenderObject MakeTrainingObject(std::uint32_t mesh,
                                const StwMaterial& material,
                                const glm::vec3& position,
                                const glm::vec3& scale) {
  RenderObject object;
  object.mesh = mesh;
  object.material = material;
  const glm::mat4 transform =
      glm::translate(glm::mat4(1.0f), position) *
      glm::scale(glm::mat4(1.0f), scale);
  std::memcpy(object.transform.data(), glm::value_ptr(transform),
              sizeof(float) * 16u);
  return object;
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
  ConfigureIbl(*renderer);

  StwMesh cube = MakeCubeMesh();
  cube.tan.resize(cube.pos.size() / 3u * 4u);
  GenerateTangentsArrays(cube.pos.data(), cube.nrm.data(), cube.uv.data(),
                         cube.idx.data(), cube.idx.size(), cube.pos.size() / 3u,
                         cube.tan.data());
  StwMesh ground = MakeGroundMesh(24.0f);
  ground.tan.resize(ground.pos.size() / 3u * 4u);
  GenerateTangentsArrays(ground.pos.data(), ground.nrm.data(), ground.uv.data(),
                         ground.idx.data(), ground.idx.size(),
                         ground.pos.size() / 3u, ground.tan.data());
  const std::uint32_t groundHandle = renderer->UploadMesh(ground);
  const std::uint32_t cubeHandle = renderer->UploadMesh(cube);
  ConfigureNormalMap(*renderer);

  std::vector<RenderObject> testObjects;
  const float roughnessValues[6] = {0.05f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
  const float metallicValues[4] = {0.0f, 0.33f, 0.66f, 1.0f};
  for (int row = 0; row < 4; ++row) {
    for (int column = 0; column < 6; ++column) {
      RenderObject object;
      object.mesh = cubeHandle;
      object.material.base[0] = 0.85f;
      object.material.base[1] = 0.85f;
      object.material.base[2] = 0.9f;
      object.material.metallic = metallicValues[row];
      object.material.roughness = roughnessValues[column];
      const glm::mat4 transform =
          glm::translate(glm::mat4(1.0f),
                         glm::vec3(-20.0f + column * 4.0f, 0.7f,
                                   -30.0f + row * 3.0f)) *
          glm::scale(glm::mat4(1.0f), glm::vec3(1.4f));
      std::memcpy(object.transform.data(), glm::value_ptr(transform),
                  sizeof(float) * 16u);
      testObjects.push_back(object);
    }
  }
  RenderObject normalMapObject;
  normalMapObject.mesh = cubeHandle;
  normalMapObject.material.base[0] = 0.7f;
  normalMapObject.material.base[1] = 0.7f;
  normalMapObject.material.base[2] = 0.75f;
  normalMapObject.material.metallic = 0.2f;
  normalMapObject.material.roughness = 0.5f;
  normalMapObject.material.hasNormal = true;
  normalMapObject.material.normalScale = 1.0f;
  const glm::mat4 normalMapTransform =
      glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.2f, -24.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(2.5f));
  std::memcpy(normalMapObject.transform.data(),
              glm::value_ptr(normalMapTransform), sizeof(float) * 16u);
  testObjects.push_back(normalMapObject);

  StwMaterial groundMaterial;
  groundMaterial.base[0] = 0.16f;
  groundMaterial.base[1] = 0.22f;
  groundMaterial.base[2] = 0.28f;
  groundMaterial.roughness = 0.88f;
  groundMaterial.metallic = 0.0f;
  StwMaterial fallbackMaterial;
  fallbackMaterial.base[0] = 0.1f;
  fallbackMaterial.base[1] = 0.55f;
  fallbackMaterial.base[2] = 0.16f;
  fallbackMaterial.roughness = 0.45f;
  fallbackMaterial.metallic = 0.6f;
  StwMaterial targetMaterial;
  targetMaterial.base[0] = 0.8f;
  targetMaterial.base[1] = 0.15f;
  targetMaterial.base[2] = 0.12f;
  targetMaterial.roughness = 0.5f;
  targetMaterial.metallic = 0.2f;
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

  StwMaterial rangeWallMaterial;
  rangeWallMaterial.base[0] = 0.12f;
  rangeWallMaterial.base[1] = 0.20f;
  rangeWallMaterial.base[2] = 0.30f;
  rangeWallMaterial.metallic = 0.05f;
  rangeWallMaterial.roughness = 0.78f;
  StwMaterial rangeAccentMaterial;
  rangeAccentMaterial.base[0] = 0.04f;
  rangeAccentMaterial.base[1] = 0.58f;
  rangeAccentMaterial.base[2] = 0.86f;
  rangeAccentMaterial.metallic = 0.12f;
  rangeAccentMaterial.roughness = 0.38f;
  StwMaterial characterPlatformMaterial;
  characterPlatformMaterial.base[0] = 0.30f;
  characterPlatformMaterial.base[1] = 0.34f;
  characterPlatformMaterial.base[2] = 0.40f;
  characterPlatformMaterial.metallic = 0.2f;
  characterPlatformMaterial.roughness = 0.55f;

  // Minimal training-range enclosure: it gives the FPS camera readable depth
  // and contrast while staying on the existing cube/material renderer path.
  std::vector<RenderObject> trainingEnvironment;
  trainingEnvironment.reserve(7u);
  trainingEnvironment.push_back(MakeTrainingObject(
      cubeHandle, rangeWallMaterial, glm::vec3(0.0f, 2.5f, -25.0f),
      glm::vec3(24.0f, 5.0f, 0.5f)));
  trainingEnvironment.push_back(MakeTrainingObject(
      cubeHandle, rangeWallMaterial, glm::vec3(-12.0f, 2.0f, -12.5f),
      glm::vec3(0.5f, 4.0f, 25.0f)));
  trainingEnvironment.push_back(MakeTrainingObject(
      cubeHandle, rangeWallMaterial, glm::vec3(12.0f, 2.0f, -12.5f),
      glm::vec3(0.5f, 4.0f, 25.0f)));
  trainingEnvironment.push_back(MakeTrainingObject(
      cubeHandle, rangeAccentMaterial, glm::vec3(-4.0f, 0.02f, -12.0f),
      glm::vec3(0.12f, 0.04f, 24.0f)));
  trainingEnvironment.push_back(MakeTrainingObject(
      cubeHandle, rangeAccentMaterial, glm::vec3(4.0f, 0.02f, -12.0f),
      glm::vec3(0.12f, 0.04f, 24.0f)));
  trainingEnvironment.push_back(MakeTrainingObject(
      cubeHandle, rangeAccentMaterial, glm::vec3(0.0f, 0.02f, -8.0f),
      glm::vec3(8.0f, 0.04f, 0.12f)));
  trainingEnvironment.push_back(MakeTrainingObject(
      cubeHandle, characterPlatformMaterial, glm::vec3(0.0f, 0.15f, -8.0f),
      glm::vec3(3.0f, 0.3f, 1.8f)));

  Camera camera;
  FpsController controller;
  WeaponSystem weapon;
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

  std::cout << "STW GAME RUNTIME: real menu and training map\n"
            << "Controls: Enter/click start, WASD/mouse move/look, left fire, "
               "Q weapon, R reset, Esc quit\n";

  while (running) {
    bool resetRequested = false;
    bool cycleWeaponRequested = false;
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
                        resetRequested, cycleWeaponRequested, lastError);
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
    if (cycleWeaponRequested) weapon.cycle();

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
      if (const auto shot = weapon.tryFire(localFire || remoteInput.fire,
                                           controller.eye(),
                                           controller.forward(), events)) {
        firedThisFrame = true;
        if (const auto hit = targetWorld.hitscan(
                shot->origin, shot->dir, weapon.spec().range)) {
          events.hits.push_back({hit->second, hit->first});
          targetWorld.applyDamage(hit->first, weapon.spec().damage, events);
        }
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
      camera.pos = controller.eye();
      camera.yaw = controller.yaw;
      camera.pitch = controller.pitch;
      // The imported acceptance character's visible faces point toward +Z.
      // The former default light lit only their culled back side, leaving the
      // real skinned draw almost black once IBL replaced fallback ambient.
      light.dir[0] = -0.35f;
      light.dir[1] = -0.75f;
      light.dir[2] = -0.55f;
      light.color[0] = 1.0f;
      light.color[1] = 0.95f;
      light.color[2] = 0.86f;
      light.ambient[0] = 0.10f;
      light.ambient[1] = 0.12f;
      light.ambient[2] = 0.16f;
      light.strength = 4.5f;
      renderer->SetViewProj(glm::value_ptr(camera.viewProj()));
      renderer->SetCameraPos(camera.pos.x, camera.pos.y, camera.pos.z);
      renderer->SetLight(light);
      const glm::mat4 identity(1.0f);
      renderer->Draw(groundHandle, groundMaterial, glm::value_ptr(identity));
      for (const RenderObject& object : trainingEnvironment) {
        renderer->Draw(object.mesh, object.material, object.transform.data());
      }

      if (haveModel && model.loaded()) {
        std::string drawError;
        if (!SubmitRuntimeModels(model, *renderer, fallbackMaterial,
                                 &drawError)) {
          lastError = drawError;
          if (options.noInput) failed = true;
        }
      } else {
        for (const RenderObject& object : testObjects) {
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
        if (!target.alive) continue;
        const glm::mat4 transform =
            glm::translate(identity, target.pos) *
            glm::scale(identity, glm::vec3(0.8f, 1.0f, 0.8f));
        renderer->Draw(cubeHandle, targetMaterial,
                       glm::value_ptr(transform));
      }
      for (const HitEvent& hit : events.hits) {
        const glm::mat4 transform =
            glm::translate(identity, hit.pos) *
            glm::scale(identity, glm::vec3(0.12f));
        StwMaterial spark;
        spark.base[0] = 1.0f;
        spark.base[1] = 0.8f;
        spark.base[2] = 0.3f;
        spark.metallic = 0.0f;
        spark.roughness = 0.4f;
        renderer->Draw(cubeHandle, spark, glm::value_ptr(transform));
      }
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
                             gameplayAnimationInstance, displayedFps,
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
                      gameplayAnimationInstance, displayedFps, 0.0,
                      frameNumber, lastError,
                      &statusError);
  }
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
