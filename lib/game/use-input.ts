"use client";

import { useEffect } from "react";
import { input } from "./shared";

/**
 * Attaches keyboard + pointer-lock mouse listeners while the game is playing.
 * All state is written into the shared `input` object (no React re-renders).
 */
export function useInput(enabled: boolean, canvas: HTMLElement | null) {
  useEffect(() => {
    if (!enabled) return;

    const onKey = (down: boolean) => (e: KeyboardEvent) => {
      switch (e.code) {
        case "KeyW":
        case "ArrowUp":
          input.forward = down;
          break;
        case "KeyS":
        case "ArrowDown":
          input.backward = down;
          break;
        case "KeyA":
        case "ArrowLeft":
          input.left = down;
          break;
        case "KeyD":
        case "ArrowRight":
          input.right = down;
          break;
        case "ShiftLeft":
        case "ShiftRight":
          input.sprint = down;
          break;
        case "KeyR":
          if (down) input.reload = true;
          break;
        default:
          return;
      }
      e.preventDefault();
    };

    const keyDown = onKey(true);
    const keyUp = onKey(false);

    const onMouseMove = (e: MouseEvent) => {
      if (document.pointerLockElement) {
        input.mouseDX += e.movementX;
        input.mouseDY += e.movementY;
      }
    };

    const onMouseDown = (e: MouseEvent) => {
      if (e.button === 0) {
        input.shooting = true;
        input.firePressed = true;
      }
    };
    const onMouseUp = (e: MouseEvent) => {
      if (e.button === 0) input.shooting = false;
    };

    window.addEventListener("keydown", keyDown);
    window.addEventListener("keyup", keyUp);
    window.addEventListener("mousemove", onMouseMove);
    window.addEventListener("mousedown", onMouseDown);
    window.addEventListener("mouseup", onMouseUp);

    return () => {
      window.removeEventListener("keydown", keyDown);
      window.removeEventListener("keyup", keyUp);
      window.removeEventListener("mousemove", onMouseMove);
      window.removeEventListener("mousedown", onMouseDown);
      window.removeEventListener("mouseup", onMouseUp);
      input.forward = input.backward = input.left = input.right = false;
      input.sprint = input.shooting = false;
      input.reload = input.dodge = false;
    };
  }, [enabled, canvas]);
}
