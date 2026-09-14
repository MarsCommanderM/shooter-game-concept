#pragma once

#include <cstdint>
#include <string>

namespace stw {

inline constexpr float kRemoteLookMouseCountsPerUnit = 48.0f;

// Cursor for the bridge-to-native input file. Look is transported as a
// cumulative value so a slow native frame cannot lose relative pointer deltas
// when several newer input files replace older ones before polling.
struct RemoteInputCursor {
  std::uint64_t lastSequence = 0u;
  double lastLookTotalX = 0.0;
  double lastLookTotalY = 0.0;
};

struct RemoteInputSample {
  float strafe = 0.0f;
  float forward = 0.0f;
  float lookX = 0.0f;
  float lookY = 0.0f;
  bool fire = false;
  bool sprint = false;
};

// Parses one complete, fixed-schema input file. `updated` is false for an
// already-consumed sequence. State and output remain unchanged on failure.
bool DecodeRemoteInputPayload(const std::string& payload,
                              RemoteInputCursor& cursor,
                              RemoteInputSample& output,
                              bool& updated,
                              std::string* error = nullptr);

}  // namespace stw
