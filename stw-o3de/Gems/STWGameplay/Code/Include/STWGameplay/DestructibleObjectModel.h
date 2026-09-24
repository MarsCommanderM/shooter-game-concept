#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>
#include <cstddef>

namespace STWGameplay
{
    struct DestructibleObjectState
    {
        AZ::Vector3 m_center = AZ::Vector3::CreateZero();
        AZ::Vector3 m_halfExtents = AZ::Vector3::CreateZero();
        float m_health = 0.0f;
        float m_maxHealth = 0.0f;
        bool m_active = true;
        int m_damageEvents = 0;
        int m_destroyedEvents = 0;
    };

    //! Deterministic, engine-free model for a small, fixed set of real
    //! destructible world objects (cover crates) - deliberately separate
    //! from enemy combat, and deliberately bounded (MaxObjectCount) rather
    //! than a general destruction system: "begrenzt aber genug sinnvolle
    //! Objekte" per the user's own scoping. Owns no physics/mesh/collision
    //! state itself - STWGameplaySystemComponent reflects m_active into the
    //! real PhysX collider (StaticRigidBodyComponent::DisablePhysics) and
    //! mesh visibility once an object is destroyed.
    class DestructibleObjectModel
    {
    public:
        static constexpr size_t MaxObjectCount = 4;

        //! Registers object `index`'s real world AABB (center + half-extents,
        //! matching PhysXArenaRuntime::StaticColliderDescription's own
        //! center/full-dimensions convention) and starting health.
        void Configure(size_t index, const AZ::Vector3& center, const AZ::Vector3& halfExtents, float maxHealth);

        //! Ray-vs-AABB slab test against every still-active configured
        //! object. Returns the index of the closest hit strictly within
        //! maxRange, or MaxObjectCount if none was hit - the same
        //! closest-wins contract PlayerSliceModel::RayHitsEnemy already
        //! uses for enemies.
        size_t RayHitsObject(
            const AZ::Vector3& origin, const AZ::Vector3& direction, float maxRange, float& hitDistance) const;

        //! Applies damage to one object. Returns true only on the exact
        //! call that brings it from active to destroyed (edge-triggered,
        //! not every subsequent call against an already-destroyed object).
        bool ApplyDamage(size_t index, float damage);

        //! Restores every configured object to full health/active - used on
        //! respawn/round-reset, mirroring PlayerSliceModel::ResetPlayer().
        void Reset();

        const DestructibleObjectState& GetState(size_t index) const { return m_objects[index]; }
        size_t GetObjectCount() const { return m_objectCount; }

    private:
        AZStd::array<DestructibleObjectState, MaxObjectCount> m_objects;
        size_t m_objectCount = 0;
    };
}
