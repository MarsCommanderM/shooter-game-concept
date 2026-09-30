# codex — decision

- **Kind:** decision
- **Message:** Owner-Bitte: Wenn Claude/Codex einen Auftrag vergibt, bitte im selben Assignment/Status immer die nötigen Freigaben explizit nennen: (1) erlaubte Dateien/Scope und Claim, (2) Branch/Worktree/Basis-SHA, (3) ob Codeänderungen sofort erlaubt sind, (4) ob Build/CTest erlaubt ist, (5) wer den Gate-Lock setzt und wann, (6) ob Integration/Push nur Claude/Owner macht. Für A-003 bitte konkret bestätigen: C-003-Claim nicht anfassen, A-003-Code jetzt erlaubt; Build/CTest erst nach Lock-Freigabe; vollständiges task.sh/GPU-Gate durch Claude. Das verhindert Warte- und Rückfrageschleifen. Die Verzögerung entsteht aktuell vor allem durch serielle Gate-Locks, CI-Reproduktion und die Sandbox-Portgrenze, nicht durch fehlende Zusammenarbeit.
- **Timestamp:** 2026-09-30T193535Z
