# Tetris Vibe SGDK - See what AI can do for you

Voll funktionsfähiger Tetris-Klon insporiert von Tetris Party Wii

Ein technisches Demonstrationsprojekt für das **Sega Mega Drive**, das die Symbiose zwischen Low-Level-Programmierung (SGDK 2.1.1) und KI-gestützter Entwicklung (Google Gemini) aufzeigt.

---

## 💻 Development Insight: AI Collaboration

Dieses Projekt wurde größtenteils durch die Interaktion mit **Gemini** entwickelt. Der Fokus lag hierbei auf:
* **Prompt Engineering:** Präzise Instruktionen zur Einhaltung von 68000-Alignment-Regeln.
* **Code Optimization:** Implementierung von Bitshift-Operationen zur Vermeidung von Divisionen bei u16/s16 Multiplikationen.
* **Architecture:** Strukturierung der Game-States und Trennung von Logik (`game_logic.c`) und Rendering (`game_view.c`).

### Technische Implementierung (SGDK 2.1.1)
* **Math:** Nutzung von `F16_toInt`, `FIX16(1.5)`, `F16_mul` und `F16_div`.
* **Memory:** Dynamische Context-Verwaltung zur Minimierung des RAM-Footprints.
* **Alignment:** Striktes Padding von Datenstrukturen zur Vermeidung von Address-Errors auf Original-Hardware.

---

## 🎮 Steuerung

Das Spiel verzichtet auf eine Pause-Funktion, um den Fokus auf das unmittelbare Gameplay zu legen.

| Taste | Aktion |
| :--- | :--- |
| **Steuerkreuz Links/Rechts** | Stück bewegen |
| **Steuerkreuz Unten** | Soft Drop |
| **Steuerkreuz Oben** | Hard Drop |
| **Button A** | Rotieren (Links/Rechts) |
| **Button B** | Rotieren (Links/Rechts) |
| **Button C** | Hold Function |
| **Start** | (Nicht belegt) |

---

## 🚀 Game Features

* **Highscores:** voll funktionoerende Highscore-Liste mit 10 einträgen.
* **Customizable:** Every aspect of the playmode, items, speed is adjustable ingame.
* **RNG-System:** 7-Bag Randomizer zur Vermeidung von "Droughts".
* **Curse Engine:** Zufällige Statuseffekte (No-Rotate, Speed-Skulls, Reversed Controls).
* **Ghost Piece:** Echtzeit-Berechnung der Landeposition.
* **Sprite Management:** Dynamische Priorisierung von Tetrominos gegenüber UI-Elementen.

---

## 📝 TODOs / Roadmap

- [ ] **Bugs:** Leidet seit der Sprite-Imtegration aktzell unter memory-leaks/crash, nächtes tile wird nicht korrelt dargestellt
- [ ] **Audio:** Integration von FM-Tracks und PCM-Samples für Line-Clears.
- [ ] **Stages:** Hintergrundwechsel und steigende Schwierigkeitskurven pro Level.
- [ ] **Versus Mode:** Implementierung einer VS CPU oder 2-Player Komponente.
- [ ] **UI Polish:** Erweiterte Animationen bei "Tetris"-Clears.

---

## 📖 Dokumentation & Wiki

Detaillierte Informationen zum Memory-Layout und den KI-Prompts:
> [👉 Zum Wiki (Platzhalter)](https://github.com/darkjoy2k2-crypto/tetris-vibe/wiki)

---

## 📜 Credits
* **Development:** Gemini (AI) & darkjoy2k2.
* **SOUND:** Sound effects are taken from the game "puyo puyo". Developed by Compile, created by Einosuke Nagao.
* **SDK:** SGDK by Stephane Dallongeville.
