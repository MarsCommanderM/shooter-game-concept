# STW product vision

This is the single authoritative statement of what STW is meant to become.
Every agent and contributor reads it before making architecture, cleanup,
asset or documentation decisions. Current implementation status is **not**
recorded here; it lives in the dated audit reports under
[`docs/audits/`](audits/), latest
[`STW_PRODUCT_VISION_AUDIT_2026-09-18.md`](audits/STW_PRODUCT_VISION_AUDIT_2026-09-18.md).

## What STW is

STW is being developed as a high-end native 3D multiplayer FPS with a
hyper-realistic cinematic sci-fi presentation target. Temporary blockouts,
debug geometry, placeholder characters, placeholder materials and technical
visualizations are development tools only and are never the intended final
quality bar.

STW is not a blockout project, a tech demo or a low-fidelity prototype. It is a
production game with an AAA-quality target, built natively on O3DE (see
[`ENGINE_STACK.md`](../ENGINE_STACK.md)). Current shortcomings are tasks to
solve, not definitions of what STW is supposed to be.

Gameplay systems must be designed so that presentation quality can scale upward
without moving gameplay authority into rendering, camera, audio or VFX systems.

## Product pillars

Each pillar is a requirement. None of them is a claim that it exists today.

- Native O3DE production game.
- High-fidelity 3D FPS.
- Multiplayer-first, network-ready architecture.
- Realistic, cinematic visual target.
- High-detail physically based materials.
- High-quality Atom lighting, HDR, exposure and atmosphere.
- Realistic human-scale environments.
- Production-quality humanoid characters.
- First-person arms and weapon presentation.
- Responsive premium gunplay.
- Physical player movement.
- Bodycam-style first-person presentation (core identity, see below).
- Animation and physics integration.
- Character physical reactions.
- Ragdoll as a required final character-physics capability (see below).
- High-quality positional, environmental and weapon audio.
- Muzzle, impact and combat presentation.
- High-quality enemy presentation.
- PC first, with potential PlayStation and Xbox readiness.
- Production performance and scalability requirements.

## Visual quality target

The final rendering target is not represented by current debug or blockout
visuals. It includes believable physically based surfaces, high-quality
geometry, human anatomical silhouettes, realistic material response, controlled
HDR highlight rolloff, readable shadows, high-quality sky and IBL integration,
cinematic but gameplay-readable lighting, atmospheric depth, believable
first-person weapon rendering, high-quality animation, VFX and audio, and a
strong visual identity. Post-processing must not be used to hide poor assets or
broken lighting.

## Authority chain

```
AUTHORITATIVE GAMEPLAY
  -> PHYSICS / SIMULATION
    -> PRESENTATION DATA
      -> HIGH-FIDELITY VISUAL / AUDIO OUTPUT
```

Data flows one way. Rendering, camera, audio and VFX consume gameplay state and
never own it.

**Bodycam is presentation only.** It never owns authoritative aim, hit
validation, movement authority, PhysX authority or weapon acceptance. It
consumes synchronized snapshots and writes no gameplay state.

**Ragdoll is a required final capability.** An enum or state machine for
character physics is not a ragdoll. Native ragdoll exists only when a real
runtime adapter drives a verified ragdoll configuration on the character asset
and the transitions in and out of physics are proven at runtime. Until then it
is an open production requirement and must be reported as one.

## Visual content classification

Every visual asset, scene and character is classified with exactly one of:

| Class | Meaning |
|---|---|
| `TEMPORARY_DEBUG` | Debug overlays and geometry; delete freely. |
| `BLOCKOUT` | Shape and scale stand-ins. |
| `TECHNICAL_VALIDATION` | Exists to prove a pipeline or system works, not to look final. |
| `PRODUCTION_CANDIDATE` | Intended final content, not yet accepted. |
| `PRODUCTION_READY` | Accepted as final quality on visual and runtime evidence. |

No cube humanoid, primitive weapon, default material, test arena box, debug mesh
or placeholder character may be described as `FINAL`, `HIGH-FIDELITY`, `AAA` or
`PRODUCTION-READY` unless captured visual and runtime evidence supports the
claim.

## Truth rule

Never downgrade the product target to match the implementation. Keep the target
high and report the gap truthfully.

Status claims use one of `PRODUCT_REQUIREMENT`, `TARGET_CAPABILITY`,
`CURRENTLY_IMPLEMENTED`, `PARTIALLY_IMPLEMENTED`, `NOT_YET_IMPLEMENTED` or
`UNVERIFIED`, and follow the evidence standard in
[`AGENTS.md`](../AGENTS.md). For example, a missing native ragdoll is written
`RAGDOLL_PRODUCT_REQUIREMENT=YES`, `RAGDOLL_CURRENT_STATUS=<proven status>`,
never "ragdoll complete".

## Consequences for cleanup and refactoring

- Do not delete a system because a low-fidelity scene does not use it, if it
  supports a required production capability.
- Do not preserve obsolete placeholder architecture only because an old
  validator expects it.
- Do not add a competing vision or design document. Update this one.
