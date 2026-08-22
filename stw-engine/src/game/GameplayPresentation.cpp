#include "GameplayPresentation.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

#include <glm/gtc/type_ptr.hpp>

namespace stw {
namespace {

constexpr float kPi = 3.14159265358979323846f;

StwMaterial Material(float red,
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

glm::mat4 OrientedBox(const glm::vec3& center,
                      const glm::vec3& right,
                      const glm::vec3& up,
                      const glm::vec3& backward,
                      const glm::vec3& size) {
  glm::mat4 transform(1.0f);
  transform[0] = glm::vec4(right * size.x, 0.0f);
  transform[1] = glm::vec4(up * size.y, 0.0f);
  transform[2] = glm::vec4(backward * size.z, 0.0f);
  transform[3] = glm::vec4(center, 1.0f);
  return transform;
}

void DrawBox(IRenderer& renderer,
             std::uint32_t cubeMesh,
             const StwMaterial& material,
             const glm::vec3& center,
             const glm::vec3& right,
             const glm::vec3& up,
             const glm::vec3& backward,
             const glm::vec3& size) {
  const glm::mat4 transform =
      OrientedBox(center, right, up, backward, size);
  renderer.Draw(cubeMesh, material, glm::value_ptr(transform));
}

void DrawWorldBox(IRenderer& renderer,
                  std::uint32_t cubeMesh,
                  const StwMaterial& material,
                  const glm::vec3& center,
                  const glm::vec3& size) {
  DrawBox(renderer, cubeMesh, material, center,
          glm::vec3(1.0f, 0.0f, 0.0f),
          glm::vec3(0.0f, 1.0f, 0.0f),
          glm::vec3(0.0f, 0.0f, 1.0f), size);
}

void DrawBeam(IRenderer& renderer,
              std::uint32_t cubeMesh,
              const StwMaterial& material,
              const glm::vec3& start,
              const glm::vec3& end,
              float width) {
  const glm::vec3 delta = end - start;
  const float length = glm::length(delta);
  if (!std::isfinite(static_cast<double>(length)) || length < 1.0e-4f) return;
  const glm::vec3 axis = delta / length;
  const glm::vec3 reference = std::fabs(axis.y) > 0.95f
      ? glm::vec3(1.0f, 0.0f, 0.0f)
      : glm::vec3(0.0f, 1.0f, 0.0f);
  const glm::vec3 right = glm::normalize(glm::cross(reference, axis));
  const glm::vec3 up = glm::normalize(glm::cross(axis, right));
  DrawBox(renderer, cubeMesh, material, (start + end) * 0.5f,
          right, up, axis, glm::vec3(width, width, length));
}

}  // namespace

void GameplayPresentation::Reset() noexcept { *this = GameplayPresentation{}; }

void GameplayPresentation::Update(float deltaSeconds,
                                  float movementAmount) noexcept {
  if (!std::isfinite(static_cast<double>(deltaSeconds)) ||
      deltaSeconds < 0.0f) {
    return;
  }
  elapsed_ += deltaSeconds;
  movementAmount_ = std::clamp(movementAmount, 0.0f, 1.0f);
  recoil_ = std::max(0.0f, recoil_ - deltaSeconds * 7.5f);
  muzzleRemaining_ = std::max(0.0f, muzzleRemaining_ - deltaSeconds);
  tracerRemaining_ = std::max(0.0f, tracerRemaining_ - deltaSeconds);
  hitRemaining_ = std::max(0.0f, hitRemaining_ - deltaSeconds);
  switchKick_ = std::max(0.0f, switchKick_ - deltaSeconds * 4.5f);
  reloadKick_ = std::max(0.0f, reloadKick_ - deltaSeconds * 2.5f);
}

void GameplayPresentation::OnWeaponFired(
    const WeaponFiredEvent& event,
    float range,
    std::optional<glm::vec3> hitPoint) noexcept {
  if (!std::isfinite(static_cast<double>(range)) || range <= 0.0f) return;
  const float directionLength = glm::length(event.dir);
  if (!std::isfinite(static_cast<double>(directionLength)) ||
      directionLength <= 1.0e-5f) {
    return;
  }
  const glm::vec3 direction = event.dir / directionLength;
  recoil_ = std::min(1.0f, recoil_ + (event.weapon == 4 ? 1.0f : 0.62f));
  muzzleRemaining_ = 0.065f;
  tracerRemaining_ = 0.055f;
  tracerStart_ = event.origin + direction * 0.65f;
  tracerEnd_ = hitPoint.value_or(
      event.origin + direction * std::min(range, 42.0f));
  if (hitPoint) {
    hitPoint_ = *hitPoint;
    hitRemaining_ = 0.14f;
  }
}

void GameplayPresentation::OnWeaponSwitched() noexcept {
  switchKick_ = 1.0f;
}

void GameplayPresentation::OnReloadStarted() noexcept {
  reloadKick_ = 1.0f;
}

void GameplayPresentation::SubmitFirstPersonWeapon(
    IRenderer& renderer,
    std::uint32_t cubeMesh,
    const FpsController& controller,
    const WeaponSystem& weapon) const {
  const glm::vec3 forward = controller.forward();
  glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
  if (glm::length(right) < 1.0e-4f) right = glm::vec3(1.0f, 0.0f, 0.0f);
  right = glm::normalize(right);
  glm::vec3 up = glm::normalize(glm::cross(right, forward));

  const std::size_t weaponIndex = static_cast<std::size_t>(weapon.current());
  const std::array<float, 5> lengths{0.78f, 0.82f, 0.62f, 0.92f, 0.88f};
  const std::array<float, 5> widths{0.13f, 0.145f, 0.12f, 0.17f, 0.18f};
  const std::array<glm::vec3, 5> colors{
      glm::vec3(0.12f, 0.22f, 0.28f), glm::vec3(0.30f, 0.16f, 0.07f),
      glm::vec3(0.08f, 0.13f, 0.17f), glm::vec3(0.16f, 0.22f, 0.12f),
      glm::vec3(0.24f, 0.14f, 0.07f)};
  const float length = lengths[weaponIndex];
  const float width = widths[weaponIndex];
  const glm::vec3 color = colors[weaponIndex];
  const StwMaterial body = Material(color.r, color.g, color.b, 0.72f, 0.28f);
  const StwMaterial metal = Material(0.055f, 0.065f, 0.075f, 0.92f, 0.20f);
  const StwMaterial accent = Material(0.10f, 0.52f, 0.64f, 0.45f, 0.24f);

  const float swayX = std::sin(elapsed_ * 7.2f) * 0.018f * movementAmount_;
  const float swayY = std::fabs(std::cos(elapsed_ * 7.2f)) *
      0.014f * movementAmount_;
  const float reloadArc = weapon.isReloading()
      ? std::sin(weapon.reloadProgress() * kPi)
      : 0.0f;
  const float lower = reloadArc * 0.19f + switchKick_ * 0.13f +
      reloadKick_ * 0.035f;
  const glm::vec3 base = controller.eye() + forward * (0.72f - recoil_ * 0.055f) +
      right * (0.29f + swayX + reloadArc * 0.07f) +
      up * (-0.26f - swayY - lower + recoil_ * 0.045f);
  const glm::vec3 backward = -forward;

  DrawBox(renderer, cubeMesh, body, base, right, up, backward,
          glm::vec3(width * 1.7f, width * 1.45f, length * 0.72f));
  DrawBox(renderer, cubeMesh, metal,
          base + forward * (length * 0.48f), right, up, backward,
          glm::vec3(width * 0.56f, width * 0.56f, length * 0.58f));
  DrawBox(renderer, cubeMesh, metal,
          base - forward * (length * 0.36f) - up * (width * 0.04f),
          right, up, backward,
          glm::vec3(width * 1.35f, width * 1.35f, length * 0.26f));
  DrawBox(renderer, cubeMesh, metal,
          base - forward * 0.05f - up * 0.17f, right, up, backward,
          glm::vec3(width * 0.72f, 0.27f, 0.13f));
  DrawBox(renderer, cubeMesh, accent,
          base + forward * 0.08f + up * (width * 0.95f), right, up, backward,
          glm::vec3(width * 0.35f, width * 0.25f, 0.12f));

  if (muzzleRemaining_ > 0.0f) {
    const StwMaterial flash = Material(3.2f, 0.82f, 0.10f, 0.0f, 0.20f);
    const glm::vec3 muzzle = base + forward * (length * 0.82f);
    DrawBox(renderer, cubeMesh, flash, muzzle, right, up, backward,
            glm::vec3(0.115f, 0.115f, 0.18f));
    DrawBox(renderer, cubeMesh, flash, muzzle, right, up, backward,
            glm::vec3(0.035f, 0.24f, 0.07f));
    DrawBox(renderer, cubeMesh, flash, muzzle, right, up, backward,
            glm::vec3(0.24f, 0.035f, 0.07f));
  }
}

void GameplayPresentation::SubmitTransientEffects(
    IRenderer& renderer, std::uint32_t cubeMesh) const {
  if (tracerRemaining_ > 0.0f) {
    const StwMaterial tracer = Material(2.4f, 0.55f, 0.08f, 0.0f, 0.32f);
    DrawBeam(renderer, cubeMesh, tracer, tracerStart_, tracerEnd_, 0.024f);
  }
  if (hitRemaining_ > 0.0f) {
    const StwMaterial hit = Material(2.8f, 0.92f, 0.18f, 0.0f, 0.26f);
    DrawWorldBox(renderer, cubeMesh, hit, hitPoint_,
                 glm::vec3(0.32f, 0.055f, 0.055f));
    DrawWorldBox(renderer, cubeMesh, hit, hitPoint_,
                 glm::vec3(0.055f, 0.32f, 0.055f));
  }
}

void GameplayPresentation::SubmitCrosshair(
    IRenderer& renderer,
    std::uint32_t cubeMesh,
    const FpsController& controller) const {
  const glm::vec3 forward = controller.forward();
  glm::vec3 right = glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f));
  if (glm::length(right) < 1.0e-4f) right = glm::vec3(1.0f, 0.0f, 0.0f);
  right = glm::normalize(right);
  const glm::vec3 up = glm::normalize(glm::cross(right, forward));
  const StwMaterial crosshair =
      Material(2.6f, 2.9f, 3.1f, 0.0f, 0.35f);
  const glm::vec3 center = controller.eye() + forward * 0.24f;
  DrawBox(renderer, cubeMesh, crosshair, center, right, up, -forward,
          glm::vec3(0.013f, 0.0016f, 0.0018f));
  DrawBox(renderer, cubeMesh, crosshair, center, right, up, -forward,
          glm::vec3(0.0016f, 0.013f, 0.0018f));
}

float GameplayPresentation::muzzleIntensity() const noexcept {
  return muzzleRemaining_ > 0.0f ? muzzleRemaining_ / 0.065f : 0.0f;
}

void SubmitTrainingTarget(IRenderer& renderer,
                          std::uint32_t cubeMesh,
                          const Target& target) {
  if (!target.alive) return;
  const StwMaterial armor = Material(0.50f, 0.085f, 0.055f, 0.25f, 0.42f);
  const StwMaterial plate = Material(0.82f, 0.21f, 0.075f, 0.18f, 0.34f);
  const StwMaterial joint = Material(0.055f, 0.075f, 0.085f, 0.70f, 0.30f);
  const glm::vec3 base = target.pos - glm::vec3(0.0f, 1.0f, 0.0f);

  DrawWorldBox(renderer, cubeMesh, joint,
               base + glm::vec3(-0.24f, 0.48f, 0.0f),
               glm::vec3(0.25f, 0.82f, 0.30f));
  DrawWorldBox(renderer, cubeMesh, joint,
               base + glm::vec3(0.24f, 0.48f, 0.0f),
               glm::vec3(0.25f, 0.82f, 0.30f));
  DrawWorldBox(renderer, cubeMesh, armor,
               base + glm::vec3(0.0f, 1.20f, 0.0f),
               glm::vec3(0.90f, 0.86f, 0.42f));
  DrawWorldBox(renderer, cubeMesh, plate,
               base + glm::vec3(0.0f, 1.23f, 0.24f),
               glm::vec3(0.54f, 0.50f, 0.08f));
  DrawWorldBox(renderer, cubeMesh, armor,
               base + glm::vec3(-0.60f, 1.20f, 0.0f),
               glm::vec3(0.22f, 0.76f, 0.26f));
  DrawWorldBox(renderer, cubeMesh, armor,
               base + glm::vec3(0.60f, 1.20f, 0.0f),
               glm::vec3(0.22f, 0.76f, 0.26f));
  DrawWorldBox(renderer, cubeMesh, joint,
               base + glm::vec3(0.0f, 1.88f, 0.0f),
               glm::vec3(0.48f, 0.48f, 0.48f));
}

}  // namespace stw
