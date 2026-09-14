# STOP — legacy default branch

This `main` branch is a quarantined historical landing branch. It is **not**
the STW production line and must not receive feature work.

## Canonical STW location

- Repository: `MarsCommanderM/shooter-game-concept`
- Production branch: `brauny/stw-game-production`
- Native engine: O3DE 26.05.0
- Active game root: `stw-o3de/`
- Gameplay Gem: `stw-o3de/Gems/STWGameplay/`

NOVA is retired legacy material and must not be restored or treated as
production. The browser/Next.js prototype is also not the native game target.

## Current recovery gate

Weekend multiplayer, native server, bodycam, and ragdoll work must first be
recovered losslessly from Lightning under
[Issue #5](https://github.com/MarsCommanderM/shooter-game-concept/issues/5).

The clean O3DE repository boundary is staged in
[Draft PR #6](https://github.com/MarsCommanderM/shooter-game-concept/pull/6).
Do not merge it until Issue #5 and every listed verification gate are complete.

Agents must follow [AGENTS.md](AGENTS.md) and stop when repository identity,
branch, ancestry, or worktree state is unexpected.
