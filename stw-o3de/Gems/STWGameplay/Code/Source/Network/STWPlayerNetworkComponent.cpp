#include <Network/STWPlayerNetworkComponent.h>

#include <AzCore/Serialization/SerializeContext.h>
#include <Multiplayer/Components/NetBindComponent.h>

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

    void STWPlayerNetworkComponent::OnInit()
    {
        m_netBindComponent->AddNetworkActivatedEventHandler(m_networkActivatedHandler);
    }

    void STWPlayerNetworkComponent::OnActivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    void STWPlayerNetworkComponent::OnDeactivate([[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    void STWPlayerNetworkComponent::OnNetworkActivated()
    {
    }

    STWPlayerNetworkComponentController::STWPlayerNetworkComponentController(STWPlayerNetworkComponent& parent)
        : STWPlayerNetworkComponentControllerBase(parent)
    {
    }

    void STWPlayerNetworkComponentController::OnActivate(
        [[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    void STWPlayerNetworkComponentController::OnDeactivate(
        [[maybe_unused]] Multiplayer::EntityIsMigrating entityIsMigrating)
    {
    }

    void STWPlayerNetworkComponentController::CreateInput(
        [[maybe_unused]] Multiplayer::NetworkInput& input,
        [[maybe_unused]] float deltaTime)
    {
        // Command-source integration is intentionally a separate step. This carrier does not
        // sample input or create a second gameplay authority.
    }

    void STWPlayerNetworkComponentController::ProcessInput(
        [[maybe_unused]] Multiplayer::NetworkInput& input,
        [[maybe_unused]] float deltaTime)
    {
        // Gameplay integration is intentionally a separate step. This carrier does not mutate
        // PlayerSliceModel, WeaponModel, PhysX, or presentation state.
    }
} // namespace STWGameplay
