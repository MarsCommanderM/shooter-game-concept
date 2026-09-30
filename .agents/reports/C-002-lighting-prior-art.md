# C-002: Lighting-Vorarbeit (Roadmap P1.7), Bestandsaufnahme

**Autor:** Claude, 2026-09-30, read-only. **Produktion:** `22c1ef0`.

## Befund 1: Die drei „Prior-Art“-Branches sind überholt, nicht offen

`codex/stw-cinematic-light-step1` (526a0f3), `codex/stw-sun-shadows` (023be75),
`codex/stw-diag-sun-4lux` (aea29c9) liegen 105–108 Commits hinter Produktion. Ihr
Inhalt ist in anderer Form **bereits in Produktion**:
- Sonnen-Shadowmap: `572cb3b fix(stw): give the sun a real shadow map (2048, PCF)`
- Cinematic Steps bis 9 (Accent-Lights als Practicals) in
  `stw-o3de/Gems/STWGameplay/Code/Source/EnvironmentPresentation.cpp:63-84`
- Regel `AccentLightsNeverOverpowerTheKeyLight` = Unit-Test in
  `Code/Tests/Clients/EnvironmentPresentationTests.cpp`
- Messprotokoll: `docs/audits/CINEMATIC_INTERIOR_V1_2026-09-19.md`, `..._V2_...md`

**Nicht erneut bauen.** Die Branches nicht löschen (Archiv), aber auch nicht mergen.

## Befund 2: Konvention „skalierte Lux“ vs. physikalische Rezepte

Die Szene ist auf **Sonne 25 lux** kalibriert (Accents 16–21 lux am Boden, Test
erzwingt Accent < Sonne). `Config/VisualForge/lighting_recipes.json` gibt
physikalische Werte vor (`sun_lux` 100000 / 10000 / 0.2, `exposure_ev100` 15/12/1).
Der Versuch vom 2026-09-25, diese 1:1 einzusetzen, ergab eine ausgebrannte
bzw. schwarze Szene (verworfen, siehe Memory/Roadmap). Außerdem gemessen (V2 7c):
**manuelle Exposure-Kompensation bewegt das Bild nicht**, der HDRI-Exposure-Trim schon.

Folge: Der Runtime-Adapter ist keine Feldzuweisung, sondern eine
**Architekturentscheidung**: entweder (a) Rezepte in die Szenen-Konvention
umrechnen (Faktor + dokumentierte Begründung) oder (b) die Exposure-Kette
physikalisch machen (Auto-/Manual-Exposure wirksam, dann alle Lichter neu
kalibrieren). (b) entspricht dem AAA-Ziel („controlled HDR highlight rolloff“),
ist aber deutlich größer. Entscheidung durch Claude vor jeder Implementierung,
zuerst per isoliertem Test der Atom-Exposure-Mathematik
(`ExposureControlSettings`, `PhotometricValue.h ConvertIntensityBetweenUnits`).

## Befund 3: Dokumentierte nächste Hebel (Audit V2 „Still open“)

1. Reflection Probes gegen die Himmelsspiegelung auf dem Deck bei flachen Winkeln
2. Diffuse Probe Grid (Gem aktiv, ungenutzt) für gebackenes/dynamisches Ambient
3. Schatten für die Practical-Point-Lights, Emissive-Strips
4. Ungeklärter sporadischer Launcher-Abbruch mit Schatten (gdb/Core-Dumps aktiviert)

## Plan P1.7 (Reihenfolge)

1. Exposure-Mathematik isoliert klären → Entscheidung (a)/(b) als `decision` im Hub
2. Reflection Probe(s) im Industrial Yard, je Schritt Gate + Frame-Metriken wie V2
3. Diffuse Probe Grid
4. Rezept-Adapter Day, dann Night/Overcast
Jeder Schritt: eine Variable, Gate auf exakter SHA, Frame angesehen, Metriken
(mean/RMS/clipped/crushed/fps) tabelliert wie in Audit V2.
