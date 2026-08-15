#include "FrameCapture.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <limits>
#include <utility>

#if defined(STW_HAVE_ZLIB)
#include <zlib.h>
#endif

namespace stw {
namespace {

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

void AppendBigEndian(std::vector<std::uint8_t>& output, std::uint32_t value) {
  output.push_back(static_cast<std::uint8_t>((value >> 24) & 0xffu));
  output.push_back(static_cast<std::uint8_t>((value >> 16) & 0xffu));
  output.push_back(static_cast<std::uint8_t>((value >> 8) & 0xffu));
  output.push_back(static_cast<std::uint8_t>(value & 0xffu));
}

std::uint32_t Crc32(const std::uint8_t* data, std::size_t size) {
  std::uint32_t crc = 0xffffffffu;
  for (std::size_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (int bit = 0; bit < 8; ++bit) {
      const std::uint32_t mask = 0u - (crc & 1u);
      crc = (crc >> 1) ^ (0xedb88320u & mask);
    }
  }
  return crc ^ 0xffffffffu;
}

#if !defined(STW_HAVE_ZLIB)
std::uint32_t Adler32(const std::vector<std::uint8_t>& data) {
  constexpr std::uint32_t kModulus = 65521u;
  std::uint32_t a = 1u;
  std::uint32_t b = 0u;
  for (std::uint8_t value : data) {
    a = (a + value) % kModulus;
    b = (b + a) % kModulus;
  }
  return (b << 16) | a;
}
#endif

void AppendChunk(std::vector<std::uint8_t>& png,
                 const std::array<char, 4>& type,
                 const std::vector<std::uint8_t>& data) {
  AppendBigEndian(png, static_cast<std::uint32_t>(data.size()));
  const std::size_t crcStart = png.size();
  for (char character : type) {
    png.push_back(static_cast<std::uint8_t>(character));
  }
  png.insert(png.end(), data.begin(), data.end());
  AppendBigEndian(png, Crc32(png.data() + crcStart, png.size() - crcStart));
}

bool BuildPng(int width,
              int height,
              const std::vector<std::uint8_t>& pixels,
              bool flipVertically,
              std::vector<std::uint8_t>& output,
              std::string* error) {
  if (width <= 0 || height <= 0) {
    return Fail(error, "frame capture dimensions must be positive");
  }
  const std::size_t widthSize = static_cast<std::size_t>(width);
  const std::size_t heightSize = static_cast<std::size_t>(height);
  if (widthSize > (std::numeric_limits<std::size_t>::max() / 3u) ||
      widthSize * 3u >
          (std::numeric_limits<std::size_t>::max() / heightSize)) {
    return Fail(error, "frame capture dimensions overflow addressable memory");
  }
  const std::size_t rowBytes = widthSize * 3u;
  const std::size_t pixelBytes = rowBytes * heightSize;
  if (pixels.size() != pixelBytes) {
    return Fail(error, "frame capture pixel count does not match its dimensions");
  }
  if (rowBytes == std::numeric_limits<std::size_t>::max() ||
      rowBytes + 1u >
          (std::numeric_limits<std::size_t>::max() / heightSize)) {
    return Fail(error, "frame capture scanline storage overflows");
  }

  std::vector<std::uint8_t> scanlines;
  scanlines.reserve((rowBytes + 1u) * heightSize);
  for (std::size_t outputRow = 0; outputRow < heightSize; ++outputRow) {
    scanlines.push_back(0u);  // PNG filter type: None
    const std::size_t sourceRow =
        flipVertically ? heightSize - outputRow - 1u : outputRow;
    const auto first = pixels.begin() +
                       static_cast<std::ptrdiff_t>(sourceRow * rowBytes);
    scanlines.insert(scanlines.end(), first,
                     first + static_cast<std::ptrdiff_t>(rowBytes));
  }
  if (scanlines.size() >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return Fail(error, "frame capture is too large for one PNG data chunk");
  }

  std::vector<std::uint8_t> compressed;
#if defined(STW_HAVE_ZLIB)
  if (scanlines.size() >
      static_cast<std::size_t>(std::numeric_limits<uLong>::max())) {
    return Fail(error, "frame capture exceeds zlib input limits");
  }
  uLongf compressedSize = compressBound(static_cast<uLong>(scanlines.size()));
  compressed.resize(static_cast<std::size_t>(compressedSize));
  const int compressionResult = compress2(
      reinterpret_cast<Bytef*>(compressed.data()), &compressedSize,
      reinterpret_cast<const Bytef*>(scanlines.data()),
      static_cast<uLong>(scanlines.size()), Z_BEST_SPEED);
  if (compressionResult != Z_OK) {
    return Fail(error, "zlib failed to compress the frame capture");
  }
  compressed.resize(static_cast<std::size_t>(compressedSize));
#else
  // Self-contained fallback: RFC 1950 header plus uncompressed RFC 1951
  // blocks. This keeps capture available when zlib development files are not
  // present, while CMake uses zlib level 1 when the existing host provides it.
  compressed = {0x78u, 0x01u};
  std::size_t offset = 0;
  do {
    const std::size_t remaining = scanlines.size() - offset;
    const std::size_t blockSize = std::min<std::size_t>(remaining, 65535u);
    const bool finalBlock = offset + blockSize == scanlines.size();
    compressed.push_back(finalBlock ? 0x01u : 0x00u);
    const std::uint16_t length = static_cast<std::uint16_t>(blockSize);
    const std::uint16_t inverseLength = static_cast<std::uint16_t>(~length);
    compressed.push_back(static_cast<std::uint8_t>(length & 0xffu));
    compressed.push_back(static_cast<std::uint8_t>((length >> 8) & 0xffu));
    compressed.push_back(static_cast<std::uint8_t>(inverseLength & 0xffu));
    compressed.push_back(
        static_cast<std::uint8_t>((inverseLength >> 8) & 0xffu));
    compressed.insert(compressed.end(),
                      scanlines.begin() + static_cast<std::ptrdiff_t>(offset),
                      scanlines.begin() +
                          static_cast<std::ptrdiff_t>(offset + blockSize));
    offset += blockSize;
  } while (offset < scanlines.size());
  AppendBigEndian(compressed, Adler32(scanlines));
#endif
  if (compressed.size() >
      static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
    return Fail(error, "compressed frame is too large for one PNG data chunk");
  }

  std::vector<std::uint8_t> png{
      0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au};
  std::vector<std::uint8_t> header;
  AppendBigEndian(header, static_cast<std::uint32_t>(width));
  AppendBigEndian(header, static_cast<std::uint32_t>(height));
  header.insert(header.end(), {8u, 2u, 0u, 0u, 0u});  // RGB8, no interlace
  AppendChunk(png, {'I', 'H', 'D', 'R'}, header);
  AppendChunk(png, {'I', 'D', 'A', 'T'}, compressed);
  AppendChunk(png, {'I', 'E', 'N', 'D'}, {});
  output = std::move(png);
  return true;
}

}  // namespace

bool WriteRgb8PngAtomic(const std::string& path,
                        int width,
                        int height,
                        const std::vector<std::uint8_t>& pixels,
                        bool flipVertically,
                        std::string* error) {
  if (error) error->clear();
  if (path.empty()) return Fail(error, "frame capture path must not be empty");

  std::vector<std::uint8_t> png;
  if (!BuildPng(width, height, pixels, flipVertically, png, error)) {
    return false;
  }

  const std::string temporaryPath = path + ".tmp";
  {
    std::ofstream file(temporaryPath, std::ios::binary | std::ios::trunc);
    if (!file) return Fail(error, "failed to open temporary frame capture");
    file.write(reinterpret_cast<const char*>(png.data()),
               static_cast<std::streamsize>(png.size()));
    file.flush();
    if (!file) return Fail(error, "failed to write temporary frame capture");
  }
  if (std::rename(temporaryPath.c_str(), path.c_str()) != 0) {
    return Fail(error, "failed to publish frame capture atomically");
  }
  return true;
}

}  // namespace stw
