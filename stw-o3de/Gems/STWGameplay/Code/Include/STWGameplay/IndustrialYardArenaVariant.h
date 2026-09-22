#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace STWGameplay::IndustrialYardIntegration
{
    constexpr std::size_t VisualAssetCount = 9;

    struct VisualAssetSpec
    {
        std::string_view group;
        std::string_view modelPath;
        std::string_view materialPath;
    };

    struct AssetIdentity
    {
        std::string_view path;
        bool valid = false;
    };

    using IdentitySet = std::array<AssetIdentity, VisualAssetCount>;

    enum class Variant
    {
        CurrentArena,
        IndustrialYard
    };

    struct VariantDecision
    {
        Variant variant = Variant::CurrentArena;
        bool complete = false;
        bool retryDue = false;
        bool emitFallbackDiagnostic = false;
    };

    std::array<VisualAssetSpec, VisualAssetCount> IndustrialAssetSet();
    bool IsResolved(const AssetIdentity& identity);
    bool IsCompleteIndustrialSet(const IdentitySet& models, const IdentitySet& materials);

    class VariantSelector final
    {
    public:
        static constexpr std::uint32_t RetryCadenceUpdates = 60;

        VariantDecision Update(const IdentitySet& models, const IdentitySet& materials);

    private:
        std::uint32_t m_updatesSinceRetry = 0;
        bool m_seenAnUpdate = false;
        bool m_lastComplete = false;
        bool m_fallbackDiagnosticReported = false;
    };
} // namespace STWGameplay::IndustrialYardIntegration
