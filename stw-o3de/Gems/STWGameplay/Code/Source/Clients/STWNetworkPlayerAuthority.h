#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

#include <STWGameplay/PlayerCommandHistory.h>
#include <STWGameplay/PlayerPrediction.h>
#include <STWGameplay/PlayerSliceModel.h>

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
        void Unbind();

        bool IsBound() const { return m_entityId.IsValid(); }
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
        size_t GetCommandHistorySize() const { return m_commandHistory.Size(); }
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

    private:
        void ResetNetworkState();

        AZ::EntityId m_entityId;
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
        ReconciliationEvaluation m_lastReconciliationEvaluation;
        bool m_commandAvailable = false;
    };
} // namespace STWGameplay
