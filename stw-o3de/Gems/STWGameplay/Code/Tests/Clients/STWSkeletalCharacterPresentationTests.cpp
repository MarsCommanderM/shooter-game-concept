#include <gtest/gtest.h>

#include <STWGameplay/STWSkeletalCharacterPresentation.h>

namespace STWGameplay
{
    TEST(STWSkeletalCharacterPresentationTests, IdleAndLocomotionMapFromAuthoritativeEnemyState)
    {
        EXPECT_EQ(
            STWSkeletalCharacterPresentation::MapGameplayState(EnemyBehaviorState::Idle, true),
            STWSkeletalPresentationState::Idle);
        EXPECT_EQ(
            STWSkeletalCharacterPresentation::MapGameplayState(EnemyBehaviorState::Detect, true),
            STWSkeletalPresentationState::Idle);
        EXPECT_EQ(
            STWSkeletalCharacterPresentation::MapGameplayState(EnemyBehaviorState::Chase, true),
            STWSkeletalPresentationState::Locomotion);
        EXPECT_EQ(
            STWSkeletalCharacterPresentation::MapGameplayState(EnemyBehaviorState::Attack, true),
            STWSkeletalPresentationState::Locomotion);
    }

    TEST(STWSkeletalCharacterPresentationTests, DeadStateMapsToDeathPresentation)
    {
        EXPECT_EQ(
            STWSkeletalCharacterPresentation::MapGameplayState(EnemyBehaviorState::Dead, false),
            STWSkeletalPresentationState::Death);
        EXPECT_EQ(
            STWSkeletalCharacterPresentation::MapGameplayState(EnemyBehaviorState::Idle, false),
            STWSkeletalPresentationState::Death);
    }

    TEST(STWSkeletalCharacterPresentationTests, AuthoritySnapshotIsReadOnly)
    {
        EnemyState before;
        const EnemyState after = before;
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsGameplayStateUnchanged(before, after));

        EnemyState changed = before;
        changed.m_health -= 10.0f;
        EXPECT_FALSE(STWSkeletalCharacterPresentation::IsGameplayStateUnchanged(before, changed));
    }

    TEST(STWSkeletalCharacterPresentationTests, AnimationTimeProgressionIsDeterministic)
    {
        EXPECT_FALSE(STWSkeletalCharacterPresentation::IsAnimationTimeAdvancing(1.0f, 1.0f));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsAnimationTimeAdvancing(1.0f, 1.01f));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsAnimationTimeAdvancing(1.99f, 0.01f));
    }

    TEST(STWSkeletalCharacterPresentationTests, AssetReadinessRequiresTheFullPresentationLifecycle)
    {
        EXPECT_FALSE(STWSkeletalCharacterPresentation::IsAssetLifecycleComplete(true, true, true, false));
        EXPECT_FALSE(STWSkeletalCharacterPresentation::IsAssetLifecycleComplete(true, true, false, true));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsAssetLifecycleComplete(true, true, true, true));
    }

    TEST(STWSkeletalCharacterPresentationTests, DefaultProfileIsTheShippedBoxCharacterUnchanged)
    {
        STWSkeletalCharacterPresentation presentation;
        const SkeletalCharacterProfile& profile = presentation.GetProfile();
        EXPECT_EQ(&profile, &SkeletalCharacterProfile::Box());
        EXPECT_STREQ(profile.m_actorPath, "assets/characters/stw_character_01/stw_character_01.actor");
        EXPECT_STREQ(profile.m_idleMotionPath, "assets/characters/stw_character_01/stw_character_01_idle.motion");
        EXPECT_STREQ(profile.m_locomotionMotionPath, "assets/characters/stw_character_01/stw_character_01_locomotion.motion");
        EXPECT_STREQ(profile.m_deathMotionPath, "assets/characters/stw_character_01/stw_character_01_death.motion");
        EXPECT_FLOAT_EQ(profile.m_originOffsetZ, -1.0f);
        EXPECT_STREQ(profile.m_probeJointName, "upper_spine");
        EXPECT_EQ(profile.m_materialOverridePrefix, nullptr);
    }

    TEST(STWSkeletalCharacterPresentationTests, RinProfilePointsAtTheImportedProductsAndCanBeSelected)
    {
        const SkeletalCharacterProfile& rin = SkeletalCharacterProfile::Rin();
        EXPECT_STREQ(rin.m_name, "STW_ENEMY_01_RIN");
        EXPECT_STREQ(rin.m_actorPath, "assets/enemies/stw_enemy_01_rin/stw_enemy_01_rin.actor");
        EXPECT_STREQ(rin.m_locomotionMotionPath, "assets/enemies/stw_enemy_01_rin/stw_enemy_01_rin_jog.motion");
        EXPECT_NE(&rin, &SkeletalCharacterProfile::Box());
        EXPECT_FLOAT_EQ(rin.m_originOffsetZ, -1.2f);
        EXPECT_STREQ(rin.m_probeJointName, "C_spine_03_JNT");
        EXPECT_STREQ(rin.m_materialOverridePrefix, "assets/enemies/stw_enemy_01_rin/");
        STWSkeletalCharacterPresentation presentation;
        presentation.SetProfile(rin);
        EXPECT_EQ(&presentation.GetProfile(), &rin);
    }

    TEST(STWSkeletalCharacterPresentationTests, SkeletonContractHasStableRagdollCompatibleJointNames)
    {
        EXPECT_EQ(STWSkeletalCharacterPresentation::RequiredJointCount(), 18u);
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("root"));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("pelvis"));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("upper_spine"));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("head"));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("upper_arm.L"));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("upper_arm.R"));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("upper_leg.L"));
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsRequiredJointName("upper_leg.R"));
        EXPECT_FALSE(STWSkeletalCharacterPresentation::IsRequiredJointName("physics_body"));
    }

    TEST(STWSkeletalCharacterPresentationTests, PresentationStateDoesNotBecomeGameplayAuthority)
    {
        EnemyState input;
        EnemyState afterPresentation = input;
        const STWSkeletalPresentationState presentationState =
            STWSkeletalCharacterPresentation::MapGameplayState(input.m_behaviorState, input.m_alive);
        EXPECT_EQ(presentationState, STWSkeletalPresentationState::Idle);
        EXPECT_TRUE(STWSkeletalCharacterPresentation::IsGameplayStateUnchanged(input, afterPresentation));
        EXPECT_EQ(afterPresentation.m_position, input.m_position);
        EXPECT_EQ(afterPresentation.m_health, input.m_health);
        EXPECT_EQ(afterPresentation.m_damageEvents, input.m_damageEvents);
    }
}
