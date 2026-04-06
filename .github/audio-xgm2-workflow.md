# XGM2 / Street-Fighter-Patch Workflow

Stand: 2026-04-05

## Ziel
Aus `py/sound/midi/tetris.mid` eine verwertbare Mega-Drive-Musik im `XGM2`-Format erzeugen, wobei FM-Instrumente aus den Street-Fighter-`VGZ`-Dateien übernommen werden.

## Relevante Dateien

### Runtime-Integration im Spiel
- `res/music.res` bindet die Musik über `XGM2 ... "*.vgm"` ein; `rescomp` kompiliert diese Quellen beim Build in das korrekte XGM2-Laufzeitformat
- `src/sound_manager.c` verwaltet `SOUND_playMusicById()` / `SOUND_stopMusic()` und startet die eingebetteten Daten mit `XGM2_play_FAR()`
- `src/states/sound_test.c` bietet getrennte SFX-/Musik-Auswahl


### Quellmaterial
- `py/sound/midi/tetris.mid`
- `py/sound/vgz/10 - Guile's Theme.vgz`
- `py/sound/vgz/07 - Ryu's Theme.vgz`
- `py/sound/vgz/12 - Ken's Theme.vgz`
- `py/sound/vgz/13 - Chun Li's Theme.vgz`
- `py/sound/vgz/22 - Balrog's Theme.vgz`
- `F:/Projekte/sgdk211/sample/snd/xgm-player/res/The Drum Run Rave.vgm`
- Kopie im Projekt: `res/music/sgdk_drum_run_rave.vgm`

### Python-Skripte
- `py/sound/build_guile_xgm2.py`
  - extrahiert YM2612-Patches aus einer `VGZ` oder `VGM`
  - rendert `tetris.mid` in eine erste brauchbare Fassung
- `py/sound/build_sf_punch_xgm2.py`
  - stärkeres SF-Arrangement mit Zusatzrhythmus
  - Ergebnis war klanglich zu dicht / zu aggressiv
- `py/sound/build_sf_clear_leads_xgm2.py`
  - derzeit beste Basis für die Street-Fighter-Variante
  - aktuelle Zuordnung: Hauptmelodie = Trompete, 2. Stimme = E-Gitarre
  - Begleitung: zurückgenommene Pads + FM-Drumbeat (Kick/Snare/Tom)
- `py/sound/build_sgdk_test_style_xgm2.py`
  - verwendet die FM-Instrumente aus `The Drum Run Rave.vgm`
  - baut daraus eine neue, sauberere Tetris-Fassung im SGDK-Sound-Test-Stil
  - aktuelle Abstimmung: leisere Gesamtlautstärke, stärkere Hauptstimme, leisere 2. Stimme, Intro-Pads, stampfender Beat und Vocal-ähnliche Rhythmus-Phrasen
- `py/sound/build_deflemask_dmf.py`
  - erzeugt ein DefleMask-`DMF`-Projekt aus `tetris.mid` plus extrahierten VGM-FM-Patches
  - Zielsystem: Mega Drive / Genesis (`DMF` System-ID `0x02`)
  - Output ist eine direkt in DefleMask ladbare Starter-Datei zum Nachbearbeiten

### Output-Dateien
- `py/sound/out/tetris_guile_arrangement.vgm`
- `py/sound/out/tetris_guile_arrangement.xgm2`
- `py/sound/out/tetris_sf_punch_mix.vgm`
- `py/sound/out/tetris_sf_punch_mix.xgm2`
- `py/sound/out/tetris_sf_clear_leads.vgm`
- `py/sound/out/tetris_sf_clear_leads.xgm2`
- `py/sound/out/tetris_vgm_cross.dmf`
- `py/sound/out/build_summary_deflemask_dmf.json`

## Aktuell beste Testfassung
Im ROM sind aktuell für den SGDK-Vergleich enthalten:
- `py/sound/out/sgdk_test_beat_only.vgm`
- `py/sound/out/sgdk_test_beat_only.xgm2`
- `py/sound/out/tetris_sgdk_beat_mix.vgm`
- `py/sound/out/tetris_sgdk_beat_mix.xgm2`

Der neue `SGDK MIX`-Track legt die Tetris-Melodie direkt auf das sauber extrahierte Beat-Fundament der Testdatei.

Zuweisung laut `build_summary_clear_leads.json`:
- Hauptmelodie: Track `[3, 2]` → `balrog_trumpet_lead`
- 2. Stimme: Track `[4, 3]` → `ryu_guitar_second`
- Drumbeat: `guile_drum_kick`, `chun_drum_snare`, `ken_drum_tom`
- Bass: Tracks `[7, 6]` und `[1, 0]` → `guile_bass_drive`

## Wichtige Erkenntnisse
1. **Die MIDI-Struktur darf nicht umarrangiert werden**, sonst leidet die Erkennbarkeit der Tetris-Melodie.
2. Der beste Ansatz ist aktuell:
   - Original-Noten aus `tetris.mid` beibehalten
   - nur die Instrumente gezielt ersetzen
3. **E-Gitarre für die Hauptstimme** trägt die Tetris-Melodie jetzt klar und mit kräftigem Anschlag.
4. **Trompeten als 2. Stimme** funktionieren besser, wenn hohe Lagen um eine Oktave entschärft werden.
5. Der treibende SF-Beat darf nur dezent über eine einzelne Drive-Gitarre kommen; zu dichtes Chugging kratzt sofort.
6. YM2612-Globalregister (`0x22`, `0x27`, `0x2B`) dürfen im Generator **nie** über channel-relative Registerwrites initialisiert werden. Das verursachte zuvor unerwünschte Schreibzugriffe auf `0x24/0x25` (Timer A) und führte zu einem permanenten Beep.

## Aktuelle Probleme / nächster Feinschliff
- Das Intro-"tam tam" und die 2. Stimme waren zwischenzeitlich zu scharf / kratzig.
- Die Begleitspuren wurden deshalb auf weichere Pads zurückgenommen.
- Hohe Töne der 2. Stimme werden nun entschärft.
- Ein **Loop** ist jetzt gesetzt.

## Umgesetzte Korrekturen
1. In `build_sf_clear_leads_xgm2.py`
   - `ryu_e_guitar_second` leiser gemacht
   - Begleitspuren auf `guile_support_pad` umgestellt
   - sehr hohe Noten der 2. Stimme abgemildert
2. Intro-Bereich unter ~`3.45s` entschärft
3. Loop nach dem Intro gesetzt bei `206 Frames` (`3.43s`)
4. VGM-Loopdaten werden jetzt direkt in `build_guile_xgm2.py` geschrieben

## Befehle zum Wiederholen

### 1) Erste Basisversion neu erzeugen
```powershell
py .\py\sound\build_guile_xgm2.py
```

### 2) Aktuell beste klare Lead-Version neu erzeugen
```powershell
py .\py\sound\build_sf_clear_leads_xgm2.py
```

### 3) DefleMask-Starterprojekt erzeugen
```powershell
py .\py\sound\build_deflemask_dmf.py
```

### 4) Debug-Build verifizieren
```powershell
cmd.exe /C "F:/Projekte/sgdk211\bin\make -f F:/Projekte/sgdk211\makefile.gen debug"
```

## Sound-Test im Spiel
- `UP/DOWN` wechselt zwischen `SFX` und `MUSIC`
- `LEFT/RIGHT` wählt die numerische ID
- `A` spielt den ausgewählten Sound / Track ab
- `B` oder `C` stoppt die aktuelle Musik
- Beim Verlassen von `STATE_SOUNDTEST` wird Musik gestoppt

## PC-Wiedergabe
- Player: `f:\Projekte\tools\VGMPlay\VGMPlay.exe`
- Zum Testen bevorzugt die `.vgm` öffnen, nicht die `.xgm2`

## Verifikation vom letzten Stand
- `xgm2tool -> tetris_sf_clear_leads.xgm2: ok`
- Dauer: `5864 frames / 97 seconds`
- Loop: `5658 frames / 94 seconds` ab `3.43s`
- Debug-Build `make debug`: erfolgreich
