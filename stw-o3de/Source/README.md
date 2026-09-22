# Authoring sources

Originaldateien gehören hierher, außerhalb von `Project/Assets`:
`Blender/`, `Substance/`, `ZBrush/`, `Houdini/`, `Textures/`.
Unterordner bei echtem Inhalt erstellen. Lizenz, Herkunft, Toolversion,
Einheiten, Exportkonfiguration und Hash im zugehörigen Asset-Task dokumentieren.
Runtime-Exporte kommen nach `Project/Assets/`; Import-Sidecars werden versioniert.
Backups, temporäre Simulation-Caches und Build-Produkte nicht einchecken.
Die Repository-`.gitattributes` enthält LFS-Regeln für neue Source-Binärdateien.
