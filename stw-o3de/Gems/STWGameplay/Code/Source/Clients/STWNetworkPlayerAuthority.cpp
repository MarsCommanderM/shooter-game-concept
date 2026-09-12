#include "STWNetworkPlayerAuthority.h"

namespace STWGameplay
{
    STWNetworkPlayerAuthority::~STWNetworkPlayerAuthority()
    {
        Unbind();
    }

    bool STWNetworkPlayerAuthority::BindPrimary(
        AZ::EntityId entityId, PlayerSliceModel& model, PhysXPlayerRuntime& physics)
    {
        if (!entityId.IsValid())
        {
            return false;
        }

        Unbind();
        m_entityId = entityId;
        m_externalModel = &model;
        m_externalPhysics = &physics;
        m_model = m_externalModel;
        m_physics = m_externalPhysics;
        ResetNetworkState();
        return true;
    }

    bool STWNetworkPlayerAuthority::BindAdditional(
        AZ::EntityId entityId, EnemyCollectionModel& sharedEnemies)
    {
        if (!entityId.IsValid())
        {
            return false;
        }

        Unbind();
        m_ownedModel = AZStd::make_unique<PlayerSliceModel>(sharedEnemies);
        if (!m_ownedModel)
        {
            return false;
        }

        m_entityId = entityId;
        m_model = m_ownedModel.get();
        m_physics = &m_ownedPhysics;
        ResetNetworkState();
        return true;
    }

    void STWNetworkPlayerAuthority::Unbind()
    {
        ShutdownPhysics();
        m_ownedModel.reset();
        m_entityId = AZ::EntityId();
        m_externalModel = nullptr;
        m_externalPhysics = nullptr;
        m_model = nullptr;
        m_physics = nullptr;
        ResetNetworkState();
    }

    bool STWNetworkPlayerAuthority::InitializePhysics()
    {
        if (!IsBound() || m_physics == nullptr)
        {
            return false;
        }
        return UsesCompositionRootRuntime() ? m_physics->IsValid() : m_physics->Initialize();
    }

    void STWNetworkPlayerAuthority::ShutdownPhysics()
    {
        if (!UsesCompositionRootRuntime())
        {
            m_ownedPhysics.Shutdown();
        }
    }

    bool STWNetworkPlayerAuthority::CreateCommand(const PlayerInput& sampledInput, PlayerCommand& command)
    {
        if (!IsBound() || !PlayerCommand(sampledInput, 1u).IsFinite())
        {
            return false;
        }

        m_nextCommandSequence = AdvancePlayerSimulationSequence(m_nextCommandSequence);
        command = MakePlayerCommand(sampledInput, m_nextCommandSequence);
        return command.IsFinite();
    }

    bool STWNetworkPlayerAuthority::SubmitCommand(const PlayerCommand& command)
    {
        if (!IsBound() || command.m_sequence == InvalidPlayerSimulationSequence || !command.IsFinite())
        {
            return false;
        }
        if (m_lastReceivedSequence != InvalidPlayerSimulationSequence
            && !IsNewerPlayerSimulationSequence(command.m_sequence, m_lastReceivedSequence))
        {
            return false;
        }
        if (!m_commandHistory.Push(command))
        {
            return false;
        }

        m_latestCommand = command;
        m_lastReceivedSequence = command.m_sequence;
        m_commandAvailable = true;
        return true;
    }

    bool STWNetworkPlayerAuthority::GetCommandForFixedStep(
        PlayerCommand& command, bool& isNewCommand) const
    {
        if (!m_commandAvailable || m_commandHistory.Empty())
        {
            return false;
        }

        command = m_latestCommand;
        isNewCommand = false;
        for (size_t offset = 0; offset < m_commandHistory.Size(); ++offset)
        {
            PlayerCommand candidate;
            if (!m_commandHistory.TryGetAt(offset, candidate))
            {
                return false;
            }
            if (m_lastAppliedSequence == InvalidPlayerSimulationSequence
                || IsNewerPlayerSimulationSequence(candidate.m_sequence, m_lastAppliedSequence))
            {
                command = candidate;
                isNewCommand = true;
                break;
            }
        }
        return true;
    }

    void STWNetworkPlayerAuthority::MarkCommandApplied(PlayerCommandSequence sequence)
    {
        if (sequence != InvalidPlayerSimulationSequence
            && (m_lastAppliedSequence == InvalidPlayerSimulationSequence
                || IsNewerPlayerSimulationSequence(sequence, m_lastAppliedSequence)))
        {
            m_lastAppliedSequence = sequence;
        }
    }

    void STWNetworkPlayerAuthority::CaptureAuthoritativeSnapshot(
        PlayerCommandSequence acknowledgedCommandSequence)
    {
        m_nextSnapshotSequence = AdvancePlayerSimulationSequence(m_nextSnapshotSequence);
        m_physicalReadbackSequence = AdvancePlayerSimulationSequence(m_physicalReadbackSequence);

        const PlayerState& player = m_model->GetPlayer();
        const WeaponState& weapon = m_model->GetWeapon();
        AuthoritativePlayerSnapshot snapshot;
        snapshot.m_snapshotSequence = m_nextSnapshotSequence;
        snapshot.m_acknowledgedCommandSequence = acknowledgedCommandSequence;
        snapshot.m_physicalReadbackSequence = m_physicalReadbackSequence;
        snapshot.m_physicalStateSynchronized = true;
        snapshot.m_position = player.m_position;
        snapshot.m_grounded = player.m_grounded;
        snapshot.m_requestedSimulationVelocity = m_model->GetMovementVelocity();
        snapshot.m_yaw = player.m_yaw;
        snapshot.m_pitch = player.m_pitch;
        snapshot.m_health = player.m_health;
        snapshot.m_alive = player.m_alive;
        snapshot.m_crouchDesired = player.m_crouchDesired;
        snapshot.m_slideActive = player.m_slideActive;
        snapshot.m_mantleRequested = player.m_mantleRequested;
        snapshot.m_mantleActive = player.m_mantleActive;
        snapshot.m_activeEquipmentSlot = m_model->GetActiveEquipmentSlot();
        snapshot.m_activeEquipmentProfile = m_model->GetActiveEquipmentProfileId();
        snapshot.m_magazine = weapon.m_magazine;
        snapshot.m_reserve = weapon.m_reserve;
        snapshot.m_charges = weapon.m_charges;
        snapshot.m_cooldownRemaining = weapon.m_cooldownRemaining;
        snapshot.m_reloadRemaining = weapon.m_reloadRemaining;
        snapshot.m_reloading = weapon.m_reloading;
        snapshot.m_deathEvents = player.m_deathEvents;
        snapshot.m_respawnEvents = player.m_respawnEvents;
        snapshot.m_lastAcceptedUseEventId = m_model->GetLastAcceptedUseEventId();
        m_authoritativeSnapshot = snapshot;
    }

    ReconciliationEvaluation STWNetworkPlayerAuthority::ProcessAuthoritativeSnapshot(
        const AuthoritativePlayerSnapshot& authoritativeSnapshot)
    {
        ReconciliationEvaluation evaluation = PlayerReconciliationPolicy::EvaluateIncoming(
            m_authoritativeSnapshot,
            authoritativeSnapshot,
            m_nextCommandSequence,
            m_lastAcceptedSnapshotSequence);

        if (evaluation.m_snapshotStatus == ReconciliationSnapshotStatus::Accepted)
        {
            m_lastAcceptedSnapshotSequence = authoritativeSnapshot.m_snapshotSequence;
            if (evaluation.m_acknowledgementUsable)
            {
                evaluation.m_discardedCommandCount = m_commandHistory.DiscardThrough(
                    authoritativeSnapshot.m_acknowledgedCommandSequence);
            }
        }

        m_lastReconciliationEvaluation = evaluation;
        return evaluation;
    }

    void STWNetworkPlayerAuthority::ResetNetworkState()
    {
        m_commandHistory.Reset();
        m_latestCommand = {};
        m_nextCommandSequence = InvalidPlayerSimulationSequence;
        m_lastReceivedSequence = InvalidPlayerSimulationSequence;
        m_lastAppliedSequence = InvalidPlayerSimulationSequence;
        m_nextSnapshotSequence = InvalidPlayerSimulationSequence;
        m_physicalReadbackSequence = InvalidPlayerSimulationSequence;
        m_lastAcceptedSnapshotSequence = InvalidPlayerSimulationSequence;
        m_authoritativeSnapshot = {};
        m_lastReconciliationEvaluation = {};
        m_commandAvailable = false;
    }
} // namespace STWGameplay
