#include "game/RemoteInputProtocol.hpp"

#include <algorithm>
#include <cmath>
#include <sstream>
#include <string>

namespace stw {
namespace {

constexpr double kMaximumLookTotal = 1.0e12;
constexpr double kMaximumRecoveredLookDelta = 8.0;

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool IsFiniteUnit(double value) {
  return std::isfinite(value) && value >= -1.0 && value <= 1.0;
}

bool IsFiniteLookTotal(double value) {
  return std::isfinite(value) && std::fabs(value) <= kMaximumLookTotal;
}

}  // namespace

bool DecodeRemoteInputPayload(const std::string& payload,
                              RemoteInputCursor& cursor,
                              RemoteInputSample& output,
                              bool& updated,
                              std::string* error) {
  if (error) error->clear();
  updated = false;

  std::istringstream stream(payload);
  std::uint64_t sequence = 0u;
  std::string kind;
  double strafe = 0.0;
  double forward = 0.0;
  double lookX = 0.0;
  double lookY = 0.0;
  int fire = 0;
  int sprint = 0;
  std::string trailing;
  if (!(stream >> sequence >> kind >> strafe >> forward >> lookX >> lookY >>
        fire >> sprint) ||
      (stream >> trailing) ||
      (kind != "input_v2" && kind != "input") ||
      !IsFiniteUnit(strafe) || !IsFiniteUnit(forward) ||
      (fire != 0 && fire != 1) || (sprint != 0 && sprint != 1)) {
    return Fail(error, "remote input contains invalid values");
  }
  const bool cumulative = kind == "input_v2";
  if ((cumulative &&
       (!IsFiniteLookTotal(lookX) || !IsFiniteLookTotal(lookY))) ||
      (!cumulative && (!IsFiniteUnit(lookX) || !IsFiniteUnit(lookY)))) {
    return Fail(error, "remote input contains invalid look values");
  }
  if (sequence <= cursor.lastSequence) return true;

  RemoteInputCursor candidateCursor = cursor;
  RemoteInputSample candidateOutput;
  candidateOutput.strafe = static_cast<float>(strafe);
  candidateOutput.forward = static_cast<float>(forward);
  candidateOutput.fire = fire != 0;
  candidateOutput.sprint = sprint != 0;
  if (cumulative) {
    // Recover every skipped browser delta while bounding a pathological stall
    // or tampered internal file to less than one large camera turn.
    candidateOutput.lookX = static_cast<float>(std::clamp(
        lookX - cursor.lastLookTotalX, -kMaximumRecoveredLookDelta,
        kMaximumRecoveredLookDelta));
    candidateOutput.lookY = static_cast<float>(std::clamp(
        lookY - cursor.lastLookTotalY, -kMaximumRecoveredLookDelta,
        kMaximumRecoveredLookDelta));
    candidateCursor.lastLookTotalX = lookX;
    candidateCursor.lastLookTotalY = lookY;
  } else {
    // Rolling-deploy compatibility with the previous internal delta protocol.
    candidateOutput.lookX = static_cast<float>(lookX);
    candidateOutput.lookY = static_cast<float>(lookY);
  }
  candidateCursor.lastSequence = sequence;

  cursor = candidateCursor;
  output = candidateOutput;
  updated = true;
  return true;
}

}  // namespace stw
