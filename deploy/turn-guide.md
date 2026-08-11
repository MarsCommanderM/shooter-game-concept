# Voice-Relay (TURN) auf deinem Contabo – für strenge NATs

P2P-Voice reicht für ~90 % der Verbindungen. Für Firmennetze/strict-NAT:
eigener TURN-Server auf derselben VPS:

```bash
sudo apt install -y coturn
sudo nano /etc/turnserver.conf
```

Minimale Config:
```
listening-port=3478
realm=wirrwarr
user=wirrwarr:DEIN_SECRET
lt-cred-mech
fingerprint
```

```bash
sudo systemctl restart coturn
sudo ufw allow 3478/tcp
sudo ufw allow 3478/udp
```

Danach im Spiel: Online-Menü → 🎙 Voice-Settings:
- URL: `turn:DEINE-IP:3478`
- Username: `wirrwarr`
- Credential: `DEIN_SECRET`

Fertig – Voice geht jetzt durch jede Firewall. 🎙
