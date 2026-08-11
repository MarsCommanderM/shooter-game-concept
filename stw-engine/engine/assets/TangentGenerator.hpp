#pragma once
#include "../render/Vertex.hpp"
#include <cstdint>
#include <vector>
namespace stw {
void GenerateTangents(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices);
void GenerateTangentsArrays(const float* pos, const float* nrm, const float* uv,
                            const std::uint32_t* idx, std::size_t idxCount, std::size_t vertCount,
                            float* tanOut);
}
