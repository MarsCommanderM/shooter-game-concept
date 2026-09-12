#include <AzTest/AzTest.h>

#include "Network/STWPlayerNetworkComponent.h"

#include "Clients/STWGameplaySystemComponent.h"

#include <limits>

namespace STWGameplay
{
    TEST(STWPlayerNetworkComponentTests, CommandRoundTripPreservesGameplayInput)
    {
        PlayerInput sampledInput;
        sampledInput.m_forward = 0.75f;
        sampledInput.m_strafe = -0.25f;
        sampledInput.m_lookX = 1.5f;
        sampledInput.m_lookY = -0.5f;
        sampledInput.m_sprint = true;
        sampledInput.m_jump = true;
        sampledInput.m_crouch = true;
        sampledInput.m_mantle = true;
        sampledInput.m_fire = true;
        sampledInput.m_reload = true;
        sampledInput.m_switchWeapon = true;
        sampledInput.m_requestedEquipmentSlot = 2;
        const PlayerCommand expected = MakePlayerCommand(sampledInput, 41u);

        STWPlayerNetworkComponentNetworkInput networkInput;
        STWPlayerNetworkComponent::WriteCommand(networkInput, expected);

        PlayerCommand actual;
        ASSERT_TRUE(STWPlayerNetworkComponent::ReadCommand(networkInput, actual));
        EXPECT_EQ(actual.m_sequence, expected.m_sequence);
        EXPECT_FLOAT_EQ(actual.m_forward, expected.m_forward);
        EXPECT_FLOAT_EQ(actual.m_strafe, expected.m_strafe);
        EXPECT_FLOAT_EQ(actual.m_lookX, expected.m_lookX);
        EXPECT_FLOAT_EQ(actual.m_lookY, expected.m_lookY);
        EXPECT_EQ(actual.m_sprint, expected.m_sprint);
        EXPECT_EQ(actual.m_jump, expected.m_jump);
        EXPECT_EQ(actual.m_crouch, expected.m_crouch);
        EXPECT_EQ(actual.m_mantle, expected.m_mantle);
        EXPECT_EQ(actual.m_fire, expected.m_fire);
        EXPECT_EQ(actual.m_reload, expected.m_reload);
        EXPECT_EQ(actual.m_switchWeapon, expected.m_switchWeapon);
        EXPECT_EQ(actual.m_requestedEquipmentSlot, expected.m_requestedEquipmentSlot);
    }

    TEST(STWPlayerNetworkComponentTests, InvalidCommandSequenceIsRejected)
    {
        STWPlayerNetworkComponentNetworkInput networkInput;
        PlayerCommand decoded;

        EXPECT_FALSE(STWPlayerNetworkComponent::ReadCommand(networkInput, decoded));
    }

    TEST(STWPlayerNetworkComponentTests, NonFiniteGameplayInputIsRejected)
    {
        PlayerInput sampledInput;
        sampledInput.m_forward = std::numeric_limits<float>::quiet_NaN();
        const PlayerCommand command = MakePlayerCommand(sampledInput, 7u);

        STWPlayerNetworkComponentNetworkInput networkInput;
        STWPlayerNetworkComponent::WriteCommand(networkInput, command);

        PlayerCommand decoded;
        EXPECT_FALSE(STWPlayerNetworkComponent::ReadCommand(networkInput, decoded));
    }

    TEST(STWPlayerNetworkComponentTests, NetworkCommandBoundaryAcceptsOnlyOneBoundPlayerAndDeduplicates)
    {
        STWGameplaySystemComponent gameplay;
        const AZ::EntityId boundEntityId(42);
        const AZ::EntityId otherEntityId(43);

        EXPECT_TRUE(gameplay.BindNetworkPlayer(boundEntityId));
        EXPECT_FALSE(gameplay.BindNetworkPlayer(otherEntityId));

        PlayerInput sampledInput;
        sampledInput.m_forward = 1.0f;
        const PlayerCommand firstCommand = MakePlayerCommand(sampledInput, 1u);
        EXPECT_TRUE(gameplay.SubmitNetworkCommand(boundEntityId, firstCommand));
        EXPECT_FALSE(gameplay.SubmitNetworkCommand(boundEntityId, firstCommand));
        EXPECT_FALSE(gameplay.SubmitNetworkCommand(otherEntityId, firstCommand));

        const PlayerCommand newerCommand = MakePlayerCommand(sampledInput, 2u);
        EXPECT_TRUE(gameplay.SubmitNetworkCommand(boundEntityId, newerCommand));

        gameplay.UnbindNetworkPlayer(boundEntityId);
        EXPECT_FALSE(gameplay.SubmitNetworkCommand(boundEntityId, newerCommand));
    }
}
