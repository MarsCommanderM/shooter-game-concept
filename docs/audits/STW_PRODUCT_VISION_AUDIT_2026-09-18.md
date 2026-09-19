# STW product vision audit — 2026-09-18

Audited tree: `codex/stw-integration-2026-09-17` @ `d1289cc`
(production `bfdc27f` is an ancestor; 0 behind, 41 ahead).
Vision statement: [`docs/STW_PRODUCT_VISION.md`](../STW_PRODUCT_VISION.md).

## Method and limits

- Source inspection of `stw-o3de/Gems/STWGameplay/Code` and
  `stw-o3de/Project/Assets`, plus the provenance file of each asset.
- Runtime evidence is the existing gate run
  `stw-o3de-gate/player-slice-20260918T181554Z` (`RESULT=PASS`, source commit
  `7d0c6ee4`). Only a docs file changed between that commit and `d1289cc`, so
  the run still describes the code.
- **Not done in this audit:** no build, no unit-test run and no new gate run.
  Test names below come from reading the test files, not from executing them.
- Only the integration branch was inspected. The weekend recovery lineages
  (Issue #5, PRs #7 and #8) reportedly contain ragdoll work and were not read.
  Status below is for this tree only.

## Bodycam (presentation only)

Code: `Include/STWGameplay/BodycamCameraPresentation.h`,
`Source/BodycamCameraPresentation.cpp`, wired in
`Source/Clients/STWGameplaySystemComponent.cpp` (input built at ~1268, applied
to the camera at ~2818). Gate marker: `BODYCAM_PRESENTATION_ACCEPTANCE result=PASS`.
Authority boundary: input is copied from gameplay state; `m_shotFired` comes
from the accepted-shot event, not raw trigger input; a test asserts the input
snapshot stays unchanged.

| Aspect | Status | Evidence |
|---|---|---|
| Look inertia | IMPLEMENTED | `m_lookOffset`; test `LookInertiaRespondsAndRecovers` |
| Locomotion response | IMPLEMENTED | locomotion blend and bob; `SprintResponseIsStrongerThanNormalLocomotion` |
| Acceleration / braking | PARTIAL | signed planar-acceleration lag, bounded, tested; no braking-specific test |
| Sprint response | IMPLEMENTED | sprint frequency scale; `SprintResponseIsStrongerThanNormalLocomotion` |
| Crouch / slide response | PARTIAL | scale factors and inputs wired; no crouch or slide test |
| Jump / airborne / landing | PARTIAL | landing pulse tested (`LandingPulseTriggersOnce...`); airborne offset in code, untested |
| Mantle response | IMPLEMENTED | `MantleResponseIsPresentationOnlyAndBounded` |
| ADS composition | PARTIAL | motion suppression and FOV blend tested; viewmodel ADS composition not audited |
| Camera recoil | IMPLEMENTED | `AcceptedShotSignalProducesPresentationRecoil`, `RecoilRecoversWithoutAdditionalShot` |
| Reduced Motion | PARTIAL | tuning and tests exist; runtime code never calls `SetProfile`, so a player cannot select it |
| Presentation interpolation | PARTIAL | `PresentationInterpolation` class and tests exist; enemy interpolation is wired; use for the player camera is unverified |

`BODYCAM_CURRENT_STATUS=PARTIAL` — a working, tested, gate-accepted foundation.
Feel and visual quality are UNVERIFIED: no captured motion evidence was reviewed.

## Ragdoll / character physics

| Field | Status | Evidence |
|---|---|---|
| `CHARACTER_PHYSICAL_STATE_BOUNDARY` | IMPLEMENTED | `CharacterPhysicalState.h`, engine-independent state machine, 6 tests |
| `NATIVE_RAGDOLL_RUNTIME` | ABSENT | `ragdoll` appears in `Source/` only in one comment (`STWGameplaySystemComponent.h:247`); no PhysX ragdoll component, configuration or asset |
| `ANIMATION_TO_PHYSICS_TRANSITION` | PARTIAL | state moves to `TransitionToPhysics` on death, but nothing calls `ConfirmPhysicsAuthority` or `TryTransition`, so it never reaches `Ragdoll`. Death is shown by playing the `DEATH` motion clip. |
| `PHYSICS_TO_RESET_TRANSITION` | PARTIAL | state machine resets on respawn; there is no physics body to reset |
| `RESPAWN_INTEGRATION` | PARTIAL | `SynchronizeSkeletalCharacterPhysicalState` follows gameplay lifecycle and respawn events; state-only |
| `NETWORK_READY_CHARACTER_PHYSICS_BOUNDARY` | PARTIAL | `Alive` and `DeathEvents` replicate from authority; no character physics mode or ragdoll state replicates |

`RAGDOLL_CORE_REQUIREMENT=YES`, `RAGDOLL_CURRENT_STATUS=NOT_YET_IMPLEMENTED`
(state-machine boundary only). **Open production requirement.** The character
provenance file already reserves a separate PhysX ragdoll joint mapping, so the
boundary was designed for this.

## Visual state classification

Evidence frame: `player-slice-20260918T181554Z/stw-player-slice.png`
(1920x1080, Atom on Vulkan). It shows a modular hard-edged arena and a
box-style humanoid.

| Content | Class | Basis |
|---|---|---|
| Arena `STW_ARENA_01` kit | BLOCKOUT | script-generated OBJ modules, eight generated PBR families, hard-edged geometry in the frame |
| `STW_CHARACTER_01` | TECHNICAL_VALIDATION | script-authored 18-joint rig for the EMotionFX pipeline; a box-style humanoid appears in the frame; no evidence of production anatomy |
| `STW_ENEMY_01_RIN` | TECHNICAL_VALIDATION | unmodified O3DE engine sample character, 16 authored materials; a stand-in, not STW art |
| Weapons (all 10) | BLOCKOUT | text-authored OBJ, one untextured StandardPBR material each |
| HUD text and Atom stats overlay | TEMPORARY_DEBUG | plain overlay text |
| Lighting and exposure | TECHNICAL_VALIDATION | recent commits calibrate exposure; no acceptance against the cinematic target |

Nothing in the repository is `PRODUCTION_CANDIDATE` or `PRODUCTION_READY`.
The frame reports 7.8 FPS; this is a property of the capture environment and is
not a performance claim about the game.

## Report fields

```
PRODUCT_VISION_DOCUMENTED=YES
PRODUCT_VISION_DOCUMENT=docs/STW_PRODUCT_VISION.md
AAA_QUALITY_TARGET_DOCUMENTED=YES
HYPERREALISTIC_CINEMATIC_TARGET_DOCUMENTED=YES
MULTIPLAYER_FPS_TARGET_DOCUMENTED=YES
BODYCAM_CORE_REQUIREMENT_DOCUMENTED=YES
BODYCAM_CURRENT_STATUS=PARTIAL (tested, gate-accepted foundation; see table)
RAGDOLL_CORE_REQUIREMENT_DOCUMENTED=YES
RAGDOLL_CURRENT_STATUS=NOT_YET_IMPLEMENTED (state-machine boundary only)
HUMANOID_PRODUCTION_CHARACTER_REQUIREMENT_DOCUMENTED=YES
CURRENT_VISUAL_STATE_CLASSIFICATION=BLOCKOUT / TECHNICAL_VALIDATION / TEMPORARY_DEBUG
CURRENT_SCENE_IS_FINAL_QUALITY=NO
PLACEHOLDER_CONTENT_STILL_PRESENT=arena kit, STW_CHARACTER_01, engine-sample enemy, all weapon meshes, HUD text overlay
MAJOR_VISUAL_PRODUCTION_GAPS=production humanoid characters; production first-person arms and weapons; production environment art and materials; lighting and atmosphere accepted against the cinematic target; VFX; captured evidence of any of these
MAJOR_CHARACTER_PHYSICS_GAPS=native ragdoll runtime and configuration; animation-to-physics handoff; physics-to-animation reset; replicated character physics state; ragdoll gate evidence
```

## Other gaps found

- The bodycam Reduced Motion profile has no runtime selection path.
- `docs/STW_PRODUCT_VISION.md` did not exist before this audit. `README.md`,
  `ENGINE_STACK.md` and `STORY_MODE_GDD.md` (a separate, older design document)
  describe engine and recovery state but not the product target.
- Asset provenance files do not carry a visual classification yet. Adding one to
  each is a follow-up.
