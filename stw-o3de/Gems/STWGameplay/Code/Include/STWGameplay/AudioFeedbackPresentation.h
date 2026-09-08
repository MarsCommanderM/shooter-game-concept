#pragma once

#include <AzCore/base.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace STWGameplay
{
    enum class AudioFeedbackEventType : AZ::u8
    {
        Fire = 0,
        Reload,
        Hit,
        Impact,
        EnemyState,
        Movement,
        Count
    };

    struct AudioFeedbackInput
    {
        bool m_shotFired = false;
        bool m_reloading = false;
        bool m_hitConfirmed = false;
        bool m_impactEvent = false;
        bool m_enemyStateChanged = false;
        bool m_enemyAttackEvent = false;
        bool m_enemyDeathEvent = false;
        bool m_movementActive = false;
        bool m_sprinting = false;
        bool m_crouched = false;
        bool m_sliding = false;
        bool m_reset = false;
        AZ::Vector3 m_impactPosition = AZ::Vector3::CreateZero();
    };

    //! Presentation-only audio feedback. It consumes copied gameplay events and never writes
    //! to gameplay, physics, weapon, enemy, encounter, or network state.
    class AudioFeedbackPresentation
    {
    public:
        AudioFeedbackPresentation();
        ~AudioFeedbackPresentation();

        AudioFeedbackPresentation(const AudioFeedbackPresentation&) = delete;
        AudioFeedbackPresentation& operator=(const AudioFeedbackPresentation&) = delete;

        bool Activate();
        void Deactivate();
        bool Update(float deltaTime, const AudioFeedbackInput& input);
        void Reset();

        bool IsPresentationActive() const { return m_presentationActive; }
        bool IsVisualOnly() const { return true; }
        bool IsBackendReady() const { return m_backendReady; }
        bool WasReset() const { return m_resetCount > 0; }
        AZ::u32 GetResetCount() const { return m_resetCount; }
        AZ::u32 GetEventCount(AudioFeedbackEventType eventType) const;
        const AZStd::vector<AudioFeedbackEventType>& GetEventSequence() const { return m_eventSequence; }

    private:
        struct Impl;

        void TryInitializeBackend();
        void Emit(AudioFeedbackEventType eventType, float volume);

        AZStd::unique_ptr<Impl> m_impl;
        AZStd::array<AZ::u32, static_cast<size_t>(AudioFeedbackEventType::Count)> m_eventCounts{};
        AZStd::vector<AudioFeedbackEventType> m_eventSequence;
        bool m_presentationActive = false;
        bool m_backendReady = false;
        bool m_previousShotFired = false;
        bool m_previousReloading = false;
        bool m_previousHitConfirmed = false;
        bool m_previousImpactEvent = false;
        bool m_previousEnemyEvent = false;
        float m_movementTimer = 0.0f;
        AZ::u32 m_resetCount = 0;
    };
}
