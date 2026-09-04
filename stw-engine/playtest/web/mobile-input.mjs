export const POINTER_ROLE = Object.freeze({
  Move: "move",
  Look: "look",
  Fire: "button:fire",
  Sprint: "button:sprint",
});

export const MOBILE_LOOK_CONFIG = Object.freeze({
  viewportFraction: 0.12,
  minPixelsPerUnit: 44,
  maxPixelsPerUnit: 72,
  maxEventDeltaPixels: 96,
});

// One normalized remote unit is converted back to this many mouse counts by
// GameRuntime. Matching the browser pointer-lock scale keeps desktop-browser
// look close to native SDL mouse look while mobile uses viewport scaling.
export const REMOTE_LOOK_MOUSE_COUNTS_PER_UNIT = 48;

const HELD_BUTTONS = new Set(["fire", "sprint"]);

function clamp(value, minimum, maximum) {
  return Math.max(minimum, Math.min(maximum, value));
}

function validPointerId(pointerId) {
  return Number.isInteger(pointerId) && pointerId >= 0;
}

function finitePoint(x, y) {
  return Number.isFinite(x) && Number.isFinite(y);
}

export function mobileLookPixelsPerUnit(viewportShortSide) {
  const shortSide = Number.isFinite(viewportShortSide) && viewportShortSide > 0
    ? viewportShortSide
    : 390;
  return clamp(
    shortSide * MOBILE_LOOK_CONFIG.viewportFraction,
    MOBILE_LOOK_CONFIG.minPixelsPerUnit,
    MOBILE_LOOK_CONFIG.maxPixelsPerUnit,
  );
}

export class MobilePointerInput {
  constructor() {
    this.roles = new Map();
    this.movePointer = null;
    this.lookPointer = null;
    this.lookPrevious = null;
    this.pendingLookX = 0;
    this.pendingLookY = 0;
    this.movementState = { strafe: 0, forward: 0, knobX: 0, knobY: 0 };
    this.buttonPointers = new Map([
      ["fire", new Set()],
      ["sprint", new Set()],
    ]);
  }

  roleOf(pointerId) {
    return this.roles.get(pointerId) ?? null;
  }

  get movement() {
    return { ...this.movementState };
  }

  isHeld(field) {
    return (this.buttonPointers.get(field)?.size ?? 0) > 0;
  }

  beginMove(pointerId) {
    if (!validPointerId(pointerId) || this.roles.has(pointerId) ||
        this.movePointer !== null) {
      return false;
    }
    this.roles.set(pointerId, POINTER_ROLE.Move);
    this.movePointer = pointerId;
    return true;
  }

  updateMove(pointerId, clientX, clientY, bounds) {
    if (this.roleOf(pointerId) !== POINTER_ROLE.Move ||
        !finitePoint(clientX, clientY) || !bounds ||
        !Number.isFinite(bounds.left) || !Number.isFinite(bounds.top) ||
        !Number.isFinite(bounds.width) || !Number.isFinite(bounds.height) ||
        bounds.width <= 0 || bounds.height <= 0) {
      return null;
    }
    const radius = Math.min(bounds.width, bounds.height) * 0.38;
    let x = clientX - bounds.left - bounds.width / 2;
    let y = clientY - bounds.top - bounds.height / 2;
    const length = Math.hypot(x, y);
    if (length > radius) {
      x = x / length * radius;
      y = y / length * radius;
    }
    this.movementState = {
      strafe: x / radius,
      forward: -y / radius,
      knobX: x,
      knobY: y,
    };
    return this.movement;
  }

  beginLook(pointerId, clientX, clientY) {
    if (!validPointerId(pointerId) || this.roles.has(pointerId) ||
        this.lookPointer !== null || !finitePoint(clientX, clientY)) {
      return false;
    }
    this.roles.set(pointerId, POINTER_ROLE.Look);
    this.lookPointer = pointerId;
    this.lookPrevious = { x: clientX, y: clientY };
    return true;
  }

  updateLook(pointerId, clientX, clientY, viewportShortSide) {
    if (this.roleOf(pointerId) !== POINTER_ROLE.Look ||
        !this.lookPrevious || !finitePoint(clientX, clientY)) {
      return false;
    }
    const rawX = clientX - this.lookPrevious.x;
    const rawY = clientY - this.lookPrevious.y;
    this.lookPrevious = { x: clientX, y: clientY };
    this.addLookPixels(
      rawX,
      rawY,
      mobileLookPixelsPerUnit(viewportShortSide),
    );
    return true;
  }

  addLookPixels(deltaX, deltaY, pixelsPerUnit) {
    if (!finitePoint(deltaX, deltaY) || !Number.isFinite(pixelsPerUnit) ||
        pixelsPerUnit <= 0) {
      return false;
    }
    const boundedX = clamp(
      deltaX,
      -MOBILE_LOOK_CONFIG.maxEventDeltaPixels,
      MOBILE_LOOK_CONFIG.maxEventDeltaPixels,
    );
    const boundedY = clamp(
      deltaY,
      -MOBILE_LOOK_CONFIG.maxEventDeltaPixels,
      MOBILE_LOOK_CONFIG.maxEventDeltaPixels,
    );
    this.pendingLookX = clamp(this.pendingLookX + boundedX / pixelsPerUnit, -1, 1);
    this.pendingLookY = clamp(this.pendingLookY + boundedY / pixelsPerUnit, -1, 1);
    return true;
  }

  consumeLook() {
    const look = { x: this.pendingLookX, y: this.pendingLookY };
    this.pendingLookX = 0;
    this.pendingLookY = 0;
    return look;
  }

  beginButton(pointerId, field) {
    if (!validPointerId(pointerId) || !HELD_BUTTONS.has(field) ||
        this.roles.has(pointerId)) {
      return false;
    }
    this.roles.set(pointerId, `button:${field}`);
    this.buttonPointers.get(field).add(pointerId);
    return true;
  }

  releasePointer(pointerId, { cancelled = false } = {}) {
    const role = this.roleOf(pointerId);
    if (!role) return false;
    this.roles.delete(pointerId);
    if (role === POINTER_ROLE.Move) {
      this.movePointer = null;
      this.movementState = { strafe: 0, forward: 0, knobX: 0, knobY: 0 };
    } else if (role === POINTER_ROLE.Look) {
      this.lookPointer = null;
      this.lookPrevious = null;
      if (cancelled) {
        this.pendingLookX = 0;
        this.pendingLookY = 0;
      }
    } else if (role.startsWith("button:")) {
      this.buttonPointers.get(role.slice(7))?.delete(pointerId);
    }
    return true;
  }

  clearAll() {
    this.roles.clear();
    this.movePointer = null;
    this.lookPointer = null;
    this.lookPrevious = null;
    this.pendingLookX = 0;
    this.pendingLookY = 0;
    this.movementState = { strafe: 0, forward: 0, knobX: 0, knobY: 0 };
    for (const pointers of this.buttonPointers.values()) pointers.clear();
  }
}
