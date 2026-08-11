#include "TangentGenerator.hpp"

#include <glm/glm.hpp>

#include <cmath>

namespace stw {
void GenerateTangents(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
    std::vector<glm::vec3> tangentSum(vertices.size(), glm::vec3(0.0f));
    std::vector<glm::vec3> bitangentSum(vertices.size(), glm::vec3(0.0f));
    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const std::uint32_t i0 = indices[i + 0];
        const std::uint32_t i1 = indices[i + 1];
        const std::uint32_t i2 = indices[i + 2];
        Vertex& v0 = vertices[i0];
        Vertex& v1 = vertices[i1];
        Vertex& v2 = vertices[i2];
        const glm::vec3 e1 = v1.position - v0.position;
        const glm::vec3 e2 = v2.position - v0.position;
        const glm::vec2 duv1 = v1.texCoord - v0.texCoord;
        const glm::vec2 duv2 = v2.texCoord - v0.texCoord;
        const float determinant = duv1.x * duv2.y - duv1.y * duv2.x;
        if (std::abs(determinant) < 1.0e-8f) continue;
        const float inv = 1.0f / determinant;
        const glm::vec3 tangent = inv * (e1 * duv2.y - e2 * duv1.y);
        const glm::vec3 bitangent = inv * (e2 * duv1.x - e1 * duv2.x);
        tangentSum[i0] += tangent;
        tangentSum[i1] += tangent;
        tangentSum[i2] += tangent;
        bitangentSum[i0] += bitangent;
        bitangentSum[i1] += bitangent;
        bitangentSum[i2] += bitangent;
    }
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const glm::vec3 n = glm::normalize(vertices[i].normal);
        glm::vec3 t = tangentSum[i];
        // Gram-Schmidt
        t -= n * glm::dot(n, t);
        if (glm::dot(t, t) < 1.0e-10f) {
            const glm::vec3 helper = std::abs(n.y) < 0.999f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(1.0f, 0.0f, 0.0f);
            t = glm::normalize(glm::cross(helper, n));
        } else {
            t = glm::normalize(t);
        }
        const glm::vec3 b = bitangentSum[i];
        const float handedness = glm::dot(glm::cross(n, t), b) < 0.0f ? -1.0f : 1.0f;
        vertices[i].tangent = glm::vec4(t, handedness);
    }
}
void GenerateTangentsArrays(const float* pos, const float* nrm, const float* uv,
                            const std::uint32_t* idx, std::size_t idxCount, std::size_t vertCount,
                            float* tanOut) {
  std::vector<Vertex> verts(vertCount);
  for (std::size_t i = 0; i < vertCount; i++) {
    verts[i].position = glm::vec3(pos[i * 3], pos[i * 3 + 1], pos[i * 3 + 2]);
    verts[i].normal = glm::vec3(nrm[i * 3], nrm[i * 3 + 1], nrm[i * 3 + 2]);
    verts[i].texCoord = uv ? glm::vec2(uv[i * 2], uv[i * 2 + 1]) : glm::vec2(0.0f);
  }
  std::vector<std::uint32_t> indices(idx, idx + idxCount);
  GenerateTangents(verts, indices);
  for (std::size_t i = 0; i < vertCount; i++) {
    tanOut[i * 4 + 0] = verts[i].tangent.x;
    tanOut[i * 4 + 1] = verts[i].tangent.y;
    tanOut[i * 4 + 2] = verts[i].tangent.z;
    tanOut[i * 4 + 3] = verts[i].tangent.w;
  }
}
}  // namespace stw
