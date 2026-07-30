"use client";

import { useSyncExternalStore } from "react";

export type GamePhase = "menu" | "playing" | "gameover";

export interface HudState {
  phase: GamePhase;
  health: number;
  maxHealth: number;
  score: number;
  kills: number;
  wave: number;
  enemiesRemaining: number;
  ammo: number;
  maxAmmo: number;
  reloading: boolean;
  hitMarkerAt: number;
  killMarkerAt: number;
  damageAt: number;
}

const initialState: HudState = {
  phase: "menu",
  health: 100,
  maxHealth: 100,
  score: 0,
  kills: 0,
  wave: 0,
  enemiesRemaining: 0,
  ammo: 30,
  maxAmmo: 30,
  reloading: false,
  hitMarkerAt: 0,
  killMarkerAt: 0,
  damageAt: 0,
};

/**
 * Lightweight external store so the HUD (React) can subscribe to game state
 * without forcing re-renders inside the R3F render loop.
 */
class GameStore {
  private state: HudState = { ...initialState };
  private listeners = new Set<() => void>();

  getSnapshot = (): HudState => this.state;

  subscribe = (listener: () => void): (() => void) => {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  };

  private emit() {
    for (const l of this.listeners) l();
  }

  set(partial: Partial<HudState>) {
    this.state = { ...this.state, ...partial };
    this.emit();
  }

  get() {
    return this.state;
  }

  reset() {
    this.state = { ...initialState };
    this.emit();
  }
}

export const gameStore = new GameStore();

export function useGameState<T>(selector: (s: HudState) => T): T {
  return useSyncExternalStore(
    gameStore.subscribe,
    () => selector(gameStore.getSnapshot()),
    () => selector(initialState),
  );
}
