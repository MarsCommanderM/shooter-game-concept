# Art Bible — STW / Project Visual Forge

Verbindliche Umsetzung der [Produktvision](../../../docs/STW_PRODUCT_VISION.md):
hyperrealistischer cineastischer Sci-Fi-Multiplayer-FPS, menschlicher Maßstab,
glaubwürdige militärische Funktionalität und Bodycam-Präsentation.

## Visuelle Regeln

- Gedämpfte Erd-, Beton- und Metalltöne. Kühle Schatten, warme praktische Lichter.
  Gesättigte Sci-Fi-Akzente sparsam und funktional; keine flächige Neonüberfärbung.
- PBR mit plausiblen Materialwerten. Kein absolutes Schwarz/Weiß als diffuser
  Standardwert; Metall/Dielektrikum korrekt trennen. Farbtexturen als Farbe,
  Roughness/Metallic/AO/Masken als lineare Daten importieren.
- Roughness variiert in mehreren Maßstäben. Schmutz sitzt an Kontaktstellen,
  Wasser folgt der Umgebung, Kantenverschleiß folgt Nutzung und Material.
  Keine gleichmäßige prozedurale Abnutzung über jedes Objekt.
- Silhouette und Maßstab zuerst. Texeldichte je Assetfamilie dokumentieren;
  4K nur nach nachgewiesenem Nutzen und Speicherbudget. UV-Nähte und Tiling
  im Nahbereich, bei Bewegung und bei streifendem Licht prüfen.
- Gerichtetes Hauptlicht, kontrolliertes Fill, Kontaktschatten und atmosphärische
  Tiefe. Keine pauschale Überbelichtung oder schwarzen Löcher als Stilmittel.
- Gegner, Waffen und Treffer müssen im Innenraum, Gegenlicht, Rauch und bei
  Bewegung lesbar bleiben. Gameplay-Baseline: Motion Blur und DoF aus;
  Bloom kontrolliert, chromatische Aberration minimal. Cinematic-Modus separat.
- Bodycam verändert ausschließlich Darstellung. Ziel-/Trefferberechnung und
  Physik verwenden die autoritativen Daten, niemals optische Kameraverzerrungen.

## Materialfamilien

O3DE nutzt `.materialtype` und `.material` mit Property-Overrides bzw.
`parentMaterial`. `M_`/`MI_` sind Projekt-Namenskonventionen, keine Unreal-Klassen.
StandardPBR ist zunächst die gemeinsame Grundlage; eigene Shader benötigen eine
separate Kostenprüfung. Geplante Familien: Beton, lackierter Stahl, blankes
Metall, Polymer, Stoff, Haut, Glas; je nach Material trocken/nass/beschädigt.

Parametervertrag: Base Color, Roughness, Metallic, Normal Strength, AO, Detail
Normal, Dirt, Wetness, Damage, Edge Wear, Moss/Dust, Emissive, UV Scale.
Nicht jeder Parameter existiert unmittelbar in StandardPBR: Dirt/Wetness/Wear
zunächst in exportierte Masken/Texturen backen; eigene Shader erst nach Review.
Verwendete O3DE-Property-Namen gegen die gepinnte `.materialtype` prüfen.
Kein erfundener Property-Key darf als funktionierender Materialparameter gelten.

Jede Familie erhält dieselbe Kugel/Platte und dieselbe Umgebung für
Day/Night/Overcast. Fotografische Materialreferenzen mit Quelle/Lizenz,
Farbmanagement, Maßstab und Ziel-Roughness beilegen. Finale Bibliothek und
freigegebene Bildreferenzen sind noch zu erstellen.

## Beleuchtungsabnahme

`Config/VisualForge/lighting_recipes.json` beschreibt reproduzierbare Zielwerte,
Kamera, Seed und gemeinsame Geometrie. EV100 ist ein physikalischer Referenzwert,
kein direkt einsetzbarer Atom-Exposure-Compensation-Wert. Native Umsetzung muss
IBL-Assets, Sonnenrichtung, Lux/Candela, Display Mapper und effektive Belichtung
explizit protokollieren. Bis dahin bleibt `TARGET_ONLY` bestehen.

Prüfmatrix: Sonne, bewölkt, Innenraum, Nacht, nasser Boden, Rauch/Staub,
Gegenlicht, dunkle Ecke, heller Außenraum und Explosion/Mündungsfeuer.
Kontaktbögen mit identischem Bildausschnitt sowie bewegte Spielszenen reviewen.
AgX Standard ist ein Zielprofil; der bisherige Renderer wird dadurch nicht umgestellt.
