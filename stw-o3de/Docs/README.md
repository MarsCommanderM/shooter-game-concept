# Project Visual Forge

Reproduzierbare Visual Production Pipeline für STW. Maßgeblich bleibt die
[Produktvision](../../docs/STW_PRODUCT_VISION.md). Der AAA-Anspruch ist ein
Qualitätsziel; eine Freigabe benötigt überprüfbare Bild- und Laufzeitnachweise.

## Einstieg und täglicher Ablauf

1. Repository-Guard ausführen. Task nach `Config/VisualForge/Tasks/weapon_rifle.json`
   anlegen; Owner, Referenzen, Visual Target und Budgetklasse festlegen.
2. **Vor** Asset-/Level-Änderungen betroffene Dateien, erwartete Performancewirkung
   und Rollback im Task-Bericht dokumentieren. Danach den kleinen Scope bearbeiten.
3. Source → Blockout → Highpoly → Retopology → UV → Baking → Texturing → LODs.
   Bei nicht anwendbaren Schritten begründete Technical-Art-Entscheidung hinterlegen.
4. Deterministisch exportieren: Toolversion, Einheiten, Achsen, Exportoptionen,
   Dateihashes und Import-Sidecars festhalten. O3DE Import und Material Setup prüfen.
5. Asset Processor und Asset-Validierung ausführen. Warnungen blockieren die Freigabe.
6. Dieselbe Umgebung mit Day/Night/Overcast, festen Kameras und Seeds aufnehmen.
7. Drei Performance-Läufe nach Warmup; acht reale Verbindungen und effektive
   Einstellungen nachweisen. Gameplay-Rauch/Deckung bleiben über Qualitätsstufen konsistent.
8. Art, Technical Art, Rendering und Multiplayer prüfen dieselbe Revision und
   denselben Asset-Stand. Erst dann kontrolliert ins Produktionslevel übernehmen.
9. Bericht mit Tests, Exit-Codes, Messwerten, offenen Problemen und Rollback ablegen.

Kein ungeprüftes Editor-Tuning: Editor-Änderungen müssen in versionierten
Quelldateien/Presets reproduzierbar sein. Screenshots allein beweisen weder
Framerate noch Netzwerkverhalten. Ein CI-PASS der Werkzeuge ist keine Art-Freigabe.

## Kanonische Pfade

| Inhalt | Tatsächlicher Pfad unter `stw-o3de/` |
|---|---|
| O3DE-Projekt und CMake | `Project/project.json`, `Project/CMakeLists.txt` |
| Native Module | `Gems/STWGameplay/Code/` (bestehende Ownership erhalten) |
| DCC-Originale | `Source/{Blender,Substance,ZBrush,Houdini,Textures}/` |
| Exportierte Runtime-Assets | `Project/Assets/{Environments,Characters,Weapons,Materials,VFX,Audio,UI,Lighting,Cinematics,TestScenes}/` |
| Bestehende Umgebung/Gegner | `Project/Assets/Environment/`, `Project/Assets/Enemies/` |
| Levels | `Project/Levels/{Graybox,Lighting,Gameplay,Multiplayer,Cinematic}/` bei neuen Szenen |
| Richtlinien und Zielprofile | `Config/VisualForge/` |
| Validierung und Budget-Auswertung | `Tools/visual_forge/forge.py` |
| Bestehende Import-/Materialwerkzeuge | `../tools/assets/` |
| Bestehende Build-/Multiplayer-Gates | `../.github/lightning-t4/` |
| Bestehende Screenshot-Aufzeichnung | `../tools/evidence/capture_gameplay_sequence.sh` |
| Standards/Entscheidungen/Berichte | `Docs/` |
| Generierte Ergebnisse | `Build/VisualForge/<run-id>/` (ignoriert) |

Ordner werden bei echtem Inhalt angelegt. Bestehende Pfade/Asset-IDs werden nicht
pauschal umbenannt. `_Source` wird durch `Source/` außerhalb des Asset-Scanroots
abgebildet. Keine zweite CMake-Projektwurzel und keine leeren System-Gems.

## Ausführbare Prüfungen

Vom Repository-Root, Python 3.10+ ohne zusätzliche Pakete:

```bash
python3 stw-o3de/Tools/visual_forge/forge.py contracts
python3 -m unittest discover -s stw-o3de/Tools/visual_forge -p 'test_*.py' -v
python3 stw-o3de/Tools/visual_forge/forge.py --output stw-o3de/Build/VisualForge/run-001/inventory.json audit
python3 stw-o3de/Tools/visual_forge/forge.py asset /path/to/asset.manifest.json
python3 stw-o3de/Tools/visual_forge/forge.py scene /path/to/capture.json --revision <full-candidate-sha>
python3 stw-o3de/Tools/visual_forge/forge.py promote /path/to/asset.manifest.json /path/to/capture.json --revision <full-candidate-sha>
```

Exit 0 = die bezeichnete Prüfung bestanden, Exit 1 = fehlende/ungültige Daten
oder Budgetüberschreitung, Exit 2 = CLI-Aufruffehler. `audit` meldet Bestandsdaten
und Namensschulden; es erteilt keine Produktionsfreigabe. `promote` prüft
Freigabefähigkeit, verändert aber weder Klassifikation noch Dateien.
Die existierende geometrische Spezialprüfung bleibt zusätzlich erforderlich:
`python3 tools/assets/validate_stw_obj.py`.

[Datenverträge und Grenzen](TechnicalBible/evidence-contracts.md) ·
[Art Bible](ArtBible/README.md) · [Namensregeln](Naming/README.md) ·
[Budgets](PerformanceBudgets/README.md) · [Entscheidung](Decisions/0001-project-visual-forge.md)

## Rollout in der verlangten Reihenfolge

| Phase | Abnahmekriterium | Aktueller Stand |
|---|---|---|
| 01 Repository/Standards | Pfade, Source-Trennung, LFS-Regeln, Review/Rollback | Grundlage implementiert; Altbestandsmigration offen |
| 02 Art Bible | Verbindliche Stil-/Lesbarkeitsregeln, referenzierte Kontaktbögen | Regeln vorhanden; freigegebene Referenzbilder offen |
| 03 Material Library | StandardPBR-Familien, Texturkanäle, Lichtvergleich | Workflow definiert; finale Familien offen |
| 04 Lighting Framework | Native Preset-Anbindung + drei geprüfte Szenen | Zielrezepte vorhanden; Runtime-Anbindung offen |
| 05 Character/Weapon | Exporter, Rig, Animation, LOD, Collider, Physik | Datenvertrag vorhanden; DCC-Adapter/Asset-Abnahme offen |
| 06 VFX | Backend-Adapter, Varianten, Grenzen, Netzwerknachweise | Vertrag definiert; Backend-Abnahme offen |
| 07 Multiplayer Slice | Kleine vollständige Map und acht Spieler | Scope definiert; Acht-Spieler-Abnahme offen |
| 08 Automated Validation | Asset-, Import-, Bild-, Laufzeitprüfungen | Offline-Gates implementiert; Adapter/Bildabnahme offen |
| 09 Performance Profiles | Alle vier Profile wirksam und vermessen | Vier Ziele definiert; Runtime-Telemetrie offen |
| 10 Scaling | Mehrere freigegebene Assetpakete ohne Regression | Gesperrt bis Slice-Abnahme |

Gate-Infrastruktur wird früh gebaut, damit die vorhergehenden Phasen messbar
sind. Das zieht keine inhaltliche Freigabe späterer Phasen vor.

## Rollen und Branches

Art Pipeline verantwortet Exporte, Namen, Materialfamilien und Quellen.
Rendering verantwortet Frame Captures, effektive Profile und GPU-/Shaderkosten.
Gameplay verantwortet Waffen, Bewegung, Animation/Physik und Netzwerkautorität.
Build verantwortet Editor/Launcher/Server, Tests und archivierte Logs.
Das sind Verantwortungsbereiche; parallele Agenten sind dafür nicht erforderlich.

Die bestehenden Repository-Regeln gelten: Produktion ist
`brauny/stw-game-production`; Branches nur mit entsprechender Autorisierung.
Kein spontaner Wechsel auf `main`/`develop`, kein History-Rewrite für LFS.
Große Assetgruppen benötigen einen eigenen Review. Vor einem Push sicherstellen,
dass LFS installiert ist und alle benötigten Objekte verfügbar sind.
