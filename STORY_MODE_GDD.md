# WIRRWARR: BIOMASS-PROTOKOLL – Der perfekte Story-Modus
## Game Design Document · Akt-Struktur, Sucht-Loop, Tech-Plan

> Ziel: Ein Story-Modus, der **süchtig macht**, ohne Pay-to-Win, ohne Fett.
> Sitzungs-Länge pro Mission: **8–12 Minuten** ("nur noch eine" funktioniert auch nach Mitternacht).

---

## 1 · Die Sucht-Formel (Warum man weiterspielt)

Drei Belohnungs-Kanäle, die **jede Mission** gleichzeitig füttern:

| Kanal | Was | Sucht-Psychologie |
|---|---|---|
| **Power** | XP, Gen-Codes, Waffen/Attachment-Unlocks | "Ich werde stärker" |
| **Story** | Lore-Fragmente, Codex-Einträge, Begleiter-Dialoge | "Ich WILL wissen, was das ist" |
| **Mastery** | Medaillen, S-Ränge, Speedrun-Timer, NG+-Mutationen | "Ich will es PERFEKT machen" |

**Der Debrief ist der Dealer:** Nach jeder Mission zeigt der Endscreen immer
genau drei offene Hooks an: `Nächste Mission frei` + `1 Lore-Fragment entdeckt (2 fehlen)` + `S-Rang verfehlt um 0:14`.
Der Spieler sieht gleichzeitig Fortschritt UND Lücke → Weiterspielen.

**Pacing-Regel (CoD-Prinzip):** Nie länger als 20–30 s ohne neuen Beat
(Cinematic, Funk, Gegner-Welle, Objective-Wechsel, Pickup, Shake).

**Failure-Kultur:** Niederlage = `"SIMULATION NEUSTART"` in 2 s, Checkpoint
sofort, kein Menü-Marathon. Frust < 5 s, sonst churn.

---

## 2 · Die präzise Missions-Anatomie (CoD-Stil)

Jede Mission = 8 Beats, datengetrieben (alles existiert bereits als Engine-Feature):

```
BEAT 1  COLD OPEN      Engine-Cinematic (Kamera-Keys, Letterbox, Subtitles, Funk-Sound)   20–30 s
BEAT 2  BRIEFING       Objective-Card + optionaler Begleiter-Banter (1–2 Zeilen)
BEAT 3  ONBOARDING     Neue Mechanik der Mission in GEFAHRLOSER Variante lernen
BEAT 4  ESCALATION     Erstes Scripted-Event (Funk + Shake) + Gegnerdruck steigt
BEAT 5  MIDPOINT       Objective-Flip (Ziel wechselt / neue Info dreht die Mission)
BEAT 6  CLIMAX         Boss / Mini-Boss / finale Welle / Timer-Hölle
BEAT 7  COOL-DOWN      Loot-Beat: Codes + Lore-Fragment sichtbar einsammeln (Ritual!)
BEAT 8  DEBRIEF        Cinematic + Cliffhanger-Hook + Endscreen (3 Hooks, s.o.)
```

**Story-Trigger-Typen** (alle in der aktuellen Engine abbildbar):
`time` · `kills` · `destroyed` · `zone-held` · `pickup` · `hp-threshold` · `flag` (Entscheidung)

---

## 3 · Story: Drei Akte, zwölf Missionen + Prolog

**Prämisse:** Die "Biomass" ist keine Invasion – sie ist **Terraforming**.
Und jemand hat es bestellt.

### PROLOG · „Kontakt" (Tutorial, getarnt als Story)
- Absturz in Sektor 7. Bewegung/Sprint/Mantling = **Flucht vor einstürzender Trümmerzone** (Tutorial durch Panik, nicht durch Text).
- Erster Schuss = erste Waffe findet DICH (aus einem toten Soldaten → emotional).
- Erster **Sporen-Sicht-Effekt** (S1-Mechanik als Story-Moment: „Was… sehe ich da?").
- Cliff: VEGA funkt: „Du bist nicht der Einzige, der gelandet ist."

### AKT 1 · ERNTE (M1–M4, gebaut ✅) + M5-Twist
- M1–M4 wie implementiert (Ernte/Abriss/Stellung/Nest) – dienen als **Akt-1-Rampe**.
- **M5 „Die Koordinate"** (setzt M4-Cliffhanger fort): Die Koordinate führt zu einem
  **KORP-Labor** (Menschen!). Neue Mechanik: **Infiltration/Alarm** – lautlose
  Melee-Takedowns, Sichtkegel, Alarm = Wellen. Midpoint: Labor-Daten: Die Biomass
  wurde **ausgesetzt** – von der KORP, der Firma, die dich geschickt hat.
  **Cliff:** KOMMANDO ist nicht dein KOMMANDO. Funk wechselt die Seite.

### AKT 2 · SPIEGEL (M6–M9) – Du jagst deine eigenen Auftraggeber
- **M6 „Defector":** Eskortiere die KORP-Wissenschaftlerin DR. ILSE MAREN
  (neuer Begitter-Slot! → Escort-Mechanik: Befehle aus dem Taktik-Modus: WARTEN/FOLGEN/DECKUNG).
- **M7 „Zwei Fronten":** KORP-Mechs UND Biomass auf einer Karte –
  du lässt sie **gegeneinander laufen** (Aggro-Redirect als neue Taktik-Mechanik:
  Lockdrock wirft → Biomass greift Mech-Trupp an).
- **M8 „Das Labor brennt" – DIE ENTSCHEIDUNG:**
  Feuer-Alarm, Zeit läuft. **Entscheide in 10 s:**
  🔴 VEGA aus der Kapsel retten (Begleiter lebt, Daten weg) ODER
  🔵 Den Datenkern sichern (VEGA „stirbt", Akt 3 hat anderen Einstieg + andere Dialoge).
  → Persistenter Flag `save_vega` in localStorage → ändert M9–M12-Intros, Funk-Zeilen, Endings.
- **M9 „Spiegelbild":** 1-gegen-1 gegen **KADE**, einen Ex-Soldaten mit **deinem Loadout**
  (er nutzt DEINE Killstreaks gegen dich → Spiegel-Kampf, gelerntes wird geprüft).

### AKT 3 · NEST (M10–M12)
- **M10 „Der Garten":** Environmental-Storytelling pur: keine Gegner am Anfang.
  Die Map erzählt (Kapseln, Kinderzeichnungen, KORP-Propaganda). Dann: **die Ernte beginnt – andersherum.**
- **M11 „Belagerung":** Alle Begleiter (abhängig von M8!), alle Mechaniken,
  3-Phasen-Defense auf der brennenden Stahlwiege. **Mini-Boss: ERNTE-LÄUFER**
  (Panzerphase → Weakpoint nach Breschen-Sprengung = Destruction als Boss-Loop!).
- **M12 „DER GÄRTNER" (Finale + Boss):** KORP-KI, verschmolzen mit Biomass.
  **Boss-Phasen:** ① Schild-Pylone (sprengen = Destruction) → ② Add-Welle mit
  Taktik-Befehlen managen → ③ Enrage: Arena zerfällt live (Map-Destruction als
  Stage-Gimmick, Plattformen fallen weg).
  **Drei Endings:** Zerstören (frei, aber Erde stirbt) · Verschmelzen (Menschheit
  evolves, düster-schön) · **Geheim-Ending** nur mit 100 % Lore-Fragmenten
  (der Gärtner legt seine Waffe nieder – „Dann pflanze du.").

---

## 4 · Systeme, die den Modus tragen

### 4.1 Begleiter & Beziehungen
- VEGA (präzise, trocken) / JUNO (intuitiv, warm) / ab M6: DR. MAREN.
- **Banter-System:** kontextuelle Einzeiler (bei Headshot, bei Slide, bei Fehler des Spielers)
  → Welt fühlt sich lebendig an; Radio-Filter + Subtitles (existiert ✅).
- Beziehung = rein narrativ (Dialog-Varianten), keine Stat-Pflege → kein Chore-Gefühl.

### 4.2 Lore-Fragmente & Codex
- 3 Fragmente pro Mission, versteckt: ① an Off-Route-Position (Exploration),
  ② hinter optionaler Zerstörung (Destruction-Recherche!), ③ als Drop eines
  markierten Elite-Gegners (Risiko-Belohnung).
- Codex = Bestiarium + KORP-Akten; jeder Eintrag mit Audio-Fetzen (Funk-Noise) →
  Completion-Drag. 100 % = Geheim-Ending.

### 4.3 Ränge & Replay
- Ränge C/B/A/S pro Mission: Zeit + Tode + Fragmente + „Stil" (Melee/Headshot-Bonus).
- **NG+ „Mutationen":** nach Abschluss: pro Mission wählbare Modifikatoren
  (Biomass aggressiv / halbe Schild / nur Melee = „Eisen-Modus") mit eigenen Skins als Reward.

### 4.4 Entscheidungen & Konsequenz
- Nur **zwei große** Entscheidungen (M8 + M12), dafür spürbar überall:
  andere Funk-Zeilen, andere Begleiter-Kommentare, andere Endings.
  Kleine pseudo-Entscheidungen vermeiden (Entscheidungs-Müdigkeit).

---

## 5 · Tech-Plan (auf der bestehenden Engine)

| Feature | Status | Bau-Aufwand |
|---|---|---|
| Cinematics, Letterbox, Subtitles, Funk | ✅ gebaut | – |
| Trigger (time/kills/destroyed) | ✅ gebaut | + zone/pickup/hp/flag (klein) |
| Medaillen/Endscreen/XP/Loadout | ✅ gebaut | + S-Rang-Berechnung (klein) |
| Alarm/Infiltration (M5) | ❌ | Sichtkegel + Takedown = mittel |
| Escort + Aggro-Redirect (M6/M7) | ❌ | Taktik-Modus erweitern = mittel |
| Boss-Phasen (M11/M12) | ❌ | HP-Phasen + Weakpoints = mittel |
| Lore-Fragmente/Codex | ❌ | Pickup-System existiert → klein |
| Entscheidungs-Flags | ❌ | localStorage + Dialog-Varianten = klein |
| NG+-Mutationen | ❌ | Modifikator-Flags = klein |

**Empfohlene Bau-Reihenfolge:** ① Prolog + M5 (neue Mechanik: Infiltration) →
② Fragmente/Codex → ③ M6–M9 (Escort/Entscheidung) → ④ Bosse M10–M12 → ⑤ NG+.

---

*„Der perfekte Story-Modus ist kein Film zwischen den Kämpfen.
Er ist der Grund, warum du den Kampf noch einmal spielst."*
