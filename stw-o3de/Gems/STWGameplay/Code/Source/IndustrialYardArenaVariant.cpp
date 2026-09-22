#include <STWGameplay/IndustrialYardArenaVariant.h>

namespace STWGameplay::IndustrialYardIntegration
{
    std::array<VisualAssetSpec, VisualAssetCount> IndustrialAssetSet()
    {
        return {{
            { "deck", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_deck.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_concrete.azmaterial" },
            { "wall", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_wall.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_facade.azmaterial" },
            { "struct", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_struct.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_steel.azmaterial" },
            { "cover", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_cover.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_steel.azmaterial" },
            { "arch", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_arch.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_steel.azmaterial" },
            { "beacon", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_beacon.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_steel.azmaterial" },
            { "trim", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_trim.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_hazard.azmaterial" },
            { "props", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_props.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_sign.azmaterial" },
            { "mark", "assets/industrialyard/stw_industrial_yard_01/environment/stw_industrial_yard_01_mark.fbx.azmodel", "assets/industrialyard/stw_industrial_yard_01/environment/materials/stw_industrial_yard_01_water.azmaterial" },
        }};
    }

    bool IsResolved(const AssetIdentity& identity)
    {
        return identity.valid && !identity.path.empty();
    }

    bool IsCompleteIndustrialSet(const IdentitySet& models, const IdentitySet& materials)
    {
        for (std::size_t index = 0; index < VisualAssetCount; ++index)
        {
            if (!IsResolved(models[index]) || !IsResolved(materials[index]))
            {
                return false;
            }
        }
        return true;
    }

    VariantDecision VariantSelector::Update(const IdentitySet& models, const IdentitySet& materials)
    {
        const bool complete = IsCompleteIndustrialSet(models, materials);
        VariantDecision decision;
        decision.complete = complete;
        decision.variant = complete ? Variant::IndustrialYard : Variant::CurrentArena;

        if (!m_seenAnUpdate)
        {
            m_seenAnUpdate = true;
            decision.retryDue = !complete;
        }
        else if (!complete)
        {
            if (m_lastComplete)
            {
                decision.retryDue = true;
                m_updatesSinceRetry = 0;
            }
            else if (++m_updatesSinceRetry >= RetryCadenceUpdates)
            {
                decision.retryDue = true;
                m_updatesSinceRetry = 0;
            }
        }
        else
        {
            m_updatesSinceRetry = 0;
        }

        if (!complete)
        {
            decision.emitFallbackDiagnostic = !m_fallbackDiagnosticReported;
            m_fallbackDiagnosticReported = true;
        }
        else
        {
            m_fallbackDiagnosticReported = false;
        }
        m_lastComplete = complete;
        return decision;
    }
} // namespace STWGameplay::IndustrialYardIntegration
