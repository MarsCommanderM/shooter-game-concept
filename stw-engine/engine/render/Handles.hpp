#pragma once
// Backend-härtung Phase 3: IRenderer abstrahiert ENGINE-Konzepte, nicht GL-Begriffe.
// OpenGLRenderer/WebRenderer implementieren dasselbe Interface; BindVAO & Co bleiben intern.
#include <cstdint>
namespace stw {
using BufferHandle = std::uint32_t;
using TextureHandle = std::uint32_t;
using SamplerHandle = std::uint32_t;
using PipelineHandle = std::uint32_t;

struct BufferDesc {
    enum class Kind { Vertex, Index, Uniform, Storage } kind = Kind::Vertex;
    std::uint64_t size = 0;
    bool dynamic = false;
};
struct TextureDesc {
    std::uint32_t width = 1, height = 1;
    std::uint32_t mips = 1;
    bool hdr = false;         // RGBA16F
    bool cube = false;
    bool depth = false;
};
struct SamplerDesc {
    bool filter = true;
    bool clampToBorder = false;
};
struct PipelineDesc {
    const char* vertexGLSL = nullptr;
    const char* fragmentGLSL = nullptr;
    bool cullFront = false;
    bool depthWrite = true;
    bool usesShadow = false;
    bool usesIBL = false;
};
struct FrameInfo {
    const float* view;
    const float* projection;
    const float* lightVP;
    const float* cameraPosition;
};
struct DrawCommand {
    PipelineHandle pipeline = 0;
    BufferHandle vertexBuffer = 0;
    BufferHandle indexBuffer = 0;
    std::uint32_t indexCount = 0;
    const float* model;
    const void* materialUniforms = nullptr;
};

class IRenderer {
   public:
    virtual ~IRenderer() = default;
    virtual BufferHandle CreateBuffer(const BufferDesc& desc, const void* initialData) = 0;
    virtual TextureHandle CreateTexture(const TextureDesc& desc) = 0;
    virtual SamplerHandle CreateSampler(const SamplerDesc& desc) = 0;
    virtual PipelineHandle CreatePipeline(const PipelineDesc& desc) = 0;
    virtual void BeginFrame(const FrameInfo& frame) = 0;
    virtual void Submit(const DrawCommand& command) = 0;
    virtual void EndFrame() = 0;
};
}  // namespace stw
