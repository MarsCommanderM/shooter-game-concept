#pragma once

#include <AzCore/Math/Vector3.h>

#include <STWGameplay/PlayerCommand.h>
#include <STWGameplay/WeaponModel.h>

namespace STWGameplay
{
    //! Transport-neutral state captured by the composition root after a successful PhysX readback.
    //! The command acknowledgement and physical readback identities are intentionally separate:
    //! the current runtime does not prove that a queued command has completed in PhysX yet.
    struct AuthoritativePlayerSnapshot final
    {
        PlayerSnapshotSequence m_snapshotSequence = InvalidPlayerSimulationSequence;
        PlayerCommandSequence m_acknowledgedCommandSequence = InvalidPlayerSimulationSequence;
        PlayerSnapshotSequence m_physicalReadbackSequence = InvalidPlayerSimulationSequence;
        bool m_physicalStateSynchronized = false;

        // This is the latest physical base position returned by PhysX::Synchronize.
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        bool m_grounded = false;

        // This is the gameplay request submitted to PhysX, not resolved physical velocity.
        AZ::Vector3 m_requestedSimulationVelocity = AZ::Vector3::CreateZero();
        float m_yaw = 0.0f;
        float m_pitch = 0.0f;
        float m_health = 100.0f;
        bool m_alive = true;
        bool m_crouchDesired = false;
        bool m_slideActive = false;
        bool m_mantleRequested = false;
        bool m_mantleActive = false;

        EquipmentSlot m_activeEquipmentSlot = EquipmentSlot::Primary;
        EquipmentProfileId m_activeEquipmentProfile = EquipmentProfileId::STW_SMG_01;
        int m_magazine = 0;
        int m_reserve = 0;
        int m_charges = 0;
        float m_cooldownRemaining = 0.0f;
        float m_reloadRemaining = 0.0f;
        bool m_reloading = false;

        int m_deathEvents = 0;
        int m_respawnEvents = 0;
        WeaponEventId m_lastAcceptedUseEventId = 0;
    };
}
