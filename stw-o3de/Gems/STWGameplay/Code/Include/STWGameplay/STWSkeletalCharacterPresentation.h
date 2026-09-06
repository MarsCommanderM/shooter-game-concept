#pragma once

#include <cstddef>

#include <AzCore/Asset/AssetCommon.h>
#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Transform.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>
#include <AzCore/std/string/string.h>

#include <STWGameplay/EnemyCombatModel.h>

namespace AZ
{
    class Entity;
}

namespace STWGameplay
{
    enum class STWSkeletalPresentationState : AZ::u8
    {
        Uninitialized = 0,
        Idle,
        Locomotion,
        Death,
        ResetToIdle
    };

    //! Presentation-only EMotionFX/Atom character integration.
    //! Gameplay and physics state is supplied as a const snapshot and is never owned here.
    class STWSkeletalCharacterPresentation final
    {
    public:
        STWSkeletalCharacterPresentation() = default;
        ~STWSkeletalCharacterPresentation();

        STWSkeletalCharacterPresentation(const STWSkeletalCharacterPresentation&) = delete;
        STWSkeletalCharacterPresentation& operator=(const STWSkeletalCharacterPresentation&) = delete;

        //! Resolves the registered products and creates the presentation entity when ready.
        void Update(float deltaTime, const EnemyState& gameplayState);
        void Shutdown();
        void ResetToIdle();

        static STWSkeletalPresentationState MapGameplayState(EnemyBehaviorState behaviorState, bool alive);
        static bool IsGameplayStateUnchanged(const EnemyState& before, const EnemyState& after);
        static bool IsAnimationTimeAdvancing(float previousTime, float currentTime);
        static bool IsAssetLifecycleComplete(
            bool actorAssetReady, bool actorInstanceReady, bool skinnedMeshVisible, bool motionAssetReady);
        static bool IsRequiredJointName(const char* name);
        static size_t RequiredJointCount();

        bool IsActorAssetReady() const { return m_actorAssetReady; }
        bool IsMotionAssetReady() const { return m_motionAssetReady; }
        bool IsActorInstanceReady() const { return m_actorInstanceReady; }
        bool IsSkinnedMeshVisible() const { return m_skinnedMeshVisible; }
        bool IsAnimationActive() const { return m_animationActive; }
        bool IsAnimationTimeAdvancing() const { return m_animationTimeAdvancing; }
        bool IsBoneTransformDeltaPositive() const { return m_boneTransformDeltaPositive; }
        bool IsAuthoritySeparated() const { return m_authoritySeparated; }
        bool IsEntityContextCorrect() const { return m_entityContextCorrect; }
        bool IsMainSceneResolved() const { return m_mainSceneResolved; }
        bool IsSkinnedFeatureProcessorAvailable() const { return m_skinnedFeatureProcessorAvailable; }
        bool IsMeshFeatureProcessorAvailable() const { return m_meshFeatureProcessorAvailable; }
        size_t GetSkeletonNodeCount() const { return m_skeletonNodeCount; }
        float GetAnimationTime() const { return m_animationTime; }
        STWSkeletalPresentationState GetState() const { return m_state; }
        const char* GetStateName() const;
        bool WasIdleObserved() const { return m_idleObserved; }
        bool WasLocomotionObserved() const { return m_locomotionObserved; }
        bool WasDeathObserved() const { return m_deathObserved; }
        bool WasResetToIdleObserved() const { return m_resetToIdleObserved; }

    private:
        bool TryCreatePresentationEntity();
        bool ResolveProducts();
        void SelectMotion(const AZ::Data::AssetId& motionAssetId, bool loop);
        void SampleRuntimeDiagnostics();

        // The game entity context owns this presentation entity after it is registered.
        AZ::Entity* m_entity = nullptr;
        AZ::EntityId m_entityId;
        AZ::Data::AssetId m_actorAssetId;
        AZ::Data::AssetId m_idleMotionAssetId;
        AZ::Data::AssetId m_locomotionMotionAssetId;
        AZ::Data::AssetId m_deathMotionAssetId;
        AZStd::string m_actorAssetPath;
        AZStd::string m_idleMotionAssetPath;
        AZStd::string m_locomotionMotionAssetPath;
        AZStd::string m_deathMotionAssetPath;
        AZ::Data::AssetId m_currentMotionAssetId;

        bool m_productsResolved = false;
        bool m_actorAssetReady = false;
        bool m_motionAssetReady = false;
        bool m_actorInstanceReady = false;
        bool m_skinnedMeshVisible = false;
        bool m_animationActive = false;
        bool m_animationTimeAdvancing = false;
        bool m_boneTransformDeltaPositive = false;
        bool m_authoritySeparated = true;
        bool m_entityContextCorrect = false;
        bool m_mainSceneResolved = false;
        bool m_skinnedFeatureProcessorAvailable = false;
        bool m_meshFeatureProcessorAvailable = false;
        bool m_haveAnimationSample = false;
        bool m_haveBoneSample = false;
        float m_animationTime = 0.0f;
        float m_previousAnimationTime = 0.0f;
        AZ::Quaternion m_previousBoneRotation = AZ::Quaternion::CreateIdentity();
        size_t m_boneRotationDiagnosticTotalSamples = 0;
        size_t m_boneRotationDiagnosticStateSamples = 0;
        STWSkeletalPresentationState m_boneRotationDiagnosticLastState = STWSkeletalPresentationState::Uninitialized;
        size_t m_skeletonNodeCount = 0;
        size_t m_probeJointIndex = 0;
        STWSkeletalPresentationState m_state = STWSkeletalPresentationState::Uninitialized;
        STWSkeletalPresentationState m_lastMappedState = STWSkeletalPresentationState::Uninitialized;
        bool m_idleObserved = false;
        bool m_locomotionObserved = false;
        bool m_deathObserved = false;
        bool m_resetToIdleObserved = false;
    };
}
