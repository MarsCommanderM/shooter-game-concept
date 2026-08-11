#include "gltf.h"
#include "assets/TangentGenerator.hpp"
#include "assets/StwcFormat.hpp"
#include "json_mini.h"

#include <sys/stat.h>

#include <cmath>
#include <cstring>
#include <fstream>

namespace stw {

namespace {

void ComputeBounds(StwMesh& m) {
  if (m.pos.size() < 3) return;
  for (int k = 0; k < 3; k++) {
    m.bmin[k] = m.bmax[k] = m.pos[k];
  }
  for (size_t i = 3; i + 2 < m.pos.size(); i += 3) {
    for (int k = 0; k < 3; k++) {
      m.bmin[k] = std::min(m.bmin[k], m.pos[i + k]);
      m.bmax[k] = std::max(m.bmax[k], m.pos[i + k]);
    }
  }
}

std::vector<uint8_t> ReadFile(const std::string& p) {
  std::ifstream f(p, std::ios::binary);
  if (!f) return {};
  return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

bool B64Decode(const std::string& in, std::vector<uint8_t>& out) {
  auto val = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
  };
  out.clear();
  int buf = 0, bits = 0;
  for (char c : in) {
    int v = val(c);
    if (v < 0) continue;
    buf = (buf << 6) | v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
    }
  }
  return true;
}

time_t MTime(const std::string& p) {
  struct stat st {};
  if (stat(p.c_str(), &st) != 0) return 0;
  return st.st_mtime;
}

// ---------- Binary-Cache ----------
void W32(std::ofstream& f, uint32_t v) { f.write(reinterpret_cast<char*>(&v), 4); }
void WF(std::ofstream& f, float v) { f.write(reinterpret_cast<char*>(&v), 4); }
uint32_t R32(std::ifstream& f) {
  uint32_t v = 0;
  f.read(reinterpret_cast<char*>(&v), 4);
  return v;
}
float RF(std::ifstream& f) {
  float v = 0;
  f.read(reinterpret_cast<char*>(&v), 4);
  return v;
}

// FNV-1a 64 – reicht als Source-Hash gegen stille Format-Drifts
static uint64_t FnvHash(const std::vector<uint8_t>& d) {
  uint64_t h = 1469598103934665603ull;
  for (uint8_t b : d) { h ^= b; h *= 1099511628211ull; }
  return h;
}
constexpr uint32_t CACHE_VERSION = stw::STWC_VERSION; // v3: +tangents, +normalScale
bool TryCache(const std::string& src, StwModel& out) {
  const std::string cp = src + ".stwc";
  if (MTime(cp) <= MTime(src)) return false;
  auto raw = ReadFile(src);
  uint64_t src_hash = FnvHash(raw);
  std::ifstream f(cp, std::ios::binary);
  char magic[4];
  f.read(magic, 4);
  if (!f || std::memcmp(magic, "STWC", 4) != 0) return false;
  uint32_t ver = R32(f);
  uint32_t h0 = R32(f), h1 = R32(f);
  if (ver != CACHE_VERSION) return false;
  if (h0 != static_cast<uint32_t>(src_hash & 0xFFFFFFFF) || h1 != static_cast<uint32_t>(src_hash >> 32)) return false;
  uint32_t nm = R32(f), nmat = R32(f);
  out.meshes.resize(nm);
  out.mats.resize(nmat);
  out.meshMat.resize(nm);
  for (auto& m : out.mats) {
    for (int k = 0; k < 4; k++) m.base[k] = RF(f);
    m.metallic = RF(f);
    m.roughness = RF(f);
    m.normalScale = RF(f);
    m.hasNormal = R32(f) != 0;
  }
  for (uint32_t i = 0; i < nm; i++) {
    out.meshMat[i] = static_cast<int>(R32(f));
    auto& m = out.meshes[i];
    uint32_t np = R32(f);
    m.pos.resize(np);
    for (auto& v : m.pos) v = RF(f);
    uint32_t nn = R32(f);
    m.nrm.resize(nn);
    for (auto& v : m.nrm) v = RF(f);
    uint32_t nu = R32(f);
    m.uv.resize(nu);
    for (auto& v : m.uv) v = RF(f);
    uint32_t nt = R32(f);
    m.tan.resize(nt);
    for (auto& v : m.tan) v = RF(f);
    uint32_t ni = R32(f);
    m.idx.resize(ni);
    for (auto& v : m.idx) v = R32(f);
  }
  return f.good();
}

void WriteCache(const std::string& src, const StwModel& m, uint64_t src_hash) {
  const std::string cp = src + ".stwc";
  std::ofstream f(cp, std::ios::binary);
  if (!f) return;
  f.write("STWC", 4);
  W32(f, CACHE_VERSION);
  W32(f, static_cast<uint32_t>(src_hash & 0xFFFFFFFF));
  W32(f, static_cast<uint32_t>(src_hash >> 32));
  W32(f, static_cast<uint32_t>(m.meshes.size()));
  W32(f, static_cast<uint32_t>(m.mats.size()));
  for (const auto& mat : m.mats) {
    for (int k = 0; k < 4; k++) WF(f, mat.base[k]);
    WF(f, mat.metallic);
    WF(f, mat.roughness);
    WF(f, mat.normalScale);
    W32(f, mat.hasNormal ? 1 : 0);
  }
  for (size_t i = 0; i < m.meshes.size(); i++) {
    W32(f, static_cast<uint32_t>(m.meshMat[i]));
    const auto& me = m.meshes[i];
    W32(f, static_cast<uint32_t>(me.pos.size()));
    for (float v : me.pos) WF(f, v);
    W32(f, static_cast<uint32_t>(me.nrm.size()));
    for (float v : me.nrm) WF(f, v);
    W32(f, static_cast<uint32_t>(me.uv.size()));
    for (float v : me.uv) WF(f, v);
    W32(f, static_cast<uint32_t>(me.tan.size()));
    for (float v : me.tan) WF(f, v);
    W32(f, static_cast<uint32_t>(me.idx.size()));
    for (uint32_t v : me.idx) W32(f, v);
  }
}

}  // namespace

bool LoadGLTF(const std::string& path, StwModel& out) {
  if (TryCache(path, out)) return true;

  auto raw = ReadFile(path);
  if (raw.empty()) return false;

  std::string jsonText;
  std::vector<uint8_t> glbBin;

  if (raw.size() > 12 && raw[0] == 'g' && raw[1] == 'l' && raw[2] == 'T' && raw[3] == 'F') {
    // GLB: Header(12) + Chunks
    size_t off = 12;
    while (off + 8 <= raw.size()) {
      uint32_t len, type;
      std::memcpy(&len, raw.data() + off, 4);
      std::memcpy(&type, raw.data() + off + 4, 4);
      off += 8;
      if (off + len > raw.size()) break;
      if (type == 0x4E4F534A) jsonText.assign(reinterpret_cast<char*>(raw.data() + off), len);
      if (type == 0x004E4942) glbBin.assign(raw.begin() + off, raw.begin() + off + len);
      off += len;
    }
  } else {
    jsonText.assign(reinterpret_cast<char*>(raw.data()), raw.size());
  }

  Json doc;
  if (!ParseJson(jsonText, doc)) return false;

  // Buffer
  std::vector<uint8_t> bin;
  const Json* buffers = doc.get("buffers");
  if (buffers && !buffers->arr.empty()) {
    const Json& b0 = buffers->arr[0];
    const Json* uri = b0.get("uri");
    if (uri && uri->t == Json::T::Str) {
      if (uri->str.rfind("data:", 0) == 0) {
        auto comma = uri->str.find(',');
        if (comma != std::string::npos) B64Decode(uri->str.substr(comma + 1), bin);
      } else {
        // relativer .bin-Pfad
        std::string base = path.substr(0, path.find_last_of('/') + 1);
        bin = ReadFile(base + uri->str);
      }
    } else if (!glbBin.empty()) {
      bin = glbBin;
    }
  }

  const Json* bvs = doc.get("bufferViews");
  const Json* accs = doc.get("accessors");
  if (!bvs || !accs) return false;

  auto accData = [&](int ai, std::vector<float>& outF, int comps) -> bool {
    if (ai < 0 || ai >= static_cast<int>(accs->arr.size())) return false;
    const Json& a = accs->arr[ai];
    const Json* bvJ = a.get("bufferView");
    if (!bvJ) return false;
    const Json& bv = bvs->arr[static_cast<size_t>(bvJ->numOr())];
    size_t off = static_cast<size_t>(bv.get("byteOffset") ? bv.get("byteOffset")->numOr() : 0);
    size_t bo = static_cast<size_t>(a.get("byteOffset") ? a.get("byteOffset")->numOr() : 0);
    size_t count = static_cast<size_t>(a.get("count") ? a.get("count")->numOr() : 0);
    int ct = static_cast<int>(a.get("componentType") ? a.get("componentType")->numOr(5126) : 5126);
    outF.resize(count * comps);
    const uint8_t* p = bin.data() + off + bo;
    for (size_t i = 0; i < count * static_cast<size_t>(comps); i++) {
      if (ct == 5126) {
        float v;
        std::memcpy(&v, p + i * 4, 4);
        outF[i] = v;
      } else if (ct == 5123) {
        uint16_t v;
        std::memcpy(&v, p + i * 2, 2);
        outF[i] = v;
      } else if (ct == 5125) {
        uint32_t v;
        std::memcpy(&v, p + i * 4, 4);
        outF[i] = static_cast<float>(v);
      } else if (ct == 5121) {
        outF[i] = p[i];
      } else {
        return false;
      }
    }
    return true;
  };

  // Materialien
  const Json* mats = doc.get("materials");
  if (mats) {
    for (const auto& m : mats->arr) {
      StwMaterial sm;
      const Json* pbr = m.get("pbrMetallicRoughness");
      if (pbr) {
        const Json* bcf = pbr->get("baseColorFactor");
        if (bcf) for (size_t k = 0; k < 4 && k < bcf->arr.size(); k++) sm.base[k] = static_cast<float>(bcf->arr[k].numOr());
        sm.metallic = static_cast<float>(pbr->get("metallicFactor") ? pbr->get("metallicFactor")->numOr(1) : 1);
        sm.roughness = static_cast<float>(pbr->get("roughnessFactor") ? pbr->get("roughnessFactor")->numOr(1) : 1);
      }
      out.mats.push_back(sm);
    }
  }
  if (out.mats.empty()) out.mats.push_back(StwMaterial{});

  const Json* meshes = doc.get("meshes");
  if (!meshes) return false;
  for (const auto& mesh : meshes->arr) {
    const Json* prims = mesh.get("primitives");
    if (!prims) continue;
    for (const auto& pr : prims->arr) {
      StwMesh sm;
      const Json* attrs = pr.get("attributes");
      if (!attrs) continue;
      const Json* pJ = attrs->get("POSITION");
      const Json* nJ = attrs->get("NORMAL");
      const Json* uJ = attrs->get("TEXCOORD_0");
      if (!pJ) continue;
      if (!accData(static_cast<int>(pJ->numOr()), sm.pos, 3)) continue;
      if (nJ) accData(static_cast<int>(nJ->numOr()), sm.nrm, 3);
      if (uJ) accData(static_cast<int>(uJ->numOr()), sm.uv, 2);
      const Json* tJ = attrs->get("TANGENT");
      if (tJ) accData(static_cast<int>(tJ->numOr()), sm.tan, 4);
      const Json* iJ = pr.get("indices");
      if (iJ) {
        std::vector<float> idxF;
        if (accData(static_cast<int>(iJ->numOr()), idxF, 1)) {
          sm.idx.resize(idxF.size());
          for (size_t i = 0; i < idxF.size(); i++) sm.idx[i] = static_cast<uint32_t>(idxF[i]);
        }
      }
      if (sm.idx.empty()) {
        for (uint32_t i = 0; i < sm.pos.size() / 3; i++) sm.idx.push_back(i);
      }
      out.meshMat.push_back(pr.get("material") ? static_cast<int>(pr.get("material")->numOr()) : 0);
      if (sm.tan.empty() && !sm.pos.empty()) {
        sm.tan.resize(sm.pos.size() / 3 * 4);
        stw::GenerateTangentsArrays(sm.pos.data(), sm.nrm.data(),
                                    sm.uv.empty() ? nullptr : sm.uv.data(),
                                    sm.idx.data(), sm.idx.size(), sm.pos.size() / 3, sm.tan.data());
      }
      ComputeBounds(sm);
      out.meshes.push_back(std::move(sm));
    }
  }

  if (out.meshes.empty()) return false;
  { auto raw2 = ReadFile(path); WriteCache(path, out, FnvHash(raw2)); }
  return true;
}

StwMesh MakeCubeMesh() {
  StwMesh m;
  const float c[8][3] = {{-0.5, -0.5, -0.5}, {0.5, -0.5, -0.5}, {0.5, 0.5, -0.5}, {-0.5, 0.5, -0.5},
                         {-0.5, -0.5, 0.5},  {0.5, -0.5, 0.5},  {0.5, 0.5, 0.5},  {-0.5, 0.5, 0.5}};
  const int f[6][4] = {{0, 1, 2, 3}, {5, 4, 7, 6}, {4, 0, 3, 7}, {1, 5, 6, 2}, {3, 2, 6, 7}, {4, 5, 1, 0}};
  const float n[6][3] = {{0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}, {0, 1, 0}, {0, -1, 0}};
  for (int q = 0; q < 6; q++) {
    uint32_t b = static_cast<uint32_t>(m.pos.size() / 3);
    for (int k = 0; k < 4; k++) {
      const float* p = c[f[q][k]];
      m.pos.insert(m.pos.end(), {p[0], p[1], p[2]});
      m.nrm.insert(m.nrm.end(), {n[q][0], n[q][1], n[q][2]});
      m.uv.insert(m.uv.end(), {k == 1 || k == 2 ? 1.f : 0.f, k >= 2 ? 1.f : 0.f});
    }
    m.idx.insert(m.idx.end(), {b, b + 1, b + 2, b, b + 2, b + 3});
  }
  ComputeBounds(m);
  return m;
}

StwMesh MakeGroundMesh(float h) {
  StwMesh m;
  m.pos = {-h, 0, -h, h, 0, -h, h, 0, h, -h, 0, h};
  m.nrm = {0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0};
  m.uv = {0, 0, 1, 0, 1, 1, 0, 1};
  m.idx = {0, 1, 2, 0, 2, 3};
  ComputeBounds(m);
  return m;
}

}  // namespace stw
