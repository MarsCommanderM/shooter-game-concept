#include <Network/STWPlayerNetworkComponent.h>

#include <AzCore/Interface/Interface.h>
#include <AzCore/Serialization/SerializeContext.h>
#include <Clients/STWGameplaySystemComponent.h>
#include <Multiplayer/Components/NetBindComponent.h>

#include <cmath>
#include <cstdlib>

namespace STWGameplay
{
    namespace
    {
        enum ActionFlag : AZ::u8
        {
            Sprint = 1u << 0,
            Jump = 1u << 1,
            Crouch = 1u << 2,
            Mantle = 1u << 3,
            Fire = 1u << 4,
            Reload = 1u << 5,
            SwitchWeapon = 1u << 6
        };

        AZ::u8 MakeActionFlags(const PlayerCommand& command)
        {
            AZ::u8 flags = 0;
            flags |= command.m_sprint ? Sprint : 0;
            flags |= command.m_jump ? Jump : 0;
            flags |= command.m_crouch ? Crouch : 0;
            flags |= command.m_mantle ? Mantle : 0;
            flags |= command.m_fire ? Fire : 0;
            flags |= command.m_reload ? Reload : 0;
            flags |= command.m_switchWeapon ? SwitchWeapon : 0;
            return flags;
        }
    } // namespace

    STWPlayerNetworkComponent::STWPlayerNetworkComponent()
        : m_snapshotSequenceChangedHandler(
            [this](uint32_t snapshotSequence) { OnAuthoritativeSnapshotSequenceChanged(snapshotSequence); })
    {
    }

    void STWPlayerNetworkComponent::Reflect(AZ::ReflectContext* context)
    {
        AZ::SerializeContext* serializeContext = azrtti_cast<AZ::SerializeContext*>(context);
        if (serializeContext)
        {
            serializeContext->Class<STWPlayerNetworkComponent, STWPlayerNetworkComponentBase>()
                ->Version(1);
        }
        STWPlayerNetworkComponentBase::Reflect(context);
    }

    void STWPlayerNetworkComponent::WriteCommand(
        STWPlayerNetworkComponentNetworkInput& networkInput,
        const PlayerCommand& command)
    {
        networkInput.m_commandSequence = command.m_sequence;
        networkInput.m_forward = command.m_forward;
        networkInput.m_strafe = command.m_strafe;
        networkInput.m_lookX = command.m_lookX;
        networkInput.m_lookY = command.m_lookY;
        networkInput.m_actionFlags = MakeActionFlags(command);
        networkInput.m_requestedEquipmentSlot = command.m_requestedEquipmentSlot;
    }

    bool STWPlayerNetworkComponent::ReadCommand(
        const STWPlayerNetworkComponentNetworkInput& networkInput,
        PlayerCommand& command)
    {
        if (networkInput.m_commandSequence == InvalidPlayerSimulationSequence)
        {
            return false;
        }

        PlayerInput input;
        input.m_forward = networkInput.m_forward;
        input.m_strafe = networkInput.m_strafe;
        input.m_lookX = networkInput.m_lookX;
        input.m_lookY = networkInput.m_lookY;
        input.m_sprint = (networkInput.m_actionFlags & Sprint) != 0;
        input.m_jump = (networkInput.m_actionFlags & Jump) != 0;
        input.m_crouch = (networkInput.m_actionFlags & Crouch) != 0;
        input.m_mantle = (networkInput.m_actionFlags & Mantle) != 0;
        input.m_fire = (networkInput.m_actionFlags & Fire) != 0;
        input.m_reload = (networkInput.m_actionFlags & Reload) != 0;
        input.m_switchWeapon = (networkInput.m_actionFlags & SwitchWeapon) != 0;
        input.m_requestedEquipmentSlot = networkInput.m_requestedEquipmentSlot;

        const PlayerCommand decoded = MakePlayerCommand(input, networkInput.m_commandSequence);
        if (!decoded.IsFinite())
        {
            return false;
        }

        command = decoded;
        return true;
    }

    void STWPlayerNetworkComponent::PublishAuthoritativeSnapshot(
        const AuthoritativePlayerSnapshot& snapshot)
    {
#if AZ_TRAIT_SERVER
        if (!IsNetEntityRoleAuthority() || !HasController())
        {
            return;
        }

        auto* controller = static_cast<STWPlayerNetworkComponentController*>(GetController());
        if (!m_authoritativeSnapshotReported)
        {
            AZ_Printf("STWGameplay",
                "STW_MP_AUTHORITY_SNAPSHOT_PUBLISHED entity=%s role=Authority sequence=%u\n",
                GetEntityId().ToString().c_str(), snapshot.m_snapshotSequence);
            m_authoritativeSnapshotReported = true;
        }
        controller->SetAcknowledgedCommandSequence(snapshot.m_acknowledgedCommandSequence);
        controller->SetPhysicalReadbackSequence(snapshot.m_physicalReadbackSequence);
        controller->SetPhysicalStateSynchronized(snapshot.m_physicalStateSynchronized);
        controller->SetPosition(snapshot.m_position);
        controller->SetGrounded(snapshot.m_grounded);
        controller->SetRequestedSimulationVelocity(snapshot.m_requestedSimulationVelocity);
        controller->SetYaw(snapshot.m_yaw);
        controller->SetPitch(snapshot.m_pitch);
        controller->SetHealth(snapshot.m_health);
        controller->SetAlive(snapshot.m_alive);
        controller->SetCrouchDesired(snapshot.m_crouchDesired);
        controller->SetSlideActive(snapshot.m_slideActive);
        controller->SetMantleRequested(snapshot.m_mantleRequested);
        controller->SetMantleActive(snapshot.m_mantleActive);
        controller->SetActiveEquipmentSlot(static_cast<uint8_t>(snapshot.m_activeEquipmentSlot));
        controller->SetActiveEquipmentProfile(static_cast<uint8_t>(snapshot.m_activeEquipmentProfile));
        controller->SetMagazine(snapshot.m_magazine);
        controller->SetReserve(snapshot.m_reserve);
        controller->SetCharges(snapshot.m_charges);
        controller->SetCooldownRemaining(snapshot.m_cooldownRemaining);
        controller->SetReloadRemaining(snapshot.m_reloadRemaining);
        controller->SetReloading(snapshot.m_reloading);
        controller->SetDeathEvents(snapshot.m_deathEvents);
        controller->SetRespawnEvents(snapshot.m_respawnEvents);
        controller->SetLastAcceptedUseEventId(snapshot.m_lastAcceptedUseEventId);
        // Set last so proxy listeners only observe a complete property set.
        controller->SetSnapshotSequence(snapshot.m_snapshotSequence);
#else
        AZ_UNUSED(snapshot);
#endif
    }

    void STWPlayerNetworkComponent::OnInit()
    {
        AZ_Printf("STWGameplay", "STW_MP_PLAYER_COMPONENT_INIT entity=%s netbind=%d\n",
            GetEntityId().ToString().c_str(), GetNetBindComponent() != nullptr ? 1 : 0);
        m_netBindComponent->AddNetworkActivatedEventHandler(m_networkActivatedHandler);
    }

    void STWPlayerNetworkComponent::OnActivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
        AZ_Printf("STWGameplay", "STW_MP_PLAYER_COMPONENT_ACTIVATE entity=%s\n",
            GetEntityId().ToString().c_str());
        m_networkRoleReported = false;
        m_authoritativeSnapshotReported = false;
        m_remoteSnapshotReported = false;
        m_remoteSnapshotDiagnosticReported = false;
        SnapshotSequenceAddEvent(m_snapshotSequenceChangedHandler);
    }

    void STWPlayerNetworkComponent::OnDeactivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
        if (m_remoteSnapshotBound)
        {
            if (STWGameplaySystemComponent* gameplay = AZ::Interface<STWGameplaySystemComponent>::Get())
            {
                gameplay->UnbindNetworkPlayer(GetEntityId());
            }
            m_remoteSnapshotBound = false;
        }
        m_snapshotSequenceChangedHandler.Disconnect();
    }

    void STWPlayerNetworkComponent::OnNetworkActivated()
    {
        AZ_Printf("STWGameplay", "STW_MP_PLAYER_NETWORK_ACTIVATED entity=%s netbind=%d\n",
            GetEntityId().ToString().c_str(), GetNetBindComponent() != nullptr ? 1 : 0);
        if (!m_networkRoleReported && GetNetBindComponent() != nullptr)
        {
            const Multiplayer::NetEntityRole role = GetNetBindComponent()->GetNetEntityRole();
            AZ_Printf("STWGameplay",
                "STW_MP_NETWORK_ENTITY_ROLE entity=%s role=%s authority=%d autonomous=%d proxy=%d\n",
                GetEntityId().ToString().c_str(),
                Multiplayer::GetEnumString(role),
                IsNetEntityRoleAuthority() ? 1 : 0,
                IsNetEntityRoleAutonomous() ? 1 : 0,
                IsNetEntityRoleClient() ? 1 : 0);
            m_networkRoleReported = true;
        }
        if (IsNetEntityRoleClient())
        {
            if (STWGameplaySystemComponent* gameplay = AZ::Interface<STWGameplaySystemComponent>::Get())
            {
                m_remoteSnapshotBound = gameplay->BindRemoteNetworkPlayer(GetEntityId());
            }
        }
    }

    void STWPlayerNetworkComponent::OnAuthoritativeSnapshotSequenceChanged(
        [[maybe_unused]] uint32_t snapshotSequence)
    {
        if (IsNetEntityRoleAuthority())
        {
            return;
        }

        AuthoritativePlayerSnapshot snapshot;
        if (!ReadAuthoritativeSnapshot(snapshot))
        {
            if (!m_remoteSnapshotDiagnosticReported && IsNetEntityRoleClient())
            {
                AZ_Printf("STWGameplay",
                    "STW_MP_REMOTE_SNAPSHOT_REJECTED entity=%s sequence=%u physical_sync=%d slot=%u profile=%u\n",
                    GetEntityId().ToString().c_str(), GetSnapshotSequence(),
                    GetPhysicalStateSynchronized() ? 1 : 0,
                    static_cast<unsigned>(GetActiveEquipmentSlot()),
                    static_cast<unsigned>(GetActiveEquipmentProfile()));
                m_remoteSnapshotDiagnosticReported = true;
            }
            return;
        }

        if (STWGameplaySystemComponent* gameplay = AZ::Interface<STWGameplaySystemComponent>::Get())
        {
            const bool accepted = gameplay->ReceiveNetworkSnapshot(GetEntityId(), snapshot);
            if (accepted && IsNetEntityRoleClient() && !m_remoteSnapshotReported
                && gameplay->GetRemoteNetworkSnapshot(GetEntityId()) != nullptr)
            {
                AZ_Printf("STWGameplay",
                    "STW_MP_REMOTE_SNAPSHOT_RECEIVED entity=%s sequence=%u\n",
                    GetEntityId().ToString().c_str(), snapshot.m_snapshotSequence);
                m_remoteSnapshotReported = true;
            }
            else if (!accepted && !m_remoteSnapshotDiagnosticReported && IsNetEntityRoleClient())
            {
                AZ_Printf("STWGameplay",
                    "STW_MP_REMOTE_SNAPSHOT_NOT_ACCEPTED entity=%s sequence=%u\n",
                    GetEntityId().ToString().c_str(), snapshot.m_snapshotSequence);
                m_remoteSnapshotDiagnosticReported = true;
            }
        }
    }

    bool STWPlayerNetworkComponent::ReadAuthoritativeSnapshot(
        AuthoritativePlayerSnapshot& snapshot) const
    {
        const uint32_t snapshotSequence = GetSnapshotSequence();
        const uint8_t activeEquipmentSlot = GetActiveEquipmentSlot();
        const uint8_t activeEquipmentProfile = GetActiveEquipmentProfile();
        if (snapshotSequence == InvalidPlayerSimulationSequence
            || !GetPhysicalStateSynchronized()
            || activeEquipmentSlot >= WeaponModel::EquipmentSlotCount
            || activeEquipmentProfile >= WeaponModel::EquipmentProfileCount)
        {
            return false;
        }

        snapshot.m_snapshotSequence = snapshotSequence;
        snapshot.m_acknowledgedCommandSequence = GetAcknowledgedCommandSequence();
        snapshot.m_physicalReadbackSequence = GetPhysicalReadbackSequence();
        snapshot.m_physicalStateSynchronized = GetPhysicalStateSynchronized();
        snapshot.m_position = GetPosition();
        snapshot.m_grounded = GetGrounded();
        snapshot.m_requestedSimulationVelocity = GetRequestedSimulationVelocity();
        snapshot.m_yaw = GetYaw();
        snapshot.m_pitch = GetPitch();
        snapshot.m_health = GetHealth();
        snapshot.m_alive = GetAlive();
        snapshot.m_crouchDesired = GetCrouchDesired();
        snapshot.m_slideActive = GetSlideActive();
        snapshot.m_mantleRequested = GetMantleRequested();
        snapshot.m_mantleActive = GetMantleActive();
        snapshot.m_activeEquipmentSlot = static_cast<EquipmentSlot>(activeEquipmentSlot);
        snapshot.m_activeEquipmentProfile = static_cast<EquipmentProfileId>(activeEquipmentProfile);
        snapshot.m_magazine = GetMagazine();
        snapshot.m_reserve = GetReserve();
        snapshot.m_charges = GetCharges();
        snapshot.m_cooldownRemaining = GetCooldownRemaining();
        snapshot.m_reloadRemaining = GetReloadRemaining();
        snapshot.m_reloading = GetReloading();
        snapshot.m_deathEvents = GetDeathEvents();
        snapshot.m_respawnEvents = GetRespawnEvents();
        snapshot.m_lastAcceptedUseEventId = GetLastAcceptedUseEventId();

        return snapshot.m_position.IsFinite()
            && snapshot.m_requestedSimulationVelocity.IsFinite()
            && std::isfinite(snapshot.m_yaw)
            && std::isfinite(snapshot.m_pitch)
            && std::isfinite(snapshot.m_health)
            && std::isfinite(snapshot.m_cooldownRemaining)
            && std::isfinite(snapshot.m_reloadRemaining);
    }

    STWPlayerNetworkComponentController::STWPlayerNetworkComponentController(STWPlayerNetworkComponent& parent)
        : STWPlayerNetworkComponentControllerBase(parent)
    {
    }

    void STWPlayerNetworkComponentController::OnActivate(
        [[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
        AZ_Printf("STWGameplay", "STW_MP_PLAYER_CONTROLLER_ACTIVATE entity=%s\n",
            GetEntityId().ToString().c_str());
        TryBindGameplayAuthority();
    }

    void STWPlayerNetworkComponentController::OnDeactivate(
        [[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
        UnbindGameplayAuthority();
    }

    void STWPlayerNetworkComponentController::PopHeldCommand()
    {
        if (m_heldCount == 0)
        {
            return;
        }
        for (size_t index = 1; index < m_heldCount; ++index)
        {
            m_heldCommands[index - 1] = m_heldCommands[index];
        }
        --m_heldCount;
    }

    void STWPlayerNetworkComponentController::CreateInput(
        Multiplayer::NetworkInput& input,
        [[maybe_unused]] float deltaTime)
    {
        if (!TryBindGameplayAuthority())
        {
            return;
        }

        if (STWGameplaySystemComponent* gameplay = AZ::Interface<STWGameplaySystemComponent>::Get())
        {
            PlayerCommand command;
            if (!gameplay->CreateNetworkCommand(GetEntityId(), command))
            {
                return;
            }
            if (m_heldCount < MaxHeldCommands)
            {
                m_heldCommands[m_heldCount++] = command;
            }

            const char* delayText = std::getenv("STW_MP_COMMAND_DELAY_STEPS");
            const char* lossText = std::getenv("STW_MP_COMMAND_LOSS_EVERY");
            const AZ::u32 delaySteps = delayText != nullptr ? static_cast<AZ::u32>(std::strtoul(delayText, nullptr, 10)) : 0u;
            const AZ::u32 lossEvery = lossText != nullptr ? static_cast<AZ::u32>(std::strtoul(lossText, nullptr, 10)) : 0u;
            const CommandTransportDecision decision = DecideCommandTransport(
                static_cast<AZ::u32>(m_heldCount), delaySteps, m_heldCommands[0].m_sequence, lossEvery);
            if (decision.m_dropped)
            {
                const PlayerCommandSequence droppedSequence = m_heldCommands[0].m_sequence;
                PopHeldCommand();
                if (!m_transportDropLogged)
                {
                    AZ_Printf(
                        "STWGameplay",
                        "STW_MP_COMMAND_TRANSPORT sent=0 dropped=1 delay_steps=%u loss_every=%u sequence=%u\n",
                        delaySteps, lossEvery, droppedSequence);
                    m_transportDropLogged = true;
                }
                return;
            }
            if (!decision.m_send)
            {
                return;
            }

            const PlayerCommand released = m_heldCommands[0];
            PopHeldCommand();
            if ((delaySteps > 0u || lossEvery > 0u) && !m_transportReleaseLogged)
            {
                AZ_Printf(
                    "STWGameplay",
                    "STW_MP_COMMAND_TRANSPORT sent=1 dropped=0 delay_steps=%u loss_every=%u sequence=%u\n",
                    delaySteps, lossEvery, released.m_sequence);
                m_transportReleaseLogged = true;
            }
            if (auto* networkInput = input.FindComponentInput<STWPlayerNetworkComponentNetworkInput>())
            {
                STWPlayerNetworkComponent::WriteCommand(*networkInput, released);
            }
        }
    }

    void STWPlayerNetworkComponentController::ProcessInput(
        Multiplayer::NetworkInput& input,
        [[maybe_unused]] float deltaTime)
    {
        if (!TryBindGameplayAuthority())
        {
            return;
        }

        const auto* networkInput = input.FindComponentInput<STWPlayerNetworkComponentNetworkInput>();
        if (networkInput == nullptr)
        {
            return;
        }

        PlayerCommand command;
        if (!STWPlayerNetworkComponent::ReadCommand(*networkInput, command))
        {
            return;
        }

        if (STWGameplaySystemComponent* gameplay = AZ::Interface<STWGameplaySystemComponent>::Get())
        {
            gameplay->SubmitNetworkCommand(GetEntityId(), command);
        }
    }

    bool STWPlayerNetworkComponentController::TryBindGameplayAuthority()
    {
        if (m_gameplayAuthorityBound)
        {
            return true;
        }
        if (!IsNetEntityRoleAuthority() && !IsNetEntityRoleAutonomous())
        {
            return false;
        }
        if (STWGameplaySystemComponent* gameplay = AZ::Interface<STWGameplaySystemComponent>::Get())
        {
            m_gameplayAuthorityBound = gameplay->BindNetworkPlayer(GetEntityId());
        }
        return m_gameplayAuthorityBound;
    }

    void STWPlayerNetworkComponentController::UnbindGameplayAuthority()
    {
        if (!m_gameplayAuthorityBound)
        {
            return;
        }
        if (STWGameplaySystemComponent* gameplay = AZ::Interface<STWGameplaySystemComponent>::Get())
        {
            gameplay->UnbindNetworkPlayer(GetEntityId());
        }
        m_gameplayAuthorityBound = false;
    }
} // namespace STWGameplay
