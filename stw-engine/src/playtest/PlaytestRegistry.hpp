#pragma once

namespace stw {

bool IsPlaytestCommand(int argc, char** argv) noexcept;
int RunPlaytestCommand(int argc, char** argv);

}  // namespace stw
