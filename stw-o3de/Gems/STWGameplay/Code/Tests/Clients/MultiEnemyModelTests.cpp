#include <AzTest/AzTest.h>
#include <STWGameplay/EnemyCollectionModel.h>
#include <STWGameplay/EnemyPresentation.h>
#include <STWGameplay/PlayerSliceModel.h>
#include "Clients/PhysXEnemyRuntime.h"

namespace STWGameplay
{
    namespace
    {
        constexpr EnemyId EnemyA = 1;
        constexpr EnemyId EnemyB = 2;
        constexpr EnemyId EnemyC = 3;

        void Eliminate(EnemyCollectionModel& enemies, EnemyId id)
        {
            ASSERT_TRUE(enemies.ApplyDamage(id, 1000.0f));
        }

        void PutNearPlayer(EnemyCollectionModel& enemies, EnemyId id, float x)
        {
            ASSERT_TRUE(enemies.SynchronizePhysicalPosition(id, AZ::Vector3(x, 1.5f, 1.2f)));
        }
    }

    TEST(MultiEnemyModelTests, ThreeEnemiesCanExistSimultaneously)
    {
        EnemyCollectionModel enemies;
        EXPECT_EQ(enemies.GetEnemyCount(), 3);
        EXPECT_EQ(enemies.GetRequiredEnemyCount(), 3);
        EXPECT_EQ(enemies.GetAliveCount(), 3);
        EXPECT_TRUE(enemies.AreRequiredEnemiesAlive());
    }

    TEST(MultiEnemyModelTests, EnemyIdsAreStableAndUnique)
    {
        EnemyCollectionModel enemies;
        const EnemyId first = enemies.GetInstanceByIndex(0).m_combat.GetId();
        const EnemyId second = enemies.GetInstanceByIndex(1).m_combat.GetId();
        const EnemyId third = enemies.GetInstanceByIndex(2).m_combat.GetId();
        EXPECT_NE(first, second);
        EXPECT_NE(first, third);
        EXPECT_NE(second, third);
        EXPECT_EQ(first, EnemyA);
        EXPECT_EQ(second, EnemyB);
        EXPECT_EQ(third, EnemyC);
    }

    TEST(MultiEnemyModelTests, DamageTargetsOnlySelectedEnemy)
    {
        EnemyCollectionModel enemies;
        const float healthB = enemies.GetEnemy(EnemyB)->GetState().m_health;
        const float healthC = enemies.GetEnemy(EnemyC)->GetState().m_health;
        ASSERT_TRUE(enemies.ApplyDamage(EnemyA, 16.0f));
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyA)->GetState().m_health, 84.0f);
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_health, healthB);
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyC)->GetState().m_health, healthC);
        EXPECT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_damageEvents, 0);
        EXPECT_EQ(enemies.GetEnemy(EnemyC)->GetState().m_damageEvents, 0);
    }

    TEST(MultiEnemyModelTests, IndependentHealthStateAcrossEnemies)
    {
        EnemyCollectionModel enemies;
        ASSERT_TRUE(enemies.ApplyDamage(EnemyA, 40.0f));
        ASSERT_TRUE(enemies.ApplyDamage(EnemyB, 20.0f));
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyA)->GetState().m_health, 60.0f);
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_health, 160.0f);
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyC)->GetState().m_health, 75.0f);
        EXPECT_EQ(enemies.GetEnemy(EnemyA)->GetState().m_deathEvents, 0);
        EXPECT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_deathEvents, 0);
    }

    TEST(MultiEnemyModelTests, IndependentAiStateAcrossEnemies)
    {
        EnemyCollectionModel enemies;
        PutNearPlayer(enemies, EnemyA, 0.0f);
        ASSERT_TRUE(enemies.SynchronizePhysicalPosition(EnemyB, AZ::Vector3(0.0f, 30.0f, 1.2f)));
        ASSERT_TRUE(enemies.SynchronizePhysicalPosition(EnemyC, AZ::Vector3(5.0f, 0.0f, 1.2f)));
        const AZ::Vector3 player(0.0f, 0.0f, 1.2f);
        ASSERT_TRUE(enemies.Update(0.1f, player));
        ASSERT_TRUE(enemies.Update(0.1f, player));
        EXPECT_EQ(enemies.GetEnemy(EnemyA)->GetState().m_behaviorState, EnemyBehaviorState::Attack);
        EXPECT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_behaviorState, EnemyBehaviorState::Idle);
        EXPECT_NE(enemies.GetEnemy(EnemyA)->GetState().m_behaviorState,
            enemies.GetEnemy(EnemyB)->GetState().m_behaviorState);
    }

    TEST(MultiEnemyModelTests, EnemyDeathDoesNotMutateOtherEnemies)
    {
        EnemyCollectionModel enemies;
        const EnemyState beforeB = enemies.GetEnemy(EnemyB)->GetState();
        const EnemyState beforeC = enemies.GetEnemy(EnemyC)->GetState();
        Eliminate(enemies, EnemyA);
        EXPECT_FALSE(enemies.GetEnemy(EnemyA)->GetState().m_alive);
        EXPECT_TRUE(enemies.GetEnemy(EnemyB)->GetState().m_alive);
        EXPECT_TRUE(enemies.GetEnemy(EnemyC)->GetState().m_alive);
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_health, beforeB.m_health);
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyC)->GetState().m_health, beforeC.m_health);
        EXPECT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_deathEvents, beforeB.m_deathEvents);
        EXPECT_EQ(enemies.GetEnemy(EnemyC)->GetState().m_deathEvents, beforeC.m_deathEvents);
    }

    TEST(MultiEnemyModelTests, EnemyRespawnDoesNotResetOtherEnemies)
    {
        EnemyCollectionModel enemies;
        ASSERT_TRUE(enemies.ApplyDamage(EnemyA, 20.0f));
        ASSERT_TRUE(enemies.ApplyDamage(EnemyB, 20.0f));
        const EnemyState beforeB = enemies.GetEnemy(EnemyB)->GetState();
        ASSERT_TRUE(enemies.ResetEnemy(EnemyA));
        EXPECT_TRUE(enemies.GetEnemy(EnemyA)->GetState().m_alive);
        EXPECT_EQ(enemies.GetEnemy(EnemyA)->GetState().m_respawnEvents, 1);
        EXPECT_FLOAT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_health, beforeB.m_health);
        EXPECT_TRUE(enemies.GetEnemy(EnemyB)->GetState().m_alive);
        EXPECT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_respawnEvents, beforeB.m_respawnEvents);
    }

    TEST(MultiEnemyModelTests, MultipleEnemiesCanAttackIndependently)
    {
        EnemyCollectionModel enemies;
        PutNearPlayer(enemies, EnemyA, -1.0f);
        PutNearPlayer(enemies, EnemyB, 0.0f);
        PutNearPlayer(enemies, EnemyC, 1.0f);
        const AZ::Vector3 player(0.0f, 0.0f, 1.2f);
        ASSERT_TRUE(enemies.Update(0.1f, player));
        ASSERT_TRUE(enemies.Update(0.1f, player));
        EXPECT_TRUE(enemies.TryAttackPlayer(EnemyA));
        EXPECT_TRUE(enemies.TryAttackPlayer(EnemyB));
        EXPECT_TRUE(enemies.TryAttackPlayer(EnemyC));
        EXPECT_FALSE(enemies.TryAttackPlayer(EnemyA));
        EXPECT_FALSE(enemies.TryAttackPlayer(EnemyB));
        EXPECT_FALSE(enemies.TryAttackPlayer(EnemyC));
        EXPECT_EQ(enemies.GetEnemy(EnemyA)->GetState().m_attackEvents, 1);
        EXPECT_EQ(enemies.GetEnemy(EnemyB)->GetState().m_attackEvents, 1);
        EXPECT_EQ(enemies.GetEnemy(EnemyC)->GetState().m_attackEvents, 1);
    }

    TEST(MultiEnemyModelTests, PlayerDamageStillUsesExistingAuthority)
    {
        PlayerSliceModel player;
        const float playerY = player.GetPlayer().m_position.GetY();
        ASSERT_TRUE(player.GetEnemies().SynchronizePhysicalPosition(EnemyA, AZ::Vector3(-1.0f, playerY, 1.2f)));
        ASSERT_TRUE(player.GetEnemies().SynchronizePhysicalPosition(EnemyB, AZ::Vector3(0.0f, playerY, 1.2f)));
        ASSERT_TRUE(player.GetEnemies().SynchronizePhysicalPosition(EnemyC, AZ::Vector3(1.0f, playerY, 1.2f)));
        ASSERT_TRUE(player.Update(0.1f, PlayerInput{}));
        ASSERT_TRUE(player.Update(0.1f, PlayerInput{}));
        EXPECT_EQ(player.GetPlayer().m_damageEvents, 3);
        EXPECT_FLOAT_EQ(player.GetPlayer().m_health, 40.0f);
        EXPECT_TRUE(player.GetPlayer().m_alive);
    }

    TEST(MultiEnemyModelTests, EncounterDoesNotCompleteAfterFirstEnemyDeath)
    {
        EnemyCollectionModel enemies;
        EncounterModel encounter;
        Eliminate(enemies, EnemyA);
        ASSERT_TRUE(encounter.Update(enemies));
        EXPECT_TRUE(encounter.IsActive());
        EXPECT_EQ(encounter.GetCompletedCount(), 0);
    }

    TEST(MultiEnemyModelTests, EncounterDoesNotCompleteAfterSecondEnemyDeath)
    {
        EnemyCollectionModel enemies;
        EncounterModel encounter;
        Eliminate(enemies, EnemyA);
        Eliminate(enemies, EnemyB);
        ASSERT_TRUE(encounter.Update(enemies));
        EXPECT_TRUE(encounter.IsActive());
        EXPECT_EQ(encounter.GetCompletedCount(), 0);
    }

    TEST(MultiEnemyModelTests, EncounterCompletesAfterRequiredEnemySetEliminated)
    {
        EnemyCollectionModel enemies;
        EncounterModel encounter;
        Eliminate(enemies, EnemyA);
        Eliminate(enemies, EnemyB);
        Eliminate(enemies, EnemyC);
        ASSERT_TRUE(encounter.Update(enemies));
        EXPECT_TRUE(encounter.IsCompleted());
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
    }

    TEST(MultiEnemyModelTests, EncounterCompletionTriggersExactlyOnce)
    {
        EnemyCollectionModel enemies;
        EncounterModel encounter;
        Eliminate(enemies, EnemyA);
        Eliminate(enemies, EnemyB);
        Eliminate(enemies, EnemyC);
        ASSERT_TRUE(encounter.Update(enemies));
        ASSERT_TRUE(encounter.Update(enemies));
        ASSERT_TRUE(encounter.Update(enemies));
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
    }

    TEST(MultiEnemyModelTests, EncounterRearmRestoresRequiredEnemySet)
    {
        EnemyCollectionModel enemies;
        EncounterModel encounter;
        Eliminate(enemies, EnemyA);
        Eliminate(enemies, EnemyB);
        Eliminate(enemies, EnemyC);
        ASSERT_TRUE(encounter.Update(enemies));
        ASSERT_TRUE(enemies.ResetRequiredEnemies());
        ASSERT_TRUE(encounter.Rearm(enemies));
        EXPECT_TRUE(encounter.IsActive());
        EXPECT_TRUE(enemies.AreRequiredEnemiesAlive());
        EXPECT_EQ(encounter.GetCompletedCount(), 1);
    }

    TEST(MultiEnemyModelTests, EnemyProfilesRemainDeterministic)
    {
        EnemyCollectionModel first;
        EnemyCollectionModel second;
        for (size_t index = 0; index < first.GetEnemyCount(); ++index)
        {
            const EnemyState& a = first.GetInstanceByIndex(index).m_combat.GetState();
            const EnemyState& b = second.GetInstanceByIndex(index).m_combat.GetState();
            EXPECT_EQ(a.m_id, b.m_id);
            EXPECT_EQ(a.m_archetype, b.m_archetype);
            EXPECT_FLOAT_EQ(a.m_maxHealth, b.m_maxHealth);
            EXPECT_FLOAT_EQ(a.m_health, b.m_health);
            EXPECT_TRUE(a.m_position.IsClose(b.m_position));
        }
    }

    TEST(MultiEnemyModelTests, AssaultProfileLoadsCorrectly)
    {
        const EnemyProfile& profile = GetEnemyProfile(EnemyArchetype::Assault);
        EXPECT_STREQ(profile.m_identifier, "ASSAULT");
        EXPECT_FLOAT_EQ(profile.m_maxHealth, 100.0f);
        EXPECT_FLOAT_EQ(profile.m_moveSpeed, 1.5f);
        EXPECT_FLOAT_EQ(profile.m_attackDamage, 20.0f);
    }

    TEST(MultiEnemyModelTests, HeavyProfileLoadsCorrectly)
    {
        const EnemyProfile& profile = GetEnemyProfile(EnemyArchetype::Heavy);
        EXPECT_STREQ(profile.m_identifier, "HEAVY");
        EXPECT_FLOAT_EQ(profile.m_maxHealth, 180.0f);
        EXPECT_FLOAT_EQ(profile.m_attackCooldown, 1.4f);
        EXPECT_FLOAT_EQ(profile.m_presentationScale, 1.20f);
    }

    TEST(MultiEnemyModelTests, SkirmisherProfileLoadsCorrectly)
    {
        const EnemyProfile& profile = GetEnemyProfile(EnemyArchetype::Skirmisher);
        EXPECT_STREQ(profile.m_identifier, "SKIRMISHER");
        EXPECT_FLOAT_EQ(profile.m_maxHealth, 75.0f);
        EXPECT_FLOAT_EQ(profile.m_moveSpeed, 2.2f);
        EXPECT_FLOAT_EQ(profile.m_attackCooldown, 0.65f);
    }

    TEST(MultiEnemyModelTests, EnemyPhysicalAuthorityRemainsInPhysXRuntime)
    {
        PhysXEnemyRuntime runtime;
        EXPECT_EQ(runtime.GetBoundEnemyId(), InvalidEnemyId);

        EnemyCollectionModel enemies;
        const AZ::Vector3 position(1.0f, 2.0f, 1.2f);
        ASSERT_TRUE(enemies.SynchronizePhysicalPosition(EnemyB, position));
        EXPECT_TRUE(enemies.GetEnemy(EnemyB)->GetState().m_position.IsClose(position));
        EXPECT_EQ(runtime.GetBoundEnemyId(), InvalidEnemyId);
    }

    TEST(MultiEnemyModelTests, EnemyPresentationCannotMutateGameplayState)
    {
        EnemyCollectionModel enemies;
        EnemyCombatModel& enemy = *enemies.GetEnemy(EnemyB);
        ASSERT_TRUE(enemy.ApplyDamage(20.0f));
        const EnemyState before = enemy.GetState();
        EnemyPresentation presentation;
        ASSERT_TRUE(presentation.Update(0.0f, enemy.GetState()));
        const EnemyState& after = enemy.GetState();
        EXPECT_EQ(after.m_id, before.m_id);
        EXPECT_FLOAT_EQ(after.m_health, before.m_health);
        EXPECT_EQ(after.m_damageEvents, before.m_damageEvents);
        EXPECT_EQ(after.m_behaviorState, before.m_behaviorState);
        EXPECT_FLOAT_EQ(presentation.GetScale().GetX(), GetEnemyProfile(EnemyArchetype::Heavy).m_presentationScale);
    }

    TEST(MultiEnemyModelTests, MultiEnemyCollectionSupportsMoreThanThreeEntries)
    {
        EnemyCollectionModel enemies;
        ASSERT_TRUE(enemies.AddEnemy(4, EnemyArchetype::Assault, AZ::Vector3(0.0f, 9.0f, 1.2f)));
        EXPECT_EQ(enemies.GetEnemyCount(), 4);
        EXPECT_EQ(enemies.GetRequiredEnemyCount(), 3);
        EXPECT_NE(enemies.GetEnemy(4), nullptr);
        EXPECT_EQ(enemies.GetEnemy(4)->GetId(), 4);
    }

    TEST(MultiEnemyModelTests, ResetPreservesStableIdentityPolicy)
    {
        EnemyCollectionModel enemies;
        EnemyCombatModel& enemy = *enemies.GetEnemy(EnemyC);
        const EnemyId id = enemy.GetId();
        const EnemyArchetype archetype = enemy.GetState().m_archetype;
        ASSERT_TRUE(enemy.ApplyDamage(1000.0f));
        enemy.Reset();
        EXPECT_EQ(enemy.GetId(), id);
        EXPECT_EQ(enemy.GetState().m_id, id);
        EXPECT_EQ(enemy.GetState().m_archetype, archetype);
        EXPECT_TRUE(enemy.GetState().m_alive);
        EXPECT_EQ(enemy.GetState().m_respawnEvents, 1);
    }
}
