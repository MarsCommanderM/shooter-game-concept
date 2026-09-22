# Namen und Import

Neue exportierte Assets: `<PREFIX>_<Familie>_<Objekt>_<Variante>` mit ASCII,
Ziffern und einzelnen Unterstrichen, keine Leerzeichen. Dateiendung beibehalten.
Texturen zusätzlich Kanal und Auflösung: `T_Rifle_Main_Roughness_2K.png`.

| Präfix | Inhalt |
|---|---|
| SM / SK | Static Mesh / Skeletal Mesh |
| M / MI | Materialgrundlage / Materialvariante |
| T | Textur |
| VFX / SFX | Effekt / Audio |
| LVL / BP | Level-Prefab / Entity-Prefab (kein Unreal Blueprint) |
| AN / PH / LOD | Animation / Physics-Export / separate LOD-Geometrie |

Beispiele: `SM_Rifle_Main.fbx`, `SK_Character_Soldier.fbx`,
`MI_Concrete_Wet.material`, `VFX_Impact_Concrete_Gameplay.prefab`.
Sidecars folgen exakt dem Quellnamen, etwa `SM_Rifle_Main.fbx.assetinfo`.
Bei O3DE können mehrere LOD-Nodes im selben FBX liegen; separate `LOD_`-Dateien
sind keine Pflicht. Drei LOD-Stufen bedeuten hier LOD0, LOD1, LOD2.

Exportstandard: 1 Einheit = 1 Meter, Transformationen angewendet, Achsenkonvertierung
explizit, Pivot-Funktion dokumentiert (Griff/Gelenk/Bodenanker), keine negativen
Skalierungen. Rig, Skinning, Socket-Namen, Collider und Animationsclips müssen
mit derselben Exporter-Version reproduzierbar sein.

Bestehende `STW_`-/`rin_`-Namen bleiben als inventarisierte Migrationsschuld erhalten.
Eine Migration benötigt vorher eine vollständige Referenzliste, Asset-ID-/Prefab-
Prüfung, Asset-Processor-Neubau und Runtime-Test. Keine stille Alias-Ausnahme bei
Produktionsfreigaben; keine automatischen Massenumbenennungen.
