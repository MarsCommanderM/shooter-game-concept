#include "PlaytestRegistry.hpp"

#include "Playtest.hpp"
#include "game/GameRuntime.hpp"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace stw {
namespace {

using SceneFactory = std::unique_ptr<IPlaytestScene> (*)();

struct PlaytestDescriptor {
  const char* name;
  const char* description;
  SceneFactory factory;
};

const std::vector<PlaytestDescriptor>& Registry() {
  static const std::vector<PlaytestDescriptor> registry{
      {"game", "real STW menu, map, player, weapons, and renderer", nullptr},
      {"skinning", "T3-A procedural two-joint GPU skinning",
       &CreateSkinningPlaytest},
      {"animation", "T3-B imported glTF animation runtime binding",
       &CreateAnimationPlaytest},
  };
  return registry;
}

void PrintUsage(std::ostream& output) {
  output << "Usage:\n"
         << "  stw --playtest list\n"
         << "  stw --playtest <name> [--frames N] [--duration SECONDS]\n"
         << "      [--no-input] [--capture PATH] [--hidden]\n"
         << "      [--remote-dir PATH] [--stream-fps 1..30] [--auto-start]\n"
         << "      [--quality hq|standard] (game only)\n";
}

bool ParsePositiveInt(const char* text, int& value) {
  if (!text || !*text) return false;
  errno = 0;
  char* end = nullptr;
  const long parsed = std::strtol(text, &end, 10);
  if (errno != 0 || end == text || *end != '\0' || parsed <= 0 ||
      parsed > std::numeric_limits<int>::max()) {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}

bool ParsePositiveFloat(const char* text, float& value) {
  if (!text || !*text) return false;
  errno = 0;
  char* end = nullptr;
  const float parsed = std::strtof(text, &end);
  if (errno != 0 || end == text || *end != '\0' ||
      !std::isfinite(parsed) || parsed <= 0.0f) {
    return false;
  }
  value = parsed;
  return true;
}

const PlaytestDescriptor* FindPlaytest(const std::string& name) {
  for (const PlaytestDescriptor& descriptor : Registry()) {
    if (name == descriptor.name) return &descriptor;
  }
  return nullptr;
}

}  // namespace

bool IsPlaytestCommand(int argc, char** argv) noexcept {
  return argc >= 2 && argv && argv[1] &&
         std::strcmp(argv[1], "--playtest") == 0;
}

int RunPlaytestCommand(int argc, char** argv) {
  if (!IsPlaytestCommand(argc, argv)) return 64;
  if (argc < 3 || !argv[2]) {
    PrintUsage(std::cerr);
    return 64;
  }

  const std::string requestedName = argv[2];
  if (requestedName == "list") {
    std::cout << "Available STW playtests:\n";
    for (const PlaytestDescriptor& descriptor : Registry()) {
      std::cout << "  " << descriptor.name << " - "
                << descriptor.description << '\n';
    }
    return 0;
  }

  const PlaytestDescriptor* descriptor = FindPlaytest(requestedName);
  if (!descriptor) {
    std::cerr << "Unknown STW playtest: " << requestedName << "\n";
    PrintUsage(std::cerr);
    return 64;
  }

  PlaytestOptions options;
  options.name = descriptor->name;
  bool qualitySpecified = false;
  for (int argument = 3; argument < argc; ++argument) {
    const std::string flag = argv[argument] ? argv[argument] : "";
    auto requireValue = [&](const char* option) -> const char* {
      if (argument + 1 >= argc || !argv[argument + 1]) {
        std::cerr << option << " requires a value\n";
        return nullptr;
      }
      return argv[++argument];
    };

    if (flag == "--frames") {
      const char* value = requireValue("--frames");
      if (!value) return 64;
      if (!ParsePositiveInt(value, options.frameLimit)) {
        std::cerr << "--frames must be a positive integer\n";
        return 64;
      }
    } else if (flag == "--duration") {
      const char* value = requireValue("--duration");
      if (!value) return 64;
      if (!ParsePositiveFloat(value, options.durationSeconds)) {
        std::cerr << "--duration must be a positive finite number\n";
        return 64;
      }
    } else if (flag == "--capture") {
      const char* value = requireValue("--capture");
      if (!value || !*value) return 64;
      options.capturePath = value;
    } else if (flag == "--remote-dir") {
      const char* value = requireValue("--remote-dir");
      if (!value || !*value) return 64;
      options.remoteDirectory = value;
      options.hiddenWindow = true;
    } else if (flag == "--stream-fps") {
      const char* value = requireValue("--stream-fps");
      if (!value || !ParsePositiveInt(value, options.streamFramesPerSecond) ||
          options.streamFramesPerSecond > 30) {
        std::cerr << "--stream-fps must be between 1 and 30\n";
        return 64;
      }
    } else if (flag == "--no-input") {
      options.noInput = true;
    } else if (flag == "--hidden") {
      options.hiddenWindow = true;
    } else if (flag == "--auto-start") {
      options.autoStart = true;
    } else if (flag == "--quality") {
      qualitySpecified = true;
      const char* value = requireValue("--quality");
      if (!value) return 64;
      if (std::strcmp(value, "hq") == 0) {
        options.hq = true;
      } else if (std::strcmp(value, "standard") == 0) {
        options.hq = false;
      } else {
        std::cerr << "--quality must be hq or standard\n";
        return 64;
      }
    } else {
      std::cerr << "Unknown playtest option: " << flag << "\n";
      PrintUsage(std::cerr);
      return 64;
    }
  }

  if (qualitySpecified && requestedName != "game") {
    std::cerr << "--quality is supported only by --playtest game\n";
    return 64;
  }

  if (requestedName == "game") {
    GameRuntimeOptions gameOptions;
    gameOptions.frameLimit = options.frameLimit;
    gameOptions.durationSeconds = options.durationSeconds;
    gameOptions.noInput = options.noInput;
    gameOptions.hiddenWindow = options.hiddenWindow;
    gameOptions.autoStart = options.autoStart;
    gameOptions.capturePath = options.capturePath;
    gameOptions.remoteDirectory = options.remoteDirectory;
    gameOptions.streamFramesPerSecond = options.streamFramesPerSecond;
    gameOptions.visualQuality = options.hq ? GameVisualQuality::HQ
                                           : GameVisualQuality::Standard;
    return RunGameRuntime(gameOptions);
  }
  return RunPlaytestRuntime(options, descriptor->factory());
}

}  // namespace stw
