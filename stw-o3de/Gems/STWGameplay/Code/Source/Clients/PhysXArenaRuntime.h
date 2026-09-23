#pragma once

#include <cstddef>

#include <AzCore/Math/Quaternion.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/array.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ
{
    class Entity;
}

namespace STWGameplay
{
    //! Owns the static PhysX collision course independently from player physics.
    class PhysXArenaRuntime final
    {
    public:
        struct StaticColliderDescription
        {
            const char* m_name = nullptr;
            AZ::Vector3 m_center = AZ::Vector3::CreateZero();
            AZ::Vector3 m_dimensions = AZ::Vector3::CreateZero();
            //! Identity for every existing axis-aligned box. Only the West Annex
            //! ramp uses a non-identity rotation (its walkable slope).
            AZ::Quaternion m_rotation = AZ::Quaternion::CreateIdentity();
        };

        static constexpr size_t StaticColliderCount = 46;

        ~PhysXArenaRuntime();

        bool Initialize();
        void Shutdown();

        bool IsValid() const;
        size_t GetActiveColliderCount() const { return m_staticColliderEntities.size(); }

        //! Single canonical description of the existing static arena collision course.
        static const AZStd::array<StaticColliderDescription, StaticColliderCount>&
        GetStaticColliderDescriptions();

    private:
        bool CreateStaticBox(const StaticColliderDescription& description);

        AZStd::vector<AZStd::unique_ptr<AZ::Entity>> m_staticColliderEntities;
    };
}
