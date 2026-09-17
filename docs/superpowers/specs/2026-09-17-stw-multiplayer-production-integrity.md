# STW Multiplayer Production Integrity

## Scope

This specification covers the first real cross-process multiplayer slice: one
dedicated O3DE server, two independent clients, authoritative player entities,
remote snapshot transport, and remote character presentation.

It is a production integrity gate, not a local simulation substitute.

## Authority invariants

- The server spawns each player from `stw_player.network.spawnable`.
- Every server player has `NetBindComponent`, `STWPlayerNetworkComponent`, a
  `TransformComponent`, `LocalPredictionPlayerInputComponent`, and
  `NetworkTransformComponent`.
- Server-side entities are `Authority`; each owning client entity is
  `Autonomous`; the other player is a `Client` proxy.
- An autonomous/authority player may own gameplay and PhysX simulation.
- A client proxy owns neither gameplay simulation nor PhysX authority.
- Remote proxy snapshots do not validate the owning client's command
  acknowledgement against a nonexistent local command history.
- Remote presentation consumes only an interpolated snapshot and never writes
  gameplay, physics, or reconciliation state.

## Required runtime proof

The gate must observe all of the following:

- two incoming server connections and two spawned authorities;
- two server authority roles and two published snapshots;
- on each client: one autonomous role, one client role, and one accepted remote
  snapshot;
- on each client: an active remote EMotionFX actor and a render-ready skinned
  character presentation;
- no remote snapshot rejection marker;
- both client and server processes alive through the observation window.

The executable gate is
`.github/lightning-t4/multiplayer_gate.sh`.

## Explicit limits

This gate proves transport, role separation, snapshot acceptance, and visual
remote-character activation. It does not yet claim latency simulation,
rewind/rollback, hit validation, matchmaking, persistence, or production
dedicated-server deployment hardening.
