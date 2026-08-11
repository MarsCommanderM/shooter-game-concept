#pragma once
// Versionierter Binary-Cache: magic + formatVersion + importerVersion + source-Timestamp/Hash.
// Vertex-Format-Änderung => STWC_VERSION++ ; Importer-Logik-Änderung => IMPORTER_VERSION++.
#include <array>
#include <cstdint>
namespace stw {
inline constexpr std::array<char, 4> STWC_MAGIC{'S', 'T', 'W', 'C'};
inline constexpr std::uint32_t STWC_VERSION = 3;        // Vertex v4: +tangent
inline constexpr std::uint32_t IMPORTER_VERSION = 2;    // +normalTexture, tangents
struct StwcHeader {
    std::array<char, 4> magic = STWC_MAGIC;
    std::uint32_t formatVersion = STWC_VERSION;
    std::uint32_t importerVersion = IMPORTER_VERSION;
    std::uint64_t sourceTimestamp = 0;
    std::uint64_t sourceHash = 0;
    std::uint32_t meshCount = 0;
    std::uint32_t materialCount = 0;
    std::uint32_t textureCount = 0;
};
inline bool IsCacheCompatible(const StwcHeader& h, std::uint64_t sourceTimestamp, std::uint64_t sourceHash) {
    return h.magic == STWC_MAGIC && h.formatVersion == STWC_VERSION && h.importerVersion == IMPORTER_VERSION &&
           h.sourceTimestamp == sourceTimestamp && h.sourceHash == sourceHash;
}
}  // namespace stw
