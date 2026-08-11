#pragma once
// Radiance-.hdr-Loader (RGBE, RLE + plain) für HDR-Environments (Phase 3B).
#include <string>
#include <vector>

namespace stw {
// out: RGB float, scanline-major; return false bei Parse-Fehler (Fallback prozedural)
bool LoadHdr(const std::string& path, std::vector<float>& out, int& w, int& h);
}  // namespace stw
