# WIRRWARR auf deinem iPhone (Termius + Safari)

## 1) Server starten (einmalig, ~2 Minuten)
1. Termius öffnen → Neuer Host:
   - Host: `169.58.152.88`
   - Username: `root`
   - Password: dein Contabo-VPS-Passwort (aus der Contabo-Mail)
2. Verbinden. Dann diese EINE Zeile pasten + Enter:

```bash
apt update && apt install -y git curl && git clone https://github.com/MarsCommanderM/shooter-game-concept.git && cd shooter-game-concept && bash deploy/setup.sh
```

3. Warten bis „FERTIG! Dein Spiel läuft auf: http://169.58.152.88:3000“

## 2) Als App aufs iPhone
1. Safari → `http://169.58.152.88:3000`
2. Teilen-Button (□↑) → „Zum Home-Bildschirm“
3. Fertig: WIRRWARR liegt als App-Icon auf deinem Screen.

## 3) Spielen
- App öffnen → Modus wählen → Touch-Controls: linker Daumen = Stick, rechter Bereich = schauen, FEUER/JUMP/DUCK-Buttons rechts.
- `/online` = mit Freunden (gleiche URL schicken).
- Voice: Mic-Freigabe erlauben → 🎙 MIC AN.

## Updates holen
In Termius: `cd shooter-game-concept && git pull && npm run build && pm2 restart wirrwarr`
