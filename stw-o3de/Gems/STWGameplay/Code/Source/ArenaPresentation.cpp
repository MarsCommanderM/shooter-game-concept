#include <STWGameplay/ArenaPresentation.h>

#include <AzCore/Asset/AssetManagerBus.h>
#include <AzCore/Component/ComponentApplicationBus.h>
#include <AzCore/Component/Entity.h>
#include <AzCore/Math/Color.h>
#include <AzCore/Math/Frustum.h>
#include <AzCore/Math/Matrix3x3.h>
#include <AzCore/Math/ShapeIntersection.h>
#include <AzFramework/Components/CameraBus.h>
#include <Atom/Feature/CoreLights/PhotometricValue.h>
#include <AtomLyIntegration/CommonFeatures/Grid/GridComponentBus.h>
#include <AtomLyIntegration/CommonFeatures/Mesh/MeshComponentBus.h>
#include <Atom/RPI.Public/Material/Material.h>
#include <Atom/RPI.Public/MeshDrawPacket.h>
#include <Atom/RPI.Public/Model/Model.h>
#include <Atom/RPI.Public/Model/ModelLod.h>
#include <Atom/RPI.Public/Scene.h>
#include <Atom/RPI.Reflect/Material/MaterialAsset.h>
#include <Atom/RPI.Reflect/Model/ModelAsset.h>

#include <cctype>

namespace STWGameplay
{
    namespace
    {
        struct VisualAssetSpec
        {
            const char* m_id;
            const char* m_modelPath;
            const char* m_materialPath;
        };

        // One mesh per material family: the mesh feature processor binds a single
        // material per handle through the default custom-material slot, so material
        // separation is expressed by splitting the geometry, not by model slots.
        // Paths must stay lowercase - DiscoverAssets lowercase-compares them against
        // the asset catalog's relative paths.
        constexpr VisualAssetSpec VisualAssets[ArenaPresentation::VisualAssetCount] =
        {
            {
                "arena_deck",
                "assets/environment/stw_arena_01/stw_arena_deck_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_01.azmaterial"
            },
            {
                "arena_wall",
                "assets/environment/stw_arena_01/stw_arena_wall_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_wallpanel_01.azmaterial"
            },
            {
                "arena_struct",
                "assets/environment/stw_arena_01/stw_arena_struct_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_struct_01.azmaterial"
            },
            {
                "arena_cover",
                "assets/environment/stw_arena_01/stw_arena_covermod_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_cover_01.azmaterial"
            },
            {
                "arena_arch",
                "assets/environment/stw_arena_01/stw_arena_arch_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_struct_01.azmaterial"
            },
            {
                "arena_landmark",
                "assets/environment/stw_arena_01/stw_arena_beacon_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_landmark_01.azmaterial"
            },
            {
                "arena_trim",
                "assets/environment/stw_arena_01/stw_arena_trimkit_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_trim_01.azmaterial"
            },
            {
                "arena_props",
                "assets/environment/stw_arena_01/stw_arena_prop_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_prop_01.azmaterial"
            },
            {
                "arena_markings",
                "assets/environment/stw_arena_01/stw_arena_mark_01.obj.azmodel",
                "assets/environment/stw_arena_01/stw_arena_mark_01.azmaterial"
            }
        };

        AZStd::string LowercaseAssetPath(const AZStd::string& path)
        {
            AZStd::string lowercase = path;
            for (char& character : lowercase)
            {
                character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            return lowercase;
        }

        AZStd::string MaterialPropertySummary(const AZ::Data::Instance<AZ::RPI::Material>& material)
        {
            if (!material)
            {
                return "material_property=UNKNOWN";
            }

            const AZ::RPI::MaterialPropertyIndex baseColorIndex =
                material->FindPropertyIndex(AZ::Name("baseColor.color"));
            if (!baseColorIndex.IsValid())
            {
                return "baseColor=UNKNOWN";
            }

            const AZ::RPI::MaterialPropertyValue& baseColor = material->GetPropertyValue(baseColorIndex);
            if (!baseColor.Is<AZ::Color>())
            {
                return AZStd::string::format(
                    "baseColor_type=%s",
                    baseColor.GetTypeId().ToString<AZStd::string>().c_str());
            }

            const AZ::Color& color = baseColor.GetValue<AZ::Color>();
            return AZStd::string::format(
                "baseColor=(%.3f,%.3f,%.3f,%.3f)", color.GetR(), color.GetG(), color.GetB(), color.GetA());
        }

        AZStd::string ModelSlotSummary(const AZ::Data::Instance<AZ::RPI::Model>& model)
        {
            if (!model || !model->GetModelAsset())
            {
                return "model_slots=UNKNOWN";
            }

            AZStd::string slots;
            size_t count = 0;
            for (const auto& [stableId, slot] : model->GetModelAsset()->GetMaterialSlots())
            {
                if (count++ != 0)
                {
                    slots += ",";
                }
                slots += AZStd::string::format("%u:%s", stableId, slot.m_displayName.GetCStr());
            }
            return AZStd::string::format("model_slot_count=%zu model_slots=%s", count, slots.c_str());
        }

        AZStd::string CustomMaterialKeySummary(
            const AZ::Render::CustomMaterialMap& customMaterials, const AZ::Data::Instance<AZ::RPI::Model>& model,
            bool& exactSlotKeyMatch, bool& fallbackKeyPresent)
        {
            exactSlotKeyMatch = false;
            fallbackKeyPresent = customMaterials.find(AZ::Render::DefaultCustomMaterialId) != customMaterials.end();

            AZStd::string keys;
            size_t count = 0;
            for (const auto& [key, info] : customMaterials)
            {
                AZ_UNUSED(info);
                if (count++ != 0)
                {
                    keys += ",";
                }
                keys += AZStd::string::format("(%llu,%u)", static_cast<unsigned long long>(key.first), key.second);
                if (model && model->GetModelAsset()
                    && model->GetModelAsset()->GetMaterialSlots().find(key.second)
                        != model->GetModelAsset()->GetMaterialSlots().end())
                {
                    exactSlotKeyMatch = true;
                }
            }
            return AZStd::string::format("custom_key_count=%zu custom_keys=%s", count, keys.c_str());
        }

        AZStd::string AabbSummary(const AZ::Aabb& aabb)
        {
            if (!aabb.IsValid())
            {
                return "invalid";
            }
            const AZ::Vector3 min = aabb.GetMin();
            const AZ::Vector3 max = aabb.GetMax();
            return AZStd::string::format(
                "min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)",
                min.GetX(), min.GetY(), min.GetZ(), max.GetX(), max.GetY(), max.GetZ());
        }

        AZStd::string ModelGeometrySummary(const AZ::Data::Instance<AZ::RPI::Model>& model)
        {
            if (!model || !model->GetModelAsset())
            {
                return "lods=UNKNOWN vertices=UNKNOWN indices=UNKNOWN";
            }

            size_t lodCount = 0;
            size_t vertexCount = 0;
            size_t indexCount = 0;
            for (const AZ::Data::Asset<AZ::RPI::ModelLodAsset>& lodAsset : model->GetModelAsset()->GetLodAssets())
            {
                if (!lodAsset)
                {
                    continue;
                }
                ++lodCount;
                for (const AZ::RPI::ModelLodAsset::Mesh& mesh : lodAsset->GetMeshes())
                {
                    vertexCount += mesh.GetVertexCount();
                    indexCount += mesh.GetIndexCount();
                }
            }
            return AZStd::string::format("lods=%zu vertices=%zu indices=%zu", lodCount, vertexCount, indexCount);
        }
    }

    ArenaPresentation::~ArenaPresentation()
    {
        Shutdown();
    }

    void ArenaPresentation::Initialize(const AZ::Uuid& contextId)
    {
        if (m_initialized || contextId.IsNull())
        {
            return;
        }

        m_meshFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<AZ::Render::MeshFeatureProcessorInterface>(contextId);
        m_directionalLightFeatureProcessor =
            AZ::RPI::Scene::GetFeatureProcessorForEntityContextId<
                AZ::Render::DirectionalLightFeatureProcessorInterface>(contextId);
        m_initialized = m_meshFeatureProcessor != nullptr;
        if (m_initialized)
        {
            InitializeEnvironmentLight();
        }
    }

    void ArenaPresentation::Update()
    {
        if (!m_initialized || m_meshFeatureProcessor == nullptr)
        {
            return;
        }

        IsolateDefaultLevelScaffold();

        if (!m_assetsDiscovered
            && (m_discoveryAttempts == 0 || ++m_updatesSinceDiscoveryAttempt >= DiscoveryRetryUpdates))
        {
            m_updatesSinceDiscoveryAttempt = 0;
            DiscoverAssets();
        }

        for (size_t index = 0; index < VisualAssetCount; ++index)
        {
            UpdateAsset(m_assets[index], VisualAssets[index].m_modelPath, VisualAssets[index].m_materialPath);
            VisualAssetState& asset = m_assets[index];
            if (asset.m_meshHandle.IsValid() && !asset.m_materialAppliedToModel
                && m_meshFeatureProcessor->GetModel(asset.m_meshHandle) != nullptr)
            {
                // Re-apply once the live model exists so the material is present in the
                // draw-packet rebuild, not only in the pre-load mesh descriptor.
                m_meshFeatureProcessor->SetCustomMaterials(asset.m_meshHandle, asset.m_material);
                asset.m_materialAppliedToModel = true;
                ++asset.m_materialRebindCount;
            }

            const AZ::Data::Instance<AZ::RPI::Model> model =
                asset.m_meshHandle.IsValid() ? m_meshFeatureProcessor->GetModel(asset.m_meshHandle) : nullptr;
            if (model != nullptr && asset.m_runtimeDiagnosticAttempts < 20
                && (!asset.m_runtimeDiagnosticReported || !asset.m_visibilityDiagnosticReported))
            {
                ++asset.m_runtimeDiagnosticAttempts;
                const AZ::RPI::MeshDrawPacketLods& packets = m_meshFeatureProcessor->GetDrawPackets(asset.m_meshHandle);
                size_t packetCount = 0;
                for (const auto& lodPackets : packets)
                {
                    packetCount += lodPackets.size();
                }
                const bool packetObservationReady = packetCount > 0 || asset.m_runtimeDiagnosticAttempts == 20;
                if (packetObservationReady)
                {
                    if (!asset.m_runtimeDiagnosticReported)
                    {
                        ReportRuntimeMaterialIdentity(index);
                        asset.m_runtimeDiagnosticReported = true;
                    }
                    if (!asset.m_visibilityDiagnosticReported)
                    {
                        // Block 25B: runtime visibility diagnostic disabled at the call
                        // site. ReportRuntimeGeometryState() is observational logging
                        // only and aborted after STW_ARENA_VISIBILITY_STEP=CAMERA_SPACE_MATH
                        // during the native R7E attempt (crash cause unproven). Arena
                        // rendering / material binding / lighting are unaffected — they
                        // are established in UpdateAsset() and InitializeEnvironmentLight().
                        // ReportRuntimeGeometryState() and CalculateCameraSpaceRelation()
                        // and their unit tests are retained, only the call is removed.
                        asset.m_visibilityDiagnosticReported = true;
                    }
                }
            }
        }
    }

    void ArenaPresentation::IsolateDefaultLevelScaffold()
    {
        if (m_defaultLevelGroundHidden && m_defaultLevelGridHidden)
        {
            return;
        }

        AZ::ComponentApplicationBus::Broadcast(
            [&](AZ::ComponentApplicationRequests* application)
            {
                if (application == nullptr)
                {
                    return;
                }
                application->EnumerateEntities(
                    [this](AZ::Entity* entity)
                    {
                        if (entity == nullptr)
                        {
                            return;
                        }

                        if (!m_defaultLevelGroundHidden && entity->GetName() == "Ground"
                            && AZ::Render::MeshComponentRequestBus::FindFirstHandler(entity->GetId()) != nullptr)
                        {
                            AZ::Render::MeshComponentRequestBus::Event(
                                entity->GetId(), &AZ::Render::MeshComponentRequestBus::Events::SetVisibility, false);
                            m_defaultLevelGroundHidden = true;
                        }

                        if (!m_defaultLevelGridHidden && entity->GetName() == "Grid"
                            && AZ::Render::GridComponentRequestBus::FindFirstHandler(entity->GetId()) != nullptr)
                        {
                            // GridComponentController treats a zero grid size as a disabled draw.
                            AZ::Render::GridComponentRequestBus::Event(
                                entity->GetId(), &AZ::Render::GridComponentRequestBus::Events::SetSize, 0.0f);
                            m_defaultLevelGridHidden = true;
                        }
                    });
            });

        if (!m_defaultLevelIsolationReported && (m_defaultLevelGroundHidden || m_defaultLevelGridHidden))
        {
            AZ_Printf(
                "STWGameplay",
                "STW_ARENA_SCENE_ISOLATION ground_hidden=%d grid_hidden=%d source=DefaultLevel\n",
                m_defaultLevelGroundHidden ? 1 : 0, m_defaultLevelGridHidden ? 1 : 0);
            m_defaultLevelIsolationReported = true;
        }
    }

    void ArenaPresentation::ReportRuntimeGeometryState(size_t index)
    {
        if (m_meshFeatureProcessor == nullptr || index >= VisualAssetCount)
        {
            return;
        }

        const char* visualId = VisualAssets[index].m_id;
        const VisualAssetState& state = m_assets[index];
        const bool meshHandleValid = state.m_meshHandle.IsValid();
        AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=BEGIN handle_valid=%d\n", visualId, meshHandleValid);

        AZ::Data::Instance<AZ::RPI::Model> model;
        if (meshHandleValid)
        {
            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=MODEL_QUERY\n", visualId);
            model = m_meshFeatureProcessor->GetModel(state.m_meshHandle);
        }

        AZ::Transform transform = AZ::Transform::CreateIdentity();
        AZ::Vector3 scale = AZ::Vector3::CreateOne();
        AZ::Aabb localBounds = AZ::Aabb::CreateNull();
        bool explicitlyVisible = false;
        if (meshHandleValid)
        {
            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=TRANSFORM_QUERY\n", visualId);
            transform = m_meshFeatureProcessor->GetTransform(state.m_meshHandle);
            scale = m_meshFeatureProcessor->GetNonUniformScale(state.m_meshHandle);

            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=LOCAL_AABB_QUERY\n", visualId);
            localBounds = m_meshFeatureProcessor->GetLocalAabb(state.m_meshHandle);

            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=EXPLICIT_VISIBILITY_QUERY\n", visualId);
            explicitlyVisible = m_meshFeatureProcessor->GetVisible(state.m_meshHandle);
        }

        const bool transformFinite = transform.IsFinite();
        const bool scaleFinite = scale.IsFinite();
        const bool localBoundsValid = localBounds.IsValid() && localBounds.IsFinite();
        AZ::Aabb worldBounds = AZ::Aabb::CreateNull();
        if (transformFinite && localBoundsValid)
        {
            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=WORLD_AABB_TRANSFORM\n", visualId);
            worldBounds = localBounds.GetTransformedAabb(transform);
        }

        size_t packetCount = 0;
        if (meshHandleValid)
        {
            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=DRAW_PACKET_QUERY\n", visualId);
            const AZ::RPI::MeshDrawPacketLods& drawPackets = m_meshFeatureProcessor->GetDrawPackets(state.m_meshHandle);
            for (const auto& lodPackets : drawPackets)
            {
                packetCount += lodPackets.size();
            }
        }

        bool frustumKnown = false;
        bool frustumIntersects = false;
        bool cameraRelationKnown = false;
        CameraSpaceRelation cameraRelation;
        Camera::Configuration cameraConfiguration;
        AZ::Transform cameraTransform = AZ::Transform::CreateIdentity();
        AZ::Vector3 cameraRight = AZ::Vector3::CreateAxisX();
        AZ::Vector3 cameraForward = AZ::Vector3::CreateAxisY();
        AZ::Vector3 cameraUp = AZ::Vector3::CreateAxisZ();
        bool cameraConfigurationValid = false;
        bool cameraBasisValid = false;
        if (Camera::ActiveCameraRequestBus::HasHandlers())
        {
            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=CAMERA_TRANSFORM_QUERY\n", visualId);
            Camera::ActiveCameraRequestBus::BroadcastResult(
                cameraTransform, &Camera::ActiveCameraRequests::GetActiveCameraTransform);
            cameraRight = cameraTransform.GetBasisX();
            cameraForward = cameraTransform.GetBasisY();
            cameraUp = cameraTransform.GetBasisZ();

            AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=CAMERA_CONFIG_QUERY\n", visualId);
            Camera::ActiveCameraRequestBus::BroadcastResult(
                cameraConfiguration, &Camera::ActiveCameraRequests::GetActiveCameraConfiguration);

            cameraConfigurationValid = cameraConfiguration.m_frustumWidth > 0.0f
                && cameraConfiguration.m_frustumHeight > 0.0f && cameraConfiguration.m_fovRadians > 0.0f
                && cameraConfiguration.m_nearClipDistance > 0.0f
                && cameraConfiguration.m_farClipDistance > cameraConfiguration.m_nearClipDistance;
            cameraBasisValid = cameraTransform.IsFinite() && cameraRight.IsFinite() && cameraForward.IsFinite()
                && cameraUp.IsFinite() && cameraRight.GetLengthSq() > AZ::Constants::Tolerance
                && cameraForward.GetLengthSq() > AZ::Constants::Tolerance
                && cameraUp.GetLengthSq() > AZ::Constants::Tolerance;

            if (cameraConfigurationValid && cameraBasisValid && worldBounds.IsValid() && worldBounds.IsFinite())
            {
                AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=FRUSTUM_QUERY\n", visualId);
                const AZ::ViewFrustumAttributes attributes(
                    cameraTransform,
                    cameraConfiguration.m_frustumWidth / cameraConfiguration.m_frustumHeight,
                    cameraConfiguration.m_fovRadians,
                    cameraConfiguration.m_nearClipDistance,
                    cameraConfiguration.m_farClipDistance);
                frustumIntersects = AZ::ShapeIntersection::Overlaps(AZ::Frustum(attributes), worldBounds);
                frustumKnown = true;
            }
        }

        const AZ::Vector3 cameraSpaceCenter = cameraRelationKnown
            ? cameraRelation.m_cameraSpaceCenter
            : AZ::Vector3::CreateZero();

        AZ_Printf(
            "STWGameplay",
            "STW_ARENA_VISIBILITY_DIAG id=%s model_ready=%d mesh_handle_valid=%d transform_translation=(%.3f,%.3f,%.3f) "
            "scale=(%.3f,%.3f,%.3f) local_bounds=%s world_bounds=%s finite=%d explicitly_visible=%d "
            "draw_packets=%zu draw_packet_observation_ready=%d frustum_intersects=%s camera_position=(%.3f,%.3f,%.3f) "
            "camera_space_center=(%.3f,%.3f,%.3f) camera_inside_bounds=%s lateral_offset=%.3f forward_distance=%.3f "
            "vertical_offset=%.3f camera_relation=%s near_clip=%.3f far_clip=%.3f camera_config_valid=%d "
            "culling_state=UNAVAILABLE geometry=%s\n",
            VisualAssets[index].m_id,
            model != nullptr,
            meshHandleValid,
            transform.GetTranslation().GetX(), transform.GetTranslation().GetY(), transform.GetTranslation().GetZ(),
            scale.GetX(), scale.GetY(), scale.GetZ(),
            AabbSummary(localBounds).c_str(), AabbSummary(worldBounds).c_str(),
            transformFinite && scaleFinite && localBoundsValid && worldBounds.IsValid() && worldBounds.IsFinite(),
            explicitlyVisible,
            packetCount,
            packetCount > 0,
            frustumKnown ? (frustumIntersects ? "YES" : "NO") : "UNAVAILABLE",
            cameraTransform.GetTranslation().GetX(), cameraTransform.GetTranslation().GetY(),
            cameraTransform.GetTranslation().GetZ(),
            cameraSpaceCenter.GetX(), cameraSpaceCenter.GetY(), cameraSpaceCenter.GetZ(),
            cameraRelationKnown ? (cameraRelation.m_cameraInsideBounds ? "YES" : "NO") : "UNAVAILABLE",
            cameraSpaceCenter.GetX(), cameraSpaceCenter.GetY(), cameraSpaceCenter.GetZ(),
            cameraRelationKnown ? cameraSpaceCenter.GetZ() : 0.0f,
            cameraRelationKnown ? "YES" : "UNAVAILABLE",
            cameraConfigurationValid ? cameraConfiguration.m_nearClipDistance : 0.0f,
            cameraConfigurationValid ? cameraConfiguration.m_farClipDistance : 0.0f,
            cameraConfigurationValid && cameraBasisValid,
            ModelGeometrySummary(model).c_str());

        AZ_Printf("STWGameplay", "STW_ARENA_VISIBILITY_STEP id=%s step=END\n", visualId);
    }

    ArenaPresentation::CameraSpaceRelation ArenaPresentation::CalculateCameraSpaceRelation(
        const AZ::Aabb& worldBounds,
        const AZ::Vector3& cameraPosition,
        const AZ::Vector3& cameraRight,
        const AZ::Vector3& cameraForward,
        const AZ::Vector3& cameraUp)
    {
        CameraSpaceRelation relation;
        if (!worldBounds.IsValid() || !worldBounds.IsFinite() || !cameraPosition.IsFinite()
            || !cameraRight.IsFinite() || !cameraForward.IsFinite() || !cameraUp.IsFinite()
            || cameraRight.GetLengthSq() <= AZ::Constants::Tolerance
            || cameraForward.GetLengthSq() <= AZ::Constants::Tolerance
            || cameraUp.GetLengthSq() <= AZ::Constants::Tolerance)
        {
            return relation;
        }

        const AZ::Vector3 normalizedRight = cameraRight.GetNormalizedSafe();
        const AZ::Vector3 normalizedForward = cameraForward.GetNormalizedSafe();
        const AZ::Vector3 normalizedUp = cameraUp.GetNormalizedSafe();
        const AZ::Vector3 centerDelta = worldBounds.GetCenter() - cameraPosition;
        relation.m_valid = true;
        relation.m_cameraInsideBounds = worldBounds.Contains(cameraPosition);
        relation.m_cameraSpaceCenter = AZ::Vector3(
            centerDelta.Dot(normalizedRight), centerDelta.Dot(normalizedForward), centerDelta.Dot(normalizedUp));
        return relation;
    }

    void ArenaPresentation::ReportRuntimeMaterialIdentity(size_t index)
    {
        if (m_meshFeatureProcessor == nullptr || index >= VisualAssetCount)
        {
            return;
        }

        VisualAssetState& state = m_assets[index];
        const AZ::Data::Instance<AZ::RPI::Model> model = m_meshFeatureProcessor->GetModel(state.m_meshHandle);
        const AZ::RPI::MeshDrawPacketLods& drawPackets = m_meshFeatureProcessor->GetDrawPackets(state.m_meshHandle);
        size_t packetCount = 0;
        size_t packetMaterialMatches = 0;
        AZStd::string packetMaterials;
        AZStd::string packetSlots;
        for (size_t lod = 0; lod < drawPackets.size(); ++lod)
        {
            for (size_t mesh = 0; mesh < drawPackets[lod].size(); ++mesh)
            {
                const AZ::RPI::MeshDrawPacket& packet = drawPackets[lod][mesh];
                const AZ::Data::Instance<AZ::RPI::Material> packetMaterial = packet.GetMaterial();
                if (packetCount++ != 0)
                {
                    packetMaterials += ",";
                    packetSlots += ",";
                }
                packetSlots += AZStd::string::format("%u", packet.GetMesh().m_materialSlotStableId);
                if (packetMaterial)
                {
                    packetMaterials += packetMaterial->GetAsset().GetHint();
                    if (packetMaterial == state.m_material)
                    {
                        ++packetMaterialMatches;
                    }
                }
                else
                {
                    packetMaterials += "INVALID";
                }
            }
        }

        bool exactSlotKeyMatch = false;
        bool fallbackKeyPresent = false;
        const AZStd::string customKeys = CustomMaterialKeySummary(
            m_meshFeatureProcessor->GetCustomMaterials(state.m_meshHandle), model, exactSlotKeyMatch, fallbackKeyPresent);
        const bool packetMaterialMatchesExpected = packetCount > 0 && packetMaterialMatches == packetCount;
        AZ_Printf(
            "STWGameplay",
            "STW_ARENA_MATERIAL_IDENTITY_DIAG id=%s model_path=%s model_id=%s model_ready=%d mesh_handle_valid=%d "
            "material_path=%s material_id=%s material_instance_valid=%d %s %s packet_count=%zu packet_slots=%s "
            "packet_materials=%s packet_material_matches=%d custom_slot_key_match=%d custom_fallback_key_present=%d "
            "runtime_material_properties=%s "
            "custom_material_applied=%d rebind_count=%u\n",
            VisualAssets[index].m_id, state.m_modelPath.c_str(), state.m_modelAssetId.ToString<AZStd::string>().c_str(),
            model != nullptr, state.m_meshHandle.IsValid(), state.m_materialAsset.GetHint().c_str(),
            state.m_materialAssetId.ToString<AZStd::string>().c_str(), state.m_material != nullptr,
            ModelSlotSummary(model).c_str(), customKeys.c_str(), packetCount, packetSlots.c_str(), packetMaterials.c_str(),
            packetMaterialMatchesExpected, exactSlotKeyMatch, fallbackKeyPresent, MaterialPropertySummary(state.m_material).c_str(),
            packetMaterialMatchesExpected,
            state.m_materialRebindCount);
    }

    void ArenaPresentation::Shutdown()
    {
        if (m_meshFeatureProcessor != nullptr)
        {
            for (VisualAssetState& asset : m_assets)
            {
                if (asset.m_meshHandle.IsValid())
                {
                    m_meshFeatureProcessor->ReleaseMesh(asset.m_meshHandle);
                }
            }
        }
        if (m_directionalLightFeatureProcessor != nullptr && m_directionalLightHandle.IsValid())
        {
            m_directionalLightFeatureProcessor->ReleaseLight(m_directionalLightHandle);
        }

        m_assets = {};
        m_meshFeatureProcessor = nullptr;
        m_directionalLightFeatureProcessor = nullptr;
        m_directionalLightHandle = {};
        m_assetsDiscovered = false;
        m_discoveryAttempts = 0;
        m_updatesSinceDiscoveryAttempt = 0;
        m_reportedUnresolvedMask = 0xFFFFFFFFu;
        m_environmentLightInitialized = false;
        m_initialized = false;
        m_defaultLevelGroundHidden = false;
        m_defaultLevelGridHidden = false;
        m_defaultLevelIsolationReported = false;
    }

    bool ArenaPresentation::IsGeometryReady() const
    {
        if (m_meshFeatureProcessor == nullptr)
        {
            return false;
        }
        for (const VisualAssetState& asset : m_assets)
        {
            if (!asset.m_meshHandle.IsValid() || m_meshFeatureProcessor->GetModel(asset.m_meshHandle) == nullptr)
            {
                return false;
            }
        }
        return true;
    }

    bool ArenaPresentation::IsMaterialSetReady() const
    {
        for (const VisualAssetState& asset : m_assets)
        {
            if (!asset.m_materialAsset.IsReady() || asset.m_material == nullptr)
            {
                return false;
            }
        }
        return true;
    }

    bool ArenaPresentation::IsEnvironmentPresentationReady() const
    {
        return m_environmentLightInitialized && m_directionalLightHandle.IsValid();
    }

    bool ArenaPresentation::IsReady() const
    {
        return IsGeometryReady() && IsMaterialSetReady() && IsEnvironmentPresentationReady();
    }

    uint32_t ArenaPresentation::ComputeUnresolvedMask(
        const AZStd::array<bool, VisualAssetCount>& modelFound,
        const AZStd::array<bool, VisualAssetCount>& materialFound,
        const AZStd::array<bool, VisualAssetCount>& modelIdValid,
        const AZStd::array<bool, VisualAssetCount>& materialIdValid,
        const AZStd::array<bool, VisualAssetCount>& alreadyDiscovered)
    {
        uint32_t unresolved = 0;
        for (size_t index = 0; index < VisualAssetCount; ++index)
        {
            // Exactly the criterion UpdateAsset() needs to acquire a mesh: the
            // catalog produced BOTH identities and BOTH of them are valid.
            // A path match with an invalid AssetId is not a resolved entry.
            const bool modelResolved = modelFound[index] && modelIdValid[index];
            const bool materialResolved = materialFound[index] && materialIdValid[index];
            if (alreadyDiscovered[index] || (modelResolved && materialResolved))
            {
                continue;
            }
            unresolved |= (1u << index);
        }
        return unresolved;
    }

    void ArenaPresentation::DiscoverAssets()
    {
        // Discovery stays open until every visual asset resolves. Marking it
        // complete on the first pass - as this did before Block 26E-R1 - made an
        // early catalog miss permanent and silent: the affected mesh was never
        // acquired, IsGeometryReady() never returned true, and nothing said why.
        ++m_discoveryAttempts;

        AZStd::array<bool, VisualAssetCount> modelFound{};
        AZStd::array<bool, VisualAssetCount> materialFound{};
        AZStd::array<bool, VisualAssetCount> alreadyDiscovered{};
        for (size_t index = 0; index < VisualAssetCount; ++index)
        {
            alreadyDiscovered[index] = m_assets[index].m_discovered;
        }

        AZ::Data::AssetCatalogRequestBus::Broadcast(
            &AZ::Data::AssetCatalogRequests::EnumerateAssets,
            []() {},
            [this, &modelFound, &materialFound](const AZ::Data::AssetId assetId, const AZ::Data::AssetInfo& info)
            {
                const AZStd::string lowercasePath = LowercaseAssetPath(info.m_relativePath);
                for (size_t index = 0; index < VisualAssetCount; ++index)
                {
                    // Leave an already-resolved entry untouched so a retry never
                    // reassigns ids the mesh handle was acquired from.
                    if (m_assets[index].m_discovered)
                    {
                        continue;
                    }
                    if (lowercasePath == VisualAssets[index].m_modelPath)
                    {
                        modelFound[index] = true;
                        m_assets[index].m_modelAssetId = assetId;
                        m_assets[index].m_modelPath = info.m_relativePath;
                    }
                    if (lowercasePath == VisualAssets[index].m_materialPath)
                    {
                        materialFound[index] = true;
                        m_assets[index].m_materialAssetId = assetId;
                    }
                }
            },
            []() {});

        // Identity validity is captured alongside the path hits so the
        // termination signal below is computed from the same facts the
        // per-entry acquisition uses, not from the weaker path-found flags.
        AZStd::array<bool, VisualAssetCount> modelIdValid{};
        AZStd::array<bool, VisualAssetCount> materialIdValid{};
        for (size_t index = 0; index < VisualAssetCount; ++index)
        {
            modelIdValid[index] = m_assets[index].m_modelAssetId.IsValid();
            materialIdValid[index] = m_assets[index].m_materialAssetId.IsValid();
        }

        for (size_t index = 0; index < VisualAssetCount; ++index)
        {
            if (m_assets[index].m_discovered)
            {
                continue;
            }
            m_assets[index].m_discovered = modelFound[index] && materialFound[index]
                && modelIdValid[index] && materialIdValid[index];
            if (m_assets[index].m_discovered)
            {
                m_assets[index].m_materialAsset = AZ::Data::Asset<AZ::RPI::MaterialAsset>(
                    m_assets[index].m_materialAssetId,
                    azrtti_typeid<AZ::RPI::MaterialAsset>(),
                    VisualAssets[index].m_materialPath);
                m_assets[index].m_materialAsset.QueueLoad();
            }
        }

        const uint32_t unresolved = ComputeUnresolvedMask(
            modelFound, materialFound, modelIdValid, materialIdValid, alreadyDiscovered);
        // Discovery only stops when every entry is usable, so an entry whose
        // path matched but whose identity is invalid keeps m_assetsDiscovered
        // false and Update() keeps re-running DiscoverAssets() on its cadence.
        m_assetsDiscovered = unresolved == 0;
        ReportDiscoveryState(unresolved, modelFound, materialFound);
    }

    void ArenaPresentation::ReportDiscoveryState(
        uint32_t unresolvedMask,
        const AZStd::array<bool, VisualAssetCount>& modelFound,
        const AZStd::array<bool, VisualAssetCount>& materialFound)
    {
        // One line per state transition, not per Update(). The mask only ever
        // loses bits, so this is bounded by VisualAssetCount + 1 reports.
        if (unresolvedMask == m_reportedUnresolvedMask)
        {
            return;
        }
        m_reportedUnresolvedMask = unresolvedMask;

        if (unresolvedMask == 0)
        {
            AZ_Printf(
                "STWGameplay",
                "STW_ARENA_ASSET_DISCOVERY=COMPLETE resolved=%zu/%zu attempts=%u\n",
                VisualAssetCount, VisualAssetCount, m_discoveryAttempts);
            return;
        }

        size_t resolvedCount = 0;
        for (size_t index = 0; index < VisualAssetCount; ++index)
        {
            if ((unresolvedMask & (1u << index)) == 0)
            {
                ++resolvedCount;
            }
        }
        AZ_Printf(
            "STWGameplay",
            "STW_ARENA_ASSET_DISCOVERY=PENDING resolved=%zu/%zu attempts=%u retry_updates=%u\n",
            resolvedCount, VisualAssetCount, m_discoveryAttempts, DiscoveryRetryUpdates);
        for (size_t index = 0; index < VisualAssetCount; ++index)
        {
            if ((unresolvedMask & (1u << index)) == 0)
            {
                continue;
            }
            AZ_Printf(
                "STWGameplay",
                "STW_ARENA_ASSET_UNRESOLVED id=%s model=%s model_in_catalog=%d "
                "material=%s material_in_catalog=%d\n",
                VisualAssets[index].m_id,
                VisualAssets[index].m_modelPath, modelFound[index] ? 1 : 0,
                VisualAssets[index].m_materialPath, materialFound[index] ? 1 : 0);
        }
    }

    void ArenaPresentation::UpdateAsset(
        VisualAssetState& state, const char* modelPath, const char* materialPath)
    {
        AZ_UNUSED(materialPath);
        if (!state.m_discovered || state.m_meshAcquired)
        {
            return;
        }
        if (!state.m_materialAsset.IsReady())
        {
            return;
        }

        state.m_material = AZ::RPI::Material::FindOrCreate(state.m_materialAsset);
        AZ::Data::Asset<AZ::RPI::ModelAsset> modelAsset(
            state.m_modelAssetId, azrtti_typeid<AZ::RPI::ModelAsset>(), modelPath);
        modelAsset.QueueLoad();
        AZ::Render::MeshHandleDescriptor descriptor(modelAsset, state.m_material);
        state.m_meshHandle = m_meshFeatureProcessor->AcquireMesh(descriptor);
        if (!state.m_meshHandle.IsValid())
        {
            return;
        }
        // O3DE imports OBJ vertices as (-x, z, y); the arena OBJ set is authored raw Z-up,
        // so the imported model arrives rotated. Compensate with the proper rotation whose
        // basis columns are (-WorldX, WorldZ, WorldY), mapping imported (-ax, az, ay) back to
        // world (ax, ay, az). This is the world-static form of the same (-X, Z, Y) correction
        // the viewmodel and enemy presentations already apply. Determinant +1 (pure rotation);
        // scale stays Vector3::CreateOne().
        const AZ::Quaternion importOrientationCompensation = AZ::Quaternion::CreateFromMatrix3x3(
            AZ::Matrix3x3::CreateFromColumns(
                -AZ::Vector3::CreateAxisX(), AZ::Vector3::CreateAxisZ(), AZ::Vector3::CreateAxisY()));
        m_meshFeatureProcessor->SetTransform(
            state.m_meshHandle, AZ::Transform::CreateFromQuaternion(importOrientationCompensation),
            AZ::Vector3::CreateOne());
        m_meshFeatureProcessor->SetCustomMaterials(state.m_meshHandle, state.m_material);
        ++state.m_materialRebindCount;
        state.m_meshAcquired = true;
    }

    void ArenaPresentation::InitializeEnvironmentLight()
    {
        if (m_directionalLightFeatureProcessor == nullptr || m_environmentLightInitialized)
        {
            return;
        }

        m_directionalLightHandle = m_directionalLightFeatureProcessor->AcquireLight();
        if (!m_directionalLightHandle.IsValid())
        {
            return;
        }

        const AZ::Vector3 direction(-0.38f, -0.52f, -0.76f);
        m_directionalLightFeatureProcessor->SetDirection(m_directionalLightHandle, direction.GetNormalized());
        m_directionalLightFeatureProcessor->SetRgbIntensity(
            m_directionalLightHandle,
            AZ::Render::PhotometricColor<AZ::Render::PhotometricUnit::Lux>(
                AZ::Color(1.0f, 0.91f, 0.78f, 1.0f) * 25000.0f));
        m_directionalLightFeatureProcessor->SetShadowEnabled(m_directionalLightHandle, true);
        m_directionalLightFeatureProcessor->SetCascadeCount(m_directionalLightHandle, 3);
        m_directionalLightFeatureProcessor->SetShadowmapFrustumSplitSchemeRatio(m_directionalLightHandle, 0.7f);
        m_directionalLightFeatureProcessor->SetShadowFarClipDistance(m_directionalLightHandle, 80.0f);
        m_directionalLightFeatureProcessor->SetAngularDiameter(m_directionalLightHandle, 0.5f);
        m_environmentLightInitialized = true;
    }
}
