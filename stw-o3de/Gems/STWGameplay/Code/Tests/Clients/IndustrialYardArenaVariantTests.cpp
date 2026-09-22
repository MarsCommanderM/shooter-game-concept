#include <AzTest/AzTest.h>

#include <STWGameplay/IndustrialYardArenaVariant.h>

namespace STWGameplay
{
    using namespace IndustrialYardIntegration;

    namespace
    {
        IdentitySet CompleteIdentities(const std::array<VisualAssetSpec, VisualAssetCount>& specs, bool models)
        {
            IdentitySet identities{};
            for (size_t index = 0; index < VisualAssetCount; ++index)
            {
                identities[index] = { models ? specs[index].modelPath : specs[index].materialPath, true };
            }
            return identities;
        }
    }

    TEST(IndustrialYardArenaVariantTests, NineCompletePairsSelectIndustrialYard)
    {
        const auto specs = IndustrialAssetSet();
        VariantSelector selector;
        const VariantDecision decision =
            selector.Update(CompleteIdentities(specs, true), CompleteIdentities(specs, false));
        EXPECT_TRUE(decision.complete);
        EXPECT_EQ(decision.variant, Variant::IndustrialYard);
    }

    TEST(IndustrialYardArenaVariantTests, SingleMissingModelKeepsCompleteCurrentArena)
    {
        const auto specs = IndustrialAssetSet();
        IdentitySet models = CompleteIdentities(specs, true);
        IdentitySet materials = CompleteIdentities(specs, false);
        models[3] = { specs[3].modelPath, false }; // one missing identity must not produce a mixed scene
        VariantSelector selector;
        const VariantDecision decision = selector.Update(models, materials);
        EXPECT_FALSE(decision.complete);
        EXPECT_EQ(decision.variant, Variant::CurrentArena);
    }

    TEST(IndustrialYardArenaVariantTests, InvalidIdentityWithPresentPathStaysUnresolved)
    {
        const auto specs = IndustrialAssetSet();
        IdentitySet models = CompleteIdentities(specs, true);
        IdentitySet materials = CompleteIdentities(specs, false);
        materials[7].valid = false; // path found by the catalog but the resulting AssetId is invalid
        EXPECT_FALSE(IsResolved(materials[7]));
        EXPECT_FALSE(IsCompleteIndustrialSet(models, materials));
    }

    TEST(IndustrialYardArenaVariantTests, RepeatedUnresolvedUpdatesReportFallbackOnce)
    {
        const auto specs = IndustrialAssetSet();
        IdentitySet models = CompleteIdentities(specs, true);
        IdentitySet materials{}; // fully unresolved
        VariantSelector selector;
        const VariantDecision first = selector.Update(models, materials);
        const VariantDecision second = selector.Update(models, materials);
        const VariantDecision third = selector.Update(models, materials);
        EXPECT_TRUE(first.emitFallbackDiagnostic);
        EXPECT_FALSE(second.emitFallbackDiagnostic);
        EXPECT_FALSE(third.emitFallbackDiagnostic);
        EXPECT_EQ(first.variant, Variant::CurrentArena);
        EXPECT_EQ(second.variant, Variant::CurrentArena);
        EXPECT_EQ(third.variant, Variant::CurrentArena);
    }

    TEST(IndustrialYardArenaVariantTests, CompletingAfterUnresolvedReportsFallbackAgainOnRegression)
    {
        const auto specs = IndustrialAssetSet();
        IdentitySet completeModels = CompleteIdentities(specs, true);
        IdentitySet completeMaterials = CompleteIdentities(specs, false);
        IdentitySet emptyMaterials{};
        VariantSelector selector;
        selector.Update(completeModels, emptyMaterials); // unresolved: reports once
        const VariantDecision resolved = selector.Update(completeModels, completeMaterials);
        EXPECT_TRUE(resolved.complete);
        EXPECT_EQ(resolved.variant, Variant::IndustrialYard);
        const VariantDecision regressed = selector.Update(completeModels, emptyMaterials);
        EXPECT_TRUE(regressed.emitFallbackDiagnostic); // a fresh unresolved episode reports again
        EXPECT_EQ(regressed.variant, Variant::CurrentArena);
    }
}
