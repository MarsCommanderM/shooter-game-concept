#pragma once
// STW-ENGINE: glTF-2.0-Loader (Subset: POSITION/NORMAL/TEXCOORD_0, metallic-roughness)
// + eigener Binary-Cache (.stwc) gegen Load-Lags – wie im Bauplan gefordert.
#include "animation/AnimationClip.hpp"
#include "animation/Skeleton.hpp"
#include "animation/SkinInfluence.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace stw {

struct StwMesh {
  std::vector<float> pos;   // xyz
  std::vector<float> nrm;   // xyz
  std::vector<float> uv;    // uv
  std::vector<float> tan;   // xyzw (tangent + handedness)
  std::vector<uint32_t> idx;
  // Leer für statische Primitives. Joint-Indizes sind skin-lokal und zeigen
  // in StwSkin::jointNodes, nicht direkt in den globalen glTF-Node-Indexraum.
  std::vector<SkinInfluence4> skinInfluences;
  float bmin[3] = {0, 0, 0};  // Bounds für Frustum-Culling
  float bmax[3] = {0, 0, 0};
};

struct StwMaterial {
  float base[4] = {0.8f, 0.8f, 0.8f, 1.0f};
  float metallic = 0.0f;
  float roughness = 0.9f;
  float normalScale = 1.0f;
  bool hasNormal = false;
};

struct StwSkin {
  std::string name;
  // Skin-lokaler Joint-Index -> glTF-Node-Index, in originaler glTF-Reihenfolge.
  std::vector<uint32_t> jointNodes;
  int skeletonRootNode = -1;
  Skeleton skeleton;
};

struct StwSkinnedMesh {
  uint32_t meshIndex = 0;  // Index in StwModel::meshes (geflattete Primitive)
  uint32_t skinIndex = 0;  // Index in StwModel::skins
  uint32_t nodeIndex = 0;  // glTF-Node, der Mesh und Skin gemeinsam referenziert
};

struct StwAnimation {
  std::string name;
  uint32_t sourceAnimationIndex = 0;
  // Ein Clip pro Skin, falls die Quellanimation mindestens einen Joint dieses
  // Skins adressiert. Die Clips sind Asset-Daten; Laufzeitstatus lebt im Animator.
  std::vector<std::optional<AnimationClip>> skinClips;
};

struct StwModel {
  std::vector<StwMesh> meshes;
  std::vector<StwMaterial> mats;
  std::vector<int> meshMat;  // pro Mesh: Index in mats (-1 = default)
  std::vector<StwSkin> skins;
  std::vector<StwSkinnedMesh> skinnedMeshes;
  std::vector<StwAnimation> animations;
};

// Lädt .gltf/.glb. Statische Modelle nutzen weiterhin <pfad>.stwc.
// Skin-/Influence-/Animationsdaten umgehen STWC v3, weil das Format sie nicht
// serialisiert. Der Import ist transaktional: bei Fehler bleibt out unverändert.
bool LoadGLTF(const std::string& path,
              StwModel& out,
              std::string* error = nullptr);

// Fallback-Geometrie, falls kein Asset da ist (Phase-1-Test)
StwMesh MakeCubeMesh();
StwMesh MakeGroundMesh(float half);

}  // namespace stw
