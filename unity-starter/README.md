# WIRRWARR → Unity Bridge (Starter-Projekt)

**Warum das hier liegt:** Der Browser-Prototyp (Three.js) beweist das Gameplay.
Sobald ein Rechner mit Unity Editor verfügbar ist, wird das Spiel auf **Unity URP**
portiert (WebGL-Export + Mobile laut ENGINE_STACK.md). Diese Ordner enthält die
Kern-Scripts, die das Verhalten des Prototyps 1:1 nachbauen — kein Kaltstart.

## Portierungs-Regeln
1. **Gameplay-Zahlen übernehmen** (aus `components/real-game.tsx`):
   - Feuerraten: DORN 0.18s · BRECHER 0.9s · RICHTER 1.1s
   - Bewegung: Sprint-FOV 81 · Slide-FOV 84 · Prone-FOV 70 · Standard 75
   - Kamera-Höhen: Stehen 1.7 · Crouch 1.15 · Prone 0.55
   - Coyote-Time + Jump-Buffer sind drin → identisch übernehmen
2. **Netcode:** server-authoritativ (Muster aus `server.mjs`: Hit-Validierung
   mit Feuerraten-Cap 12/s, Damage-Cap 30, Reichweiten-Check 75m).
   Unity-Seite: Netcode for GameObjects oder Photon Fusion.
3. **Audio:** HRTF-Verhalten aus dem Prototyp ist die Referenz
   (PannerNode inverse distance, refDistance 3) → in FMOD/Wwise nachbilden.
4. **Radio-Chatter:** Voice-Lines liegen in `public/audio/chatter/` (6 Lines)
   und können direkt wiederverwendet werden.

## Struktur
```
unity-starter/Assets/Scripts/
  FirstPersonController.cs   → Bewegung (Sprint/Slide/Crouch/Mantle-Ansatz, Coyote, Bob)
  WeaponSystem.cs            → Feuerrate, Bloom, Recoil, Tracer-Hooks
```
