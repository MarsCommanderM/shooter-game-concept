#include <STWGameplay/DestructibleObjectModel.h>

#include <AzCore/std/algorithm.h>

#include <cmath>

namespace STWGameplay
{
    void DestructibleObjectModel::Configure(
        size_t index, const AZ::Vector3& center, const AZ::Vector3& halfExtents, float maxHealth)
    {
        if (index >= MaxObjectCount)
        {
            return;
        }
        DestructibleObjectState& state = m_objects[index];
        state.m_center = center;
        state.m_halfExtents = halfExtents;
        state.m_maxHealth = maxHealth;
        state.m_health = maxHealth;
        state.m_active = true;
        state.m_damageEvents = 0;
        state.m_destroyedEvents = 0;
        m_objectCount = AZStd::max(m_objectCount, index + 1);
    }

    size_t DestructibleObjectModel::RayHitsObject(
        const AZ::Vector3& origin, const AZ::Vector3& direction, float maxRange, float& hitDistance) const
    {
        size_t closestIndex = MaxObjectCount;
        float closestDistance = maxRange;
        for (size_t index = 0; index < m_objectCount; ++index)
        {
            const DestructibleObjectState& object = m_objects[index];
            if (!object.m_active)
            {
                continue;
            }

            // Standard ray-vs-AABB slab test.
            float tMin = 0.0f;
            float tMax = closestDistance;
            bool hit = true;
            for (int axis = 0; axis < 3; ++axis)
            {
                const float originAxis = origin.GetElement(axis);
                const float dirAxis = direction.GetElement(axis);
                const float centerAxis = object.m_center.GetElement(axis);
                const float halfAxis = object.m_halfExtents.GetElement(axis);
                const float minBound = centerAxis - halfAxis;
                const float maxBound = centerAxis + halfAxis;
                if (fabsf(dirAxis) < 1e-6f)
                {
                    if (originAxis < minBound || originAxis > maxBound)
                    {
                        hit = false;
                        break;
                    }
                    continue;
                }
                float t1 = (minBound - originAxis) / dirAxis;
                float t2 = (maxBound - originAxis) / dirAxis;
                if (t1 > t2)
                {
                    AZStd::swap(t1, t2);
                }
                tMin = AZStd::max(tMin, t1);
                tMax = AZStd::min(tMax, t2);
                if (tMin > tMax)
                {
                    hit = false;
                    break;
                }
            }
            if (hit && tMin >= 0.0f && tMin < closestDistance)
            {
                closestDistance = tMin;
                closestIndex = index;
            }
        }
        hitDistance = closestDistance;
        return closestIndex;
    }

    bool DestructibleObjectModel::ApplyDamage(size_t index, float damage)
    {
        if (index >= m_objectCount || damage <= 0.0f)
        {
            return false;
        }
        DestructibleObjectState& state = m_objects[index];
        if (!state.m_active)
        {
            return false;
        }
        state.m_health = AZStd::max(0.0f, state.m_health - damage);
        ++state.m_damageEvents;
        if (state.m_health <= 0.0f)
        {
            state.m_active = false;
            ++state.m_destroyedEvents;
            return true;
        }
        return false;
    }

    void DestructibleObjectModel::Reset()
    {
        for (size_t index = 0; index < m_objectCount; ++index)
        {
            DestructibleObjectState& state = m_objects[index];
            state.m_health = state.m_maxHealth;
            state.m_active = true;
        }
    }
}
