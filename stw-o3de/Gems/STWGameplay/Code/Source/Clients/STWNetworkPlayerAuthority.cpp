#include "STWNetworkPlayerAuthority.h"

#include <AzCore/Debug/Trace.h>

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

    bool STWNetworkPlayerAuthority::BindRemote(AZ::EntityId entityId)
    {
        if (!entityId.IsValid())
        {
            return false;
        }

        Unbind();
        m_entityId = entityId;
        m_isRemote = true;
        ResetNetworkState();
        return true;
    }

    void STWNetworkPlayerAuthority::Unbind()
    {
        ShutdownPhysics();
        m_ownedModel.reset();
        m_entityId = AZ::EntityId();
        m_isRemote = false;
        m_externalModel = nullptr;
        m_externalPhysics = nullptr;
        m_model = nullptr;
        m_physics = nullptr;
        ResetNetworkState();
    }

    bool STWNetworkPlayerAuthority::InitializePhysics()
    {
        if (!IsBound() || m_isRemote || m_physics == nullptr)
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
        if (!IsBound() || m_isRemote || !PlayerCommand(sampledInput, 1u).IsFinite())
        {
            return false;
        }

        m_nextCommandSequence = AdvancePlayerSimulationSequence(m_nextCommandSequence);
        command = MakePlayerCommand(sampledInput, m_nextCommandSequence);
        return command.IsFinite();
    }

    bool STWNetworkPlayerAuthority::SubmitCommand(const PlayerCommand& command)
    {
        if (!IsBound() || m_isRemote || command.m_sequence == InvalidPlayerSimulationSequence
            || !command.IsFinite())
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
        if (!IsBound() || m_isRemote || m_model == nullptr)
        {
            return;
        }

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
        ReconciliationEvaluation evaluation = m_isRemote
            ? PlayerReconciliationPolicy::EvaluateRemoteIncoming(
                authoritativeSnapshot, m_lastAcceptedSnapshotSequence)
            : PlayerReconciliationPolicy::EvaluateIncoming(
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
            if (!m_isRemote
                && evaluation.m_comparison.m_comparisonValid
                && evaluation.m_comparison.m_decision == ReconciliationDecision::CorrectionRequired
                && m_model != nullptr
                && m_model->ApplyAuthoritativeCorrection(authoritativeSnapshot))
            {
                const AuthoritativePlayerSnapshot predictedBefore = m_authoritativeSnapshot;
                PlayerReconciliationPolicy::CopyComparedFields(
                    m_authoritativeSnapshot, authoritativeSnapshot);
                const bool physxReset = m_physics != nullptr
                    && m_physics->ResetPosition(authoritativeSnapshot.m_position);
                BeginPhysxRewind(m_commandHistory.Size(), authoritativeSnapshot.m_position);
                const AZStd::string entityText = m_entityId.ToString();
                AZ_Printf(
                    "STWGameplay",
                    "STW_MP_PHYSX_REWIND queued=%zu reset=%d entity=%s\n",
                    m_physxRewindRemaining,
                    physxReset ? 1 : 0,
                    entityText.c_str());
                AZ_Printf(
                    "STWGameplay",
                    "STW_MP_RECONCILIATION_CORRECTION applied=1 replayed=0 physx_rewind_queued=%zu entity=%s field=%s predicted=(%.3f,%.3f,%.3f) pred_health=%.3f auth=(%.3f,%.3f,%.3f) auth_health=%.3f\n",
                    m_physxRewindRemaining,
                    entityText.c_str(),
                    PlayerReconciliationPolicy::FirstMismatch(predictedBefore, authoritativeSnapshot),
                    static_cast<float>(predictedBefore.m_position.GetX()),
                    static_cast<float>(predictedBefore.m_position.GetY()),
                    static_cast<float>(predictedBefore.m_position.GetZ()),
                    predictedBefore.m_health,
                    static_cast<float>(authoritativeSnapshot.m_position.GetX()),
                    static_cast<float>(authoritativeSnapshot.m_position.GetY()),
                    static_cast<float>(authoritativeSnapshot.m_position.GetZ()),
                    authoritativeSnapshot.m_health);
            }
            if (m_isRemote)
            {
                PresentationFrameState presentationState;
                presentationState.m_position = authoritativeSnapshot.m_position;
                presentationState.m_yaw = authoritativeSnapshot.m_yaw;
                presentationState.m_pitch = authoritativeSnapshot.m_pitch;
                if (m_remotePresentationInterpolation.Advance(presentationState))
                {
                    m_remoteSnapshot = authoritativeSnapshot;
                    m_hasRemoteSnapshot = true;
                }
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
        m_remoteSnapshot = {};
        m_remotePresentationInterpolation = {};
        m_lastReconciliationEvaluation = {};
        m_hasRemoteSnapshot = false;
        m_commandAvailable = false;
        m_physxRewindRemaining = 0;
    }
} // namespace STWGameplay
