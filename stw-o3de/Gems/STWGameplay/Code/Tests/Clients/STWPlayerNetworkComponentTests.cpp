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

    TEST(STWPlayerNetworkComponentTests, NetworkCommandBoundaryKeepsMultiplePlayersIndependent)
    {
        STWGameplaySystemComponent gameplay;
        const AZ::EntityId boundEntityId(42);
        const AZ::EntityId otherEntityId(43);

        EXPECT_TRUE(gameplay.BindNetworkPlayer(boundEntityId));
        EXPECT_TRUE(gameplay.BindNetworkPlayer(otherEntityId));

        PlayerInput sampledInput;
        sampledInput.m_forward = 1.0f;
        const PlayerCommand firstCommand = MakePlayerCommand(sampledInput, 1u);
        const PlayerCommand otherFirstCommand = MakePlayerCommand(sampledInput, 1u);
        EXPECT_TRUE(gameplay.SubmitNetworkCommand(boundEntityId, firstCommand));
        EXPECT_FALSE(gameplay.SubmitNetworkCommand(boundEntityId, firstCommand));
        EXPECT_TRUE(gameplay.SubmitNetworkCommand(otherEntityId, otherFirstCommand));

        const PlayerCommand newerCommand = MakePlayerCommand(sampledInput, 2u);
        EXPECT_TRUE(gameplay.SubmitNetworkCommand(boundEntityId, newerCommand));
        EXPECT_FALSE(gameplay.SubmitNetworkCommand(otherEntityId, otherFirstCommand));

        EXPECT_EQ(gameplay.GetNetworkPlayerCount(), 2u);
        EXPECT_EQ(gameplay.GetNetworkPlayerCommandHistorySize(boundEntityId), 2u);
        EXPECT_EQ(gameplay.GetNetworkPlayerCommandHistorySize(otherEntityId), 1u);
        EXPECT_EQ(gameplay.GetPlayerCommandHistory().Size(), 2u);

        gameplay.UnbindNetworkPlayer(boundEntityId);
        EXPECT_FALSE(gameplay.SubmitNetworkCommand(boundEntityId, newerCommand));
        EXPECT_TRUE(gameplay.GetPlayerCommandHistory().Empty());
        EXPECT_TRUE(gameplay.SubmitNetworkCommand(otherEntityId, MakePlayerCommand(sampledInput, 2u)));
    }

    TEST(STWPlayerNetworkComponentTests, NetworkPlayerAuthoritySlotsAreBounded)
    {
        STWGameplaySystemComponent gameplay;
        for (size_t index = 0; index < STWGameplaySystemComponent::MaxNetworkPlayerCount; ++index)
        {
            EXPECT_TRUE(gameplay.BindNetworkPlayer(AZ::EntityId(100 + index)));
        }

        EXPECT_EQ(gameplay.GetNetworkPlayerCount(), STWGameplaySystemComponent::MaxNetworkPlayerCount);
        EXPECT_FALSE(gameplay.BindNetworkPlayer(AZ::EntityId(999)));

        gameplay.UnbindNetworkPlayer(AZ::EntityId(103));
        EXPECT_EQ(gameplay.GetNetworkPlayerCount(), STWGameplaySystemComponent::MaxNetworkPlayerCount - 1);
        EXPECT_TRUE(gameplay.BindNetworkPlayer(AZ::EntityId(999)));
        EXPECT_EQ(gameplay.GetNetworkPlayerCount(), STWGameplaySystemComponent::MaxNetworkPlayerCount);
    }

    TEST(STWPlayerNetworkComponentTests, AdditionalAuthoritiesShareEnemiesButOwnPlayerState)
    {
        EnemyCollectionModel sharedEnemies;
        STWNetworkPlayerAuthority first;
        STWNetworkPlayerAuthority second;

        ASSERT_TRUE(first.BindAdditional(AZ::EntityId(200), sharedEnemies));
        ASSERT_TRUE(second.BindAdditional(AZ::EntityId(201), sharedEnemies));
        EXPECT_EQ(&first.GetModel().GetEnemies(), &sharedEnemies);
        EXPECT_EQ(&second.GetModel().GetEnemies(), &sharedEnemies);
        EXPECT_NE(&first.GetModel(), &second.GetModel());
        EXPECT_NE(&first.GetPhysics(), &second.GetPhysics());

        PlayerCommand firstCommand;
        PlayerCommand secondCommand;
        EXPECT_TRUE(first.CreateCommand(PlayerInput{}, firstCommand));
        EXPECT_TRUE(second.CreateCommand(PlayerInput{}, secondCommand));
        EXPECT_EQ(firstCommand.m_sequence, 1u);
        EXPECT_EQ(secondCommand.m_sequence, 1u);
        EXPECT_TRUE(first.SubmitCommand(firstCommand));
        EXPECT_TRUE(second.SubmitCommand(secondCommand));
        EXPECT_EQ(first.GetCommandHistorySize(), 1u);
        EXPECT_EQ(second.GetCommandHistorySize(), 1u);
    }

    TEST(STWPlayerNetworkComponentTests, SnapshotBoundaryRoutesOnlyTheBoundNetworkPlayer)
    {
        STWGameplaySystemComponent gameplay;
        const AZ::EntityId boundEntityId(42);
        const AZ::EntityId otherEntityId(43);
        AuthoritativePlayerSnapshot snapshot;
        snapshot.m_snapshotSequence = 1u;

        EXPECT_TRUE(gameplay.BindNetworkPlayer(boundEntityId));
        EXPECT_FALSE(gameplay.ReceiveNetworkSnapshot(otherEntityId, snapshot));
        EXPECT_TRUE(gameplay.ReceiveNetworkSnapshot(boundEntityId, snapshot));
    }

    TEST(STWPlayerNetworkComponentTests, SimulatedProxyConsumesOrderedRemoteSnapshotsWithoutAuthority)
    {
        EXPECT_EQ(
            GetSTWNetworkRoleBinding(Multiplayer::NetEntityRole::Client),
            STWNetworkRoleBinding::RemoteSnapshot);
        EXPECT_EQ(
            GetSTWNetworkRoleBinding(Multiplayer::NetEntityRole::Authority),
            STWNetworkRoleBinding::GameplayAuthority);
        EXPECT_EQ(
            GetSTWNetworkRoleBinding(Multiplayer::NetEntityRole::Autonomous),
            STWNetworkRoleBinding::GameplayAuthority);

        STWGameplaySystemComponent gameplay;
        const AZ::EntityId remoteEntityId(77);
        ASSERT_TRUE(gameplay.BindRemoteNetworkPlayer(remoteEntityId));

        AuthoritativePlayerSnapshot snapshot;
        snapshot.m_snapshotSequence = 10u;
        snapshot.m_physicalReadbackSequence = 10u;
        snapshot.m_physicalStateSynchronized = true;
        snapshot.m_acknowledgedCommandSequence = 27u;
        snapshot.m_position = AZ::Vector3(12.0f, -4.0f, 2.0f);
        snapshot.m_yaw = 0.8f;
        snapshot.m_pitch = -0.2f;
        snapshot.m_health = 45.0f;
        snapshot.m_alive = false;
        snapshot.m_crouchDesired = true;
        snapshot.m_activeEquipmentSlot = EquipmentSlot::Secondary;
        snapshot.m_activeEquipmentProfile = EquipmentProfileId::STW_SIDEARM_01;
        snapshot.m_magazine = 3;
        snapshot.m_reserve = 12;
        snapshot.m_deathEvents = 2;
        snapshot.m_respawnEvents = 3;

        EXPECT_TRUE(gameplay.ReceiveNetworkSnapshot(remoteEntityId, snapshot));

        const AuthoritativePlayerSnapshot* remoteSnapshot =
            gameplay.GetRemoteNetworkSnapshot(remoteEntityId);
        ASSERT_NE(remoteSnapshot, nullptr);
        EXPECT_EQ(remoteSnapshot->m_snapshotSequence, 10u);
        EXPECT_EQ(remoteSnapshot->m_position, snapshot.m_position);
        EXPECT_FLOAT_EQ(remoteSnapshot->m_yaw, snapshot.m_yaw);
        EXPECT_FLOAT_EQ(remoteSnapshot->m_pitch, snapshot.m_pitch);
        EXPECT_FLOAT_EQ(remoteSnapshot->m_health, snapshot.m_health);
        EXPECT_EQ(remoteSnapshot->m_alive, snapshot.m_alive);
        EXPECT_EQ(remoteSnapshot->m_activeEquipmentSlot, snapshot.m_activeEquipmentSlot);
        EXPECT_EQ(remoteSnapshot->m_activeEquipmentProfile, snapshot.m_activeEquipmentProfile);
        EXPECT_EQ(remoteSnapshot->m_deathEvents, snapshot.m_deathEvents);
        EXPECT_EQ(remoteSnapshot->m_respawnEvents, snapshot.m_respawnEvents);

        const PresentationFrameState* presentationState =
            gameplay.GetRemotePlayerPresentationState(remoteEntityId);
        ASSERT_NE(presentationState, nullptr);
        EXPECT_EQ(presentationState->m_position, snapshot.m_position);
        EXPECT_FLOAT_EQ(presentationState->m_yaw, snapshot.m_yaw);
        EXPECT_FLOAT_EQ(presentationState->m_pitch, snapshot.m_pitch);

        EXPECT_EQ(gameplay.GetNetworkPlayerCount(), 0u);
        PlayerCommand command;
        EXPECT_FALSE(gameplay.CreateNetworkCommand(remoteEntityId, command));

        AuthoritativePlayerSnapshot newerSnapshot = snapshot;
        newerSnapshot.m_snapshotSequence = 11u;
        newerSnapshot.m_position = AZ::Vector3(-8.0f, 6.0f, 1.0f);
        EXPECT_TRUE(gameplay.ReceiveNetworkSnapshot(remoteEntityId, newerSnapshot));

        AuthoritativePlayerSnapshot staleSnapshot = snapshot;
        staleSnapshot.m_position = AZ::Vector3(100.0f, 100.0f, 100.0f);
        EXPECT_FALSE(gameplay.ReceiveNetworkSnapshot(remoteEntityId, staleSnapshot));
        EXPECT_FALSE(gameplay.ReceiveNetworkSnapshot(remoteEntityId, newerSnapshot));

        remoteSnapshot = gameplay.GetRemoteNetworkSnapshot(remoteEntityId);
        ASSERT_NE(remoteSnapshot, nullptr);
        EXPECT_EQ(remoteSnapshot->m_snapshotSequence, newerSnapshot.m_snapshotSequence);
        EXPECT_EQ(remoteSnapshot->m_position, newerSnapshot.m_position);
    }
}
