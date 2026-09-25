#include <STWGameplay/MatchRulesetModel.h>

#include <cmath>

namespace STWGameplay
{
    namespace
    {
        // Same ray-vs-sphere test as PlayerSliceModel::RayHitsEnemy, kept as
        // an independent copy rather than a shared helper: MatchRulesetModel
        // is deliberately not allowed to depend on PlayerSliceModel (or vice
        // versa) so the two can be verified in isolation, matching this
        // codebase's existing per-model independence convention.
        bool RayHitsSphere(
            const AZ::Vector3& targetPosition, float radius, const AZ::Vector3& origin, const AZ::Vector3& direction,
            float maximumRange, float& projectedDistance)
        {
            const AZ::Vector3 toTarget = targetPosition - origin;
            const float projected = toTarget.Dot(direction);
            if (projected < 0.0f || projected > maximumRange)
            {
                return false;
            }
            projectedDistance = projected;
            const AZ::Vector3 closest = origin + direction * projected;
            return (closest - targetPosition).GetLengthSq() <= radius * radius;
        }
    }

    void MatchRulesetModel::StartTeamDeathmatch()
    {
        m_mode = GameMode::TeamDeathmatch;
        m_scoreA = 0;
        m_scoreB = 0;
        m_winReported = false;
    }

    void MatchRulesetModel::EndMatch()
    {
        m_mode = GameMode::None;
        m_teamAssignments.clear();
        m_playerStates.clear();
        m_scoreA = 0;
        m_scoreB = 0;
        m_winReported = false;
    }

    TeamId MatchRulesetModel::AssignTeam(AZ::EntityId entityId)
    {
        for (const auto& entry : m_teamAssignments)
        {
            if (entry.first == entityId)
            {
                return entry.second;
            }
        }
        const TeamId team = (m_teamAssignments.size() % 2 == 0) ? TeamId::A : TeamId::B;
        m_teamAssignments.push_back(AZStd::make_pair(entityId, team));
        return team;
    }

    TeamId MatchRulesetModel::GetTeam(AZ::EntityId entityId) const
    {
        for (const auto& entry : m_teamAssignments)
        {
            if (entry.first == entityId)
            {
                return entry.second;
            }
        }
        return TeamId::A;
    }

    void MatchRulesetModel::ClearTeamAssignments()
    {
        m_teamAssignments.clear();
    }

    const char* MatchRulesetModel::HitValidationName(HitValidation validation)
    {
        switch (validation)
        {
        case HitValidation::Accept:
            return "accept";
        case HitValidation::RejectNonFinite:
            return "non_finite";
        case HitValidation::RejectNotPresent:
            return "not_present";
        case HitValidation::RejectSelf:
            return "self";
        case HitValidation::RejectDead:
            return "dead";
        case HitValidation::RejectTeammate:
            return "teammate";
        case HitValidation::RejectRange:
            return "range";
        }
        return "not_present";
    }

    MatchRulesetModel::HitValidation MatchRulesetModel::ValidateAuthoritativeHit(
        AZ::EntityId shooterEntityId, AZ::EntityId targetEntityId, float damage, float maxRange) const
    {
        if (!shooterEntityId.IsValid() || !targetEntityId.IsValid()
            || !std::isfinite(damage) || damage <= 0.0f
            || !std::isfinite(maxRange) || maxRange <= 0.0f)
        {
            return HitValidation::RejectNonFinite;
        }

        const PvpPlayerState* shooter = nullptr;
        const PvpPlayerState* target = nullptr;
        for (const PvpPlayerState& state : m_playerStates)
        {
            if (state.m_entityId == shooterEntityId)
            {
                shooter = &state;
            }
            if (state.m_entityId == targetEntityId)
            {
                target = &state;
            }
        }
        if (shooter == nullptr || target == nullptr)
        {
            return HitValidation::RejectNotPresent;
        }
        if (shooterEntityId == targetEntityId)
        {
            return HitValidation::RejectSelf;
        }
        if (!shooter->m_alive || !target->m_alive)
        {
            return HitValidation::RejectDead;
        }
        if (GetTeam(shooterEntityId) == GetTeam(targetEntityId))
        {
            return HitValidation::RejectTeammate;
        }
        const float limit = maxRange + PlayerHitRadius;
        if ((target->m_position - shooter->m_position).GetLengthSq() > limit * limit)
        {
            return HitValidation::RejectRange;
        }
        return HitValidation::Accept;
    }

    AZ::EntityId MatchRulesetModel::ResolvePvpHit(
        AZ::EntityId shooterEntityId, const AZ::Vector3& origin, const AZ::Vector3& direction, float maxRange,
        float& outDistance) const
    {
        const TeamId shooterTeam = GetTeam(shooterEntityId);
        AZ::EntityId hitEntity;
        float bestDistance = maxRange;
        for (const PvpPlayerState& target : m_playerStates)
        {
            if (!target.m_alive || target.m_entityId == shooterEntityId || target.m_team == shooterTeam)
            {
                continue;
            }
            float projected = 0.0f;
            if (RayHitsSphere(target.m_position, PlayerHitRadius, origin, direction, bestDistance, projected))
            {
                hitEntity = target.m_entityId;
                bestDistance = projected;
            }
        }
        outDistance = bestDistance;
        return hitEntity;
    }

    bool MatchRulesetModel::RegisterKill(TeamId scoringTeam, TeamId& outWinningTeam)
    {
        int& score = (scoringTeam == TeamId::A) ? m_scoreA : m_scoreB;
        ++score;
        if (!m_winReported && score >= TeamDeathmatchScoreLimit)
        {
            m_winReported = true;
            outWinningTeam = scoringTeam;
            return true;
        }
        return false;
    }

    int MatchRulesetModel::GetScore(TeamId team) const
    {
        return (team == TeamId::A) ? m_scoreA : m_scoreB;
    }
} // namespace STWGameplay
