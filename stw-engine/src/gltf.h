#pragma once
// STW-ENGINE: glTF-2.0-Loader (Subset: POSITION/NORMAL/TEXCOORD_0, metallic-roughness)
// + eigener Binary-Cache (.stwc) gegen Load-Lags – wie im Bauplan gefordert.
#include <cstdint>
#include <string>
#include <vector>

namespace stw {

struct StwMesh {
  std::vector<float> pos;   // xyz
  std::vector<float> nrm;   // xyz
  std::vector<float> uv;    // uv
  std::vector<uint32_t> idx;
  float bmin[3] = {0, 0, 0};  // Bounds für Frustum-Culling
  float bmax[3] = {0, 0, 0};
};

struct StwMaterial {
  float base[4] = {0.8f, 0.8f, 0.8f, 1.0f};
  float metallic = 0.0f;
  float roughness = 0.9f;
};

struct StwModel {
  std::vector<StwMesh> meshes;
  std::vector<StwMaterial> mats;
  std::vector<int> meshMat;  // pro Mesh: Index in mats (-1 = default)
};

// Lädt .gltf/.glb. Beim ersten Load wird <pfad>.stwc geschrieben,
// danach (solange Cache neuer als Quelle) wird direkt der Cache gelesen.
bool LoadGLTF(const std::string& path, StwModel& out);

// Fallback-Geometrie, falls kein Asset da ist (Phase-1-Test)
StwMesh MakeCubeMesh();
StwMesh MakeGroundMesh(float half);

}  // namespace stw
