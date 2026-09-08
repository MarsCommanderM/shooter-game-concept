# STW O3DE Player Vertical Slice V1

This slice makes `STWGameplay` the native C++ owner of the first O3DE player
and weapon state. Browser transport and the legacy OpenGL renderer are not
dependencies of the Gem.

## Legacy behavior inventory

| Legacy system | Decision | O3DE boundary |
|---|---|---|
| `FpsInput` | REIMPLEMENT WITH O3DE API | Native O3DE keyboard/mouse channels feed an immutable per-tick input value. |
| `WeaponSystem` | REUSE LOGIC | MP5 fire interval, damage, range, magazine, reserve and reload semantics are retained in `PlayerSliceModel`. |
| `GameRuntime` | PORT | Only deterministic ownership and update order move into `STWGameplay`; SDL/OpenGL/frame capture do not. |
| `CombatLoop` | DEFER | Bots and the combat sandbox are outside this slice. |
| Legacy renderer / browser bridge | REMOVE FROM FUTURE PATH | Atom renders the native camera and presentation. No browser gameplay is introduced. |

## Ownership

`PlayerSliceModel` owns player health/pose intent, MP5 state, the target state,
and authoritative hit results. `STWGameplaySystemComponent` is the adapter for
O3DE input, active-camera transforms, ticking, and Atom debug presentation.
Presentation consumes events but cannot mutate weapon or target state.

The deterministic model uses a fixed ground plane, bounded training arena and
axis-separated collision against the slice cover. This keeps unit tests free
of rendering. A PhysX Character Controller remains the required runtime
replacement when the authored production player entity is added; this slice
does not introduce a second general-purpose physics engine.

## Desktop controls

- `WASD`: movement
- Mouse: yaw/pitch look
- Left Shift: sprint
- Left mouse: fire
- `R`: reload

The fixed Lightning job builds only `STWGameplay.Tests` and the affected
`STW.GameLauncher` target, then verifies the real Atom/Vulkan/Tesla T4 path and
captures a native frame. Build artifacts, cache, logs and screenshots remain
outside Git.
