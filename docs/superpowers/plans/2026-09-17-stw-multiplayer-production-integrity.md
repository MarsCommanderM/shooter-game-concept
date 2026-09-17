# STW Multiplayer Production Integrity Plan

## Objective

Turn the network player slice from a code-only boundary into a reproducible,
cross-process production proof with visible remote players.

## Work completed

- [x] Load `STWGameplay` in AssetProcessorBatch through the Tools/Builders
  aliases so custom network components are not silently stripped.
- [x] Add O3DE's real `Multiplayer::LocalPredictionPlayerInputComponent` to
  the network player prefab.
- [x] Prove two dedicated-server authorities and two client connections.
- [x] Separate remote proxy snapshot acceptance from local command
  acknowledgement validation.
- [x] Feed interpolated remote state into an independent EMotionFX/Atom
  character presentation without granting gameplay or PhysX authority.
- [x] Add the two-client production gate with process-liveness and marker
  assertions.
- [x] Run the gate successfully with two real clients:
  `stw-o3de-gate/multiplayer-20260917T093449Z-473371`.

## Verification

- `STWGameplay.Tests`: 1 registered CTest, passed.
- Multiplayer gate: `RESULT=PASS`.
- Server markers: 2 connections, 2 joins, 2 prefab component sets, 2
  authorities, 2 snapshots, 2 ideal connection-quality transitions.
- Each client: autonomous role, client proxy role, accepted remote snapshot,
  active remote presentation, and render-ready remote presentation.

## Next production slice

The next gate should add controlled latency/loss and prove command input,
server reconciliation, remote interpolation under movement, and disconnect/
respawn cleanup. Those are intentionally not reported as complete by this
slice.
