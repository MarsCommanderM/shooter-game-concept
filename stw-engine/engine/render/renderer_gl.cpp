// STW-ENGINE: OpenGL-4.5-Backend (Forward+, PBR metallic/roughness, ACES)
// Phase 2+: Render-Paesse getrennt (Shadow-Depth -> Main), Frustum-Culling, PCF-Shadows
#include "renderer.h"
#include "render/IBL.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstring>
#include <string>
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
  return glGenVertexArrays_ && glBindVertexArray_ && glGenBuffers_ && glBufferData_ &&
         glVertexAttribPointer_ && glCreateShader_ && glCreateProgram_ && glGenFramebuffers_;
}

/* ---------------- Shaders ---------------- */
const char* kVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTan;
uniform mat4 uM;
uniform mat4 uVP;
out vec3 vN; out vec3 vW; out vec2 vUV; out vec3 vT; out float vTS;
void main() {
  vec4 w = uM * vec4(aPos, 1.0);
  vW = w.xyz;
  mat3 nm = mat3(uM);
  vec3 N = normalize(nm * aNrm);
  vec3 T = normalize(nm * aTan.xyz);
  T = normalize(T - N * dot(N, T));
  vN = N; vT = T; vTS = aTan.w;
  vUV = aUV;
  gl_Position = uVP * w;
}
)";

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

const char* kShadowVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
uniform mat4 uM;
uniform mat4 uLVP;
void main() { gl_Position = uLVP * uM * vec4(aPos, 1.0); }
)";

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

struct GLMesh {
  GLuint vao = 0, vbo = 0, ibo = 0;
  GLsizei count = 0;
  glm::vec3 bmin{0}, bmax{0};
};

struct DrawCmd {
  uint32_t mesh;
  StwMaterial mat;
  float model[16];
};

class GLRenderer : public IRenderer {
 public:
  bool Init(void* sdlWindow) override {
    win_ = static_cast<SDL_Window*>(sdlWindow);
    ctx_ = SDL_GL_CreateContext(win_);
    if (!ctx_) return false;
    SDL_GL_MakeCurrent(win_, ctx_);
    if (!LoadGL()) return false;
    prog_ = BuildProgram(kVS, kFS);
    shadowProg_ = BuildProgram(kShadowVS, kShadowFS);
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
    // 1x1 weiße Default-Normalmap
    glGenTextures(1, &whiteTex_);
    glBindTexture(GL_TEXTURE_2D, whiteTex_);
    { const unsigned char w[4] = {128, 128, 255, 255};
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, w); }
    normalTex_ = whiteTex_;
    uLVP_ = glGetUniformLocation_(prog_, "uLightVP");
    sM_ = glGetUniformLocation_(shadowProg_, "uM");
    sLVP_ = glGetUniformLocation_(shadowProg_, "uLVP");

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
    GLMesh g;
    g.bmin = glm::vec3(m.bmin[0], m.bmin[1], m.bmin[2]);
    g.bmax = glm::vec3(m.bmax[0], m.bmax[1], m.bmax[2]);
    glGenVertexArrays_(1, &g.vao);
    glGenBuffers_(1, &g.vbo);
    glGenBuffers_(1, &g.ibo);
    glBindVertexArray_(g.vao);
    glBindBuffer_(GL_ARRAY_BUFFER, g.vbo);
    const size_t n = m.pos.size() / 3;
    std::vector<float> blk;
    blk.reserve(n * 8);
    for (size_t i = 0; i < n; i++) {
      blk.insert(blk.end(), {m.pos[i * 3], m.pos[i * 3 + 1], m.pos[i * 3 + 2]});
      if (m.nrm.size() >= (i + 1) * 3)
        blk.insert(blk.end(), {m.nrm[i * 3], m.nrm[i * 3 + 1], m.nrm[i * 3 + 2]});
      else
        blk.insert(blk.end(), {0, 1, 0});
      if (m.uv.size() >= (i + 1) * 2)
        blk.insert(blk.end(), {m.uv[i * 2], m.uv[i * 2 + 1]});
      else
        blk.insert(blk.end(), {0, 0});
      if (m.tan.size() >= (i + 1) * 4)
        blk.insert(blk.end(), {m.tan[i * 4], m.tan[i * 4 + 1], m.tan[i * 4 + 2], m.tan[i * 4 + 3]});
      else
        blk.insert(blk.end(), {1, 0, 0, 1});
    }
    glBufferData_(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(blk.size() * 4), blk.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray_(0);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 12 * 4, (void*)0);
    glEnableVertexAttribArray_(1);
    glVertexAttribPointer_(1, 3, GL_FLOAT, GL_FALSE, 12 * 4, (void*)(3 * 4));
    glEnableVertexAttribArray_(2);
    glVertexAttribPointer_(2, 2, GL_FLOAT, GL_FALSE, 12 * 4, (void*)(6 * 4));
    glEnableVertexAttribArray_(3);
    glVertexAttribPointer_(3, 4, GL_FLOAT, GL_FALSE, 12 * 4, (void*)(8 * 4));
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m.idx.size() * 4), m.idx.data(), GL_STATIC_DRAW);
    g.count = static_cast<GLsizei>(m.idx.size());
    meshes_.push_back(g);
    return static_cast<uint32_t>(meshes_.size() - 1);
  }

  void Draw(uint32_t h, const StwMaterial& mat, const float model[16]) override {
    glUniform1f_(uHasNormal_, mat.hasNormal ? 1.0f : 0.0f);
    glUniform1f_(uNormalScale_, mat.normalScale);
    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, mat.hasNormal ? normalTex_ : whiteTex_);
    glUniform1i_(uNormalTex_, 4);
    cmds_.push_back({h, mat, {}});
    std::memcpy(cmds_.back().model, model, sizeof(float) * 16);
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
      if (!SphereVisible(planes_, glm::vec3(cw), r)) continue;
      glUniformMatrix4fv_(uM_, 1, GL_FALSE, c.model);
      glUniform3f_(uBase_, c.mat.base[0], c.mat.base[1], c.mat.base[2]);
      glUniform1f_(uMetal_, c.mat.metallic);
      glUniform1f_(uRough_, c.mat.roughness);
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
  GLint sM_ = 0, sLVP_ = 0;
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
