#pragma once

#include <cstdint>

namespace STWGameplay
{
    //! Engine-independent authority state for a character's animated/physical handoff.
    //! Gameplay health, transforms, respawn data, weapons, presentation, and networking
    //! remain outside this state object.
    enum class CharacterPhysicsMode : std::uint8_t
    {
        Animated = 0,
        TransitionToPhysics,
        Ragdoll,
        Settled,
        TransitionToAnimation
    };

    class CharacterPhysicalState final
    {
    public:
        CharacterPhysicsMode GetMode() const { return m_mode; }

        bool RequestDeath()
        {
            switch (m_mode)
            {
            case CharacterPhysicsMode::Animated:
                return TransitionTo(CharacterPhysicsMode::TransitionToPhysics);
            case CharacterPhysicsMode::TransitionToPhysics:
            case CharacterPhysicsMode::Ragdoll:
            case CharacterPhysicsMode::Settled:
                return true;
            case CharacterPhysicsMode::TransitionToAnimation:
                return false;
            }

            return false;
        }

        bool ConfirmPhysicsAuthority()
        {
            return TransitionTo(CharacterPhysicsMode::Ragdoll);
        }

        bool MarkSettled()
        {
            return TransitionTo(CharacterPhysicsMode::Settled);
        }

        bool RequestReset()
        {
            switch (m_mode)
            {
            case CharacterPhysicsMode::Animated:
            case CharacterPhysicsMode::TransitionToAnimation:
                return true;
            case CharacterPhysicsMode::TransitionToPhysics:
            case CharacterPhysicsMode::Ragdoll:
            case CharacterPhysicsMode::Settled:
                return TransitionTo(CharacterPhysicsMode::TransitionToAnimation);
            }

            return false;
        }

        bool CompleteAnimationRestore()
        {
            if (m_mode == CharacterPhysicsMode::Animated)
            {
                return true;
            }

            return TransitionTo(CharacterPhysicsMode::Animated);
        }

        //! Explicit transition entry point for a verified runtime adapter.
        bool TryTransition(CharacterPhysicsMode target)
        {
            return TransitionTo(target);
        }

        static bool IsValidTransition(CharacterPhysicsMode from, CharacterPhysicsMode to)
        {
            if (from == to)
            {
                return true;
            }

            switch (from)
            {
            case CharacterPhysicsMode::Animated:
                return to == CharacterPhysicsMode::TransitionToPhysics;
            case CharacterPhysicsMode::TransitionToPhysics:
                return to == CharacterPhysicsMode::Ragdoll || to == CharacterPhysicsMode::TransitionToAnimation;
            case CharacterPhysicsMode::Ragdoll:
                return to == CharacterPhysicsMode::Settled || to == CharacterPhysicsMode::TransitionToAnimation;
            case CharacterPhysicsMode::Settled:
                return to == CharacterPhysicsMode::TransitionToAnimation;
            case CharacterPhysicsMode::TransitionToAnimation:
                return to == CharacterPhysicsMode::Animated;
            }

            return false;
        }

    private:
        bool TransitionTo(CharacterPhysicsMode target)
        {
            if (!IsValidTransition(m_mode, target))
            {
                return false;
            }

            m_mode = target;
            return true;
        }

        CharacterPhysicsMode m_mode = CharacterPhysicsMode::Animated;
    };
} // namespace STWGameplay
