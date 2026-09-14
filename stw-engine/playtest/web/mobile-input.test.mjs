import assert from "node:assert/strict";
import test from "node:test";

import {
  MOBILE_LOOK_CONFIG,
  MobilePointerInput,
  POINTER_ROLE,
  mobileLookPixelsPerUnit,
} from "./mobile-input.mjs";

function near(actual, expected, epsilon = 1e-9) {
  assert.ok(Math.abs(actual - expected) <= epsilon,
    `expected ${actual} to be near ${expected}`);
}

test("look pointer claims without an initial camera jump and uses relative deltas", () => {
  const state = new MobilePointerInput();
  assert.equal(state.beginLook(20, 100, 200), true);
  assert.equal(state.roleOf(20), POINTER_ROLE.Look);
  assert.deepEqual(state.consumeLook(), { x: 0, y: 0 });

  const pixelsPerUnit = mobileLookPixelsPerUnit(390);
  assert.equal(state.updateLook(20, 100 + pixelsPerUnit / 2,
    200 - pixelsPerUnit / 4, 390), true);
  let look = state.consumeLook();
  near(look.x, 0.5);
  near(look.y, -0.25);

  assert.equal(state.updateLook(20, 100 + pixelsPerUnit * 0.75,
    200, 390), true);
  look = state.consumeLook();
  near(look.x, 0.25);
  near(look.y, 0.25);
});

test("look release clears ownership and pointercancel clears queued input", () => {
  const state = new MobilePointerInput();
  assert.equal(state.beginLook(3, 10, 20), true);
  state.updateLook(3, 25, 35, 390);
  assert.equal(state.releasePointer(3), true);
  assert.equal(state.roleOf(3), null);
  assert.notDeepEqual(state.consumeLook(), { x: 0, y: 0 });
  assert.deepEqual(state.consumeLook(), { x: 0, y: 0 });

  assert.equal(state.beginLook(4, 0, 0), true);
  state.updateLook(4, 30, 30, 390);
  assert.equal(state.releasePointer(4, { cancelled: true }), true);
  assert.equal(state.roleOf(4), null);
  assert.deepEqual(state.consumeLook(), { x: 0, y: 0 });
});

test("move, look, and fire pointers remain independent", () => {
  const state = new MobilePointerInput();
  assert.equal(state.beginMove(1), true);
  const movement = state.updateMove(1, 85, 25, {
    left: 0, top: 0, width: 100, height: 100,
  });
  assert.ok(movement.strafe > 0);
  assert.ok(movement.forward > 0);

  assert.equal(state.beginLook(2, 200, 100), true);
  assert.equal(state.beginButton(3, "fire"), true);
  assert.equal(state.updateLook(2, 225, 110, 390), true);
  assert.equal(state.roleOf(1), POINTER_ROLE.Move);
  assert.equal(state.roleOf(2), POINTER_ROLE.Look);
  assert.equal(state.roleOf(3), POINTER_ROLE.Fire);
  assert.equal(state.isHeld("fire"), true);
  assert.notDeepEqual(state.consumeLook(), { x: 0, y: 0 });
  assert.ok(state.movement.forward > 0);

  state.releasePointer(3);
  assert.equal(state.isHeld("fire"), false);
  assert.equal(state.roleOf(1), POINTER_ROLE.Move);
  assert.equal(state.roleOf(2), POINTER_ROLE.Look);
});

test("independent pointer IDs cannot steal occupied roles", () => {
  const state = new MobilePointerInput();
  assert.equal(state.beginMove(10), true);
  assert.equal(state.beginMove(11), false);
  assert.equal(state.beginButton(10, "fire"), false);
  assert.equal(state.beginLook(20, 0, 0), true);
  assert.equal(state.beginLook(21, 0, 0), false);
  assert.equal(state.beginButton(30, "sprint"), true);
  assert.equal(state.roleOf(10), POINTER_ROLE.Move);
  assert.equal(state.roleOf(20), POINTER_ROLE.Look);
  assert.equal(state.roleOf(30), POINTER_ROLE.Sprint);
});

test("lost focus reset clears movement, look, fire, and sprint", () => {
  const state = new MobilePointerInput();
  state.beginMove(1);
  state.updateMove(1, 90, 10, { left: 0, top: 0, width: 100, height: 100 });
  state.beginLook(2, 0, 0);
  state.updateLook(2, 30, 20, 390);
  state.beginButton(3, "fire");
  state.beginButton(4, "sprint");

  state.clearAll();
  assert.deepEqual(state.movement,
    { strafe: 0, forward: 0, knobX: 0, knobY: 0 });
  assert.deepEqual(state.consumeLook(), { x: 0, y: 0 });
  assert.equal(state.isHeld("fire"), false);
  assert.equal(state.isHeld("sprint"), false);
  assert.equal(state.roles.size, 0);
});

test("look sensitivity is viewport-aware and extreme event spikes are bounded", () => {
  assert.equal(mobileLookPixelsPerUnit(100),
    MOBILE_LOOK_CONFIG.minPixelsPerUnit);
  assert.equal(mobileLookPixelsPerUnit(2000),
    MOBILE_LOOK_CONFIG.maxPixelsPerUnit);

  const state = new MobilePointerInput();
  state.beginLook(9, 0, 0);
  state.updateLook(9, 10000, -10000, 390);
  const look = state.consumeLook();
  assert.equal(look.x, 1);
  assert.equal(look.y, -1);
  assert.deepEqual(state.consumeLook(), { x: 0, y: 0 });
});
