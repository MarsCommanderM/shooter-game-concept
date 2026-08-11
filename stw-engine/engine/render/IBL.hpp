#pragma once
// Phase 3B: HDR-Environment -> Cubemap -> Irradiance + Prefilter(Mips) + BRDF-LUT.
#include <cstdint>
#include <vector>

namespace stw {
struct IBLResources {
    unsigned int environment = 0;   // cubemap HDR (RGBA16F)
    unsigned int irradiance = 0;    // cubemap
    unsigned int prefiltered = 0;   // cubemap mit Mip-Chain
    unsigned int brdfLut = 0;       // 2D RG16F
    float maxReflectionLod = 4.0f;
    bool ready = false;
};

// equirect HDR-Pixel (RGB float, beliebige Größe, z. B. 256x128 prozedural)
IBLResources BuildIBLFromEquirect(const float* hdrPixels, int w, int h);
}  // namespace stw
