#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace stw {

// Writes an RGB8 image as a standards-compliant PNG without an external image
// dependency. When flipVertically is true, OpenGL's lower-left origin is
// converted to the conventional PNG top-left origin. The final rename keeps
// readers from observing a half-written frame.
bool WriteRgb8PngAtomic(const std::string& path,
                        int width,
                        int height,
                        const std::vector<std::uint8_t>& pixels,
                        bool flipVertically,
                        std::string* error = nullptr);

}  // namespace stw
