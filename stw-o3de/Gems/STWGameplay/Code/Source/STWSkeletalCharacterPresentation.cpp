#include <STWGameplay/STWSkeletalCharacterPresentation.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Component/TransformBus.h>
#include <AzCore/Debug/Trace.h>
#include <AzCore/Math/MathUtils.h>
#include <AzFramework/Components/TransformComponent.h>
#include <AzFramework/Entity/EntityContextBus.h>
#include <AzFramework/Entity/GameEntityContextBus.h>

#include <Atom/Feature/Mesh/MeshFeatureProcessorInterface.h>
#include <Atom/Feature/SkinnedMesh/SkinnedMeshFeatureProcessorInterface.h>
#include <Atom/RPI.Public/Scene.h>

#include <Integration/Assets/ActorAsset.h>
#include <Integration/Assets/MotionAsset.h>
#include <Integration/Components/ActorComponent.h>
#include <Integration/Components/SimpleMotionComponent.h>
#include <Integration/ActorComponentBus.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/Material/MaterialComponentConstants.h>
#include <Integration/SimpleMotionComponentBus.h>

#include <cmath>
#include <cstring>

namespace STWGameplay
{
    namespace
    {
        constexpr const char* ActorPath = "assets/characters/stw_character_01/stw_character_01.actor";
        constexpr const char* IdleMotionPath = "assets/characters/stw_character_01/stw_character_01_idle.motion";
        constexpr const char* LocomotionMotionPath =
            "assets/characters/stw_character_01/stw_character_01_locomotion.motion";
        constexpr const char* DeathMotionPath = "assets/characters/stw_character_01/stw_character_01_death.motion";
        // Angular distance in radians. This is intentionally a rotation epsilon rather than
        // a squared translation threshold because upper_spine's authored motion changes rotation.
        constexpr float BoneAngularDeltaEpsilon = 0.0001f;
        constexpr size_t BoneRotationDiagnosticMaxSamples = 64;
        constexpr size_t BoneRotationDiagnosticSamplesPerState = 8;
        constexpr float AnimationTimeEpsilon = 0.00001f;
        constexpr float CharacterOriginOffset = -1.0f;
        constexpr const char* RequiredJointNames[] = {
            "root", "pelvis", "spine", "upper_spine", "neck", "head",
            "upper_arm.L", "lower_arm.L", "hand.L", "upper_arm.R", "lower_arm.R", "hand.R",
            "upper_leg.L", "lower_leg.L", "foot.L", "upper_leg.R", "lower_leg.R", "foot.R"};

        template<class AssetType>
        AZ::Data::AssetId FindAssetId(const char* path)
        {
            AZ::Data::AssetId assetId;
            AZ::Data::AssetCatalogRequestBus::BroadcastResult(
                assetId, &AZ::Data::AssetCatalogRequests::GetAssetIdByPath,
                path, azrtti_typeid<AssetType>(), false);
            return assetId;
        }
    }

    const SkeletalCharacterProfile& SkeletalCharacterProfile::Box()
    {
        static const SkeletalCharacterProfile profile{
            "STW_CHARACTER_01", ActorPath, IdleMotionPath, LocomotionMotionPath, DeathMotionPath, CharacterOriginOffset,
            "upper_spine", nullptr};
        return profile;
    }

    const SkeletalCharacterProfile& SkeletalCharacterProfile::Rin()
    {
        static const SkeletalCharacterProfile profile{
            "STW_ENEMY_01_RIN",
            "assets/enemies/stw_enemy_01_rin/stw_enemy_01_rin.actor",
            "assets/enemies/stw_enemy_01_rin/stw_enemy_01_rin_walk.motion",
            "assets/enemies/stw_enemy_01_rin/stw_enemy_01_rin_jog.motion",
            "assets/enemies/stw_enemy_01_rin/stw_enemy_01_rin_walk.motion",
            -1.2f, // enemy capsule centre height (PhysXEnemyRuntime::CenterHeight): Rin's feet are at its origin
            "C_spine_03_JNT", "assets/enemies/stw_enemy_01_rin/"};
        return profile;
    }

    STWSkeletalCharacterPresentation::~STWSkeletalCharacterPresentation()
    {
        Shutdown();
    }

    size_t STWSkeletalCharacterPresentation::RequiredJointCount()
    {
        return AZ_ARRAY_SIZE(RequiredJointNames);
    }

    bool STWSkeletalCharacterPresentation::IsRequiredJointName(const char* name)
    {
        if (name == nullptr)
        {
            return false;
        }
        for (const char* requiredName : RequiredJointNames)
        {
            if (std::strcmp(name, requiredName) == 0)
            {
                return true;
            }
        }
        return false;
    }

    STWSkeletalPresentationState STWSkeletalCharacterPresentation::MapGameplayState(
        EnemyBehaviorState behaviorState, bool alive)
    {
        if (!alive || behaviorState == EnemyBehaviorState::Dead)
        {
            return STWSkeletalPresentationState::Death;
        }
        if (behaviorState == EnemyBehaviorState::Chase || behaviorState == EnemyBehaviorState::Attack)
        {
            return STWSkeletalPresentationState::Locomotion;
        }
        return STWSkeletalPresentationState::Idle;
    }

    bool STWSkeletalCharacterPresentation::IsGameplayStateUnchanged(
        const EnemyState& before, const EnemyState& after)
    {
        return before.m_id == after.m_id
            && before.m_archetype == after.m_archetype
            && before.m_position.IsClose(after.m_position)
            && before.m_radius == after.m_radius
            && before.m_maxHealth == after.m_maxHealth
            && before.m_health == after.m_health
            && before.m_alive == after.m_alive
            && before.m_behaviorState == after.m_behaviorState
            && before.m_damageEvents == after.m_damageEvents
            && before.m_deathEvents == after.m_deathEvents
            && before.m_respawnEvents == after.m_respawnEvents
            && before.m_detectionEvents == after.m_detectionEvents
            && before.m_chaseEvents == after.m_chaseEvents
            && before.m_attackEvents == after.m_attackEvents
            && before.m_presentationScale == after.m_presentationScale;
    }

    bool STWSkeletalCharacterPresentation::IsAnimationTimeAdvancing(float previousTime, float currentTime)
    {
        return std::abs(currentTime - previousTime) > AnimationTimeEpsilon;
    }

    bool STWSkeletalCharacterPresentation::IsAssetLifecycleComplete(
        bool actorAssetReady, bool actorInstanceReady, bool skinnedMeshVisible, bool motionAssetReady)
    {
        return actorAssetReady && actorInstanceReady && skinnedMeshVisible && motionAssetReady;
    }

    const char* STWSkeletalCharacterPresentation::GetStateName() const
    {
        switch (m_state)
        {
        case STWSkeletalPresentationState::Idle:
            return "IDLE";
        case STWSkeletalPresentationState::Locomotion:
            return "LOCOMOTION";
        case STWSkeletalPresentationState::Death:
            return "DEATH";
        case STWSkeletalPresentationState::ResetToIdle:
            return "RESET_TO_IDLE";
        default:
            return "UNINITIALIZED";
        }
    }

    bool STWSkeletalCharacterPresentation::ResolveProducts()
    {
        if (m_productsResolved)
        {
            return true;
        }

        m_actorAssetId = FindAssetId<EMotionFX::Integration::ActorAsset>(m_profile->m_actorPath);
        m_idleMotionAssetId = FindAssetId<EMotionFX::Integration::MotionAsset>(m_profile->m_idleMotionPath);
        m_locomotionMotionAssetId = FindAssetId<EMotionFX::Integration::MotionAsset>(m_profile->m_locomotionMotionPath);
        m_deathMotionAssetId = FindAssetId<EMotionFX::Integration::MotionAsset>(m_profile->m_deathMotionPath);
        if (!m_actorAssetId.IsValid() || !m_idleMotionAssetId.IsValid()
            || !m_locomotionMotionAssetId.IsValid() || !m_deathMotionAssetId.IsValid())
        {
            return false;
        }

        m_actorAssetPath = m_profile->m_actorPath;
        m_idleMotionAssetPath = m_profile->m_idleMotionPath;
        m_locomotionMotionAssetPath = m_profile->m_locomotionMotionPath;
        m_deathMotionAssetPath = m_profile->m_deathMotionPath;
        m_productsResolved = true;
        return true;
    }

    bool STWSkeletalCharacterPresentation::TryCreatePresentationEntity()
    {
        if (m_entity)
        {
            return true;
        }
        if (!ResolveProducts())
        {
            return false;
        }

        auto actorConfiguration = EMotionFX::Integration::ActorComponent::Configuration{};
        actorConfiguration.m_actorAsset = AZ::Data::Asset<EMotionFX::Integration::ActorAsset>(
            m_actorAssetId, azrtti_typeid<EMotionFX::Integration::ActorAsset>(), m_actorAssetPath.c_str());

        auto motionConfiguration = EMotionFX::Integration::SimpleMotionComponent::Configuration{};
        motionConfiguration.m_motionAsset = AZ::Data::Asset<EMotionFX::Integration::MotionAsset>(
            m_idleMotionAssetId, azrtti_typeid<EMotionFX::Integration::MotionAsset>(), m_idleMotionAssetPath.c_str());
        motionConfiguration.m_loop = true;
        motionConfiguration.m_retarget = false;
        motionConfiguration.m_playOnActivation = true;
        motionConfiguration.m_inPlace = true;
        motionConfiguration.m_freezeAtLastFrame = true;

        AzFramework::EntityContextId gameContextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            gameContextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        if (gameContextId.IsNull())
        {
            return false;
        }

        AZ::Entity* entity = aznew AZ::Entity("STW_CHARACTER_01 Presentation");
        entity->SetRuntimeActiveByDefault(false);
        entity->CreateComponent<AzFramework::TransformComponent>();
        entity->CreateComponent<EMotionFX::Integration::ActorComponent>(&actorConfiguration);
        if (m_profile->m_materialOverridePrefix != nullptr)
        {
            entity->CreateComponent(AZ::Render::MaterialComponentTypeId);
        }
        entity->CreateComponent<EMotionFX::Integration::SimpleMotionComponent>(&motionConfiguration);
        m_entityId = entity->GetId();

        // GameEntityContext owns the entity after registration. Its add callback initializes
        // the configured components while also registering the EntityIdContextQueryBus mapping
        // required by Atom's entity-to-scene feature-processor lookup.
        AzFramework::GameEntityContextRequestBus::Broadcast(
            &AzFramework::GameEntityContextRequests::AddGameEntity, entity);
        m_entity = entity;
        AzFramework::GameEntityContextRequestBus::Broadcast(
            &AzFramework::GameEntityContextRequests::ActivateGameEntity, m_entityId);
        m_actorAssetReady = true;
        return true;
    }

    void STWSkeletalCharacterPresentation::SelectMotion(const AZ::Data::AssetId& motionAssetId, bool loop)
    {
        if (!m_entity || !motionAssetId.IsValid() || m_currentMotionAssetId == motionAssetId)
        {
            return;
        }
        EMotionFX::Integration::SimpleMotionComponentRequestBus::Event(
            m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::LoopMotion, loop);
        EMotionFX::Integration::SimpleMotionComponentRequestBus::Event(
            m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::Motion, motionAssetId);
        EMotionFX::Integration::SimpleMotionComponentRequestBus::Event(
            m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::PlayMotion);
        m_currentMotionAssetId = motionAssetId;
        m_motionAssetReady = false;
        m_haveAnimationSample = false;
        m_haveBoneSample = false;
        m_boneSampleReseedPending = true;
    }

    void STWSkeletalCharacterPresentation::ApplyMaterialOverrides()
    {
        if (m_materialsApplied || m_profile->m_materialOverridePrefix == nullptr)
        {
            return;
        }
        AZ::Render::MaterialAssignmentMap slots;
        AZ::Render::MaterialComponentRequestBus::EventResult(
            slots, m_entityId, &AZ::Render::MaterialComponentRequests::GetDefaultMaterialMap);
        if (slots.empty())
        {
            return; // the model is not resolved yet; try again on the next update
        }
        size_t matched = 0;
        for (const auto& slot : slots)
        {
            AZStd::string label;
            AZ::Render::MaterialComponentRequestBus::EventResult(
                label, m_entityId, &AZ::Render::MaterialComponentRequests::GetMaterialLabel, slot.first);
            AZStd::to_lower(label.begin(), label.end());
            // Slot labels look like "rin_m_face" (possibly with the FBX prefix and a numeric hash): keep "rin_m_<part>".
            AZStd::string token;
            const size_t start = label.find("rin_m_");
            if (start != AZStd::string::npos)
            {
                token = label.substr(start);
                const size_t dot = token.find('.');
                if (dot != AZStd::string::npos)
                {
                    token = token.substr(0, dot);
                }
                const size_t tail = token.rfind('_');
                if (tail != AZStd::string::npos && tail + 1 < token.size())
                {
                    bool allDigits = true;
                    for (size_t i = tail + 1; i < token.size(); ++i)
                    {
                        allDigits = allDigits && token[i] >= '0' && token[i] <= '9';
                    }
                    if (allDigits)
                    {
                        token = token.substr(0, tail); // trailing numeric hash only
                    }
                }
            }
            bool applied = false;
            if (!token.empty())
            {
                const AZStd::string path = AZStd::string(m_profile->m_materialOverridePrefix) + token + ".azmaterial";
                const AZ::Data::AssetId materialId = FindAssetId<AZ::RPI::MaterialAsset>(path.c_str());
                if (materialId.IsValid())
                {
                    AZ::Render::MaterialComponentRequestBus::Event(
                        m_entityId, &AZ::Render::MaterialComponentRequests::SetMaterialAssetId, slot.first, materialId);
                    applied = true;
                    ++matched;
                }
            }
            AZ_Printf("STWGameplay", "CHARACTER_MATERIAL_SLOT label=%s override=%d\n", label.c_str(), applied ? 1 : 0);
        }
        AZ_Printf("STWGameplay", "CHARACTER_MATERIAL_OVERRIDES profile=%s matched=%zu slots=%zu\n",
            m_profile->m_name, matched, slots.size());
        m_materialsApplied = true;
    }

    void STWSkeletalCharacterPresentation::SampleRuntimeDiagnostics()
    {
        if (!m_entity)
        {
            return;
        }

        AzFramework::EntityContextId owningContextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::EntityIdContextQueryBus::EventResult(
            owningContextId, m_entityId, &AzFramework::EntityIdContextQueries::GetOwningContextId);
        AzFramework::EntityContextId gameContextId = AzFramework::EntityContextId::CreateNull();
        AzFramework::GameEntityContextRequestBus::BroadcastResult(
            gameContextId, &AzFramework::GameEntityContextRequests::GetGameEntityContextId);
        m_entityContextCorrect = !owningContextId.IsNull() && owningContextId == gameContextId;

        AZ::RPI::Scene* atomScene = AZ::RPI::Scene::GetSceneForEntityId(m_entityId);
        m_mainSceneResolved = atomScene != nullptr && atomScene->GetName() == AZ::Name("Main");
        m_skinnedFeatureProcessorAvailable = atomScene != nullptr
            && atomScene->GetFeatureProcessor<AZ::Render::SkinnedMeshFeatureProcessorInterface>() != nullptr;
        m_meshFeatureProcessorAvailable = atomScene != nullptr
            && atomScene->GetFeatureProcessor<AZ::Render::MeshFeatureProcessorInterface>() != nullptr;

        EMotionFX::ActorInstance* actorInstance = nullptr;
        EMotionFX::Integration::ActorComponentRequestBus::EventResult(
            actorInstance, m_entityId, &EMotionFX::Integration::ActorComponentRequests::GetActorInstance);
        m_actorInstanceReady = actorInstance != nullptr;
        if (!m_actorInstanceReady)
        {
            return;
        }

        ApplyMaterialOverrides();
        EMotionFX::Integration::ActorComponentRequestBus::EventResult(
            m_skeletonNodeCount, m_entityId, &EMotionFX::Integration::ActorComponentRequests::GetNumJoints);
        m_probeJointIndex = EMotionFX::Integration::ActorComponentRequests::s_invalidJointIndex;
        EMotionFX::Integration::ActorComponentRequestBus::EventResult(
            m_probeJointIndex, m_entityId, &EMotionFX::Integration::ActorComponentRequests::GetJointIndexByName,
            m_profile->m_probeJointName);
        EMotionFX::Integration::ActorComponentRequestBus::EventResult(
            m_skinnedMeshVisible, m_entityId,
            &EMotionFX::Integration::ActorComponentRequests::GetRenderActorVisible);

        float duration = 0.0f;
        EMotionFX::Integration::SimpleMotionComponentRequestBus::EventResult(
            duration, m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::GetDuration);
        EMotionFX::Integration::SimpleMotionComponentRequestBus::EventResult(
            m_animationTime, m_entityId, &EMotionFX::Integration::SimpleMotionComponentRequests::GetPlayTime);
        m_motionAssetReady = m_currentMotionAssetId.IsValid() && duration > AnimationTimeEpsilon;
        m_animationActive = m_motionAssetReady && m_animationTime >= 0.0f;
        if (m_haveAnimationSample)
        {
            m_animationTimeAdvancing = m_animationTimeAdvancing
                || IsAnimationTimeAdvancing(m_previousAnimationTime, m_animationTime);
        }
        m_previousAnimationTime = m_animationTime;
        m_haveAnimationSample = true;

        if (m_probeJointIndex != EMotionFX::Integration::ActorComponentRequests::s_invalidJointIndex)
        {
            // A motion reseed (SelectMotion) resets m_haveBoneSample this same tick, but EMotionFX
            // has not yet run its Update/Output pass for the newly started motion. Skip the probe
            // for exactly one tick and let the first real sample land once EMotionFX has produced
            // a pose for the new motion.
            if (m_boneSampleReseedPending)
            {
                m_boneSampleReseedPending = false;
                return;
            }

            AZ::Transform currentBoneTransform;
            EMotionFX::Integration::ActorComponentRequestBus::EventResult(
                currentBoneTransform, m_entityId,
                &EMotionFX::Integration::ActorComponentRequests::GetJointTransform,
                m_probeJointIndex, EMotionFX::Integration::Space::ModelSpace);
            const AZ::Quaternion currentBoneRotation = currentBoneTransform.GetRotation().GetNormalized();
            const bool baselineValidBeforeSample = m_haveBoneSample;
            const bool baselineReseeded = !baselineValidBeforeSample;
            AZ::Quaternion previousBoneRotation = AZ::Quaternion::CreateIdentity();
            float rawRotationDot = 0.0f;
            float absoluteRotationDot = 0.0f;
            float clampedRotationDot = 0.0f;
            float angularDeltaRadians = 0.0f;
            bool epsilonPass = false;
            if (baselineValidBeforeSample)
            {
                previousBoneRotation = m_previousBoneRotation.GetNormalized();
                rawRotationDot = previousBoneRotation.Dot(currentBoneRotation);
                absoluteRotationDot = std::abs(rawRotationDot);
                clampedRotationDot = AZ::GetClamp(absoluteRotationDot, 0.0f, 1.0f);
                angularDeltaRadians = 2.0f * AZ::Acos(clampedRotationDot);
                epsilonPass = angularDeltaRadians > BoneAngularDeltaEpsilon;
                m_boneTransformDeltaPositive = m_boneTransformDeltaPositive
                    || epsilonPass;
            }
            m_previousBoneRotation = currentBoneRotation;
            m_haveBoneSample = true;

            const STWSkeletalPresentationState diagnosticState = m_state;
            const bool diagnosticStateChanged = diagnosticState != m_boneRotationDiagnosticLastState;
            if (diagnosticStateChanged)
            {
                m_boneRotationDiagnosticLastState = diagnosticState;
                m_boneRotationDiagnosticStateSamples = 0;
            }
            const bool emitDiagnostic = m_boneRotationDiagnosticTotalSamples < BoneRotationDiagnosticMaxSamples
                && (diagnosticStateChanged
                    || m_boneRotationDiagnosticStateSamples < BoneRotationDiagnosticSamplesPerState);
            if (emitDiagnostic)
            {
                ++m_boneRotationDiagnosticTotalSamples;
                ++m_boneRotationDiagnosticStateSamples;
                if (baselineValidBeforeSample)
                {
                    AZ_Printf(
                        "STWGameplay",
                        "STW_BONE_ROT_DIAG state=%s anim_time=%.6f baseline_valid=%d reseed=%d "
                        "current=(%.9f,%.9f,%.9f,%.9f) previous=(%.9f,%.9f,%.9f,%.9f) "
                        "raw_dot=%.9f abs_dot=%.9f clamped_dot=%.9f angle=%.9f epsilon_pass=%d latched=%d\n",
                        GetStateName(), m_animationTime, baselineValidBeforeSample ? 1 : 0,
                        baselineReseeded ? 1 : 0,
                        currentBoneRotation.GetX(), currentBoneRotation.GetY(), currentBoneRotation.GetZ(),
                        currentBoneRotation.GetW(), previousBoneRotation.GetX(), previousBoneRotation.GetY(),
                        previousBoneRotation.GetZ(), previousBoneRotation.GetW(), rawRotationDot,
                        absoluteRotationDot, clampedRotationDot, angularDeltaRadians, epsilonPass ? 1 : 0,
                        m_boneTransformDeltaPositive ? 1 : 0);
                }
                else
                {
                    AZ_Printf(
                        "STWGameplay",
                        "STW_BONE_ROT_DIAG state=%s anim_time=%.6f baseline_valid=%d reseed=%d "
                        "current=(%.9f,%.9f,%.9f,%.9f) previous=UNAVAILABLE raw_dot=UNAVAILABLE "
                        "abs_dot=UNAVAILABLE clamped_dot=UNAVAILABLE angle=UNAVAILABLE "
                        "epsilon_pass=UNAVAILABLE latched=%d\n",
                        GetStateName(), m_animationTime, baselineValidBeforeSample ? 1 : 0,
                        baselineReseeded ? 1 : 0,
                        currentBoneRotation.GetX(), currentBoneRotation.GetY(), currentBoneRotation.GetZ(),
                        currentBoneRotation.GetW(), m_boneTransformDeltaPositive ? 1 : 0);
                }
            }
        }
    }

    void STWSkeletalCharacterPresentation::Update(float deltaTime, const EnemyState& gameplayState)
    {
        PresentationFrameState presentationState;
        presentationState.m_position = gameplayState.m_position;
        Update(deltaTime, gameplayState, presentationState);
    }

    void STWSkeletalCharacterPresentation::Update(
        float deltaTime, const EnemyState& gameplayState, const PresentationFrameState& presentationState)
    {
        if (!TryCreatePresentationEntity())
        {
            return;
        }

        const STWSkeletalPresentationState mappedState =
            MapGameplayState(gameplayState.m_behaviorState, gameplayState.m_alive);
        if (mappedState != m_lastMappedState)
        {
            if (mappedState == STWSkeletalPresentationState::Death)
            {
                SelectMotion(m_deathMotionAssetId, false);
            }
            else if (mappedState == STWSkeletalPresentationState::Locomotion)
            {
                SelectMotion(m_locomotionMotionAssetId, true);
            }
            else
            {
                SelectMotion(m_idleMotionAssetId, true);
            }

            if (m_lastMappedState == STWSkeletalPresentationState::Death
                && mappedState != STWSkeletalPresentationState::Death)
            {
                m_state = STWSkeletalPresentationState::ResetToIdle;
                m_resetToIdleObserved = true;
            }
            else
            {
                m_state = mappedState;
            }
            m_lastMappedState = mappedState;
        }
        else if (m_state == STWSkeletalPresentationState::ResetToIdle)
        {
            m_state = STWSkeletalPresentationState::Idle;
        }

        m_idleObserved = m_idleObserved || m_state == STWSkeletalPresentationState::Idle;
        m_locomotionObserved = m_locomotionObserved || m_state == STWSkeletalPresentationState::Locomotion;
        m_deathObserved = m_deathObserved || m_state == STWSkeletalPresentationState::Death;

        // This is a presentation transform. PhysX remains the source of gameplay position.
        const AZ::Transform presentationTransform = AZ::Transform::CreateFromQuaternionAndTranslation(
            AZ::Quaternion::CreateRotationZ(presentationState.m_yaw),
            presentationState.m_position + AZ::Vector3(0.0f, 0.0f, m_profile->m_originOffsetZ));
        AZ::TransformBus::Event(m_entityId, &AZ::TransformBus::Events::SetWorldTM, presentationTransform);
        SampleRuntimeDiagnostics();

        (void)deltaTime;
    }

    void STWSkeletalCharacterPresentation::ResetToIdle()
    {
        if (m_entity && m_idleMotionAssetId.IsValid())
        {
            SelectMotion(m_idleMotionAssetId, true);
        }
        m_state = STWSkeletalPresentationState::ResetToIdle;
        m_lastMappedState = STWSkeletalPresentationState::Idle;
        m_resetToIdleObserved = true;
    }

    void STWSkeletalCharacterPresentation::Shutdown()
    {
        if (m_entity)
        {
            AzFramework::GameEntityContextRequestBus::Broadcast(
                &AzFramework::GameEntityContextRequests::DestroyGameEntity, m_entityId);
        }
        m_entity = nullptr;
        m_entityId = AZ::EntityId();
        m_currentMotionAssetId = AZ::Data::AssetId();
        m_actorInstanceReady = false;
        m_skinnedMeshVisible = false;
        m_motionAssetReady = false;
        m_animationActive = false;
        m_animationTimeAdvancing = false;
        m_boneTransformDeltaPositive = false;
        m_haveAnimationSample = false;
        m_haveBoneSample = false;
        m_state = STWSkeletalPresentationState::Uninitialized;
        m_lastMappedState = STWSkeletalPresentationState::Uninitialized;
    }
}
