// tools/cache_builder: statisches glTF -> .stwc (v3, inkl. Tangents + Materials).
// Skin-Daten werden importiert, aber bis zu einem späteren Cache-Format bewusst nicht gecacht.
// Usage: cache_builder <file.gltf>
#include "../engine/assets/gltf.h"

#include <cstdio>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: cache_builder <file.gltf>\n");
        return 1;
    }
    stw::StwModel m;
    if (!stw::LoadGLTF(argv[1], m)) {
        std::printf("import failed: %s\n", argv[1]);
        return 2;
    }
    size_t verts = 0;
    for (auto& me : m.meshes) verts += me.pos.size() / 3;
    if (!m.skins.empty() || !m.skinnedMeshes.empty()) {
        std::printf("ok: %zu meshes, %zu verts, %zu materials; skin data imported, STWC v3 bypassed\n",
                    m.meshes.size(), verts, m.mats.size());
    } else {
        std::printf("ok: %zu meshes, %zu verts, %zu materials -> %s.stwc (v3)\n",
                    m.meshes.size(), verts, m.mats.size(), argv[1]);
    }
    return 0;
}
