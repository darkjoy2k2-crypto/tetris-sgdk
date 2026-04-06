# VS Club Mode Deep Analysis (No-Code-Change Pass)

Datum: 2026-04-03
Scope: src/states/vs_state.c + src/menu_bg.c (BG_MODE_CLUB)

## Zielbild
- VS Gameplay bleibt starr auf BG_A
- CLUB Background scrollt auf BG_B
- Keine BG_A-Scroll-Operationen im CLUB-Pfad

## Verifiziert (Ist korrekt)
1. VS setzt CLUB direkt beim State-Init
   - src/states/vs_state.c:1223
   - menu_bg_set_mode_instant(BG_MODE_CLUB)

2. VS löscht in init_draw nur BG_A
   - src/states/vs_state.c:1230
   - BG_B wird nicht mehr direkt gelöscht

3. VS Cleanup löscht nur BG_A
   - src/states/vs_state.c:1413

4. VS Board-Rendering schreibt auf BG_A
   - src/states/game/game_view.c:367
   - VDP_setTileMapDataRow(BG_A, ...)

5. Club-Map wird auf BG_B erzeugt
   - src/menu_bg.c:261
   - club_map = MAP_create(&club_bg_map, BG_B, baseAttr)

## Kritische Befunde (wahrscheinliche Ursachen)

### A) Union-Reset vor Club-Release (Lifecycle-Reihenfolge fehlerhaft)
- src/menu_bg.c:111
  - memset(&menuBg.mode, 0, sizeof(menuBg.mode));
- src/menu_bg.c:118
  - if ((bg_mode != BG_MODE_CLUB) && (club_map != NULL)) { MAP_release(club_map); }

Warum kritisch:
- club_map liegt in menuBg.mode.club.map
- Durch memset wird club_map vorher auf NULL gesetzt
- Danach kann MAP_release nicht mehr greifen
- Effekt: Club-Map-Lifecycle inkonsistent (Leak/invalid lifetime)
- Beobachtetes Symptom passt: sporadischer Speicherfehler/Crash nach mehreren Wechseln

### B) Mode-fremde Union-Reads im globalen Update-Pfad
- src/menu_bg.c:55
  - #define is_flashing (menuBg.mode.space.is_flashing)
- src/menu_bg.c:615
  - if (is_active || fade_level > F16_0 || is_flashing) {
- src/menu_bg.c:686
  - if (is_flashing) { ... writes flash_level/is_flashing ... }

Warum kritisch:
- Im CLUB-Mode zeigt dieselbe Union-Speicheradresse auf club.map/club.pos_x/etc.
- is_flashing liest dann SPACE-Byte über CLUB-Daten (Aliasing)
- Bei zufällig !=0 wird Flash-Block aktiv und schreibt SPACE-Felder in CLUB-Memory
- Effekt: CLUB-Datenkorruption (club_scroll, map pointer, vertikale Werte)
- Beobachtetes Symptom passt: Hintergrund bleibt starr, Spielfläche wirkt falsch/bewegt, teils später Crash

### C) Doppeltes Modell für target_dx/target_dy (shared + menu-union)
- src/menu_bg.h:
  - MenuBgMenuState enthält target_dx/target_dy
  - MenuBgState enthält ebenfalls target_dx/target_dy (shared)

Risiko:
- Semantische Verwechslung zwischen shared drift target und menu-spezifischem target
- Erhöht Wahrscheinlichkeit für erneute Cross-Mode Writes/Reads

## Zwischenfazit
- VS-State selbst wirkt aktuell weitgehend korrekt bzgl. Plane-Ownership.
- Hauptverdacht liegt in menu_bg CLUB-Update und Union-Zugriffen.
- Insbesondere Befund B ist hochkritisch und erklärt sowohl Stottern/Stillstand als auch sporadische Memory-Fails.

## Empfohlene nächste Fix-Reihenfolge (noch nicht umgesetzt)
1. Lifecycle-Fix: club_map Release vor Union-Reset sicherstellen
2. Union-Access-Fix: mode-fremde Makros aus globalen Bedingungen entfernen
3. CLUB-Update strikt mode-lokal halten (keine SPACE-Felder im CLUB-Pfad)
4. Konsolidieren: target_dx/target_dy eindeutig shared ODER menu-lokal
5. Danach nur Debug-Build + VS-Live-Test (mehrfache State-Wechsel)

## Keine Änderungen in diesem Analyse-Durchlauf
- Es wurden nur Lesetools verwendet
- Keine Code-/Headeränderung in src/ oder inc/
