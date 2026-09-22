# Datenverträge und Vertrauensgrenzen

`forge.py` verändert keine Runtime-Assets. Seine Ausgabe belegt ausschließlich
die bezeichnete Prüfung. Binärdateien werden nicht durch vorgetäuschte Metadaten-
Erkennung als geprüft ausgegeben: FBX/GLB benötigen einen echten versionierten
DCC-/O3DE-Inspektionsbericht. Der Adapter dafür ist noch nicht implementiert.
OBJ-Dreieckszahlen werden zusätzlich direkt gelesen; vollständige Topologie-
Prüfung der bestehenden Arena bleibt bei `tools/assets/validate_stw_obj.py`.

## Gemeinsame Regeln

Alle referenzierten Pfade sind relativ zu `stw-o3de/`. Artefakte haben die Form
`{"path": "Build/VisualForge/run-001/ap.log", "sha256": "<64 hex digits>"}`.
Dateien müssen existieren, innerhalb der Wurzel bleiben und ihren Hash erfüllen.
Symlinks nach außen und unaufgelöste LFS-Pointer werden zurückgewiesen.
JSON-Schlüssel müssen eindeutig sein. Fehlende Daten und NaN sind kein PASS.

Hashes belegen Dateiintegrität, **nicht** die Wahrheit eines beliebig verfassten
Berichts. Exporter, Messadapter, Logs und menschliche Reviews sind die explizite
Vertrauensgrenze. CI-Berichte und Rohdaten für die exakte geprüfte Revision als
unveränderliche Artefakte archivieren. Test-Fixtures sind synthetisch und dürfen
niemals als Spielnachweis dienen.

## Mesh-Paketmanifest (`asset`)

Das Mesh-Paket umfasst die gesamte exportierte Abhängigkeitshülle: Primärmesh,
LOD-Exporte/Import-Sidecars, Materialien, Texturen, Collider und Animationen.
Alle Dateien gehören unter `Project/Assets/<zulässige Kategorie>/`.
Der Vertrag gilt zunächst für Mesh-Pakete; eigenständige Audio-/VFX-Pakete
brauchen spezialisierte Adapter. Die Namensinventur erfasst auch diese Dateien.

Pflichtfelder:

| Feld | Inhalt |
|---|---|
| `schema_version` | `1` |
| `owner` | verantwortliche Person/Rolle |
| `classification` | eine Klasse aus der Produktvision |
| `stage` | exakter Stufenname aus `policy.json` |
| `budget_class` | `weapon_fp`, `weapon_tp`, `character`, `environment`, `cinematic` |
| `primary` | Pfad innerhalb von `files` |
| `files` | nicht leere Liste eindeutiger Artefaktobjekte |
| `inspection` | gehashter JSON-Inspektionsbericht |
| `reviews` | bei Promotion: Art, Technical Art, Rendering, Multiplayer |

`content_id` ist SHA-256 über UTF-8-kodiertes kompaktes JSON der nach Pfad/Hash
sortierten Liste `[path, sha256]`; kanonische Implementierung: `forge.content_id`.
Der Inspektionsbericht enthält:

```json
{
  "content_id": "<content hash>",
  "tool": "<actual exporter>",
  "tool_version": "<exact version>",
  "metrics": {
    "triangles": 85000, "materials": 8, "texture_memory_mib": 180,
    "texture_edge": 2048, "scale_m_per_unit": 1
  },
  "lods": [
    {"node": "LOD0", "triangles": 85000},
    {"node": "LOD1", "triangles": 40000},
    {"node": "LOD2", "triangles": 12000}
  ],
  "checks": {
    "scale": "PASS", "pivot": "PASS", "uv": "PASS",
    "texture_channels": "PASS", "material_cost": "PASS", "lods": "PASS",
    "collision": "PASS", "target_level": "PASS", "network": "PASS"
  },
  "asset_processor": {
    "errors": 0, "warnings": 0,
    "log": {"path": "Build/VisualForge/run-001/ap.log", "sha256": "<hash>"}
  },
  "target_level": "Project/Levels/DefaultLevel/DefaultLevel.prefab"
}
```

Dies ist eine **Vertragsillustration**, kein Messergebnis. Texturwerte müssen aus
importierten Produkten inklusive Mips/Kompression stammen, nicht aus Dateigrößen.
`texture_channels` umfasst Auflösung, Farbraum und alle benötigten Kanäle;
`material_cost` Shader-/Passkosten. Nicht benötigte Kollision oder Netzwerklogik
darf erst nach dokumentierter Technical-Art-/Gameplay-Entscheidung PASS sein.
Zusätzliche Detaildaten und begründete Ausnahmen gehören in den gehashten Bericht.

## Scene-Capture (`scene`)

Pflichtfelder: `schema_version: 1`, `revision` (voller Git-SHA), `engine_commit`,
`policy_sha256`, `profiles_sha256`, `profile`, `effective_settings` (vollständiges
Profilobjekt aus dem Runtime-Readback), `gpu`, `cpu`, `driver`, `os`,
`build_configuration`, `capture_tool`, `connected_players`, `output_resolution`,
`scene` (gehashter getesteter Level), `effective_settings_log`, `multiplayer_log`,
`runs` (drei oder mehr unterschiedliche CSV-Artefakte).

CSV-Kopf:

```csv
time_s,frame_ms,cpu_ms,gpu_ms,draw_calls,vram_mib,ram_mib
```

Erwartete Revision wird unabhängig mit `--revision` angegeben. Für Promotion
enthält `asset_content_ids` alle tatsächlich im Lauf verwendeten Paketstände.
Der Level muss dem Ziellevel des Inspektionsberichts entsprechen.
Runtime-Logs müssen die Angaben belegen; das Werkzeug ersetzt keinen vertrauenswürdigen
Profiler-/Netzwerkadapter und führt selbst keinen Benchmark aus.

## Promotion (`promote`)

Nur `PRODUCTION_CANDIDATE`/`PRODUCTION_READY`. Zusätzlich zu den Asset- und
Scene-Gates benötigt `reviews` die Rollen `art`, `technical_art`, `rendering`,
`multiplayer`. Jede Rolle enthält `decision: APPROVED`, `reviewer`, `revision`,
`content_id` und `evidence` als gehashten Bericht/Kontaktbogen.
Ein Durchlauf gibt nur `ELIGIBLE_FOR_REVIEWED_PROMOTION` zurück. Integration,
Klassifikationsänderung und Deployment sind eigene kontrollierte Schritte.

## VFX-Vertrag und Backend-Grenze

Kategorien: Weapons, Impacts, Explosions, Smoke, Fire, Weather, Environment,
Blood, Destruction, Cinematic. Jedes Paket braucht Gameplay-, Cinematic- und
Low-Variante, Backend-Version, Seed, maximale Sichtweite, maximale Spawn-Zahl,
Lifetime sowie gemessene CPU-/GPU-Kosten pro Instanz und im Burst.

Gameplay publiziert Ereignis-ID, Effekt-ID, Transform, Zeit/Seed und Relevanz.
Client dedupliziert, cullt und wählt das lokale Präsentationsbudget. Entfernte
Effekte werden reduziert; Gameplay-Sichtblocker und Schaden bleiben serverseitig
gleich. Cinematic-Varianten werden nicht im kompetitiven Profil aktiviert.
Ein Fallback aus Mesh/Decal/Light/Audio muss pro Effekt festgelegt sein.
Open Particle System bleibt bis zur separaten Abnahme experimentell; kein
ungeprüftes Drittanbieter-Backend wird als installiert vorausgesetzt.

## Vertical Slice

Eine Map mit Innen-/Außenbereich, Korridor, Fernsicht, mehreren Sichtlinien,
dunkler Ecke, hellem Außenraum, nasser/verschmutzter Fläche, Vegetation,
Rauch, dynamischen Lichtern und einem zerstörbaren Objekt. Ein Gewehr, eine
Pistole, eine Granate, ein vollständiger Spieler plus zweiter Spieler/Gegner;
ein Wetter- und ein Zerstörungsszenario. Kein Fahrzeug ohne Gameplay-Begründung.
Erste Spielabnahme mit einem ausgewählten Lighting-Szenario; Materialtests
trotzdem unter allen drei Standardbeleuchtungen. Acht-Spieler-Test und stabile
Performance sind Pflicht vor Phase 10.
