#include <AzTest/AzTest.h>

#include <STWGameplay/CharacterPhysicalState.h>

namespace STWGameplay
{
    TEST(CharacterPhysicalStateTests, StartsAnimatedAndCompletesThePhysicalLifecycle)
    {
        CharacterPhysicalState state;
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Animated);

        ASSERT_TRUE(state.RequestDeath());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::TransitionToPhysics);

        ASSERT_TRUE(state.ConfirmPhysicsAuthority());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Ragdoll);

        ASSERT_TRUE(state.MarkSettled());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Settled);

        ASSERT_TRUE(state.RequestReset());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::TransitionToAnimation);

        ASSERT_TRUE(state.CompleteAnimationRestore());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Animated);
    }

    TEST(CharacterPhysicalStateTests, RejectsTransitionsThatSkipAuthorityHandoffs)
    {
        CharacterPhysicalState state;
        EXPECT_FALSE(state.TryTransition(CharacterPhysicsMode::Ragdoll));
        EXPECT_FALSE(state.TryTransition(CharacterPhysicsMode::Settled));
        EXPECT_FALSE(state.TryTransition(CharacterPhysicsMode::TransitionToAnimation));
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Animated);

        ASSERT_TRUE(state.TryTransition(CharacterPhysicsMode::TransitionToPhysics));
        EXPECT_FALSE(state.TryTransition(CharacterPhysicsMode::Animated));
        EXPECT_FALSE(state.TryTransition(CharacterPhysicsMode::Settled));
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::TransitionToPhysics);
    }

    TEST(CharacterPhysicalStateTests, RepeatedDeathAndResetRequestsAreIdempotent)
    {
        CharacterPhysicalState state;

        EXPECT_TRUE(state.RequestDeath());
        EXPECT_TRUE(state.RequestDeath());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::TransitionToPhysics);

        ASSERT_TRUE(state.ConfirmPhysicsAuthority());
        EXPECT_TRUE(state.RequestDeath());
        ASSERT_TRUE(state.MarkSettled());
        EXPECT_TRUE(state.RequestDeath());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Settled);

        EXPECT_TRUE(state.RequestReset());
        EXPECT_TRUE(state.RequestReset());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::TransitionToAnimation);

        EXPECT_TRUE(state.CompleteAnimationRestore());
        EXPECT_TRUE(state.CompleteAnimationRestore());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Animated);
    }

    TEST(CharacterPhysicalStateTests, TransitionTableKeepsAnimationAndPhysicsExclusive)
    {
        EXPECT_TRUE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::Animated, CharacterPhysicsMode::TransitionToPhysics));
        EXPECT_TRUE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::TransitionToPhysics, CharacterPhysicsMode::Ragdoll));
        EXPECT_TRUE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::Ragdoll, CharacterPhysicsMode::Settled));
        EXPECT_TRUE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::Settled, CharacterPhysicsMode::TransitionToAnimation));
        EXPECT_TRUE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::TransitionToAnimation, CharacterPhysicsMode::Animated));

        EXPECT_FALSE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::Animated, CharacterPhysicsMode::Ragdoll));
        EXPECT_FALSE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::Ragdoll, CharacterPhysicsMode::Animated));
        EXPECT_FALSE(CharacterPhysicalState::IsValidTransition(
            CharacterPhysicsMode::TransitionToAnimation, CharacterPhysicsMode::Ragdoll));
    }

    TEST(CharacterPhysicalStateTests, ResetCanCancelAnIncompletePhysicalHandoff)
    {
        CharacterPhysicalState state;
        ASSERT_TRUE(state.RequestDeath());
        ASSERT_TRUE(state.RequestReset());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::TransitionToAnimation);
        EXPECT_TRUE(state.CompleteAnimationRestore());
        EXPECT_EQ(state.GetMode(), CharacterPhysicsMode::Animated);
    }
} // namespace STWGameplay
