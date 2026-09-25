#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <STWGameplay/PlayerCommandHistory.h>
#include <STWGameplay/PlayerPrediction.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <STWGameplay/PresentationInterpolation.h>

#include "PhysXPlayerRuntime.h"

namespace STWGameplay
{
    //! Owns the per-network-player gameplay and physical authority boundary.
    //!
    //! The first bound player may adapt the composition root's existing local model and
    //! character runtime for compatibility. Additional players own independent player and
    //! physical runtimes while sharing the composition root's single enemy collection.
    class STWNetworkPlayerAuthority final
    {
    public:
        STWNetworkPlayerAuthority() = default;
        ~STWNetworkPlayerAuthority();

        bool BindPrimary(AZ::EntityId entityId, PlayerSliceModel& model, PhysXPlayerRuntime& physics);
        bool BindAdditional(AZ::EntityId entityId, EnemyCollectionModel& sharedEnemies);
        bool BindRemote(AZ::EntityId entityId);
        void Unbind();

        bool IsBound() const { return m_entityId.IsValid(); }
        bool IsRemote() const { return m_isRemote; }
        bool UsesCompositionRootRuntime() const { return m_externalModel != nullptr; }
        AZ::EntityId GetEntityId() const { return m_entityId; }

        PlayerSliceModel& GetModel() { return *m_model; }
        const PlayerSliceModel& GetModel() const { return *m_model; }
        PhysXPlayerRuntime& GetPhysics() { return *m_physics; }
        const PhysXPlayerRuntime& GetPhysics() const { return *m_physics; }

        bool InitializePhysics();
        void ShutdownPhysics();

        bool CreateCommand(const PlayerInput& sampledInput, PlayerCommand& command);
        bool SubmitCommand(const PlayerCommand& command);
        bool GetCommandForFixedStep(PlayerCommand& command, bool& isNewCommand) const;
        void MarkCommandApplied(PlayerCommandSequence sequence);

        const PlayerCommandHistory& GetCommandHistory() const { return m_commandHistory; }
        //! Mirrors the composition root's own "clear command history on
        //! respawn" step (previously only reachable for the primary player,
        //! since m_commandHistory had no public mutator) - avoids replaying
        //! stale pre-respawn commands against the just-reset position.
        void ClearCommandHistory() { m_commandHistory.Clear(); }
        size_t GetCommandHistorySize() const { return m_commandHistory.Size(); }
        void BeginPhysxRewind(size_t steps) { m_physxRewindRemaining = steps; }
        bool HasPhysxRewind() const { return m_physxRewindRemaining > 0; }
        size_t ConsumePhysxRewindStep()
        {
            if (m_physxRewindRemaining == 0)
            {
                return 0;
            }
            --m_physxRewindRemaining;
            return m_physxRewindRemaining;
        }
        void NotePhysxRewindPosition(const AZ::Vector3& position, bool grounded)
        {
            m_authoritativeSnapshot.m_position = position;
            m_authoritativeSnapshot.m_grounded = grounded;
        }
        PlayerCommandSequence GetLastAppliedCommandSequence() const { return m_lastAppliedSequence; }
        PlayerCommandSequence GetLastReceivedCommandSequence() const { return m_lastReceivedSequence; }
        PlayerCommandSequence GetNextCommandSequence() const { return m_nextCommandSequence; }

        void CaptureAuthoritativeSnapshot(PlayerCommandSequence acknowledgedCommandSequence);
        const AuthoritativePlayerSnapshot& GetAuthoritativeSnapshot() const { return m_authoritativeSnapshot; }
        ReconciliationEvaluation ProcessAuthoritativeSnapshot(
            const AuthoritativePlayerSnapshot& authoritativeSnapshot);
        const ReconciliationEvaluation& GetLastReconciliationEvaluation() const
        {
            return m_lastReconciliationEvaluation;
        }

        const AuthoritativePlayerSnapshot* GetRemoteSnapshot() const
        {
            return m_hasRemoteSnapshot ? &m_remoteSnapshot : nullptr;
        }

        const PresentationFrameState* GetRemotePresentationState() const
        {
            return m_remotePresentationInterpolation.HasState()
                ? &m_remotePresentationInterpolation.GetCurrentState() : nullptr;
        }

    private:
        void ResetNetworkState();

        AZ::EntityId m_entityId;
        bool m_isRemote = false;
        PlayerSliceModel* m_externalModel = nullptr;
        PhysXPlayerRuntime* m_externalPhysics = nullptr;
        PlayerSliceModel* m_model = nullptr;
        PhysXPlayerRuntime* m_physics = nullptr;
        AZStd::unique_ptr<PlayerSliceModel> m_ownedModel;
        PhysXPlayerRuntime m_ownedPhysics;

        PlayerCommandHistory m_commandHistory;
        PlayerCommand m_latestCommand;
        PlayerCommandSequence m_nextCommandSequence = InvalidPlayerSimulationSequence;
        PlayerCommandSequence m_lastReceivedSequence = InvalidPlayerSimulationSequence;
        PlayerCommandSequence m_lastAppliedSequence = InvalidPlayerSimulationSequence;
        PlayerSnapshotSequence m_nextSnapshotSequence = InvalidPlayerSimulationSequence;
        PlayerSnapshotSequence m_physicalReadbackSequence = InvalidPlayerSimulationSequence;
        PlayerSnapshotSequence m_lastAcceptedSnapshotSequence = InvalidPlayerSimulationSequence;
        AuthoritativePlayerSnapshot m_authoritativeSnapshot;
        AuthoritativePlayerSnapshot m_remoteSnapshot;
        PresentationInterpolation m_remotePresentationInterpolation;
        ReconciliationEvaluation m_lastReconciliationEvaluation;
        bool m_hasRemoteSnapshot = false;
        bool m_commandAvailable = false;
        size_t m_physxRewindRemaining = 0;
    };
} // namespace STWGameplay
