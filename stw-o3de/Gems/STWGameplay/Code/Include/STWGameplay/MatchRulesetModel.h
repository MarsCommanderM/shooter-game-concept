#pragma once

#include <AzCore/Component/EntityId.h>
#include <AzCore/Math/Vector3.h>
#include <AzCore/std/containers/vector.h>
#include <AzCore/std/utility/pair.h>

namespace STWGameplay
{
    //! Which multiplayer mode, if any, is currently governing PvP fire
    //! resolution and scoring. Only TeamDeathmatch has a real ruleset behind
    //! it - the main menu's other four mode buttons (Domination/
    //! Headquarters/Defense/Sabotage) are navigable but inert; each needs
    //! its own new objective system (capture points, a destructible HQ
    //! target, a plant/defuse timer) that does not exist anywhere in this
    //! codebase yet, and none of that is stubbed or faked here.
    enum class GameMode
    {
        None,
        TeamDeathmatch,
    };

    enum class TeamId
    {
        A,
        B,
    };

    //! The minimal, engine-free facts MatchRulesetModel needs about one
    //! bound player to resolve PvP fire against them - deliberately not the
    //! player's own full PlayerSliceModel, which this class has no
    //! reference to and does not need (mirrors EnemyCollectionModel's own
    //! "registry owns identity, gameplay model owns state" split already
    //! used throughout this codebase).
    struct PvpPlayerState
    {
        AZ::EntityId m_entityId;
        AZ::Vector3 m_position = AZ::Vector3::CreateZero();
        TeamId m_team = TeamId::A;
        bool m_alive = true;
    };

    //! Real, bounded ruleset for exactly one multiplayer mode: Team
    //! Deathmatch. Deliberately engine-free (no AZ::Component, no PhysX, no
    //! network bus) so it can be driven and verified the same
    //! model-first way as EnemyCollectionModel/DestructibleObjectModel -
    //! the caller (STWGameplaySystemComponent) is the only place that
    //! cross-references it against real PlayerSliceModel instances and
    //! applies actual damage, matching this codebase's existing "gameplay
    //! data owns gameplay data" convention.
    class MatchRulesetModel
    {
    public:
        //! Single-sphere PvP hit radius, matching EnemyCombatModel::m_radius's
        //! own default (0.80f) for a whole-character hit test - reusing an
        //! already-established number in this codebase, not a new guess.
        static constexpr float PlayerHitRadius = 0.80f;
        static constexpr int TeamDeathmatchScoreLimit = 30;

        void StartTeamDeathmatch();
        void EndMatch();
        bool IsActive() const { return m_mode == GameMode::TeamDeathmatch; }
        GameMode GetMode() const { return m_mode; }

        //! Round-robin team assignment as players bind, alternating A/B -
        //! deterministic bind order IS the assignment, no randomness.
        //! Idempotent: an already-assigned entity keeps its team.
        TeamId AssignTeam(AZ::EntityId entityId);
        TeamId GetTeam(AZ::EntityId entityId) const;
        void ClearTeamAssignments();

        //! Refreshed once per tick by the caller from all bound authorities'
        //! real positions/alive state, before that tick's fire input is
        //! processed.
        void SetPlayerStates(const AZStd::vector<PvpPlayerState>& states) { m_playerStates = states; }

        //! Resolves one shot against every OTHER bound, alive player -
        //! never the shooter itself, and never a teammate (no friendly fire
        //! in Team Deathmatch - a deliberate rule for this mode, not an
        //! oversight). Returns the closest hit player's EntityId (invalid
        //! if none) and its distance along the ray.
        AZ::EntityId ResolvePvpHit(
            AZ::EntityId shooterEntityId, const AZ::Vector3& origin, const AZ::Vector3& direction,
            float maxRange, float& outDistance) const;

        //! Records one kill's scoring effect. Returns true exactly once,
        //! the instant the scoring team's kill count first reaches
        //! TeamDeathmatchScoreLimit - edge-triggered, matching
        //! EncounterModel::IsCompleted()'s own one-shot-transition
        //! convention elsewhere in this codebase.
        bool RegisterKill(TeamId scoringTeam, TeamId& outWinningTeam);

        int GetScore(TeamId team) const;

    private:
        GameMode m_mode = GameMode::None;
        AZStd::vector<AZStd::pair<AZ::EntityId, TeamId>> m_teamAssignments;
        AZStd::vector<PvpPlayerState> m_playerStates;
        int m_scoreA = 0;
        int m_scoreB = 0;
        bool m_winReported = false;
    };
} // namespace STWGameplay
