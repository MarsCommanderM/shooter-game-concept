// STW-ENGINE: OpenGL-4.5-Backend (Forward+, PBR metallic/roughness, ACES)
// Phase 2+: Render-Paesse getrennt (Shadow-Depth -> Main), Frustum-Culling, PCF-Shadows
#include "renderer.h"
#include "render/IBL.hpp"
#include "render/Vertex.hpp"
#include "animation/SkinningPalette.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace stw {

namespace {

template <typename F>
F Load(const char* n) {
  return reinterpret_cast<F>(SDL_GL_GetProcAddress(n));
}

PFNGLGENVERTEXARRAYSPROC glGenVertexArrays_ = nullptr;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray_ = nullptr;
PFNGLGENBUFFERSPROC glGenBuffers_ = nullptr;
PFNGLBINDBUFFERPROC glBindBuffer_ = nullptr;
PFNGLBUFFERDATAPROC glBufferData_ = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray_ = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer_ = nullptr;
PFNGLVERTEXATTRIBIPOINTERPROC glVertexAttribIPointer_ = nullptr;
PFNGLCREATESHADERPROC glCreateShader_ = nullptr;
PFNGLSHADERSOURCEPROC glShaderSource_ = nullptr;
PFNGLCOMPILESHADERPROC glCompileShader_ = nullptr;
PFNGLGETSHADERIVPROC glGetShaderiv_ = nullptr;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog_ = nullptr;
PFNGLCREATEPROGRAMPROC glCreateProgram_ = nullptr;
PFNGLATTACHSHADERPROC glAttachShader_ = nullptr;
PFNGLLINKPROGRAMPROC glLinkProgram_ = nullptr;
PFNGLGETPROGRAMIVPROC glGetProgramiv_ = nullptr;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog_ = nullptr;
PFNGLUSEPROGRAMPROC glUseProgram_ = nullptr;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation_ = nullptr;
PFNGLUNIFORM1IPROC glUniform1i_ = nullptr;
PFNGLUNIFORM1FPROC glUniform1f_ = nullptr;
PFNGLUNIFORM3FPROC glUniform3f_ = nullptr;
PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv_ = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers_ = nullptr;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ = nullptr;
PFNGLGENERATEMIPMAPPROC glGenerateMipmap_ = nullptr;
PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers_ = nullptr;
PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer_ = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D_ = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers_ = nullptr;

bool LoadGL() {
  glGenVertexArrays_ = Load<PFNGLGENVERTEXARRAYSPROC>("glGenVertexArrays");
  glBindVertexArray_ = Load<PFNGLBINDVERTEXARRAYPROC>("glBindVertexArray");
  glGenBuffers_ = Load<PFNGLGENBUFFERSPROC>("glGenBuffers");
  glBindBuffer_ = Load<PFNGLBINDBUFFERPROC>("glBindBuffer");
  glBufferData_ = Load<PFNGLBUFFERDATAPROC>("glBufferData");
  glEnableVertexAttribArray_ = Load<PFNGLENABLEVERTEXATTRIBARRAYPROC>("glEnableVertexAttribArray");
  glVertexAttribPointer_ = Load<PFNGLVERTEXATTRIBPOINTERPROC>("glVertexAttribPointer");
  glVertexAttribIPointer_ = Load<PFNGLVERTEXATTRIBIPOINTERPROC>("glVertexAttribIPointer");
  glCreateShader_ = Load<PFNGLCREATESHADERPROC>("glCreateShader");
  glShaderSource_ = Load<PFNGLSHADERSOURCEPROC>("glShaderSource");
  glCompileShader_ = Load<PFNGLCOMPILESHADERPROC>("glCompileShader");
  glGetShaderiv_ = Load<PFNGLGETSHADERIVPROC>("glGetShaderiv");
  glGetShaderInfoLog_ = Load<PFNGLGETSHADERINFOLOGPROC>("glGetShaderInfoLog");
  glCreateProgram_ = Load<PFNGLCREATEPROGRAMPROC>("glCreateProgram");
  glAttachShader_ = Load<PFNGLATTACHSHADERPROC>("glAttachShader");
  glLinkProgram_ = Load<PFNGLLINKPROGRAMPROC>("glLinkProgram");
  glGetProgramiv_ = Load<PFNGLGETPROGRAMIVPROC>("glGetProgramiv");
  glGetProgramInfoLog_ = Load<PFNGLGETPROGRAMINFOLOGPROC>("glGetProgramInfoLog");
  glUseProgram_ = Load<PFNGLUSEPROGRAMPROC>("glUseProgram");
  glGetUniformLocation_ = Load<PFNGLGETUNIFORMLOCATIONPROC>("glGetUniformLocation");
  glUniform1i_ = Load<PFNGLUNIFORM1IPROC>("glUniform1i");
  glUniform1f_ = Load<PFNGLUNIFORM1FPROC>("glUniform1f");
  glUniform3f_ = Load<PFNGLUNIFORM3FPROC>("glUniform3f");
  glUniformMatrix4fv_ = Load<PFNGLUNIFORMMATRIX4FVPROC>("glUniformMatrix4fv");
  glGenFramebuffers_ = Load<PFNGLGENFRAMEBUFFERSPROC>("glGenFramebuffers");
  glBindFramebuffer_ = Load<PFNGLBINDFRAMEBUFFERPROC>("glBindFramebuffer");
  glFramebufferTexture2D_ = Load<PFNGLFRAMEBUFFERTEXTURE2DPROC>("glFramebufferTexture2D");
  glDeleteFramebuffers_ = Load<PFNGLDELETEFRAMEBUFFERSPROC>("glDeleteFramebuffers");
  glDeleteBuffers_ = Load<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
  glDeleteVertexArrays_ = Load<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
  glGenerateMipmap_ = Load<PFNGLGENERATEMIPMAPPROC>("glGenerateMipmap");
  return glGenVertexArrays_ && glBindVertexArray_ && glGenBuffers_ &&
         glBindBuffer_ && glBufferData_ && glEnableVertexAttribArray_ &&
         glVertexAttribPointer_ && glVertexAttribIPointer_ &&
         glCreateShader_ && glShaderSource_ && glCompileShader_ &&
         glGetShaderiv_ && glGetShaderInfoLog_ && glCreateProgram_ &&
         glAttachShader_ && glLinkProgram_ && glGetProgramiv_ &&
         glGetProgramInfoLog_ && glUseProgram_ && glGetUniformLocation_ &&
         glUniform1i_ && glUniform1f_ && glUniform3f_ &&
         glUniformMatrix4fv_ && glGenFramebuffers_ && glBindFramebuffer_ &&
         glFramebufferTexture2D_ && glDeleteFramebuffers_ &&
         glDeleteBuffers_ && glDeleteVertexArrays_ && glGenerateMipmap_;
}

/* ---------------- Shaders ---------------- */
std::string BuildVertexShaderSource() {
  return std::string(R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTan;
layout(location=4) in uvec4 aJoints;
layout(location=5) in vec4 aWeights;
uniform mat4 uM;
uniform mat4 uVP;
uniform bool uHasSkin;
uniform mat4 uJoints[)") + std::to_string(kMaxSkinJoints) + R"(];
out vec3 vN; out vec3 vW; out vec2 vUV; out vec3 vT; out float vTS;

vec3 safeNormalize(vec3 value, vec3 fallbackValue) {
  float lengthSquared = dot(value, value);
  return lengthSquared > 1e-12 ? value * inversesqrt(lengthSquared) : fallbackValue;
}

void main() {
  vec4 localPosition = vec4(aPos, 1.0);
  vec3 localNormal = aNrm;
  vec3 localTangent = aTan.xyz;
  if (uHasSkin) {
    mat4 skin =
        aWeights.x * uJoints[aJoints.x] +
        aWeights.y * uJoints[aJoints.y] +
        aWeights.z * uJoints[aJoints.z] +
        aWeights.w * uJoints[aJoints.w];
    localPosition = skin * localPosition;
    // T3-A assumes rigid/uniform-scale joints for direction transforms.
    mat3 skinDirection = mat3(skin);
    localNormal = skinDirection * localNormal;
    localTangent = skinDirection * localTangent;
  }

  vec4 w = uM * localPosition;
  vW = w.xyz;
  mat3 nm = mat3(uM);
  vec3 N = safeNormalize(nm * localNormal, vec3(0.0, 1.0, 0.0));
  vec3 fallbackT = abs(N.y) < 0.999
      ? safeNormalize(cross(vec3(0.0, 1.0, 0.0), N), vec3(1.0, 0.0, 0.0))
      : safeNormalize(cross(vec3(1.0, 0.0, 0.0), N), vec3(0.0, 0.0, 1.0));
  vec3 T = nm * localTangent;
  T = safeNormalize(T - N * dot(N, T), fallbackT);
  vN = N; vT = T; vTS = aTan.w;
  vUV = aUV;
  gl_Position = uVP * w;
}
)";
}

const char* kFS = R"(
#version 330 core
in vec3 vN; in vec3 vW; in vec2 vUV; in vec3 vT; in float vTS;
uniform vec3 uCam;
uniform vec3 uBase; uniform float uMetal; uniform float uRough;
uniform float uHasNormal; uniform float uNormalScale;
uniform float uHasIBL; uniform float uMaxLod;
uniform vec3 uLightDir; uniform vec3 uLightCol; uniform vec3 uAmb; uniform float uStrength;
uniform sampler2D uShadow; uniform mat4 uLightVP;
uniform sampler2D uNormalTex;
uniform samplerCube uIrradiance; uniform samplerCube uPrefilter; uniform sampler2D uBrdfLut;
out vec4 oCol;

const float PI = 3.14159265;
float DGGX(float NoH, float r) {
  float a = r * r; float a2 = a * a;
  float d = NoH * NoH * (a2 - 1.0) + 1.0;
  return a2 / (PI * d * d + 1e-5);
}
float GSchlick(float NoV, float NoL, float r) {
  float k = (r + 1.0); k = k * k / 8.0;
  return (NoV / (NoV * (1.0 - k) + k)) * (NoL / (NoL * (1.0 - k) + k));
}
vec3 FSchlick(float VoH, vec3 F0) { return F0 + (1.0 - F0) * pow(1.0 - VoH, 5.0); }
vec3 aces(vec3 x) {
  return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}
float shadowFactor(vec3 w, float NoL) {
  vec4 sp = uLightVP * vec4(w, 1.0);
  vec3 p = sp.xyz / sp.w * 0.5 + 0.5;
  if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0) return 1.0;
  float bias = max(0.004 * (1.0 - NoL), 0.0015);
  vec2 texel = 1.0 / vec2(textureSize(uShadow, 0));
  float sh = 0.0;
  for (int dx = -1; dx <= 1; dx++)
    for (int dy = -1; dy <= 1; dy++) {
      float d = texture(uShadow, p.xy + vec2(dx, dy) * texel).r;
      sh += (p.z - bias) <= d ? 1.0 : 0.0;
    }
  return mix(0.3, 1.0, sh / 9.0);
}
vec3 getNormal() {
  vec3 N = normalize(vN);
  if (uHasNormal < 0.5) return N;
  vec3 T = normalize(vT);
  vec3 B = normalize(cross(N, T)) * vTS;
  mat3 TBN = mat3(T, B, N);
  vec3 tn = texture(uNormalTex, vUV).xyz * 2.0 - 1.0;
  tn.xy *= uNormalScale;
  return normalize(TBN * normalize(tn));
}
void main() {
  vec3 N = getNormal();
  vec3 V = normalize(uCam - vW);
  vec3 L = normalize(-uLightDir);
  vec3 H = normalize(V + L);
  float NoL = max(dot(N, L), 0.0);
  float NoV = max(dot(N, V), 1e-3);
  float NoH = max(dot(N, H), 0.0);
  float VoH = max(dot(V, H), 0.0);
  vec3 F0 = mix(vec3(0.04), uBase, uMetal);
  vec3 F = FSchlick(VoH, F0);
  float D = DGGX(NoH, max(uRough, 0.04));
  float G = GSchlick(NoV, NoL, max(uRough, 0.04));
  vec3 spec = D * G * F / (4.0 * NoV * NoL + 1e-4);
  vec3 kD = (1.0 - F) * (1.0 - uMetal);
  float sh = shadowFactor(vW, NoL);
  vec3 direct = (kD * uBase / PI + spec) * uLightCol * uStrength * NoL * sh;
  vec3 ambient;
  if (uHasIBL > 0.5) {
    vec3 Fi = FSchlick(NoV, F0);
    vec3 irr = texture(uIrradiance, N).rgb;
    vec3 diffIBL = irr * uBase;
    vec3 R = reflect(-V, N);
    vec3 pre = textureLod(uPrefilter, R, uRough * uMaxLod).rgb;
    vec2 brdf = texture(uBrdfLut, vec2(NoV, uRough)).rg;
    vec3 specIBL = pre * (Fi * brdf.x + brdf.y);
    ambient = (1.0 - Fi) * (1.0 - uMetal) * diffIBL + specIBL;
  } else {
    ambient = uAmb * uBase;
  }
  vec3 col = ambient + direct;
  col = aces(col);
  col = pow(col, vec3(1.0 / 2.2));
  oCol = vec4(col, 1.0);
}
)";

std::string BuildShadowVertexShaderSource() {
  return std::string(R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=4) in uvec4 aJoints;
layout(location=5) in vec4 aWeights;
uniform mat4 uM;
uniform mat4 uLVP;
uniform bool uHasSkin;
uniform mat4 uJoints[)") + std::to_string(kMaxSkinJoints) + R"(];
void main() {
  vec4 localPosition = vec4(aPos, 1.0);
  if (uHasSkin) {
    mat4 skin =
        aWeights.x * uJoints[aJoints.x] +
        aWeights.y * uJoints[aJoints.y] +
        aWeights.z * uJoints[aJoints.z] +
        aWeights.w * uJoints[aJoints.w];
    localPosition = skin * localPosition;
  }
  gl_Position = uLVP * uM * localPosition;
}
)";
}

const char* kShadowFS = R"(
#version 330 core
out vec4 oCol;
void main() { oCol = vec4(1.0); }
)";

/* ---------------- Frustum-Culling (Gribb/Hartmann) ---------------- */
struct Plane { glm::vec3 n; float d; };
void ExtractPlanes(const float m[16], Plane p[6]) {
  // m ist column-major: Zeilen via m[c*4+r]
  auto row = [&](int r, int k) { return m[k * 4 + r]; };
  glm::vec4 r0(row(0,0), row(0,1), row(0,2), row(0,3));
  glm::vec4 r1(row(1,0), row(1,1), row(1,2), row(1,3));
  glm::vec4 r2(row(2,0), row(2,1), row(2,2), row(2,3));
  glm::vec4 r3(row(3,0), row(3,1), row(3,2), row(3,3));
  glm::vec4 pl[6] = {r3 + r0, r3 - r0, r3 + r1, r3 - r1, r3 + r2, r3 - r2};
  for (int i = 0; i < 6; i++) {
    p[i].n = glm::vec3(pl[i]);
    p[i].d = pl[i].w;
    float len = glm::length(p[i].n);
    if (len > 1e-6f) { p[i].n /= len; p[i].d /= len; }
  }
}
bool SphereVisible(const Plane p[6], const glm::vec3& c, float r) {
  for (int i = 0; i < 6; i++) {
    if (glm::dot(p[i].n, c) + p[i].d < -r) return false;
  }
  return true;
}

bool Fail(std::string* error, const std::string& message) {
  if (error) *error = message;
  return false;
}

bool IsFinite(const glm::mat4& matrix) {
  for (int column = 0; column < 4; ++column) {
    for (int row = 0; row < 4; ++row) {
      if (!std::isfinite(static_cast<double>(matrix[column][row]))) return false;
    }
  }
  return true;
}

bool BuildGpuVertices(const StwMesh& mesh,
                      std::vector<GpuVertex>& output,
                      std::size_t& requiredJointCount,
                      std::string* error) {
  if (mesh.pos.empty() || mesh.pos.size() % 3 != 0) {
    return Fail(error, "mesh positions must contain complete vec3 vertices");
  }

  const std::size_t vertexCount = mesh.pos.size() / 3;
  const bool hasSkin = !mesh.skinInfluences.empty();
  if (hasSkin && mesh.skinInfluences.size() != vertexCount) {
    return Fail(error, "skin influence count must match vertex count");
  }
  for (std::uint32_t index : mesh.idx) {
    if (index >= vertexCount) return Fail(error, "mesh index is out of range");
  }

  std::vector<GpuVertex> candidate(vertexCount);
  std::size_t candidateRequiredJointCount = 0;
  for (std::size_t vertexIndex = 0; vertexIndex < vertexCount; ++vertexIndex) {
    GpuVertex& vertex = candidate[vertexIndex];
    for (std::size_t component = 0; component < 3; ++component) {
      vertex.position[component] = mesh.pos[vertexIndex * 3 + component];
      if (mesh.nrm.size() >= (vertexIndex + 1) * 3) {
        vertex.normal[component] = mesh.nrm[vertexIndex * 3 + component];
      }
    }
    if (mesh.uv.size() >= (vertexIndex + 1) * 2) {
      vertex.texCoord[0] = mesh.uv[vertexIndex * 2];
      vertex.texCoord[1] = mesh.uv[vertexIndex * 2 + 1];
    }
    if (mesh.tan.size() >= (vertexIndex + 1) * 4) {
      for (std::size_t component = 0; component < 4; ++component) {
        vertex.tangent[component] = mesh.tan[vertexIndex * 4 + component];
      }
    }

    if (!hasSkin) continue;
    const SkinInfluence4& influence = mesh.skinInfluences[vertexIndex];
    const float inputWeights[4] = {
        influence.weights.x,
        influence.weights.y,
        influence.weights.z,
        influence.weights.w,
    };
    double weightSum = 0.0;
    for (std::size_t component = 0; component < 4; ++component) {
      const float weight = inputWeights[component];
      if (!std::isfinite(static_cast<double>(weight)) || weight < 0.0f) {
        return Fail(error, "skin weights must be finite and non-negative");
      }
      if (weight > 0.0f) {
        const std::uint32_t joint = influence.joints[component];
        if (joint >= kMaxSkinJoints) {
          return Fail(error, "skin influence exceeds the GPU joint limit");
        }
        vertex.joints[component] = joint;
        candidateRequiredJointCount = std::max(
            candidateRequiredJointCount, static_cast<std::size_t>(joint) + 1);
      } else {
        // Auch Nullgewicht-Slots werden im GLSL-Ausdruck indiziert. Index 0
        // verhindert daher undefinierten Array-Zugriff trotz Gewicht 0.
        vertex.joints[component] = 0;
      }
      weightSum += static_cast<double>(weight);
    }
    if (!std::isfinite(weightSum) || weightSum <= 1.0e-8) {
      return Fail(error, "skin weights must have a positive finite sum");
    }
    for (std::size_t component = 0; component < 4; ++component) {
      vertex.weights[component] =
          static_cast<float>(static_cast<double>(inputWeights[component]) / weightSum);
      if (!std::isfinite(static_cast<double>(vertex.weights[component]))) {
        return Fail(error, "normalized GPU skin weight must be finite");
      }
    }
  }

  output = std::move(candidate);
  requiredJointCount = candidateRequiredJointCount;
  return true;
}

struct GLMesh {
  GLuint vao = 0, vbo = 0, ibo = 0;
  GLsizei count = 0;
  glm::vec3 bmin{0}, bmax{0};
  bool hasSkin = false;
  std::size_t requiredJointCount = 0;
  bool missingPaletteWarningIssued = false;
};

struct DrawCmd {
  uint32_t mesh;
  StwMaterial mat;
  float model[16];
  bool hasSkin = false;
  std::vector<glm::mat4> joints;
};

class GLRenderer : public IRenderer {
 public:
  bool Init(void* sdlWindow) override {
    win_ = static_cast<SDL_Window*>(sdlWindow);
    ctx_ = SDL_GL_CreateContext(win_);
    if (!ctx_) return false;
    SDL_GL_MakeCurrent(win_, ctx_);
    if (!LoadGL()) return false;
    const std::string vertexShader = BuildVertexShaderSource();
    const std::string shadowVertexShader = BuildShadowVertexShaderSource();
    prog_ = BuildProgram(vertexShader.c_str(), kFS);
    shadowProg_ = BuildProgram(shadowVertexShader.c_str(), kShadowFS);
    if (!prog_ || !shadowProg_) return false;
    uM_ = glGetUniformLocation_(prog_, "uM");
    uVP_ = glGetUniformLocation_(prog_, "uVP");
    uCam_ = glGetUniformLocation_(prog_, "uCam");
    uBase_ = glGetUniformLocation_(prog_, "uBase");
    uMetal_ = glGetUniformLocation_(prog_, "uMetal");
    uRough_ = glGetUniformLocation_(prog_, "uRough");
    uLD_ = glGetUniformLocation_(prog_, "uLightDir");
    uLC_ = glGetUniformLocation_(prog_, "uLightCol");
    uAmb_ = glGetUniformLocation_(prog_, "uAmb");
    uStr_ = glGetUniformLocation_(prog_, "uStrength");
    uShadow_ = glGetUniformLocation_(prog_, "uShadow");
    uHasNormal_ = glGetUniformLocation_(prog_, "uHasNormal");
    uNormalScale_ = glGetUniformLocation_(prog_, "uNormalScale");
    uNormalTex_ = glGetUniformLocation_(prog_, "uNormalTex");
    uHasIBL_ = glGetUniformLocation_(prog_, "uHasIBL");
    uMaxLod_ = glGetUniformLocation_(prog_, "uMaxLod");
    uIrr_ = glGetUniformLocation_(prog_, "uIrradiance");
    uPre_ = glGetUniformLocation_(prog_, "uPrefilter");
    uBrdf_ = glGetUniformLocation_(prog_, "uBrdfLut");
    uHasSkin_ = glGetUniformLocation_(prog_, "uHasSkin");
    uJoints_ = glGetUniformLocation_(prog_, "uJoints[0]");
    // 1x1 weiße Default-Normalmap
    glGenTextures(1, &whiteTex_);
    glBindTexture(GL_TEXTURE_2D, whiteTex_);
    { const unsigned char w[4] = {128, 128, 255, 255};
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, w); }
    normalTex_ = whiteTex_;
    uLVP_ = glGetUniformLocation_(prog_, "uLightVP");
    sM_ = glGetUniformLocation_(shadowProg_, "uM");
    sLVP_ = glGetUniformLocation_(shadowProg_, "uLVP");
    sHasSkin_ = glGetUniformLocation_(shadowProg_, "uHasSkin");
    sJoints_ = glGetUniformLocation_(shadowProg_, "uJoints[0]");
    if (uHasSkin_ < 0 || uJoints_ < 0 || sHasSkin_ < 0 || sJoints_ < 0) {
      SDL_Log("Skinning-Uniforms konnten nicht gebunden werden");
      return false;
    }

    // Shadow-Map-FBO (2048, Depth-only)
    glGenTextures(1, &shadowTex_);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, kShadowSize, kShadowSize, 0,
                 GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    const float border[4] = {1, 1, 1, 1};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border);
    glGenFramebuffers_(1, &shadowFbo_);
    glBindFramebuffer_(GL_FRAMEBUFFER, shadowFbo_);
    glFramebufferTexture2D_(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTex_, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    return true;
  }

  void BeginFrame() override {
    SDL_GL_MakeCurrent(win_, ctx_);
    cmds_.clear();
  }

  void SetViewProj(const float vp[16]) override {
    std::memcpy(vp_, vp, sizeof(vp_));
    ExtractPlanes(vp_, planes_);
    vpDirty_ = true;
  }
  void SetCameraPos(float x, float y, float z) override { cam_ = glm::vec3(x, y, z); }
  void SetLight(const Light& l) override { light_ = l; }

  uint32_t UploadMesh(const StwMesh& m) override {
    std::uint32_t handle = kInvalidMeshHandle;
    std::string error;
    if (!UploadMeshChecked(m, handle, &error)) {
      SDL_Log("Mesh-Upload abgelehnt: %s", error.c_str());
      return kInvalidMeshHandle;
    }
    return handle;
  }

  bool UploadMeshChecked(const StwMesh& m, uint32_t& handle,
                         std::string* error) override {
    if (error) error->clear();
    std::vector<GpuVertex> vertices;
    std::size_t requiredJointCount = 0;
    if (!BuildGpuVertices(m, vertices, requiredJointCount, error)) return false;
    if (meshes_.size() >= static_cast<std::size_t>(kInvalidMeshHandle)) {
      return Fail(error, "renderer mesh handle space is exhausted");
    }
    const std::size_t vertexBytes = vertices.size() * sizeof(GpuVertex);
    const std::size_t indexBytes = m.idx.size() * sizeof(std::uint32_t);
    if (vertexBytes > static_cast<std::size_t>(
                          std::numeric_limits<GLsizeiptr>::max()) ||
        indexBytes > static_cast<std::size_t>(
                         std::numeric_limits<GLsizeiptr>::max()) ||
        m.idx.size() > static_cast<std::size_t>(
                           std::numeric_limits<GLsizei>::max())) {
      return Fail(error, "mesh buffer is too large for OpenGL");
    }

    GLMesh g;
    g.bmin = glm::vec3(m.bmin[0], m.bmin[1], m.bmin[2]);
    g.bmax = glm::vec3(m.bmax[0], m.bmax[1], m.bmax[2]);
    g.hasSkin = !m.skinInfluences.empty();
    g.requiredJointCount = requiredJointCount;
    glGenVertexArrays_(1, &g.vao);
    glGenBuffers_(1, &g.vbo);
    glGenBuffers_(1, &g.ibo);
    glBindVertexArray_(g.vao);
    glBindBuffer_(GL_ARRAY_BUFFER, g.vbo);
    glBufferData_(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertexBytes),
                  vertices.data(), GL_STATIC_DRAW);
    const GLsizei stride = static_cast<GLsizei>(sizeof(GpuVertex));
    glEnableVertexAttribArray_(0);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<const void*>(offsetof(GpuVertex, position)));
    glEnableVertexAttribArray_(1);
    glVertexAttribPointer_(1, 3, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<const void*>(offsetof(GpuVertex, normal)));
    glEnableVertexAttribArray_(2);
    glVertexAttribPointer_(2, 2, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<const void*>(offsetof(GpuVertex, texCoord)));
    glEnableVertexAttribArray_(3);
    glVertexAttribPointer_(3, 4, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<const void*>(offsetof(GpuVertex, tangent)));
    glEnableVertexAttribArray_(4);
    glVertexAttribIPointer_(4, 4, GL_UNSIGNED_INT, stride,
                            reinterpret_cast<const void*>(offsetof(GpuVertex, joints)));
    glEnableVertexAttribArray_(5);
    glVertexAttribPointer_(5, 4, GL_FLOAT, GL_FALSE, stride,
                           reinterpret_cast<const void*>(offsetof(GpuVertex, weights)));
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(indexBytes),
                  m.idx.data(), GL_STATIC_DRAW);
    g.count = static_cast<GLsizei>(m.idx.size());
    meshes_.push_back(g);
    handle = static_cast<uint32_t>(meshes_.size() - 1);
    return true;
  }

  void Draw(uint32_t h, const StwMaterial& mat, const float model[16]) override {
    std::string error;
    if (!DrawWithSkinning(h, mat, model, nullptr, &error)) {
      bool shouldLog = true;
      if (h < meshes_.size() && meshes_[h].hasSkin) {
        shouldLog = !meshes_[h].missingPaletteWarningIssued;
        meshes_[h].missingPaletteWarningIssued = true;
      }
      if (shouldLog) SDL_Log("Draw abgelehnt: %s", error.c_str());
    }
  }

  bool DrawWithSkinning(uint32_t h, const StwMaterial& mat,
                        const float model[16],
                        const SkinningPalette* palette,
                        std::string* error) override {
    if (error) error->clear();
    if (h >= meshes_.size()) return Fail(error, "mesh handle is invalid");
    if (!model) return Fail(error, "model matrix pointer must not be null");
    for (std::size_t component = 0; component < 16; ++component) {
      if (!std::isfinite(static_cast<double>(model[component]))) {
        return Fail(error, "model matrix must be finite");
      }
    }

    const GLMesh& mesh = meshes_[h];
    const bool hasPalette = palette && !palette->empty();
    if (mesh.hasSkin && !hasPalette) {
      return Fail(error, "skinned mesh requires a non-empty skinning palette");
    }
    if (!mesh.hasSkin && hasPalette) {
      return Fail(error, "skinning palette cannot be applied to an unskinned mesh");
    }

    DrawCmd command{};
    command.mesh = h;
    command.mat = mat;
    std::memcpy(command.model, model, sizeof(command.model));
    if (hasPalette) {
      if (palette->size() > kMaxSkinJoints) {
        return Fail(error, "skinning palette exceeds the GPU joint limit");
      }
      if (palette->size() < mesh.requiredJointCount) {
        return Fail(error, "skinning palette does not cover all mesh joint indices");
      }
      for (const glm::mat4& matrix : palette->matrices()) {
        if (!IsFinite(matrix)) {
          return Fail(error, "skinning palette matrix must be finite");
        }
      }
      command.hasSkin = true;
      command.joints = palette->matrices();
    }
    cmds_.push_back(std::move(command));
    return true;
  }

  void EndFrame() override {
    // Light-VP (orthografisch, feste Box um Ursprung)
    glm::vec3 ldir = glm::normalize(glm::vec3(light_.dir[0], light_.dir[1], light_.dir[2]));
    glm::vec3 eye = -ldir * 60.f;
    glm::mat4 lvp = glm::ortho(-40.f, 40.f, -40.f, 40.f, 1.f, 140.f) *
                    glm::lookAt(eye, glm::vec3(0), glm::vec3(0, 1, 0));

    // ---- Pass 1: Shadow-Depth ----
    glBindFramebuffer_(GL_FRAMEBUFFER, shadowFbo_);
    glViewport(0, 0, kShadowSize, kShadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_FRONT);  // Peter-Panning/Acne reduzieren
    glUseProgram_(shadowProg_);
    glUniformMatrix4fv_(sLVP_, 1, GL_FALSE, glm::value_ptr(lvp));
    for (const auto& c : cmds_) {
      if (c.mesh >= meshes_.size()) continue;
      glUniformMatrix4fv_(sM_, 1, GL_FALSE, c.model);
      glUniform1i_(sHasSkin_, c.hasSkin ? 1 : 0);
      if (c.hasSkin) {
        glUniformMatrix4fv_(sJoints_, static_cast<GLsizei>(c.joints.size()),
                            GL_FALSE, glm::value_ptr(c.joints.front()));
      }
      glBindVertexArray_(meshes_[c.mesh].vao);
      glDrawElements(GL_TRIANGLES, meshes_[c.mesh].count, GL_UNSIGNED_INT, nullptr);
    }
    glCullFace(GL_BACK);

    // ---- Pass 2: Main (mit Frustum-Culling) ----
    glBindFramebuffer_(GL_FRAMEBUFFER, 0);
    int w = 0, h = 0;
    SDL_GL_GetDrawableSize(win_, &w, &h);
    glViewport(0, 0, w, h);
    glClearColor(0.012f, 0.02f, 0.012f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram_(prog_);
    glUniformMatrix4fv_(uVP_, 1, GL_FALSE, vp_);
    glUniform3f_(uCam_, cam_.x, cam_.y, cam_.z);
    glUniform3f_(uLD_, light_.dir[0], light_.dir[1], light_.dir[2]);
    glUniform3f_(uLC_, light_.color[0], light_.color[1], light_.color[2]);
    glUniform3f_(uAmb_, light_.ambient[0], light_.ambient[1], light_.ambient[2]);
    glUniform1f_(uStr_, light_.strength);
    glUniformMatrix4fv_(uLVP_, 1, GL_FALSE, glm::value_ptr(lvp));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, shadowTex_);
    glUniform1i_(uShadow_, 0);
    if (ibl_.ready) {
      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_CUBE_MAP, ibl_.irradiance);
      glUniform1i_(uIrr_, 1);
      glActiveTexture(GL_TEXTURE2);
      glBindTexture(GL_TEXTURE_CUBE_MAP, ibl_.prefiltered);
      glUniform1i_(uPre_, 2);
      glActiveTexture(GL_TEXTURE3);
      glBindTexture(GL_TEXTURE_2D, ibl_.brdfLut);
      glUniform1i_(uBrdf_, 3);
      glUniform1f_(uHasIBL_, 1.0f);
      glUniform1f_(uMaxLod_, ibl_.maxReflectionLod);
    } else {
      glUniform1f_(uHasIBL_, 0.0f);
    }
    for (const auto& c : cmds_) {
      if (c.mesh >= meshes_.size()) continue;
      const GLMesh& g = meshes_[c.mesh];
      // Culling: Bounds-Sphere gegen View-Frustum
      glm::mat4 M = glm::make_mat4(c.model);
      glm::vec3 cLocal = (g.bmin + g.bmax) * 0.5f;
      float r = glm::length(g.bmax - g.bmin) * 0.5f;
      glm::vec4 cw = M * glm::vec4(cLocal, 1.f);
      r *= glm::max(1e-3f, glm::max(glm::length(glm::vec3(M[0])), glm::max(glm::length(glm::vec3(M[1])), glm::length(glm::vec3(M[2])))));
      // Bind-Pose-Bounds sind fuer animierte Vertices nicht konservativ.
      if (!c.hasSkin && !SphereVisible(planes_, glm::vec3(cw), r)) continue;
      glUniformMatrix4fv_(uM_, 1, GL_FALSE, c.model);
      glUniform1i_(uHasSkin_, c.hasSkin ? 1 : 0);
      if (c.hasSkin) {
        glUniformMatrix4fv_(uJoints_, static_cast<GLsizei>(c.joints.size()),
                            GL_FALSE, glm::value_ptr(c.joints.front()));
      }
      glUniform3f_(uBase_, c.mat.base[0], c.mat.base[1], c.mat.base[2]);
      glUniform1f_(uMetal_, c.mat.metallic);
      glUniform1f_(uRough_, c.mat.roughness);
      glUniform1f_(uHasNormal_, c.mat.hasNormal ? 1.0f : 0.0f);
      glUniform1f_(uNormalScale_, c.mat.normalScale);
      glActiveTexture(GL_TEXTURE4);
      glBindTexture(GL_TEXTURE_2D, c.mat.hasNormal ? normalTex_ : whiteTex_);
      glUniform1i_(uNormalTex_, 4);
      glBindVertexArray_(g.vao);
      glDrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
    }
    SDL_GL_SwapWindow(win_);
  }

  void Shutdown() override {
    for (auto& g : meshes_) {
      glDeleteBuffers_(1, &g.vbo);
      glDeleteBuffers_(1, &g.ibo);
      glDeleteVertexArrays_(1, &g.vao);
    }
    if (shadowFbo_) glDeleteFramebuffers_(1, &shadowFbo_);
    if (shadowTex_) glDeleteTextures(1, &shadowTex_);
    if (ctx_) SDL_GL_DeleteContext(ctx_);
  }

 private:
  GLuint BuildProgram(const char* vsSrc, const char* fsSrc) {
    auto compile = [](GLenum type, const char* src) -> GLuint {
      GLuint s = glCreateShader_(type);
      glShaderSource_(s, 1, &src, nullptr);
      glCompileShader_(s);
      GLint ok = 0;
      glGetShaderiv_(s, GL_COMPILE_STATUS, &ok);
      if (!ok) {
        char log[2048];
        glGetShaderInfoLog_(s, sizeof(log), nullptr, log);
        SDL_Log("Shader-Log: %s", log);
        return 0;
      }
      return s;
    };
    GLuint vs = compile(GL_VERTEX_SHADER, vsSrc);
    GLuint fs = compile(GL_FRAGMENT_SHADER, fsSrc);
    if (!vs || !fs) return 0;
    GLuint p = glCreateProgram_();
    glAttachShader_(p, vs);
    glAttachShader_(p, fs);
    glLinkProgram_(p);
    GLint ok = 0;
    glGetProgramiv_(p, GL_LINK_STATUS, &ok);
    if (!ok) {
      char log[2048];
      glGetProgramInfoLog_(p, sizeof(log), nullptr, log);
      SDL_Log("Link-Log: %s", log);
      return 0;
    }
    return p;
  }

  static constexpr int kShadowSize = 2048;

  SDL_Window* win_ = nullptr;
  SDL_GLContext ctx_ = nullptr;
  GLuint prog_ = 0, shadowProg_ = 0;
  GLuint shadowFbo_ = 0, shadowTex_ = 0;
  GLint uM_ = 0, uVP_ = 0, uCam_ = 0, uBase_ = 0, uMetal_ = 0, uRough_ = 0, uLD_ = 0, uLC_ = 0, uAmb_ = 0, uStr_ = 0, uShadow_ = 0, uLVP_ = 0;
  GLint uHasNormal_ = 0, uNormalScale_ = 0, uNormalTex_ = 0, uHasIBL_ = 0, uMaxLod_ = 0, uIrr_ = 0, uPre_ = 0, uBrdf_ = 0;
  GLint uHasSkin_ = 0, uJoints_ = 0;
  GLuint whiteTex_ = 0, normalTex_ = 0;
  IBLResources ibl_;

 public:
  void SetIBL(const IBLResources& r) override { ibl_ = r; }
  uint32_t UploadTextureRGBA(const unsigned char* px, int w, int h) override {
    GLuint t = 0;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenerateMipmap_(GL_TEXTURE_2D);
    return t;
  }
  void SetNormalTexture(uint32_t t) override { normalTex_ = t ? t : whiteTex_; }

 private:
  GLint sM_ = 0, sLVP_ = 0, sHasSkin_ = 0, sJoints_ = 0;
  float vp_[16] = {};
  bool vpDirty_ = false;
  Plane planes_[6];
  glm::vec3 cam_{0, 1.7f, 4};
  Light light_;
  std::vector<GLMesh> meshes_;
  std::vector<DrawCmd> cmds_;
};

}  // namespace

IRenderer* CreateGLRenderer() { return new GLRenderer(); }

}  // namespace stw
