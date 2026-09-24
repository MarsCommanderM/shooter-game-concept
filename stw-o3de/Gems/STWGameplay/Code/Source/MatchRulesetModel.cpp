#include <STWGameplay/MatchRulesetModel.h>

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
