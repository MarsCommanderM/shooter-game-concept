#include "game/GameRuntime.hpp"
#include "playtest/PlaytestRegistry.hpp"

int main(int argc, char** argv) {
  if (stw::IsPlaytestCommand(argc, argv)) {
    return stw::RunPlaytestCommand(argc, argv);
  }

  stw::GameRuntimeOptions options;
  if (argc > 1 && argv[1]) options.assetPath = argv[1];
  return stw::RunGameRuntime(options);
}
