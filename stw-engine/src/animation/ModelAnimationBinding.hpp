#pragma once

#include <cstddef>
#include <string>

namespace stw {

class Animator;
struct StwModel;

// Bindet einen importierten, skin-lokalen Clip an einen neuen Animator-Zustand.
// model (und damit Skeleton/AnimationClip) muss den Animator überleben.
bool BindAnimation(const StwModel& model,
                   std::size_t skinIndex,
                   std::size_t animationIndex,
                   Animator& animator,
                   std::string* error = nullptr);

} // namespace stw
