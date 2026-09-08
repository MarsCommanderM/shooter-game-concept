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
