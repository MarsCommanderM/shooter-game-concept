// STW-ENGINE: OpenGL-4.5-Backend (Forward+, PBR metallic/roughness, ACES)
#include "renderer.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

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
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays_ = nullptr;
PFNGLDELETEBUFFERSPROC glDeleteBuffers_ = nullptr;

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
  glDeleteVertexArrays_ = Load<PFNGLDELETEVERTEXARRAYSPROC>("glDeleteVertexArrays");
  glDeleteBuffers_ = Load<PFNGLDELETEBUFFERSPROC>("glDeleteBuffers");
  return glGenVertexArrays_ && glBindVertexArray_ && glGenBuffers_ && glBufferData_ &&
         glVertexAttribPointer_ && glCreateShader_ && glCreateProgram_;
}

const char* kVS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;
uniform mat4 uM;
uniform mat4 uVP;
out vec3 vN; out vec3 vW; out vec2 vUV;
void main() {
  vec4 w = uM * vec4(aPos, 1.0);
  vW = w.xyz;
  vN = mat3(uM) * aNrm;
  vUV = aUV;
  gl_Position = uVP * w;
}
)";

const char* kFS = R"(
#version 330 core
in vec3 vN; in vec3 vW; in vec2 vUV;
uniform vec3 uCam;
uniform vec3 uBase; uniform float uMetal; uniform float uRough;
uniform vec3 uLightDir; uniform vec3 uLightCol; uniform vec3 uAmb; uniform float uStrength;
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

void main() {
  vec3 N = normalize(vN);
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
  vec3 col = (kD * uBase / PI + spec) * uLightCol * uStrength * NoL + uAmb * uBase;
  col = aces(col);
  col = pow(col, vec3(1.0 / 2.2));
  oCol = vec4(col, 1.0);
}
)";

struct GLMesh {
  GLuint vao = 0, vbo = 0, ibo = 0;
  GLsizei count = 0;
};

class GLRenderer : public IRenderer {
 public:
  bool Init(void* sdlWindow) override {
    win_ = static_cast<SDL_Window*>(sdlWindow);
    ctx_ = SDL_GL_CreateContext(win_);
    if (!ctx_) return false;
    SDL_GL_MakeCurrent(win_, ctx_);
    if (!LoadGL()) return false;
    prog_ = BuildProgram();
    if (!prog_) return false;
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
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    return true;
  }

  void BeginFrame() override {
    SDL_GL_MakeCurrent(win_, ctx_);
    glClearColor(0.012f, 0.02f, 0.012f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram_(prog_);
  }

  void SetViewProj(const float vp[16]) override { glUniformMatrix4fv_(uVP_, 1, GL_FALSE, vp); }
  void SetCameraPos(float x, float y, float z) override { glUniform3f_(uCam_, x, y, z); }
  void SetLight(const Light& l) override {
    glUniform3f_(uLD_, l.dir[0], l.dir[1], l.dir[2]);
    glUniform3f_(uLC_, l.color[0], l.color[1], l.color[2]);
    glUniform3f_(uAmb_, l.ambient[0], l.ambient[1], l.ambient[2]);
    glUniform1f_(uStr_, l.strength);
  }

  uint32_t UploadMesh(const StwMesh& m) override {
    GLMesh g;
    glGenVertexArrays_(1, &g.vao);
    glGenBuffers_(1, &g.vbo);
    glGenBuffers_(1, &g.ibo);
    glBindVertexArray_(g.vao);
    glBindBuffer_(GL_ARRAY_BUFFER, g.vbo);
    // interleaved-ish: separate packed blocks
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
    }
    glBufferData_(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(blk.size() * 4), blk.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray_(0);
    glVertexAttribPointer_(0, 3, GL_FLOAT, GL_FALSE, 8 * 4, (void*)0);
    glEnableVertexAttribArray_(1);
    glVertexAttribPointer_(1, 3, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(3 * 4));
    glEnableVertexAttribArray_(2);
    glVertexAttribPointer_(2, 2, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(6 * 4));
    glBindBuffer_(GL_ELEMENT_ARRAY_BUFFER, g.ibo);
    glBufferData_(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(m.idx.size() * 4), m.idx.data(), GL_STATIC_DRAW);
    g.count = static_cast<GLsizei>(m.idx.size());
    meshes_.push_back(g);
    return static_cast<uint32_t>(meshes_.size() - 1);
  }

  void Draw(uint32_t h, const StwMaterial& mat, const float model[16]) override {
    if (h >= meshes_.size()) return;
    const GLMesh& g = meshes_[h];
    glUniformMatrix4fv_(uM_, 1, GL_FALSE, model);
    glUniform3f_(uBase_, mat.base[0], mat.base[1], mat.base[2]);
    glUniform1f_(uMetal_, mat.metallic);
    glUniform1f_(uRough_, mat.roughness);
    glBindVertexArray_(g.vao);
    glDrawElements(GL_TRIANGLES, g.count, GL_UNSIGNED_INT, nullptr);
  }

  void EndFrame() override { SDL_GL_SwapWindow(win_); }

  void Shutdown() override {
    for (auto& g : meshes_) {
      glDeleteBuffers_(1, &g.vbo);
      glDeleteBuffers_(1, &g.ibo);
      glDeleteVertexArrays_(1, &g.vao);
    }
    if (ctx_) SDL_GL_DeleteContext(ctx_);
  }

 private:
  GLuint BuildProgram() {
    auto compile = [](GLenum type, const char* src) -> GLuint {
      GLuint s = glCreateShader_(type);
      glShaderSource_(s, 1, &src, nullptr);
      glCompileShader_(s);
      GLint ok = 0;
      glGetShaderiv_(s, GL_COMPILE_STATUS, &ok);
      if (!ok) {
        char log[1024];
        glGetShaderInfoLog_(s, sizeof(log), nullptr, log);
        SDL_Log("Shader-Log: %s", log);
        return 0;
      }
      return s;
    };
    GLuint vs = compile(GL_VERTEX_SHADER, kVS);
    GLuint fs = compile(GL_FRAGMENT_SHADER, kFS);
    if (!vs || !fs) return 0;
    GLuint p = glCreateProgram_();
    glAttachShader_(p, vs);
    glAttachShader_(p, fs);
    glLinkProgram_(p);
    GLint ok = 0;
    glGetProgramiv_(p, GL_LINK_STATUS, &ok);
    if (!ok) {
      char log[1024];
      glGetProgramInfoLog_(p, sizeof(log), nullptr, log);
      SDL_Log("Link-Log: %s", log);
      return 0;
    }
    return p;
  }

  SDL_Window* win_ = nullptr;
  SDL_GLContext ctx_ = nullptr;
  GLuint prog_ = 0;
  GLint uM_ = 0, uVP_ = 0, uCam_ = 0, uBase_ = 0, uMetal_ = 0, uRough_ = 0, uLD_ = 0, uLC_ = 0, uAmb_ = 0, uStr_ = 0;
  std::vector<GLMesh> meshes_;
};

}  // namespace

IRenderer* CreateGLRenderer() { return new GLRenderer(); }

}  // namespace stw
