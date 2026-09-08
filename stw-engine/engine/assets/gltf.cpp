#include "gltf.h"
#include "assets/TangentGenerator.hpp"
#include "assets/StwcFormat.hpp"
#include "json_mini.h"

#include <sys/stat.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace stw {

namespace {

constexpr int kComponentUnsignedByte = 5121;
constexpr int kComponentUnsignedShort = 5123;
constexpr int kComponentUnsignedInt = 5125;
constexpr int kComponentFloat = 5126;

bool FailImport(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool IsFinite(float value) {
  return std::isfinite(static_cast<double>(value));
}

bool IsFinite(const glm::mat4& matrix) {
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      if (!IsFinite(matrix[column][row])) return false;
    }
  }
  return true;
}

bool NormalizeQuaternion(glm::quat& value) {
  const double lengthSquared =
      static_cast<double>(value.w) * value.w +
      static_cast<double>(value.x) * value.x +
      static_cast<double>(value.y) * value.y +
      static_cast<double>(value.z) * value.z;
  if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0) return false;

  const double length = std::sqrt(lengthSquared);
  value = glm::quat(static_cast<float>(value.w / length),
                    static_cast<float>(value.x / length),
                    static_cast<float>(value.y / length),
                    static_cast<float>(value.z / length));
  return IsFinite(value.w) && IsFinite(value.x) && IsFinite(value.y) && IsFinite(value.z);
}

void ComputeBounds(StwMesh& mesh) {
  if (mesh.pos.size() < 3) return;
  for (int component = 0; component < 3; ++component) {
    mesh.bmin[component] = mesh.bmax[component] = mesh.pos[component];
  }
  for (std::size_t index = 3; index + 2 < mesh.pos.size(); index += 3) {
    for (int component = 0; component < 3; ++component) {
      mesh.bmin[component] = std::min(mesh.bmin[component], mesh.pos[index + component]);
      mesh.bmax[component] = std::max(mesh.bmax[component], mesh.pos[index + component]);
    }
  }
}

std::vector<uint8_t> ReadFile(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return {};
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
}

bool B64Decode(const std::string& input, std::vector<uint8_t>& output) {
  auto valueOf = [](char character) -> int {
    if (character >= 'A' && character <= 'Z') return character - 'A';
    if (character >= 'a' && character <= 'z') return character - 'a' + 26;
    if (character >= '0' && character <= '9') return character - '0' + 52;
    if (character == '+') return 62;
    if (character == '/') return 63;
    return -1;
  };

  output.clear();
  uint32_t buffer = 0;
  int bits = 0;
  for (char character : input) {
    const int value = valueOf(character);
    if (value < 0) continue;
    buffer = (buffer << 6) | static_cast<uint32_t>(value);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      output.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
      buffer = bits == 0 ? 0u : buffer & ((1u << bits) - 1u);
    }
  }
  return true;
}

time_t MTime(const std::string& path) {
  struct stat info {};
  if (stat(path.c_str(), &info) != 0) return 0;
  return info.st_mtime;
}

// ---------- Binary cache (static models only) ----------
void W32(std::ofstream& file, uint32_t value) {
  file.write(reinterpret_cast<char*>(&value), 4);
}

void WF(std::ofstream& file, float value) {
  file.write(reinterpret_cast<char*>(&value), 4);
}

uint32_t R32(std::ifstream& file) {
  uint32_t value = 0;
  file.read(reinterpret_cast<char*>(&value), 4);
  return value;
}

float RF(std::ifstream& file) {
  float value = 0.0f;
  file.read(reinterpret_cast<char*>(&value), 4);
  return value;
}

uint64_t FnvHash(const std::vector<uint8_t>& data) {
  uint64_t hash = 1469598103934665603ull;
  for (uint8_t byte : data) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

constexpr uint32_t CACHE_VERSION = stw::STWC_VERSION;

bool TryCache(const std::string& source, StwModel& out) {
  const std::string cachePath = source + ".stwc";
  if (MTime(cachePath) <= MTime(source)) return false;

  const std::vector<uint8_t> raw = ReadFile(source);
  const uint64_t sourceHash = FnvHash(raw);
  std::ifstream file(cachePath, std::ios::binary);
  char magic[4];
  file.read(magic, 4);
  if (!file || std::memcmp(magic, "STWC", 4) != 0) return false;

  const uint32_t version = R32(file);
  const uint32_t hashLow = R32(file);
  const uint32_t hashHigh = R32(file);
  if (version != CACHE_VERSION) return false;
  if (hashLow != static_cast<uint32_t>(sourceHash & 0xFFFFFFFF) ||
      hashHigh != static_cast<uint32_t>(sourceHash >> 32)) {
    return false;
  }

  const uint32_t meshCount = R32(file);
  const uint32_t materialCount = R32(file);
  out.meshes.resize(meshCount);
  out.mats.resize(materialCount);
  out.meshMat.resize(meshCount);

  for (StwMaterial& material : out.mats) {
    for (int component = 0; component < 4; ++component) material.base[component] = RF(file);
    material.metallic = RF(file);
    material.roughness = RF(file);
    material.normalScale = RF(file);
    material.hasNormal = R32(file) != 0;
  }

  for (uint32_t meshIndex = 0; meshIndex < meshCount; ++meshIndex) {
    out.meshMat[meshIndex] = static_cast<int>(R32(file));
    StwMesh& mesh = out.meshes[meshIndex];

    const uint32_t positionCount = R32(file);
    mesh.pos.resize(positionCount);
    for (float& value : mesh.pos) value = RF(file);

    const uint32_t normalCount = R32(file);
    mesh.nrm.resize(normalCount);
    for (float& value : mesh.nrm) value = RF(file);

    const uint32_t uvCount = R32(file);
    mesh.uv.resize(uvCount);
    for (float& value : mesh.uv) value = RF(file);

    const uint32_t tangentCount = R32(file);
    mesh.tan.resize(tangentCount);
    for (float& value : mesh.tan) value = RF(file);

    const uint32_t indexCount = R32(file);
    mesh.idx.resize(indexCount);
    for (uint32_t& value : mesh.idx) value = R32(file);
  }
  return file.good();
}

void WriteCache(const std::string& source, const StwModel& model, uint64_t sourceHash) {
  const std::string cachePath = source + ".stwc";
  std::ofstream file(cachePath, std::ios::binary);
  if (!file) return;

  file.write("STWC", 4);
  W32(file, CACHE_VERSION);
  W32(file, static_cast<uint32_t>(sourceHash & 0xFFFFFFFF));
  W32(file, static_cast<uint32_t>(sourceHash >> 32));
  W32(file, static_cast<uint32_t>(model.meshes.size()));
  W32(file, static_cast<uint32_t>(model.mats.size()));

  for (const StwMaterial& material : model.mats) {
    for (int component = 0; component < 4; ++component) WF(file, material.base[component]);
    WF(file, material.metallic);
    WF(file, material.roughness);
    WF(file, material.normalScale);
    W32(file, material.hasNormal ? 1 : 0);
  }

  for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
    W32(file, static_cast<uint32_t>(model.meshMat[meshIndex]));
    const StwMesh& mesh = model.meshes[meshIndex];

    W32(file, static_cast<uint32_t>(mesh.pos.size()));
    for (float value : mesh.pos) WF(file, value);
    W32(file, static_cast<uint32_t>(mesh.nrm.size()));
    for (float value : mesh.nrm) WF(file, value);
    W32(file, static_cast<uint32_t>(mesh.uv.size()));
    for (float value : mesh.uv) WF(file, value);
    W32(file, static_cast<uint32_t>(mesh.tan.size()));
    for (float value : mesh.tan) WF(file, value);
    W32(file, static_cast<uint32_t>(mesh.idx.size()));
    for (uint32_t value : mesh.idx) W32(file, value);
  }
}

bool ReadJsonSize(const Json* value, std::size_t& out) {
  if (!value || value->t != Json::T::Num || !std::isfinite(value->num) ||
      value->num < 0.0 || std::floor(value->num) != value->num ||
      value->num > static_cast<double>(std::numeric_limits<std::size_t>::max())) {
    return false;
  }
  out = static_cast<std::size_t>(value->num);
  return true;
}

bool ReadJsonInt(const Json* value, int& out) {
  std::size_t parsed = 0;
  if (!ReadJsonSize(value, parsed) ||
      parsed > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return false;
  }
  out = static_cast<int>(parsed);
  return true;
}

bool CheckedAdd(std::size_t first, std::size_t second, std::size_t& out) {
  if (first > std::numeric_limits<std::size_t>::max() - second) return false;
  out = first + second;
  return true;
}

bool CheckedMultiply(std::size_t first, std::size_t second, std::size_t& out) {
  if (first != 0 && second > std::numeric_limits<std::size_t>::max() / first) return false;
  out = first * second;
  return true;
}

enum class AccessorShape {
  Scalar,
  Vec2,
  Vec3,
  Vec4,
  Mat4,
};

std::size_t ShapeComponentCount(AccessorShape shape) {
  switch (shape) {
    case AccessorShape::Scalar: return 1;
    case AccessorShape::Vec2: return 2;
    case AccessorShape::Vec3: return 3;
    case AccessorShape::Vec4: return 4;
    case AccessorShape::Mat4: return 16;
  }
  return 0;
}

bool ParseAccessorShape(const Json* value, AccessorShape& out) {
  if (!value || value->t != Json::T::Str) return false;
  if (value->str == "SCALAR") {
    out = AccessorShape::Scalar;
  } else if (value->str == "VEC2") {
    out = AccessorShape::Vec2;
  } else if (value->str == "VEC3") {
    out = AccessorShape::Vec3;
  } else if (value->str == "VEC4") {
    out = AccessorShape::Vec4;
  } else if (value->str == "MAT4") {
    out = AccessorShape::Mat4;
  } else {
    return false;
  }
  return true;
}

std::size_t ComponentSize(int componentType) {
  switch (componentType) {
    case kComponentUnsignedByte: return 1;
    case kComponentUnsignedShort: return 2;
    case kComponentUnsignedInt:
    case kComponentFloat: return 4;
    default: return 0;
  }
}

struct AccessorView {
  const uint8_t* data = nullptr;
  std::size_t count = 0;
  std::size_t stride = 0;
  std::size_t componentSize = 0;
  int componentType = 0;
  AccessorShape shape = AccessorShape::Scalar;
  bool normalized = false;
};

struct AccessorContext {
  const std::vector<uint8_t>& buffer;
  const Json& bufferViews;
  const Json& accessors;
};

bool BuildAccessorView(const AccessorContext& context,
                       int accessorIndex,
                       AccessorView& out) {
  if (accessorIndex < 0 ||
      static_cast<std::size_t>(accessorIndex) >= context.accessors.arr.size()) {
    return false;
  }

  const Json& accessor = context.accessors.arr[static_cast<std::size_t>(accessorIndex)];
  if (accessor.t != Json::T::Obj || accessor.get("sparse")) return false;

  std::size_t bufferViewIndex = 0;
  if (!ReadJsonSize(accessor.get("bufferView"), bufferViewIndex) ||
      bufferViewIndex >= context.bufferViews.arr.size()) {
    return false;
  }

  int componentType = 0;
  if (!ReadJsonInt(accessor.get("componentType"), componentType)) return false;
  const std::size_t componentSize = ComponentSize(componentType);
  if (componentSize == 0) return false;

  AccessorShape shape;
  if (!ParseAccessorShape(accessor.get("type"), shape)) return false;

  std::size_t count = 0;
  if (!ReadJsonSize(accessor.get("count"), count) || count == 0) return false;

  bool normalized = false;
  if (const Json* normalizedJson = accessor.get("normalized")) {
    if (normalizedJson->t != Json::T::Bool) return false;
    normalized = normalizedJson->b;
  }
  if (normalized && componentType == kComponentFloat) return false;

  const Json& bufferView = context.bufferViews.arr[bufferViewIndex];
  if (bufferView.t != Json::T::Obj) return false;

  if (const Json* bufferIndexJson = bufferView.get("buffer")) {
    std::size_t bufferIndex = 0;
    if (!ReadJsonSize(bufferIndexJson, bufferIndex) || bufferIndex != 0) return false;
  }

  std::size_t viewOffset = 0;
  if (const Json* value = bufferView.get("byteOffset")) {
    if (!ReadJsonSize(value, viewOffset)) return false;
  }
  std::size_t viewLength = 0;
  if (!ReadJsonSize(bufferView.get("byteLength"), viewLength)) return false;

  std::size_t accessorOffset = 0;
  if (const Json* value = accessor.get("byteOffset")) {
    if (!ReadJsonSize(value, accessorOffset)) return false;
  }

  std::size_t elementSize = 0;
  if (!CheckedMultiply(componentSize, ShapeComponentCount(shape), elementSize)) return false;

  std::size_t stride = elementSize;
  if (const Json* value = bufferView.get("byteStride")) {
    if (!ReadJsonSize(value, stride) || stride < elementSize ||
        stride % componentSize != 0 || stride > 252) {
      return false;
    }
  }

  std::size_t viewEnd = 0;
  if (!CheckedAdd(viewOffset, viewLength, viewEnd) ||
      viewEnd > context.buffer.size() || accessorOffset > viewLength) {
    return false;
  }
  std::size_t dataOffset = 0;
  if (!CheckedAdd(viewOffset, accessorOffset, dataOffset) ||
      dataOffset % componentSize != 0) {
    return false;
  }

  std::size_t stridedBytes = 0;
  if (!CheckedMultiply(count - 1, stride, stridedBytes)) return false;
  std::size_t accessorEnd = 0;
  if (!CheckedAdd(accessorOffset, stridedBytes, accessorEnd) ||
      !CheckedAdd(accessorEnd, elementSize, accessorEnd) ||
      accessorEnd > viewLength) {
    return false;
  }

  out.data = context.buffer.data() + dataOffset;
  out.count = count;
  out.stride = stride;
  out.componentSize = componentSize;
  out.componentType = componentType;
  out.shape = shape;
  out.normalized = normalized;
  return true;
}

bool ReadUnsignedComponent(const AccessorView& view,
                           std::size_t element,
                           std::size_t component,
                           uint32_t& out) {
  const uint8_t* source =
      view.data + element * view.stride + component * view.componentSize;
  if (view.componentType == kComponentUnsignedByte) {
    out = *source;
    return true;
  }
  if (view.componentType == kComponentUnsignedShort) {
    uint16_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    out = value;
    return true;
  }
  if (view.componentType == kComponentUnsignedInt) {
    std::memcpy(&out, source, sizeof(out));
    return true;
  }
  return false;
}

bool ReadFloatComponent(const AccessorView& view,
                        std::size_t element,
                        std::size_t component,
                        float& out) {
  const uint8_t* source =
      view.data + element * view.stride + component * view.componentSize;
  if (view.componentType == kComponentFloat) {
    std::memcpy(&out, source, sizeof(out));
    return IsFinite(out);
  }

  uint32_t integer = 0;
  if (!ReadUnsignedComponent(view, element, component, integer)) return false;
  if (view.normalized) {
    if (view.componentType == kComponentUnsignedByte) {
      out = static_cast<float>(integer) / 255.0f;
    } else if (view.componentType == kComponentUnsignedShort) {
      out = static_cast<float>(integer) / 65535.0f;
    } else {
      out = static_cast<float>(
          static_cast<double>(integer) /
          static_cast<double>(std::numeric_limits<uint32_t>::max()));
    }
  } else {
    out = static_cast<float>(integer);
  }
  return IsFinite(out);
}

bool DecodeFloatAccessor(const AccessorContext& context,
                         int accessorIndex,
                         AccessorShape expectedShape,
                         std::vector<float>& output,
                         std::size_t& elementCount) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view) ||
      view.shape != expectedShape) {
    return false;
  }

  const std::size_t componentCount = ShapeComponentCount(view.shape);
  std::size_t valueCount = 0;
  if (!CheckedMultiply(view.count, componentCount, valueCount)) return false;
  output.resize(valueCount);
  for (std::size_t element = 0; element < view.count; ++element) {
    for (std::size_t component = 0; component < componentCount; ++component) {
      if (!ReadFloatComponent(view, element, component,
                              output[element * componentCount + component])) {
        return false;
      }
    }
  }
  elementCount = view.count;
  return true;
}

bool DecodeIndices(const AccessorContext& context,
                   int accessorIndex,
                   std::vector<uint32_t>& output) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view) ||
      view.shape != AccessorShape::Scalar || view.normalized ||
      (view.componentType != kComponentUnsignedByte &&
       view.componentType != kComponentUnsignedShort &&
       view.componentType != kComponentUnsignedInt)) {
    return false;
  }

  output.resize(view.count);
  for (std::size_t index = 0; index < view.count; ++index) {
    if (!ReadUnsignedComponent(view, index, 0, output[index])) return false;
  }
  return true;
}

bool DecodeJointIndices(const AccessorContext& context,
                        int accessorIndex,
                        std::vector<std::array<uint32_t, 4>>& output) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view) ||
      view.shape != AccessorShape::Vec4 || view.normalized ||
      (view.componentType != kComponentUnsignedByte &&
       view.componentType != kComponentUnsignedShort)) {
    return false;
  }

  output.resize(view.count);
  for (std::size_t element = 0; element < view.count; ++element) {
    for (std::size_t component = 0; component < 4; ++component) {
      if (!ReadUnsignedComponent(view, element, component,
                                 output[element][component])) {
        return false;
      }
    }
  }
  return true;
}

bool DecodeWeights(const AccessorContext& context,
                   int accessorIndex,
                   std::vector<glm::vec4>& output) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view) ||
      view.shape != AccessorShape::Vec4 ||
      (view.componentType != kComponentFloat &&
       view.componentType != kComponentUnsignedByte &&
       view.componentType != kComponentUnsignedShort)) {
    return false;
  }
  if (view.componentType != kComponentFloat && !view.normalized) return false;

  output.resize(view.count);
  for (std::size_t element = 0; element < view.count; ++element) {
    float values[4] = {};
    for (std::size_t component = 0; component < 4; ++component) {
      if (!ReadFloatComponent(view, element, component, values[component])) {
        return false;
      }
    }
    output[element] = glm::vec4(values[0], values[1], values[2], values[3]);
  }
  return true;
}

bool DecodeInverseBindMatrices(const AccessorContext& context,
                               int accessorIndex,
                               std::size_t expectedCount,
                               std::vector<glm::mat4>& output) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view) ||
      view.shape != AccessorShape::Mat4 ||
      view.componentType != kComponentFloat || view.normalized ||
      view.count != expectedCount) {
    return false;
  }

  output.assign(view.count, glm::mat4(1.0f));
  for (std::size_t element = 0; element < view.count; ++element) {
    glm::mat4 matrix(1.0f);
    for (std::size_t component = 0; component < 16; ++component) {
      float value = 0.0f;
      if (!ReadFloatComponent(view, element, component, value)) return false;
      matrix[static_cast<int>(component / 4)][static_cast<int>(component % 4)] = value;
    }
    if (!IsFinite(matrix)) return false;
    output[element] = matrix;
  }
  return true;
}

bool ReadFloatArray(const Json* value,
                    std::size_t expectedCount,
                    std::vector<float>& output) {
  if (!value || value->t != Json::T::Arr || value->arr.size() != expectedCount) {
    return false;
  }
  output.resize(expectedCount);
  for (std::size_t index = 0; index < expectedCount; ++index) {
    if (value->arr[index].t != Json::T::Num ||
        !std::isfinite(value->arr[index].num)) {
      return false;
    }
    output[index] = static_cast<float>(value->arr[index].num);
    if (!IsFinite(output[index])) return false;
  }
  return true;
}

bool DocumentRequiresUncachedRuntimeData(const Json& document) {
  if (document.get("animations")) return true;
  if (document.get("skins")) return true;
  const Json* meshes = document.get("meshes");
  if (!meshes || meshes->t != Json::T::Arr) return false;
  for (const Json& mesh : meshes->arr) {
    const Json* primitives = mesh.get("primitives");
    if (!primitives || primitives->t != Json::T::Arr) continue;
    for (const Json& primitive : primitives->arr) {
      const Json* attributes = primitive.get("attributes");
      if (!attributes || attributes->t != Json::T::Obj) continue;
      if (attributes->get("JOINTS_0") || attributes->get("WEIGHTS_0")) {
        return true;
      }
    }
  }
  return false;
}

bool MatrixToQuaternion(double r00, double r01, double r02,
                        double r10, double r11, double r12,
                        double r20, double r21, double r22,
                        glm::quat& output) {
  double w = 0.0;
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  const double trace = r00 + r11 + r22;

  if (trace > 0.0) {
    const double factor = std::sqrt(trace + 1.0) * 2.0;
    if (!std::isfinite(factor) || factor <= 1.0e-12) return false;
    w = 0.25 * factor;
    x = (r21 - r12) / factor;
    y = (r02 - r20) / factor;
    z = (r10 - r01) / factor;
  } else if (r00 > r11 && r00 > r22) {
    const double factor = std::sqrt(1.0 + r00 - r11 - r22) * 2.0;
    if (!std::isfinite(factor) || factor <= 1.0e-12) return false;
    w = (r21 - r12) / factor;
    x = 0.25 * factor;
    y = (r01 + r10) / factor;
    z = (r02 + r20) / factor;
  } else if (r11 > r22) {
    const double factor = std::sqrt(1.0 + r11 - r00 - r22) * 2.0;
    if (!std::isfinite(factor) || factor <= 1.0e-12) return false;
    w = (r02 - r20) / factor;
    x = (r01 + r10) / factor;
    y = 0.25 * factor;
    z = (r12 + r21) / factor;
  } else {
    const double factor = std::sqrt(1.0 + r22 - r00 - r11) * 2.0;
    if (!std::isfinite(factor) || factor <= 1.0e-12) return false;
    w = (r10 - r01) / factor;
    x = (r02 + r20) / factor;
    y = (r12 + r21) / factor;
    z = 0.25 * factor;
  }

  output = glm::quat(static_cast<float>(w), static_cast<float>(x),
                     static_cast<float>(y), static_cast<float>(z));
  return NormalizeQuaternion(output);
}

bool NearlyEqual(double first, double second, double tolerance = 2.0e-4) {
  const double magnitude = std::max({1.0, std::abs(first), std::abs(second)});
  return std::abs(first - second) <= tolerance * magnitude;
}

bool DecomposeNodeMatrix(const glm::mat4& matrix,
                         glm::vec3& translation,
                         glm::quat& rotation,
                         glm::vec3& scale) {
  if (!IsFinite(matrix) ||
      !NearlyEqual(matrix[0][3], 0.0) ||
      !NearlyEqual(matrix[1][3], 0.0) ||
      !NearlyEqual(matrix[2][3], 0.0) ||
      !NearlyEqual(matrix[3][3], 1.0)) {
    return false;
  }

  double columns[3][3] = {
      {matrix[0][0], matrix[0][1], matrix[0][2]},
      {matrix[1][0], matrix[1][1], matrix[1][2]},
      {matrix[2][0], matrix[2][1], matrix[2][2]},
  };

  double scales[3] = {};
  for (int column = 0; column < 3; ++column) {
    scales[column] = std::sqrt(
        columns[column][0] * columns[column][0] +
        columns[column][1] * columns[column][1] +
        columns[column][2] * columns[column][2]);
    if (!std::isfinite(scales[column]) || scales[column] <= 1.0e-8) return false;
  }

  const double rawDeterminant =
      columns[0][0] * (columns[1][1] * columns[2][2] -
                       columns[1][2] * columns[2][1]) -
      columns[1][0] * (columns[0][1] * columns[2][2] -
                       columns[0][2] * columns[2][1]) +
      columns[2][0] * (columns[0][1] * columns[1][2] -
                       columns[0][2] * columns[1][1]);
  if (!std::isfinite(rawDeterminant) || std::abs(rawDeterminant) <= 1.0e-12) {
    return false;
  }
  if (rawDeterminant < 0.0) scales[0] = -scales[0];

  double rotationColumns[3][3] = {};
  for (int column = 0; column < 3; ++column) {
    for (int row = 0; row < 3; ++row) {
      rotationColumns[column][row] = columns[column][row] / scales[column];
    }
  }

  for (int column = 0; column < 3; ++column) {
    const double lengthSquared =
        rotationColumns[column][0] * rotationColumns[column][0] +
        rotationColumns[column][1] * rotationColumns[column][1] +
        rotationColumns[column][2] * rotationColumns[column][2];
    if (!NearlyEqual(lengthSquared, 1.0)) return false;
  }
  for (int first = 0; first < 3; ++first) {
    for (int second = first + 1; second < 3; ++second) {
      const double dot =
          rotationColumns[first][0] * rotationColumns[second][0] +
          rotationColumns[first][1] * rotationColumns[second][1] +
          rotationColumns[first][2] * rotationColumns[second][2];
      if (!NearlyEqual(dot, 0.0)) return false;
    }
  }

  const double rotationDeterminant =
      rotationColumns[0][0] *
          (rotationColumns[1][1] * rotationColumns[2][2] -
           rotationColumns[1][2] * rotationColumns[2][1]) -
      rotationColumns[1][0] *
          (rotationColumns[0][1] * rotationColumns[2][2] -
           rotationColumns[0][2] * rotationColumns[2][1]) +
      rotationColumns[2][0] *
          (rotationColumns[0][1] * rotationColumns[1][2] -
           rotationColumns[0][2] * rotationColumns[1][1]);
  if (!NearlyEqual(rotationDeterminant, 1.0)) return false;

  if (!MatrixToQuaternion(
          rotationColumns[0][0], rotationColumns[1][0], rotationColumns[2][0],
          rotationColumns[0][1], rotationColumns[1][1], rotationColumns[2][1],
          rotationColumns[0][2], rotationColumns[1][2], rotationColumns[2][2],
          rotation)) {
    return false;
  }

  translation = glm::vec3(matrix[3][0], matrix[3][1], matrix[3][2]);
  scale = glm::vec3(static_cast<float>(scales[0]),
                    static_cast<float>(scales[1]),
                    static_cast<float>(scales[2]));
  if (!IsFinite(translation.x) || !IsFinite(translation.y) || !IsFinite(translation.z) ||
      !IsFinite(scale.x) || !IsFinite(scale.y) || !IsFinite(scale.z)) {
    return false;
  }

  const glm::mat4 recomposed =
      glm::translate(glm::mat4(1.0f), translation) *
      glm::mat4_cast(rotation) *
      glm::scale(glm::mat4(1.0f), scale);
  if (!IsFinite(recomposed)) return false;
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      if (!NearlyEqual(recomposed[column][row], matrix[column][row])) return false;
    }
  }
  return true;
}

struct GltfNode {
  std::string name;
  int parent = -1;
  int mesh = -1;
  int skin = -1;
  glm::vec3 translation{0.0f};
  glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
  glm::vec3 scale{1.0f};
};

bool ParseNodeTransform(const Json& node, GltfNode& output) {
  const Json* matrixJson = node.get("matrix");
  const bool hasTrs =
      node.get("translation") || node.get("rotation") || node.get("scale");
  if (matrixJson && hasTrs) return false;

  if (matrixJson) {
    std::vector<float> values;
    if (!ReadFloatArray(matrixJson, 16, values)) return false;
    glm::mat4 matrix(1.0f);
    for (std::size_t component = 0; component < values.size(); ++component) {
      matrix[static_cast<int>(component / 4)][static_cast<int>(component % 4)] =
          values[component];
    }
    return DecomposeNodeMatrix(matrix, output.translation,
                               output.rotation, output.scale);
  }

  if (const Json* translationJson = node.get("translation")) {
    std::vector<float> values;
    if (!ReadFloatArray(translationJson, 3, values)) return false;
    output.translation = glm::vec3(values[0], values[1], values[2]);
  }
  if (const Json* rotationJson = node.get("rotation")) {
    std::vector<float> values;
    if (!ReadFloatArray(rotationJson, 4, values)) return false;
    // glTF stores [x, y, z, w], while GLM's constructor is [w, x, y, z].
    output.rotation = glm::quat(values[3], values[0], values[1], values[2]);
    if (!NormalizeQuaternion(output.rotation)) return false;
  }
  if (const Json* scaleJson = node.get("scale")) {
    std::vector<float> values;
    if (!ReadFloatArray(scaleJson, 3, values)) return false;
    output.scale = glm::vec3(values[0], values[1], values[2]);
  }

  return IsFinite(output.translation.x) && IsFinite(output.translation.y) &&
         IsFinite(output.translation.z) && IsFinite(output.rotation.w) &&
         IsFinite(output.rotation.x) && IsFinite(output.rotation.y) &&
         IsFinite(output.rotation.z) && IsFinite(output.scale.x) &&
         IsFinite(output.scale.y) && IsFinite(output.scale.z);
}

bool ParseNodes(const Json& document, std::vector<GltfNode>& output) {
  const Json* nodesJson = document.get("nodes");
  if (!nodesJson) {
    output.clear();
    return true;
  }
  if (nodesJson->t != Json::T::Arr) return false;

  output.assign(nodesJson->arr.size(), GltfNode{});
  std::vector<std::vector<std::size_t>> children(nodesJson->arr.size());
  for (std::size_t nodeIndex = 0; nodeIndex < nodesJson->arr.size(); ++nodeIndex) {
    const Json& nodeJson = nodesJson->arr[nodeIndex];
    if (nodeJson.t != Json::T::Obj) return false;
    GltfNode& node = output[nodeIndex];

    if (const Json* name = nodeJson.get("name")) {
      if (name->t != Json::T::Str) return false;
      node.name = name->str;
    }
    if (const Json* mesh = nodeJson.get("mesh")) {
      if (!ReadJsonInt(mesh, node.mesh)) return false;
    }
    if (const Json* skin = nodeJson.get("skin")) {
      if (!ReadJsonInt(skin, node.skin)) return false;
    }
    if (!ParseNodeTransform(nodeJson, node)) return false;

    if (const Json* childrenJson = nodeJson.get("children")) {
      if (childrenJson->t != Json::T::Arr) return false;
      std::vector<unsigned char> seen(nodesJson->arr.size(), 0);
      for (const Json& childJson : childrenJson->arr) {
        std::size_t childIndex = 0;
        if (!ReadJsonSize(&childJson, childIndex) ||
            childIndex >= nodesJson->arr.size() ||
            childIndex == nodeIndex || seen[childIndex]) {
          return false;
        }
        seen[childIndex] = 1;
        children[nodeIndex].push_back(childIndex);
      }
    }
  }

  for (std::size_t parentIndex = 0; parentIndex < children.size(); ++parentIndex) {
    for (std::size_t childIndex : children[parentIndex]) {
      if (output[childIndex].parent != -1) return false;
      if (parentIndex > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return false;
      }
      output[childIndex].parent = static_cast<int>(parentIndex);
    }
  }

  std::vector<unsigned char> state(output.size(), 0);
  std::function<bool(std::size_t)> visit = [&](std::size_t nodeIndex) {
    if (state[nodeIndex] == 2) return true;
    if (state[nodeIndex] == 1) return false;
    state[nodeIndex] = 1;
    for (std::size_t childIndex : children[nodeIndex]) {
      if (!visit(childIndex)) return false;
    }
    state[nodeIndex] = 2;
    return true;
  };
  for (std::size_t nodeIndex = 0; nodeIndex < output.size(); ++nodeIndex) {
    if (!visit(nodeIndex)) return false;
  }
  return true;
}

bool ParseMaterials(const Json& document, StwModel& output) {
  const Json* materials = document.get("materials");
  if (materials) {
    if (materials->t != Json::T::Arr) return false;
    for (const Json& materialJson : materials->arr) {
      if (materialJson.t != Json::T::Obj) return false;
      StwMaterial material;
      if (const Json* pbr = materialJson.get("pbrMetallicRoughness")) {
        if (pbr->t != Json::T::Obj) return false;
        if (const Json* baseColor = pbr->get("baseColorFactor")) {
          std::vector<float> values;
          if (!ReadFloatArray(baseColor, 4, values)) return false;
          for (std::size_t component = 0; component < 4; ++component) {
            material.base[component] = values[component];
          }
        }
        if (const Json* metallic = pbr->get("metallicFactor")) {
          if (metallic->t != Json::T::Num || !std::isfinite(metallic->num)) return false;
          material.metallic = static_cast<float>(metallic->num);
          if (!IsFinite(material.metallic)) return false;
        } else {
          material.metallic = 1.0f;
        }
        if (const Json* roughness = pbr->get("roughnessFactor")) {
          if (roughness->t != Json::T::Num || !std::isfinite(roughness->num)) return false;
          material.roughness = static_cast<float>(roughness->num);
          if (!IsFinite(material.roughness)) return false;
        } else {
          material.roughness = 1.0f;
        }
      }
      output.mats.push_back(material);
    }
  }
  if (output.mats.empty()) output.mats.push_back(StwMaterial{});
  return true;
}

bool ParseSkins(const Json& document,
                const std::vector<GltfNode>& nodes,
                const AccessorContext& accessors,
                StwModel& output) {
  const Json* skinsJson = document.get("skins");
  if (!skinsJson) return true;
  if (skinsJson->t != Json::T::Arr) return false;

  for (const Json& skinJson : skinsJson->arr) {
    if (skinJson.t != Json::T::Obj) return false;
    const Json* jointsJson = skinJson.get("joints");
    if (!jointsJson || jointsJson->t != Json::T::Arr || jointsJson->arr.empty()) {
      return false;
    }

    StwSkin skin;
    if (const Json* name = skinJson.get("name")) {
      if (name->t != Json::T::Str) return false;
      skin.name = name->str;
    }

    if (const Json* skeletonRoot = skinJson.get("skeleton")) {
      if (!ReadJsonInt(skeletonRoot, skin.skeletonRootNode) ||
          static_cast<std::size_t>(skin.skeletonRootNode) >= nodes.size()) {
        return false;
      }
    }

    std::vector<unsigned char> seen(nodes.size(), 0);
    for (const Json& jointJson : jointsJson->arr) {
      std::size_t nodeIndex = 0;
      if (!ReadJsonSize(&jointJson, nodeIndex) || nodeIndex >= nodes.size() ||
          nodeIndex > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max()) ||
          seen[nodeIndex]) {
        return false;
      }
      seen[nodeIndex] = 1;
      skin.jointNodes.push_back(static_cast<uint32_t>(nodeIndex));
    }

    std::vector<glm::mat4> inverseBindMatrices(
        skin.jointNodes.size(), glm::mat4(1.0f));
    if (const Json* inverseBindAccessor = skinJson.get("inverseBindMatrices")) {
      int accessorIndex = 0;
      if (!ReadJsonInt(inverseBindAccessor, accessorIndex) ||
          !DecodeInverseBindMatrices(accessors, accessorIndex,
                                     skin.jointNodes.size(),
                                     inverseBindMatrices)) {
        return false;
      }
    }

    if (skin.jointNodes.size() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
      return false;
    }
    std::vector<int> nodeToJoint(nodes.size(), -1);
    for (std::size_t jointIndex = 0; jointIndex < skin.jointNodes.size(); ++jointIndex) {
      nodeToJoint[skin.jointNodes[jointIndex]] = static_cast<int>(jointIndex);
    }

    SkeletonBuildInput skeletonInput;
    skeletonInput.joints.reserve(skin.jointNodes.size());
    for (std::size_t jointIndex = 0; jointIndex < skin.jointNodes.size(); ++jointIndex) {
      const std::size_t nodeIndex = skin.jointNodes[jointIndex];
      const GltfNode& node = nodes[nodeIndex];
      SkeletonJoint joint;
      joint.name = node.name;
      joint.bindTranslation = node.translation;
      joint.bindRotation = node.rotation;
      joint.bindScale = node.scale;
      joint.inverseBindMatrix = inverseBindMatrices[jointIndex];
      if (node.parent >= 0) {
        joint.parent = nodeToJoint[static_cast<std::size_t>(node.parent)];
      }
      skeletonInput.joints.push_back(joint);
    }

    std::string skeletonError;
    if (!Skeleton::Build(skeletonInput, skin.skeleton, &skeletonError)) return false;
    output.skins.push_back(std::move(skin));
  }
  return true;
}

struct GltfAnimationSampler {
  std::vector<float> keyTimes;
  int outputAccessor = -1;
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
};

struct GltfAnimationChannel {
  std::size_t targetNode = 0;
  AnimationTarget targetPath = AnimationTarget::Translation;
  AnimationInterpolation interpolation = AnimationInterpolation::Linear;
  std::vector<float> keyTimes;
  AnimationValues values{AnimationVec3Values{}};
};

bool DecodeAnimationTimes(const AccessorContext& context,
                          int accessorIndex,
                          std::vector<float>& output,
                          std::string* error,
                          const std::string& prefix) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view)) {
    return FailImport(error, prefix + " input accessor has an invalid range or layout");
  }
  if (view.shape != AccessorShape::Scalar) {
    return FailImport(error, prefix + " input accessor must be SCALAR");
  }
  if (view.componentType != kComponentFloat || view.normalized) {
    return FailImport(error, prefix + " input accessor must use non-normalized FLOAT values");
  }

  std::vector<float> candidate(view.count, 0.0f);
  float previous = 0.0f;
  for (std::size_t keyIndex = 0; keyIndex < view.count; ++keyIndex) {
    float value = 0.0f;
    if (!ReadFloatComponent(view, keyIndex, 0, value) || !IsFinite(value)) {
      return FailImport(error, prefix + " input contains a non-finite key time");
    }
    if (value < 0.0f) {
      return FailImport(error, prefix + " input contains a negative key time");
    }
    if (keyIndex > 0 && value < previous) {
      return FailImport(error, prefix + " input key times are not monotonic");
    }
    candidate[keyIndex] = value;
    previous = value;
  }

  output = std::move(candidate);
  return true;
}

bool DecodeAnimationVec3(const AccessorContext& context,
                         int accessorIndex,
                         std::size_t expectedCount,
                         AnimationVec3Values& output,
                         std::string* error,
                         const std::string& prefix) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view)) {
    return FailImport(error, prefix + " output accessor has an invalid range or layout");
  }
  if (view.shape != AccessorShape::Vec3) {
    return FailImport(error, prefix + " output accessor must be VEC3");
  }
  if (view.componentType != kComponentFloat || view.normalized) {
    return FailImport(error, prefix + " output accessor must use non-normalized FLOAT values");
  }
  if (view.count != expectedCount) {
    return FailImport(error, prefix + " input/output key counts do not match");
  }

  AnimationVec3Values candidate(view.count, glm::vec3(0.0f));
  for (std::size_t keyIndex = 0; keyIndex < view.count; ++keyIndex) {
    float components[3] = {};
    for (std::size_t component = 0; component < 3; ++component) {
      if (!ReadFloatComponent(view, keyIndex, component, components[component])) {
        return FailImport(error, prefix + " output contains a non-finite vec3 value");
      }
    }
    candidate[keyIndex] = glm::vec3(components[0], components[1], components[2]);
  }

  output = std::move(candidate);
  return true;
}

bool DecodeAnimationQuaternions(const AccessorContext& context,
                                int accessorIndex,
                                std::size_t expectedCount,
                                AnimationQuatValues& output,
                                std::string* error,
                                const std::string& prefix) {
  AccessorView view;
  if (!BuildAccessorView(context, accessorIndex, view)) {
    return FailImport(error, prefix + " output accessor has an invalid range or layout");
  }
  if (view.shape != AccessorShape::Vec4) {
    return FailImport(error, prefix + " rotation output accessor must be VEC4");
  }
  if (view.componentType != kComponentFloat || view.normalized) {
    return FailImport(error, prefix + " rotation output accessor must use non-normalized FLOAT values");
  }
  if (view.count != expectedCount) {
    return FailImport(error, prefix + " input/output key counts do not match");
  }

  AnimationQuatValues candidate;
  candidate.reserve(view.count);
  for (std::size_t keyIndex = 0; keyIndex < view.count; ++keyIndex) {
    float components[4] = {};
    for (std::size_t component = 0; component < 4; ++component) {
      if (!ReadFloatComponent(view, keyIndex, component, components[component])) {
        return FailImport(error, prefix + " output contains a non-finite rotation");
      }
    }
    // glTF stores [x, y, z, w], while GLM's constructor is [w, x, y, z].
    glm::quat rotation(components[3], components[0], components[1], components[2]);
    if (!NormalizeQuaternion(rotation)) {
      return FailImport(error, prefix + " output contains a zero-length rotation");
    }
    candidate.push_back(rotation);
  }

  output = std::move(candidate);
  return true;
}

bool ParseAnimationInterpolation(const Json* value,
                                 AnimationInterpolation& output,
                                 std::string* error,
                                 const std::string& prefix) {
  if (!value) {
    output = AnimationInterpolation::Linear;
    return true;
  }
  if (value->t != Json::T::Str) {
    return FailImport(error, prefix + " interpolation must be a string");
  }
  if (value->str == "STEP") {
    output = AnimationInterpolation::Step;
    return true;
  }
  if (value->str == "LINEAR") {
    output = AnimationInterpolation::Linear;
    return true;
  }
  if (value->str == "CUBICSPLINE") {
    return FailImport(error, prefix + " uses unsupported CUBICSPLINE interpolation");
  }
  return FailImport(error, prefix + " uses an unknown interpolation mode");
}

bool ParseAnimationTarget(const Json* value,
                          AnimationTarget& output,
                          std::string* error,
                          const std::string& prefix) {
  if (!value || value->t != Json::T::Str) {
    return FailImport(error, prefix + " target path must be a string");
  }
  if (value->str == "translation") {
    output = AnimationTarget::Translation;
    return true;
  }
  if (value->str == "rotation") {
    output = AnimationTarget::Rotation;
    return true;
  }
  if (value->str == "scale") {
    output = AnimationTarget::Scale;
    return true;
  }
  if (value->str == "weights") {
    return FailImport(error, prefix + " uses unsupported morph weights animation");
  }
  return FailImport(error, prefix + " uses an unknown target path");
}

bool ParseAnimations(const Json& document,
                     const std::vector<GltfNode>& nodes,
                     const AccessorContext& accessors,
                     StwModel& output,
                     std::string* error) {
  const Json* animationsJson = document.get("animations");
  if (!animationsJson) {
    output.animations.clear();
    return true;
  }
  if (animationsJson->t != Json::T::Arr) {
    return FailImport(error, "glTF animations must be an array");
  }

  std::vector<StwAnimation> importedAnimations;
  importedAnimations.reserve(animationsJson->arr.size());
  for (std::size_t animationIndex = 0;
       animationIndex < animationsJson->arr.size();
       ++animationIndex) {
    if (animationIndex >
        static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
      return FailImport(error, "glTF animation index exceeds the model index range");
    }

    const Json& animationJson = animationsJson->arr[animationIndex];
    const std::string animationPrefix =
        "glTF animation " + std::to_string(animationIndex);
    if (animationJson.t != Json::T::Obj) {
      return FailImport(error, animationPrefix + " must be an object");
    }

    const Json* samplersJson = animationJson.get("samplers");
    const Json* channelsJson = animationJson.get("channels");
    if (!samplersJson || samplersJson->t != Json::T::Arr ||
        samplersJson->arr.empty()) {
      return FailImport(error, animationPrefix + " must contain samplers");
    }
    if (!channelsJson || channelsJson->t != Json::T::Arr ||
        channelsJson->arr.empty()) {
      return FailImport(error, animationPrefix + " must contain channels");
    }

    StwAnimation imported;
    imported.sourceAnimationIndex = static_cast<uint32_t>(animationIndex);
    imported.skinClips.resize(output.skins.size());
    if (const Json* name = animationJson.get("name")) {
      if (name->t != Json::T::Str) {
        return FailImport(error, animationPrefix + " name must be a string");
      }
      imported.name = name->str;
    }

    std::vector<GltfAnimationSampler> samplers;
    samplers.reserve(samplersJson->arr.size());
    for (std::size_t samplerIndex = 0;
         samplerIndex < samplersJson->arr.size();
         ++samplerIndex) {
      const Json& samplerJson = samplersJson->arr[samplerIndex];
      const std::string samplerPrefix = animationPrefix + " sampler " +
                                        std::to_string(samplerIndex);
      if (samplerJson.t != Json::T::Obj) {
        return FailImport(error, samplerPrefix + " must be an object");
      }

      int inputAccessor = -1;
      GltfAnimationSampler sampler;
      if (!ReadJsonInt(samplerJson.get("input"), inputAccessor)) {
        return FailImport(error, samplerPrefix + " has an invalid input accessor");
      }
      if (!ReadJsonInt(samplerJson.get("output"), sampler.outputAccessor) ||
          static_cast<std::size_t>(sampler.outputAccessor) >= accessors.accessors.arr.size()) {
        return FailImport(error, samplerPrefix + " has an invalid output accessor");
      }
      if (!ParseAnimationInterpolation(samplerJson.get("interpolation"),
                                       sampler.interpolation,
                                       error, samplerPrefix) ||
          !DecodeAnimationTimes(accessors, inputAccessor, sampler.keyTimes,
                                error, samplerPrefix)) {
        return false;
      }
      samplers.push_back(std::move(sampler));
    }

    std::vector<GltfAnimationChannel> sourceChannels;
    sourceChannels.reserve(channelsJson->arr.size());
    std::set<std::pair<std::size_t, AnimationTarget>> uniqueTargets;
    for (std::size_t channelIndex = 0;
         channelIndex < channelsJson->arr.size();
         ++channelIndex) {
      const Json& channelJson = channelsJson->arr[channelIndex];
      const std::string channelPrefix = animationPrefix + " channel " +
                                        std::to_string(channelIndex);
      if (channelJson.t != Json::T::Obj) {
        return FailImport(error, channelPrefix + " must be an object");
      }

      std::size_t samplerIndex = 0;
      if (!ReadJsonSize(channelJson.get("sampler"), samplerIndex) ||
          samplerIndex >= samplers.size()) {
        return FailImport(error, channelPrefix + " has an invalid sampler index");
      }
      const Json* targetJson = channelJson.get("target");
      if (!targetJson || targetJson->t != Json::T::Obj) {
        return FailImport(error, channelPrefix + " target must be an object");
      }

      GltfAnimationChannel channel;
      if (!ReadJsonSize(targetJson->get("node"), channel.targetNode) ||
          channel.targetNode >= nodes.size()) {
        return FailImport(error, channelPrefix + " has an invalid target node");
      }
      if (!ParseAnimationTarget(targetJson->get("path"), channel.targetPath,
                                error, channelPrefix)) {
        return false;
      }
      if (!uniqueTargets.emplace(channel.targetNode, channel.targetPath).second) {
        return FailImport(error, channelPrefix +
                                     " duplicates a target node/path channel");
      }

      const GltfAnimationSampler& sampler = samplers[samplerIndex];
      channel.interpolation = sampler.interpolation;
      channel.keyTimes = sampler.keyTimes;
      if (channel.targetPath == AnimationTarget::Rotation) {
        AnimationQuatValues values;
        if (!DecodeAnimationQuaternions(
                accessors, sampler.outputAccessor, sampler.keyTimes.size(),
                values, error, channelPrefix)) {
          return false;
        }
        channel.values = std::move(values);
      } else {
        AnimationVec3Values values;
        if (!DecodeAnimationVec3(
                accessors, sampler.outputAccessor, sampler.keyTimes.size(),
                values, error, channelPrefix)) {
          return false;
        }
        channel.values = std::move(values);
      }
      sourceChannels.push_back(std::move(channel));
    }

    for (std::size_t skinIndex = 0; skinIndex < output.skins.size(); ++skinIndex) {
      const StwSkin& skin = output.skins[skinIndex];
      if (skin.jointNodes.size() >
          static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        return FailImport(error, animationPrefix + " skin joint count is too large");
      }

      std::vector<int> nodeToJoint(nodes.size(), -1);
      for (std::size_t jointIndex = 0;
           jointIndex < skin.jointNodes.size();
           ++jointIndex) {
        const std::size_t nodeIndex = skin.jointNodes[jointIndex];
        if (nodeIndex >= nodes.size()) {
          return FailImport(error, animationPrefix +
                                       " skin contains an invalid joint node");
        }
        nodeToJoint[nodeIndex] = static_cast<int>(jointIndex);
      }

      AnimationClipInput clipInput;
      clipInput.name = imported.name;
      for (const GltfAnimationChannel& sourceChannel : sourceChannels) {
        const int jointIndex = nodeToJoint[sourceChannel.targetNode];
        if (jointIndex < 0) continue;

        AnimationChannel channel;
        channel.targetIndex = jointIndex;
        channel.targetPath = sourceChannel.targetPath;
        channel.interpolation = sourceChannel.interpolation;
        channel.keyTimes = sourceChannel.keyTimes;
        channel.values = sourceChannel.values;
        clipInput.channels.push_back(std::move(channel));
      }

      if (clipInput.channels.empty()) continue;
      AnimationClip clip;
      std::string clipError;
      if (!AnimationClip::Build(clipInput, clip, &clipError)) {
        return FailImport(error, animationPrefix + " failed to build skin " +
                                     std::to_string(skinIndex) + " clip: " + clipError);
      }
      imported.skinClips[skinIndex] = std::move(clip);
    }

    importedAnimations.push_back(std::move(imported));
  }

  output.animations = std::move(importedAnimations);
  return true;
}

bool ReadAccessorIndex(const Json* value, int& output) {
  return ReadJsonInt(value, output);
}

bool ParseMeshes(const Json& document,
                 const AccessorContext& accessors,
                 StwModel& output,
                 std::vector<std::vector<uint32_t>>& meshPrimitiveMap) {
  const Json* meshesJson = document.get("meshes");
  if (!meshesJson || meshesJson->t != Json::T::Arr) return false;
  meshPrimitiveMap.assign(meshesJson->arr.size(), {});

  for (std::size_t gltfMeshIndex = 0;
       gltfMeshIndex < meshesJson->arr.size();
       ++gltfMeshIndex) {
    const Json& meshJson = meshesJson->arr[gltfMeshIndex];
    if (meshJson.t != Json::T::Obj) return false;
    const Json* primitives = meshJson.get("primitives");
    if (!primitives || primitives->t != Json::T::Arr) return false;

    for (const Json& primitive : primitives->arr) {
      if (primitive.t != Json::T::Obj) return false;
      const Json* attributes = primitive.get("attributes");
      if (!attributes || attributes->t != Json::T::Obj) continue;

      const Json* positionsJson = attributes->get("POSITION");
      const Json* jointsJson = attributes->get("JOINTS_0");
      const Json* weightsJson = attributes->get("WEIGHTS_0");
      if ((jointsJson == nullptr) != (weightsJson == nullptr)) return false;
      if (!positionsJson) continue;

      StwMesh mesh;
      int accessorIndex = 0;
      if (!ReadAccessorIndex(positionsJson, accessorIndex)) return false;
      std::size_t vertexCount = 0;
      if (!DecodeFloatAccessor(accessors, accessorIndex, AccessorShape::Vec3,
                               mesh.pos, vertexCount)) {
        return false;
      }

      auto decodeOptionalFloatAttribute =
          [&](const char* name, AccessorShape shape,
              std::vector<float>& destination) {
            const Json* value = attributes->get(name);
            if (!value) return true;
            int index = 0;
            std::size_t count = 0;
            return ReadAccessorIndex(value, index) &&
                   DecodeFloatAccessor(accessors, index, shape,
                                       destination, count) &&
                   count == vertexCount;
          };

      if (!decodeOptionalFloatAttribute("NORMAL", AccessorShape::Vec3, mesh.nrm) ||
          !decodeOptionalFloatAttribute("TEXCOORD_0", AccessorShape::Vec2, mesh.uv) ||
          !decodeOptionalFloatAttribute("TANGENT", AccessorShape::Vec4, mesh.tan)) {
        return false;
      }

      if (jointsJson && weightsJson) {
        int jointAccessor = 0;
        int weightAccessor = 0;
        if (!ReadAccessorIndex(jointsJson, jointAccessor) ||
            !ReadAccessorIndex(weightsJson, weightAccessor)) {
          return false;
        }

        std::vector<std::array<uint32_t, 4>> joints;
        std::vector<glm::vec4> weights;
        if (!DecodeJointIndices(accessors, jointAccessor, joints) ||
            !DecodeWeights(accessors, weightAccessor, weights) ||
            joints.size() != vertexCount || weights.size() != vertexCount) {
          return false;
        }

        mesh.skinInfluences.resize(vertexCount);
        for (std::size_t vertex = 0; vertex < vertexCount; ++vertex) {
          mesh.skinInfluences[vertex].joints = joints[vertex];
          mesh.skinInfluences[vertex].weights = weights[vertex];
        }
      }

      if (const Json* indicesJson = primitive.get("indices")) {
        if (!ReadAccessorIndex(indicesJson, accessorIndex) ||
            !DecodeIndices(accessors, accessorIndex, mesh.idx)) {
          return false;
        }
        for (uint32_t index : mesh.idx) {
          if (index >= vertexCount) return false;
        }
      } else {
        if (vertexCount >
            static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
          return false;
        }
        mesh.idx.reserve(vertexCount);
        for (std::size_t index = 0; index < vertexCount; ++index) {
          mesh.idx.push_back(static_cast<uint32_t>(index));
        }
      }

      int materialIndex = 0;
      if (const Json* material = primitive.get("material")) {
        if (!ReadJsonInt(material, materialIndex) ||
            static_cast<std::size_t>(materialIndex) >= output.mats.size()) {
          return false;
        }
      }

      if (mesh.tan.empty() && !mesh.pos.empty() &&
          mesh.nrm.size() == mesh.pos.size()) {
        mesh.tan.resize(vertexCount * 4);
        GenerateTangentsArrays(mesh.pos.data(), mesh.nrm.data(),
                               mesh.uv.empty() ? nullptr : mesh.uv.data(),
                               mesh.idx.data(), mesh.idx.size(),
                               vertexCount, mesh.tan.data());
      }
      ComputeBounds(mesh);

      if (output.meshes.size() >
          static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
        return false;
      }
      const uint32_t flattenedIndex =
          static_cast<uint32_t>(output.meshes.size());
      output.meshMat.push_back(materialIndex);
      output.meshes.push_back(std::move(mesh));
      meshPrimitiveMap[gltfMeshIndex].push_back(flattenedIndex);
    }
  }
  return !output.meshes.empty();
}

bool BindSkinInstances(const std::vector<GltfNode>& nodes,
                       const std::vector<std::vector<uint32_t>>& meshPrimitiveMap,
                       StwModel& output) {
  std::vector<unsigned char> influenceWasBound(output.meshes.size(), 0);
  for (std::size_t nodeIndex = 0; nodeIndex < nodes.size(); ++nodeIndex) {
    const GltfNode& node = nodes[nodeIndex];
    if (node.mesh < 0) {
      if (node.skin >= 0) return false;
      continue;
    }
    if (static_cast<std::size_t>(node.mesh) >= meshPrimitiveMap.size()) return false;
    if (node.skin < 0) continue;
    if (static_cast<std::size_t>(node.skin) >= output.skins.size() ||
        nodeIndex > static_cast<std::size_t>(std::numeric_limits<uint32_t>::max())) {
      return false;
    }

    const StwSkin& skin = output.skins[static_cast<std::size_t>(node.skin)];
    for (uint32_t flattenedMeshIndex :
         meshPrimitiveMap[static_cast<std::size_t>(node.mesh)]) {
      StwMesh& mesh = output.meshes[flattenedMeshIndex];
      if (mesh.skinInfluences.empty()) {
        // A primitive without both skin attributes stays on the static path.
        continue;
      }

      for (SkinInfluence4& influence : mesh.skinInfluences) {
        std::string influenceError;
        if (!SkinInfluence4::ValidateAndNormalize(
                influence, skin.jointNodes.size(), &influenceError)) {
          return false;
        }
      }

      influenceWasBound[flattenedMeshIndex] = 1;
      StwSkinnedMesh instance;
      instance.meshIndex = flattenedMeshIndex;
      instance.skinIndex = static_cast<uint32_t>(node.skin);
      instance.nodeIndex = static_cast<uint32_t>(nodeIndex);
      output.skinnedMeshes.push_back(instance);
    }
  }

  // JOINTS_0/WEIGHTS_0 are skin-local. Without a node that supplies an active
  // skin there is no valid joint-count domain in which to validate them.
  for (std::size_t meshIndex = 0; meshIndex < output.meshes.size(); ++meshIndex) {
    if (!output.meshes[meshIndex].skinInfluences.empty() &&
        !influenceWasBound[meshIndex]) {
      return false;
    }
  }
  return true;
}

bool ExtractGlb(const std::vector<uint8_t>& raw,
                std::string& jsonText,
                std::vector<uint8_t>& binaryChunk) {
  if (raw.size() <= 12 || raw[0] != 'g' || raw[1] != 'l' ||
      raw[2] != 'T' || raw[3] != 'F') {
    jsonText.assign(reinterpret_cast<const char*>(raw.data()), raw.size());
    return true;
  }

  std::size_t offset = 12;
  while (offset + 8 <= raw.size()) {
    uint32_t length = 0;
    uint32_t type = 0;
    std::memcpy(&length, raw.data() + offset, 4);
    std::memcpy(&type, raw.data() + offset + 4, 4);
    offset += 8;
    if (length > raw.size() - offset) return false;

    if (type == 0x4E4F534A) {
      jsonText.assign(reinterpret_cast<const char*>(raw.data() + offset), length);
    } else if (type == 0x004E4942) {
      binaryChunk.assign(raw.begin() + static_cast<std::ptrdiff_t>(offset),
                         raw.begin() + static_cast<std::ptrdiff_t>(offset + length));
    }
    offset += length;
  }
  return !jsonText.empty();
}

bool LoadPrimaryBuffer(const Json& document,
                       const std::string& sourcePath,
                       const std::vector<uint8_t>& glbBinary,
                       std::vector<uint8_t>& output) {
  const Json* buffers = document.get("buffers");
  if (!buffers || buffers->t != Json::T::Arr || buffers->arr.empty() ||
      buffers->arr[0].t != Json::T::Obj) {
    return false;
  }

  const Json& buffer = buffers->arr[0];
  if (const Json* uri = buffer.get("uri")) {
    if (uri->t != Json::T::Str) return false;
    if (uri->str.rfind("data:", 0) == 0) {
      const std::size_t comma = uri->str.find(',');
      if (comma == std::string::npos ||
          !B64Decode(uri->str.substr(comma + 1), output)) {
        return false;
      }
    } else {
      const std::size_t slash = sourcePath.find_last_of("/\\");
      const std::string base =
          slash == std::string::npos ? std::string{} :
                                      sourcePath.substr(0, slash + 1);
      output = ReadFile(base + uri->str);
    }
  } else {
    output = glbBinary;
  }

  std::size_t declaredLength = 0;
  if (!ReadJsonSize(buffer.get("byteLength"), declaredLength) ||
      output.size() < declaredLength) {
    return false;
  }
  return true;
}

}  // namespace

bool LoadGLTF(const std::string& path, StwModel& out, std::string* error) {
  if (error) error->clear();
  const std::vector<uint8_t> raw = ReadFile(path);
  if (raw.empty()) return FailImport(error, "failed to read glTF source");

  std::string jsonText;
  std::vector<uint8_t> glbBinary;
  if (!ExtractGlb(raw, jsonText, glbBinary)) {
    return FailImport(error, "failed to extract glTF/GLB document");
  }

  Json document;
  if (!ParseJson(jsonText, document) || document.t != Json::T::Obj) {
    return FailImport(error, "failed to parse glTF JSON document");
  }

  const bool requiresUncachedRuntimeData =
      DocumentRequiresUncachedRuntimeData(document);
  if (!requiresUncachedRuntimeData) {
    StwModel cached;
    if (TryCache(path, cached)) {
      out = std::move(cached);
      return true;
    }
  }

  std::vector<uint8_t> binary;
  if (!LoadPrimaryBuffer(document, path, glbBinary, binary)) {
    return FailImport(error, "failed to load the primary glTF buffer");
  }

  const Json* bufferViews = document.get("bufferViews");
  const Json* accessors = document.get("accessors");
  if (!bufferViews || bufferViews->t != Json::T::Arr ||
      !accessors || accessors->t != Json::T::Arr) {
    return FailImport(error, "glTF bufferViews/accessors are missing or invalid");
  }
  const AccessorContext accessorContext{binary, *bufferViews, *accessors};

  StwModel candidate;
  if (!ParseMaterials(document, candidate)) {
    return FailImport(error, "failed to parse glTF materials");
  }

  std::vector<GltfNode> nodes;
  if (!ParseNodes(document, nodes)) {
    return FailImport(error, "failed to parse glTF nodes");
  }
  if (!ParseSkins(document, nodes, accessorContext, candidate)) {
    return FailImport(error, "failed to parse glTF skins");
  }
  if (!ParseAnimations(document, nodes, accessorContext, candidate, error)) {
    return false;
  }

  std::vector<std::vector<uint32_t>> meshPrimitiveMap;
  if (!ParseMeshes(document, accessorContext, candidate, meshPrimitiveMap)) {
    return FailImport(error, "failed to parse glTF meshes");
  }
  if (!BindSkinInstances(nodes, meshPrimitiveMap, candidate)) {
    return FailImport(error, "failed to bind glTF skin instances");
  }

  if (!requiresUncachedRuntimeData) {
    WriteCache(path, candidate, FnvHash(raw));
  }
  out = std::move(candidate);
  return true;
}

StwMesh MakeCubeMesh() {
  StwMesh mesh;
  const float corners[8][3] = {
      {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
      {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
      {-0.5f, -0.5f, 0.5f}, {0.5f, -0.5f, 0.5f},
      {0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f},
  };
  const int faces[6][4] = {
      {0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7},
      {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0},
  };
  const float normals[6][3] = {
      {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f, 1.0f},
      {-1.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f},
      {0.0f, 1.0f, 0.0f}, {0.0f, -1.0f, 0.0f},
  };

  for (int face = 0; face < 6; ++face) {
    const uint32_t base = static_cast<uint32_t>(mesh.pos.size() / 3);
    for (int vertex = 0; vertex < 4; ++vertex) {
      const float* position = corners[faces[face][vertex]];
      mesh.pos.insert(mesh.pos.end(), {position[0], position[1], position[2]});
      mesh.nrm.insert(mesh.nrm.end(),
                      {normals[face][0], normals[face][1], normals[face][2]});
      mesh.uv.insert(mesh.uv.end(),
                     {vertex == 1 || vertex == 2 ? 1.0f : 0.0f,
                      vertex >= 2 ? 1.0f : 0.0f});
    }
    mesh.idx.insert(mesh.idx.end(),
                    {base, base + 1, base + 2, base, base + 2, base + 3});
  }
  ComputeBounds(mesh);
  return mesh;
}

StwMesh MakeGroundMesh(float half) {
  StwMesh mesh;
  mesh.pos = {
      -half, 0.0f, -half,
      half, 0.0f, -half,
      half, 0.0f, half,
      -half, 0.0f, half,
  };
  mesh.nrm = {
      0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f,
  };
  mesh.uv = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
  mesh.idx = {0, 1, 2, 0, 2, 3};
  ComputeBounds(mesh);
  return mesh;
}

}  // namespace stw
