#include "IBL.hpp"

#define GL_GLEXT_PROTOTYPES 1
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstring>
#include <vector>

namespace stw {
namespace {

GLuint Compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        SDL_Log("IBL shader fail: %s", log);
    }
    return s;
}
GLuint Program(const char* vs, const char* fs) {
    GLuint v = Compile(GL_VERTEX_SHADER, vs);
    GLuint f = Compile(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    return p;
}

const char* kCubeVS = R"(#version 450 core
layout(location=0) in vec3 aPos;
uniform mat4 uProj; uniform mat4 uView;
out vec3 vDir;
void main(){ vDir = aPos; gl_Position = uProj * uView * vec4(aPos, 1.0); }
)";

const char* kEquirectFS = R"(#version 450 core
in vec3 vDir; uniform sampler2D uEq;
out vec4 o;
const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSpherical(vec3 v){
  vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
  uv *= invAtan; uv += 0.5;
  return uv;
}
void main(){ o = vec4(texture(uEq, SampleSpherical(normalize(vDir))).rgb, 1.0); }
)";

const char* kIrradianceFS = R"(#version 450 core
in vec3 vDir; uniform samplerCube uEnv;
out vec4 o;
void main(){
  vec3 n = normalize(vDir);
  vec3 up = abs(n.y) < 0.999 ? vec3(0,1,0) : vec3(1,0,0);
  vec3 t = normalize(cross(up, n));
  vec3 b = cross(n, t);
  vec3 irr = vec3(0.0);
  float nr = 0.0; float step = 0.025;
  for (float phi = 0.0; phi < 6.28318; phi += step)
  for (float th = 0.0; th < 1.57079; th += step) {
    vec3 ts = vec3(sin(th)*cos(phi), cos(th), sin(th)*sin(phi));
    vec3 ws = ts.x*t + ts.y*n + ts.z*b;
    irr += texture(uEnv, ws).rgb * cos(th) * sin(th);
    nr += 1.0;
  }
  irr = 3.14159 * irr / nr;
  o = vec4(irr, 1.0);
}
)";

const char* kPrefilterFS = R"(#version 450 core
in vec3 vDir; uniform samplerCube uEnv; uniform float uRough;
out vec4 o;
float RadicalInverse(uint bits){
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N){ return vec2(float(i)/float(N), RadicalInverse(i)); }
vec3 ImportanceGGX(vec2 xi, vec3 V, vec3 N, float rough){
  float a = rough*rough;
  float phi = 2.0*3.14159*xi.x;
  float ct = sqrt((1.0-xi.y)/(1.0+(a*a-1.0)*xi.y));
  float st = sqrt(max(0.0,1.0-ct*ct));
  vec3 H = vec3(cos(phi)*st, sin(phi)*st, ct);
  vec3 up = abs(N.z)<0.999 ? vec3(0,0,1) : vec3(1,0,0);
  vec3 t = normalize(cross(up,N));
  vec3 b = cross(N,t);
  return normalize(t*H.x + b*H.y + N*H.z);
}
void main(){
  vec3 N = normalize(vDir);
  vec3 V = N;
  float rough = uRough;
  vec3 acc = vec3(0.0); float tw = 0.0;
  const uint SAMPLES = 512u;
  for (uint i = 0u; i < SAMPLES; i++) {
    vec3 H = ImportanceGGX(Hammersley(i, SAMPLES), V, N, rough);
    vec3 L = normalize(2.0*dot(V,H)*H - V);
    float ndl = max(dot(N,L), 0.0);
    if (ndl > 0.0) { acc += texture(uEnv, L).rgb * ndl; tw += ndl; }
  }
  o = vec4(acc / max(tw, 1e-5), 1.0);
}
)";

const char* kBrdfFS = R"(#version 450 core
layout(location=0) out vec2 o;
uniform vec2 uUV; // x = NdotV, y = roughness
float RadicalInverse(uint bits){
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
  return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N){ return vec2(float(i)/float(N), RadicalInverse(i)); }
vec3 ImportanceGGX(vec2 xi, vec3 V, vec3 N, float rough){
  float a = rough*rough;
  float phi = 2.0*3.14159*xi.x;
  float ct = sqrt((1.0-xi.y)/(1.0+(a*a-1.0)*xi.y));
  float st = sqrt(max(0.0,1.0-ct*ct));
  vec3 H = vec3(cos(phi)*st, sin(phi)*st, ct);
  vec3 up = abs(N.z)<0.999 ? vec3(0,0,1) : vec3(1,0,0);
  vec3 t = normalize(cross(up,N));
  vec3 b = cross(N,t);
  return normalize(t*H.x + b*H.y + N*H.z);
}
void main(){
  float ndv = clamp(uUV.x, 0.0, 1.0);
  float rough = uUV.y;
  vec3 V = vec3(sqrt(1.0-ndv*ndv), 0.0, ndv);
  vec3 N = vec3(0,0,1);
  float A = 0.0, B = 0.0;
  const uint SAMPLES = 1024u;
  for (uint i = 0u; i < SAMPLES; i++) {
    vec3 H = ImportanceGGX(Hammersley(i, SAMPLES), V, N, rough);
    vec3 L = normalize(2.0*dot(V,H)*H - V);
    float ndl = max(L.z, 0.0);
    float ndh = max(H.z, 0.0);
    float vdh = max(dot(V,H), 0.0);
    if (ndl > 0.0) {
      float g = (ndl * (ndl*ndl + 1.0)) / (ndh * vdh + 1e-5); // V-cavity approx (Lagarde)
      float fc = pow(1.0 - vdh, 5.0);
      A += (1.0 - fc) * g;
      B += fc * g;
    }
  }
  o = vec2(A, B) / float(SAMPLES);
}
)";

const char* kQuadVS = R"(#version 450 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
void main(){ vUV = aPos * 0.5 + 0.5; gl_Position = vec4(aPos, 0.0, 1.0); }
)";

GLuint MakeCubeVAO() {
    static const float v[] = {
        -1, -1, -1, 1, -1, -1, 1, 1, -1, -1, 1, -1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1,
    };
    static const unsigned int idx[] = {0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 3, 7, 0, 7, 4,
                                       1, 5, 6, 1, 6, 2, 3, 2, 6, 3, 6, 7, 0, 4, 5, 0, 5, 1};
    GLuint vao, vbo, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    return vao;
}

GLuint MakeQuadVAO() {
    static const float v[] = {-1, -1, 1, -1, 1, 1, -1, 1};
    static const unsigned int idx[] = {0, 1, 2, 0, 2, 3};
    GLuint vao, vbo, ibo;
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(idx), idx, GL_STATIC_DRAW);
    return vao;
}

}  // namespace

IBLResources BuildIBLFromEquirect(const float* px, int w, int h) {
    IBLResources r;
    GLuint cubeVAO = MakeCubeVAO();
    GLuint quadVAO = MakeQuadVAO();

    // 1) equirect texture
    GLuint eq;
    glGenTextures(1, &eq);
    glBindTexture(GL_TEXTURE_2D, eq);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 2) environment cubemap
    const int ENV = 256;
    glGenTextures(1, &r.environment);
    glBindTexture(GL_TEXTURE_CUBE_MAP, r.environment);
    for (int i = 0; i < 6; i++) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, ENV, ENV, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    GLuint eq2cube = Program(kCubeVS, kEquirectFS);
    GLuint irrProg = Program(kCubeVS, kIrradianceFS);
    GLuint preProg = Program(kCubeVS, kPrefilterFS);
    GLuint brdfProg = Program(kQuadVS, kBrdfFS);

    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 views[6] = {
        glm::lookAt(glm::vec3(0), glm::vec3(1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(-1, 0, 0), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, -1, 0), glm::vec3(0, 0, -1)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 0, 1), glm::vec3(0, -1, 0)),
        glm::lookAt(glm::vec3(0), glm::vec3(0, 0, -1), glm::vec3(0, -1, 0)),
    };

    GLuint fbo, rbo;
    glGenFramebuffers(1, &fbo);
    glGenRenderbuffers(1, &rbo);

    auto bindFace = [&](int face, int size) {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glBindRenderbuffer(GL_RENDERBUFFER, rbo);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, size, size);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, 0, 0);
        glViewport(0, 0, size, size);
    };

    // equirect -> cube
    glUseProgram(eq2cube);
    glUniformMatrix4fv(glGetUniformLocation(eq2cube, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(eq2cube, "uEq"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, eq);
    glBindVertexArray(cubeVAO);
    for (int i = 0; i < 6; i++) {
        bindFace(i, ENV);
        glUniformMatrix4fv(glGetUniformLocation(eq2cube, "uView"), 1, GL_FALSE, glm::value_ptr(views[i]));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }

    // 3) irradiance
    const int IRR = 64;
    glGenTextures(1, &r.irradiance);
    glBindTexture(GL_TEXTURE_CUBE_MAP, r.irradiance);
    for (int i = 0; i < 6; i++) glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F, IRR, IRR, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUseProgram(irrProg);
    glUniformMatrix4fv(glGetUniformLocation(irrProg, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(irrProg, "uEnv"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, r.environment);
    for (int i = 0; i < 6; i++) {
        bindFace(i, IRR);
        glUniformMatrix4fv(glGetUniformLocation(irrProg, "uView"), 1, GL_FALSE, glm::value_ptr(views[i]));
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
    }

    // 4) prefiltered mit Mips
    const int PRE = 256;
    int mips = 5;
    r.maxReflectionLod = float(mips - 1);
    glGenTextures(1, &r.prefiltered);
    glBindTexture(GL_TEXTURE_CUBE_MAP, r.prefiltered);
    for (int i = 0; i < 6; i++)
        for (int m = 0; m < mips; m++) {
            int s = std::max(1, PRE >> m);
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m, GL_RGB16F, s, s, 0, GL_RGB, GL_FLOAT, nullptr);
        }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glUseProgram(preProg);
    glUniformMatrix4fv(glGetUniformLocation(preProg, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniform1i(glGetUniformLocation(preProg, "uEnv"), 0);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    for (int m = 0; m < mips; m++) {
        float rough = float(m) / float(mips - 1);
        glUniform1f(glGetUniformLocation(preProg, "uRough"), rough);
        int s = std::max(1, PRE >> m);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, r.environment);
        for (int i = 0; i < 6; i++) {
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, m, 0);
            glViewport(0, 0, s, s);
            glUniformMatrix4fv(glGetUniformLocation(preProg, "uView"), 1, GL_FALSE, glm::value_ptr(views[i]));
            glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr);
        }
    }

    // 5) BRDF-LUT
    const int LUT = 256;
    glGenTextures(1, &r.brdfLut);
    glBindTexture(GL_TEXTURE_2D, r.brdfLut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, LUT, LUT, 0, GL_RG, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // BRDF-LUT per Pixel-Loop auf CPU (einfach + robust, einmalig)
    {
        // schneller: GPU-Quad wäre schöner, CPU reicht für 256² einmalig nicht -> GPU:
        // Wir rendern pro Pixel via gl_FragCoord? Unser brdfProg nutzt uUV pro Draw nicht.
        // Pragmatisch: kleines 64x64 LUT per CPU-Integration.
    }
    const int LS = 64;
    std::vector<float> lut(LS * LS * 2);
    for (int y = 0; y < LS; y++) {
        for (int x = 0; x < LS; x++) {
            float ndv = (x + 0.5f) / LS;
            float rough = (y + 0.5f) / LS;
            glm::vec3 V(sqrtf(1 - ndv * ndv), 0, ndv);
            glm::vec3 N(0, 0, 1);
            float A = 0, B = 0;
            const int SAMPLES = 256;
            for (int i = 0; i < SAMPLES; i++) {
                // Hammersley
                float fx = float(i) / SAMPLES;
                unsigned bits = i;
                bits = (bits << 16u) | (bits >> 16u);
                bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
                bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
                bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
                bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
                float fy = float(bits) * 2.3283064365386963e-10f;
                float a = rough * rough;
                float phi = 2.0f * 3.14159f * fx;
                float ct = sqrtf((1 - fy) / (1 + (a * a - 1) * fy));
                float st = sqrtf(std::max(0.0f, 1 - ct * ct));
                glm::vec3 H(cosf(phi) * st, sinf(phi) * st, ct);
                glm::vec3 up(1, 0, 0);
                glm::vec3 T = glm::normalize(glm::cross(up, N));
                glm::vec3 Bv = glm::cross(N, T);
                H = glm::normalize(T * H.x + Bv * H.y + N * H.z);
                glm::vec3 L = glm::normalize(2.0f * glm::dot(V, H) * H - V);
                float ndl = std::max(L.z, 0.0f);
                float ndh = std::max(H.z, 0.0f);
                float vdh = std::max(glm::dot(V, H), 0.0f);
                if (ndl > 0) {
                    float g = (ndl * (ndl * ndl + 1.0f)) / (ndh * vdh + 1e-5f);
                    float fc = powf(1 - vdh, 5.0f);
                    A += (1 - fc) * g;
                    B += fc * g;
                }
            }
            lut[(y * LS + x) * 2 + 0] = A / SAMPLES;
            lut[(y * LS + x) * 2 + 1] = B / SAMPLES;
        }
    }
    glBindTexture(GL_TEXTURE_2D, r.brdfLut);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG16F, LS, LS, 0, GL_RG, GL_FLOAT, lut.data());
    (void)brdfProg;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    r.ready = true;
    return r;
}
}  // namespace stw
