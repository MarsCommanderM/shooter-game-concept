#pragma once

#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/smart_ptr/unique_ptr.h>

namespace AZ
{
    class Entity;
}

namespace STWGameplay
{
    class PhysXPlayerRuntime final
    {
    public:
        ~PhysXPlayerRuntime();
        static constexpr float CapsuleHeight = 1.80f;
        static constexpr float CapsuleRadius = 0.35f;
        static constexpr float StepHeight = 0.30f;
        static constexpr float SlopeLimitDegrees = 45.0f;
        static constexpr float GroundProbeHeight = 0.08f;
        static constexpr float MaximumControllerSpeed = 9.0f;

        bool Initialize();
        void Shutdown();
        bool QueueVelocity(const AZ::Vector3& velocity);
        bool ResetPosition(const AZ::Vector3& position);
        bool Synchronize(AZ::Vector3& position, bool& grounded) const;
        bool IsValid() const;

    private:
        bool CreateStaticBox(const char* name, const AZ::Vector3& center, const AZ::Vector3& dimensions);

        AZStd::unique_ptr<AZ::Entity> m_playerEntity;
        AZStd::vector<AZStd::unique_ptr<AZ::Entity>> m_environmentEntities;
    };
}
