#include "PlaytestRegistry.hpp"

#include "game/GameRuntime.hpp"

#include <cerrno>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>

namespace stw {
namespace {

struct GameCommandOptions {
  int frameLimit = 0;
  float durationSeconds = 0.0f;
  bool noInput = false;
  bool hiddenWindow = false;
  bool autoStart = false;
  std::string capturePath;
  std::string remoteDirectory;
  int streamFramesPerSecond = 10;
  bool hq = true;
};

void PrintUsage(std::ostream& output) {
  output << "Usage:\n"
         << "  stw --playtest list\n"
         << "  stw --playtest game [--frames N] [--duration SECONDS]\n"
         << "      [--no-input] [--capture PATH] [--hidden]\n"
         << "      [--remote-dir PATH] [--stream-fps 1..30] [--auto-start]\n"
         << "      [--quality hq|standard]\n";
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
    std::cout << "  game - canonical native STW GameRuntime\n";
    return 0;
  }

  if (requestedName != "game") {
    std::cerr << "Unknown STW playtest: " << requestedName << "\n";
    PrintUsage(std::cerr);
    return 64;
  }

  GameCommandOptions options;
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

}  // namespace stw
