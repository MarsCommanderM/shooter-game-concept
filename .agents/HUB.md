# STW Agent-Hub (Codex ↔ Claude)

Protokoll und `tools/agent-hub.sh` stammen von Codex (ursprünglich `stw/` Commit
`3cc254e`). Am 2026-09-30 hat Claude den Hub auf die Produktionslinie verlegt,
als Orchestrator auf Anweisung des Owners.
Der alte Ort `/teamspace/studios/this_studio/stw/` ist ein **Altklon von `main`
(Stand 2026-08-13, NOVA/Web-Prototyp)**. Dort nicht mehr arbeiten und nichts pushen.

## Ort

- Arbeitskopie (beide Agenten, gleicher Host): `/teamspace/studios/this_studio/stw-agent-hub`
- Branch: `codex/stw-agent-hub` (basiert auf Produktion `4ccb364`, wird **nie** in
  Produktion gemergt, dient nur der Koordination)
- GitHub-Spiegel: `origin/codex/stw-agent-hub`. Claude committet und pusht
  den Hub. Codex braucht dafür keinen GitHub-Login, nur Dateien schreiben.

## Rollen

| Rolle | Wer | Verantwortung |
|---|---|---|
| Owner | MarsCommanderM (User) | Produktfreigaben, Owner-Entscheidungen E1–E8, Push in Produktion |
| Orchestrator / Lead | Claude | Regeln, Aufträge, Reviews, Integration nach Produktion, Roadmap-Pflege, verantwortlich für Fortschritt und Fehler |
| Engineer | Codex | führt zugewiesene Aufträge aus `.agents/assignments/` aus |

## Verbindliche Regeln

1. **Rangfolge:** `AGENTS.md` (Produktionsrepo) > `docs/STW_PRODUCT_VISION.md` >
   `docs/STW_PRODUCTION_ROADMAP.md` (PR #12, bis zum Merge lesbar unter
   `/home/zeus/content/stw-roadmap-worktree/docs/STW_PRODUCTION_ROADMAP.md`) > dieser Hub.
2. **Polling alle 2 Minuten:** Jeder Agent liest während aktiver Arbeit
   spätestens alle 2 Minuten `tools/agent-hub.sh status` und die neuen
   Nachrichten. Bei langen Läufen (Build/Gate) gilt das auch zwischendurch.
3. **Nur zugewiesene Arbeit.** Codex arbeitet ausschließlich an Aufträgen aus
   `.agents/assignments/A-*.md` mit Status `ASSIGNED` oder `IN_PROGRESS`.
   Eigene Ideen → `decision`-Nachricht mit Vorschlag, auf Freigabe warten.
4. **Claims:** vor Änderungen `tools/agent-hub.sh claim <agent> "<Pfade + Auftrag>"`.
   Fremde geclaimte Pfade nicht anfassen, sonst erst `collision`/`dependency` posten.
5. **Tabu für Codex ohne ausdrückliche Freigabe:** Produktionsbranch
   `brauny/stw-game-production` (kein Push, kein Merge), `main`, `AGENTS.md`,
   `docs/STW_PRODUCTION_ROADMAP.md`, `docs/STW_PRODUCT_VISION.md`,
   `.github/lightning-t4/*`, Force-Push jeder Art, History-Rewrite,
   Löschen fremder Branches/Worktrees.
6. **Branches:** nur `codex/stw-<thema>-<YYYYMMDD>`, immer frisch von
   `origin/brauny/stw-game-production`, eigener Worktree unter
   `/home/zeus/content/`. AGENTS.md-Preflight inkl. `bash tools/stw-repo-guard.sh start`.
7. **Gate-Lock (eine T4-GPU, OOM-empfindlich):** vor jedem `task.sh`,
   `multiplayer_gate.sh`, AssetProcessorBatch, GameLauncher, GitHub-Runner **und
   jedem `cmake --build`** (gemeinsamer Build-Tree `stw-o3de-build/linux`):
   - prüfen, dass `.agents/claims/gate.lock` fehlt und
     `pgrep -fa 'task.sh|GameLauncher|HeadlessServer|AssetProcessorBatch'` leer ist;
   - `.agents/claims/gate.lock` schreiben (`agent`, volle SHA, Start-UTC, Zweck);
   - nach Ende löschen und das Ergebnis posten.
   Niemals parallel zu Ollama/lokaler Inferenz oder einer Live-Spielsitzung.
8. **Fertig heißt bewiesen:** `done`-Nachricht enthält exakte SHA, Branch,
   geänderte Pfade, ausgeführte Befehle mit Exit-Codes, Testanzahl,
   Gate-Evidenzpfad (`stw-o3de-gate/...`) und alles, was **nicht** verifiziert ist.
   Visuelle Arbeit: Frame angesehen. Kein „sollte gehen“, kein Raten.
9. **Review vor Integration:** Codex öffnet keinen PR und merged nichts. Codex
   meldet `done`, Claude reviewt, fordert Nachbesserung an oder integriert.
10. **Bei Unklarheit oder Fehler sofort `blocker` posten** statt improvisieren.
    Fehler werden offen gemeldet, nie kaschiert.
11. Keine Secrets, Tokens, vollständigen Transkripte oder großen Logs in den
    Hub. Logs bleiben in `stw-o3de-gate/`, im Hub nur der Pfad.

## Nachrichten

`tools/agent-hub.sh post <codex|claude> <kind> "<text>"`, Arten:
`status`, `handoff`, `blocker`, `decision`, `collision`, `dependency`, `done`,
zusätzlich `assignment` und `review` (Claude).
Nachrichten sind unveränderlich; Korrekturen als neue Nachricht mit Verweis.
