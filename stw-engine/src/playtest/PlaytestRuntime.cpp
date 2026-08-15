#include "Playtest.hpp"

#include "render/renderer.h"

#include <SDL2/SDL.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

namespace stw {
namespace {

enum class PlaybackState {
  Playing,
  Paused,
  Bind,
};

struct CameraState {
  glm::vec3 position{0.0f, 1.0f, 5.2f};
  float yawDegrees = -90.0f;
  float pitchDegrees = 0.0f;
};

struct RemoteInput {
  float strafe = 0.0f;
  float forward = 0.0f;
  float lookX = 0.0f;
  float lookY = 0.0f;
  std::chrono::steady_clock::time_point lastCameraCommand{};
  unsigned long long lastSequence = 0;
};

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

const char* StateName(PlaybackState state) {
  switch (state) {
    case PlaybackState::Playing:
      return "PLAYING";
    case PlaybackState::Paused:
      return "PAUSED";
    case PlaybackState::Bind:
      return "BIND";
  }
  return "UNKNOWN";
}

std::string JsonEscape(const std::string& input) {
  std::ostringstream output;
  for (unsigned char character : input) {
    switch (character) {
      case '"':
        output << "\\\"";
        break;
      case '\\':
        output << "\\\\";
        break;
      case '\b':
        output << "\\b";
        break;
      case '\f':
        output << "\\f";
        break;
      case '\n':
        output << "\\n";
        break;
      case '\r':
        output << "\\r";
        break;
      case '\t':
        output << "\\t";
        break;
      default:
        if (character < 0x20u) {
          output << "\\u" << std::hex << std::setw(4)
                 << std::setfill('0') << static_cast<int>(character)
                 << std::dec << std::setfill(' ');
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
    if (!file) return Fail(error, "failed to open remote status temporary file");
    file.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    file.flush();
    if (!file) return Fail(error, "failed to write remote status temporary file");
  }
  if (std::rename(temporary.string().c_str(), path.string().c_str()) != 0) {
    return Fail(error, "failed to publish remote status atomically");
  }
  return true;
}

bool WriteRemoteStatus(const std::filesystem::path& directory,
                       const IPlaytestScene& scene,
                       PlaybackState state,
                       bool slowMotion,
                       double fps,
                       double frameMilliseconds,
                       std::uint64_t frameNumber,
                       const std::string& lastError,
                       std::string* error) {
  std::ostringstream json;
  json << std::fixed << std::setprecision(3)
       << "{\n"
       << "  \"playtest\": \"" << JsonEscape(scene.name()) << "\",\n"
       << "  \"fps\": " << fps << ",\n"
       << "  \"frameTimeMs\": " << frameMilliseconds << ",\n"
       << "  \"frameNumber\": " << frameNumber << ",\n"
       << "  \"animationTime\": " << scene.animationTime() << ",\n"
       << "  \"clip\": \"" << JsonEscape(scene.clipName()) << "\",\n"
       << "  \"jointCount\": " << scene.jointCount() << ",\n"
       << "  \"paletteSize\": " << scene.paletteSize() << ",\n"
       << "  \"looping\": " << (scene.looping() ? "true" : "false") << ",\n"
       << "  \"slowMotion\": " << (slowMotion ? "true" : "false") << ",\n"
       << "  \"state\": \"" << StateName(state) << "\",\n"
       << "  \"lastError\": \"" << JsonEscape(lastError) << "\"\n"
       << "}\n";
  return WriteTextAtomic(directory / "status.json", json.str(), error);
}

bool IsFiniteUnit(float value) {
  return std::isfinite(static_cast<double>(value)) && value >= -1.0f &&
         value <= 1.0f;
}

bool ActivateAnimation(IPlaytestScene& scene,
                       bool reset,
                       PlaybackState& state,
                       std::string& lastError) {
  std::string error;
  if (!scene.SelectAnimation(reset, &error)) {
    lastError = error;
    return false;
  }
  state = PlaybackState::Playing;
  lastError.clear();
  return true;
}

bool ActivateBindPose(IPlaytestScene& scene,
                      PlaybackState& state,
                      std::string& lastError) {
  std::string error;
  if (!scene.SelectBindPose(&error)) {
    lastError = error;
    return false;
  }
  state = PlaybackState::Bind;
  lastError.clear();
  return true;
}

bool PollRemoteCommand(const std::filesystem::path& directory,
                       IPlaytestScene& scene,
                       PlaybackState& state,
                       bool& slowMotion,
                       bool& running,
                       RemoteInput& input,
                       std::string& lastError) {
  const std::filesystem::path commandPath = directory / "command.txt";
  std::error_code fileError;
  if (!std::filesystem::exists(commandPath, fileError)) return true;
  if (fileError) {
    lastError = "failed to inspect remote command file";
    return true;
  }
  const std::uintmax_t size = std::filesystem::file_size(commandPath, fileError);
  if (fileError || size > 512u) {
    lastError = "remote command file is invalid or too large";
    return true;
  }

  std::ifstream file(commandPath, std::ios::binary);
  if (!file) {
    lastError = "failed to read remote command file";
    return true;
  }
  const std::string payload((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
  std::istringstream commandStream(payload);
  unsigned long long sequence = 0;
  std::string command;
  if (!(commandStream >> sequence >> command) ||
      sequence <= input.lastSequence) {
    return true;
  }
  input.lastSequence = sequence;

  if (command == "play") {
    return ActivateAnimation(scene, false, state, lastError);
  }
  if (command == "pause") {
    if (state != PlaybackState::Bind) state = PlaybackState::Paused;
    lastError.clear();
    return true;
  }
  if (command == "reset") {
    return ActivateAnimation(scene, true, state, lastError);
  }
  if (command == "bind") {
    return ActivateBindPose(scene, state, lastError);
  }
  if (command == "slow") {
    slowMotion = !slowMotion;
    lastError.clear();
    return true;
  }
  if (command == "stop") {
    running = false;
    lastError.clear();
    return true;
  }
  if (command == "camera") {
    float strafe = 0.0f;
    float forward = 0.0f;
    float lookX = 0.0f;
    float lookY = 0.0f;
    std::string trailing;
    if (!(commandStream >> strafe >> forward >> lookX >> lookY) ||
        (commandStream >> trailing) || !IsFiniteUnit(strafe) ||
        !IsFiniteUnit(forward) || !IsFiniteUnit(lookX) ||
        !IsFiniteUnit(lookY)) {
      lastError = "remote camera command contains invalid values";
      return true;
    }
    input.strafe = strafe;
    input.forward = forward;
    input.lookX = lookX;
    input.lookY = lookY;
    input.lastCameraCommand = std::chrono::steady_clock::now();
    lastError.clear();
    return true;
  }

  lastError = "remote command is not allowlisted";
  return true;
}

glm::vec3 CameraForward(const CameraState& camera) {
  const float yaw = glm::radians(camera.yawDegrees);
  const float pitch = glm::radians(camera.pitchDegrees);
  return glm::normalize(glm::vec3(std::cos(yaw) * std::cos(pitch),
                                  std::sin(pitch),
                                  std::sin(yaw) * std::cos(pitch)));
}

void UpdateCamera(CameraState& camera,
                  float strafe,
                  float forward,
                  float lookX,
                  float lookY,
                  float deltaSeconds) {
  camera.yawDegrees += lookX * 85.0f * deltaSeconds;
  camera.pitchDegrees = std::clamp(camera.pitchDegrees -
                                       lookY * 70.0f * deltaSeconds,
                                   -80.0f, 80.0f);
  const glm::vec3 viewForward = CameraForward(camera);
  glm::vec3 groundForward(viewForward.x, 0.0f, viewForward.z);
  if (glm::dot(groundForward, groundForward) > 1.0e-8f) {
    groundForward = glm::normalize(groundForward);
  } else {
    groundForward = glm::vec3(0.0f, 0.0f, -1.0f);
  }
  const glm::vec3 right =
      glm::normalize(glm::cross(groundForward, glm::vec3(0.0f, 1.0f, 0.0f)));
  const glm::vec3 movement = groundForward * forward + right * strafe;
  const float lengthSquared = glm::dot(movement, movement);
  if (lengthSquared > 1.0f) {
    camera.position += glm::normalize(movement) * (3.2f * deltaSeconds);
  } else {
    camera.position += movement * (3.2f * deltaSeconds);
  }
}

}  // namespace

int RunPlaytestRuntime(const PlaytestOptions& options,
                       std::unique_ptr<IPlaytestScene> scene) {
  if (!scene) {
    std::cerr << "PLAYTEST ERROR: scene factory returned null\n";
    return 1;
  }

  std::filesystem::path remoteDirectory;
  if (!options.remoteDirectory.empty()) {
    remoteDirectory = options.remoteDirectory;
    std::error_code directoryError;
    if (!std::filesystem::is_directory(remoteDirectory, directoryError) ||
        directoryError) {
      std::cerr << "PLAYTEST ERROR: remote directory must already exist: "
                << remoteDirectory << '\n';
      return 1;
    }
  }

  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    std::cerr << "PLAYTEST ERROR: SDL video initialization failed: "
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
      (std::string("STW PLAYTEST // ") + scene->name()).c_str(),
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 960, 720, windowFlags);
  if (!window) {
    std::cerr << "PLAYTEST ERROR: OpenGL window creation failed: "
              << SDL_GetError() << '\n';
    SDL_Quit();
    return 1;
  }

  std::unique_ptr<IRenderer> renderer(CreateGLRenderer());
  if (!renderer || !renderer->Init(window)) {
    std::cerr << "PLAYTEST ERROR: real STW OpenGL renderer initialization failed: "
              << SDL_GetError() << '\n';
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  std::string lastError;
  if (!scene->Initialize(*renderer, &lastError)) {
    std::cerr << "PLAYTEST ERROR: " << lastError << '\n';
    renderer->Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  if (!options.hiddenWindow && !options.noInput) {
    SDL_SetRelativeMouseMode(SDL_TRUE);
  }
  std::cout << "PLAYTEST: " << scene->name() << '\n'
            << "Controls: WASD/mouse, Space pause/play, R reset, B/1 bind, "
               "2 animate, L slow, Esc quit\n";

  PlaybackState playbackState = PlaybackState::Playing;
  CameraState camera;
  RemoteInput remoteInput;
  bool slowMotion = false;
  bool running = true;
  bool failed = false;
  bool cliCaptureComplete = options.capturePath.empty();
  std::uint64_t frameNumber = 0;
  double simulatedSeconds = 0.0;
  double remoteCaptureAccumulator = 1.0;
  double statusAccumulator = 1.0;
  double fpsAccumulator = 0.0;
  std::uint64_t fpsFrames = 0;
  double displayedFps = 0.0;
  const Uint64 performanceFrequency = SDL_GetPerformanceFrequency();
  Uint64 previousCounter = SDL_GetPerformanceCounter();

  while (running) {
    float mouseLookX = 0.0f;
    float mouseLookY = 0.0f;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) running = false;
      if (options.noInput) continue;
      if (event.type == SDL_KEYDOWN && event.key.repeat == 0) {
        switch (event.key.keysym.sym) {
          case SDLK_ESCAPE:
            running = false;
            break;
          case SDLK_SPACE:
            if (playbackState == PlaybackState::Playing) {
              playbackState = PlaybackState::Paused;
            } else if (!ActivateAnimation(*scene, false, playbackState,
                                          lastError)) {
              failed = true;
              running = false;
            }
            break;
          case SDLK_r:
          case SDLK_2:
            if (!ActivateAnimation(*scene, true, playbackState, lastError)) {
              failed = true;
              running = false;
            }
            break;
          case SDLK_b:
          case SDLK_1:
            if (!ActivateBindPose(*scene, playbackState, lastError)) {
              failed = true;
              running = false;
            }
            break;
          case SDLK_l:
            slowMotion = !slowMotion;
            break;
          default:
            break;
        }
      } else if (event.type == SDL_MOUSEMOTION) {
        mouseLookX += static_cast<float>(event.motion.xrel) * 0.12f;
        mouseLookY += static_cast<float>(event.motion.yrel) * 0.12f;
      } else if (event.type == SDL_MOUSEBUTTONDOWN) {
        SDL_SetRelativeMouseMode(SDL_TRUE);
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
        ? (1.0f / 60.0f)
        : static_cast<float>(std::clamp(realDelta, 0.0, 0.05));

    if (!remoteDirectory.empty() &&
        !PollRemoteCommand(remoteDirectory, *scene, playbackState,
                           slowMotion, running, remoteInput, lastError)) {
      failed = true;
      running = false;
    }
    if (!running) break;

    if (remoteInput.lastCameraCommand.time_since_epoch().count() != 0 &&
        std::chrono::steady_clock::now() - remoteInput.lastCameraCommand >
            std::chrono::milliseconds(350)) {
      remoteInput.strafe = 0.0f;
      remoteInput.forward = 0.0f;
      remoteInput.lookX = 0.0f;
      remoteInput.lookY = 0.0f;
    }

    float localStrafe = 0.0f;
    float localForward = 0.0f;
    if (!options.noInput) {
      const Uint8* keys = SDL_GetKeyboardState(nullptr);
      localStrafe = (keys[SDL_SCANCODE_D] ? 1.0f : 0.0f) -
                     (keys[SDL_SCANCODE_A] ? 1.0f : 0.0f);
      localForward = (keys[SDL_SCANCODE_W] ? 1.0f : 0.0f) -
                      (keys[SDL_SCANCODE_S] ? 1.0f : 0.0f);
    }
    camera.yawDegrees += mouseLookX;
    camera.pitchDegrees = std::clamp(camera.pitchDegrees - mouseLookY,
                                     -80.0f, 80.0f);
    UpdateCamera(camera,
                 std::clamp(localStrafe + remoteInput.strafe, -1.0f, 1.0f),
                 std::clamp(localForward + remoteInput.forward, -1.0f, 1.0f),
                 remoteInput.lookX, remoteInput.lookY, deltaSeconds);

    if (playbackState == PlaybackState::Playing) {
      const float animationDelta = deltaSeconds * (slowMotion ? 0.2f : 1.0f);
      std::string updateError;
      if (!scene->Update(animationDelta, &updateError)) {
        lastError = updateError;
        failed = true;
        break;
      }
    }

    renderer->BeginFrame();
    int drawableWidth = 0;
    int drawableHeight = 0;
    SDL_GL_GetDrawableSize(window, &drawableWidth, &drawableHeight);
    const float aspect = drawableHeight > 0
        ? static_cast<float>(drawableWidth) /
              static_cast<float>(drawableHeight)
        : 1.0f;
    const glm::vec3 cameraForward = CameraForward(camera);
    const glm::mat4 viewProjection =
        glm::perspective(glm::radians(52.0f), aspect, 0.05f, 100.0f) *
        glm::lookAt(camera.position, camera.position + cameraForward,
                    glm::vec3(0.0f, 1.0f, 0.0f));
    renderer->SetViewProj(glm::value_ptr(viewProjection));
    renderer->SetCameraPos(camera.position.x, camera.position.y,
                           camera.position.z);
    Light light;
    light.dir[0] = -0.35f;
    light.dir[1] = -0.65f;
    light.dir[2] = -0.55f;
    light.strength = 4.0f;
    light.ambient[0] = 0.08f;
    light.ambient[1] = 0.10f;
    light.ambient[2] = 0.14f;
    renderer->SetLight(light);
    std::string drawError;
    if (!scene->Draw(*renderer, &drawError)) {
      lastError = drawError;
      failed = true;
      break;
    }

    simulatedSeconds += deltaSeconds;
    remoteCaptureAccumulator += deltaSeconds;
    statusAccumulator += deltaSeconds;
    fpsAccumulator += options.noInput ? deltaSeconds : realDelta;
    ++fpsFrames;
    const bool finalFrameByCount =
        options.frameLimit > 0 &&
        frameNumber + 1u >= static_cast<std::uint64_t>(options.frameLimit);
    const bool finalFrameByDuration =
        options.durationSeconds > 0.0f &&
        simulatedSeconds >= static_cast<double>(options.durationSeconds);
    const bool finalFrameBySingleCapture =
        !options.capturePath.empty() && options.frameLimit == 0 &&
        options.durationSeconds == 0.0f && frameNumber == 0u;

    std::string requestedCapture;
    bool captureIsCli = false;
    if (!cliCaptureComplete &&
        (finalFrameByCount || finalFrameByDuration ||
         (options.frameLimit == 0 && options.durationSeconds == 0.0f &&
          frameNumber == 0u))) {
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
      captureQueued =
          renderer->RequestFrameCapture(requestedCapture, &captureError);
      if (!captureQueued) {
        lastError = captureError;
        if (captureIsCli) {
          failed = true;
          break;
        }
      }
    }
    renderer->EndFrame();
    if (captureQueued) {
      std::string captureError;
      if (!renderer->ConsumeFrameCaptureResult(&captureError)) {
        lastError = captureError;
        if (captureIsCli) failed = true;
      } else {
        if (captureIsCli) cliCaptureComplete = true;
      }
    }

    ++frameNumber;
    if (fpsAccumulator >= 0.5) {
      displayedFps = fpsAccumulator > 0.0
          ? static_cast<double>(fpsFrames) / fpsAccumulator
          : 0.0;
      fpsAccumulator = 0.0;
      fpsFrames = 0;
      std::cout << std::fixed << std::setprecision(2)
                << "PLAYTEST: " << scene->name()
                << " | FPS " << displayedFps
                << " | frame " << (deltaSeconds * 1000.0f) << " ms"
                << " | animation " << scene->animationTime()
                << " | clip " << scene->clipName()
                << " | joints " << scene->jointCount()
                << " | palette " << scene->paletteSize()
                << " | state " << StateName(playbackState)
                << " | last error " << (lastError.empty() ? "none" : lastError)
                << '\n';
    }

    if (!remoteDirectory.empty() && statusAccumulator >= 0.1) {
      std::string statusError;
      if (!WriteRemoteStatus(remoteDirectory, *scene, playbackState,
                             slowMotion, displayedFps,
                             static_cast<double>(deltaSeconds) * 1000.0,
                             frameNumber, lastError, &statusError)) {
        lastError = statusError;
      }
      statusAccumulator = 0.0;
    }
    if (failed || finalFrameByCount || finalFrameByDuration ||
        finalFrameBySingleCapture) {
      running = false;
    }
    if (!options.noInput && running) SDL_Delay(1);
  }

  if (!remoteDirectory.empty()) {
    std::string statusError;
    WriteRemoteStatus(remoteDirectory, *scene, playbackState, slowMotion,
                      displayedFps, 0.0, frameNumber, lastError, &statusError);
  }
  renderer->Shutdown();
  SDL_DestroyWindow(window);
  SDL_Quit();

  if (!cliCaptureComplete) {
    std::cerr << "PLAYTEST ERROR: requested capture was not completed\n";
    failed = true;
  }
  if (failed) {
    std::cerr << "PLAYTEST FAILED: "
              << (lastError.empty() ? "runtime error" : lastError) << '\n';
    return 1;
  }
  std::cout << "PLAYTEST COMPLETE: " << scene->name()
            << " frames=" << frameNumber << '\n';
  return 0;
}

}  // namespace stw
