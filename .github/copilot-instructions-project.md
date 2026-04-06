# TETRIS-SGDK · PROJEKTINDEX FÜR KI

## 0. AUTORITÄTSREIHENFOLGE
- Wahrheit: `inc/states/states.h` → `src/main.c` → Modul-Header in `inc/` → Modul-Implementierungen in `src/`
- README ist **nicht** autoritativ; enthält teils veraltete Aussagen (`MEM_alloc`-Persistenz, Musikstatus, Roadmap)
- Für Architekturfragen: Code > Kommentare > README
- Für Typen/Enums/Flags/Offsets: immer Header zuerst

## 1. ORIENTIERUNG & SUCHE
Repo-Map:
- `src/main.c` → Boot, State-Machine, Fades, Main-Loop, `config`, `sctx`, globale State-Tabelle
- `inc/states/states.h` → `GameState`, alle Context-Typen, `StateUnion`, `Serializable`, `RuntimeConfig`, `GlobalConfig`, Flags
- `src/states/save_manager.c` + `inc/states/save_manager.h` → SRAM-Lesen/Schreiben, `STATE_SAVE`, Magic/Version, Verify
- `src/sound_manager.c` + `inc/sound_manager.h` → XGM2-only Audio, 99 WAV-Mapping, Sound-Flags
- `src/sprite.c` + `inc/sprite.h` → zentrale Sprite-Engine, GameSprite, Dust/Explosion, Effekt-Sprites
- `src/menu_bg.c` + `inc/menu_bg.h` → BG_B-Manager, Modi, Parallax, Palette-Freeze, Riistar/Club/Space/Menu
- `src/gfx.c` + `inc/gfx.h` → projektspezifische Zusatztiles (`tile_skull`, `tile_heart`), Font/Palette-Init
- `src/states/game.c` → Singleplayer-State-Orchestrierung
- `src/states/game/game_logic.c` → Spawn, Collision, Lock, Lines, Bag, Items, Garbage, Spezialeffekte
- `src/states/game/game_controls.c` → Input/DAS/Rotation/Hold/Drop
- `src/states/game/game_view.c` → Board-Rendering, UI, Preview/Hold, Goal/Tutorial-Panel
- `src/states/game/conditions.c` + `inc/states/game/game_conditions.h` → regelbasierter Runtime-Satz für Freegame/Challenge
- `src/states/game/quests.c` + `inc/states/game/quests.h` → Tutorial-Quest-Aktionen
- `src/states/vs_state.c` + `src/states/vs/brain.c` → VS-Mode + CPU-KI
- `res/*.h` / `res/*.res` → Ressourcen-Compiler-Outputs / SGDK-Ressourcen

Suchstrategie:
1. Typ/Enum/Flag? → `inc/states/states.h`
2. State-Übergang/Fade/Main-Loop? → `src/main.c`
3. Persistenz? → `src/states/save_manager.c`
4. Freegame-Regeln? → `src/states/game/conditions.c` + `game_logic.c`
5. Rendering? → `src/states/game/game_view.c`
6. Input? → `src/states/game/game_controls.c`
7. VS/CPU? → `src/states/vs_state.c` + `src/states/vs/brain.c`
8. Audio? → `src/sound_manager.c`
9. Sprites? → `src/sprite.c`
10. BG_B/Parallax/Freeze? → `src/menu_bg.c`

## 2. BUILD/RUN
- Build-Workspace: `f:/Projekte/tetris/tetris-sgdk`
- Tasks vorhanden:
  - `Genesis: Debug & Run (Gens KLog)`
  - `Genesis: Release & Run (BlastEm)`
  - `clean`
  - `Tile Check`
  - `Transform_csv`
  - `smartfix`
  - `py Colors`
- SGDK über `%GDK%`
- Output-ROMs:
  - `out/tetr-vibe-debug.gen`
  - `out/tetr-vibe.gen`
- Pflicht-Policy für KI nach Codeänderungen:
  - Immer Debug-Build ausführen (`make debug` bzw. Task `Genesis: Debug & Run (Gens KLog)`; Emulatorteil nur wenn gefordert).
  - Build-Output im Terminal auf Fehler prüfen.
  - Bei Fehlern: Ursache beheben, Debug-Build erneut ausführen; Schleife bis Build erfolgreich.
  - Arbeit erst nach erfolgreichem Debug-Build abschließen.

## 3. STATE-MASCHINE / BOOT / KERNEL
`GameState`:
- `STATE_NONE`
- `STATE_TITLE`
- `STATE_SELECT`
- `STATE_GAME`
- `STATE_VS`
- `STATE_SOUNDTEST`
- `STATE_GAMEOVER`
- `STATE_HIGHSCORE`
- `STATE_OPTIONS`
- `STATE_SAVE`
- `STATE_CHALLENGE`
- `STATE_GFXTEST`

State-Mapping (`src/main.c:initStateMachine`):
- `TITLE` → `title_*`
- `SELECT` → `select_*`
- `GAME` → `game_*`
- `VS` → `vs_state_*`
- `SOUNDTEST` → `sound_test_*`
- `GAMEOVER` → `gameover_*`
- `HIGHSCORE` → `highscore_*`
- `OPTIONS` → `options_*`
- `SAVE` → `saving_*`
- `CHALLENGE` → `challenge_*`
- `GFXTEST` → `gfxtest_*`

State→Context:
- `TITLE` → `TitleContext`
- `SELECT` → `SelectContext`
- `GAME` → `GameContext`
- `VS` → `VsContext`
- `SOUNDTEST` → `SoundTestContext`
- `HIGHSCORE` → `HighscoreContext`
- `OPTIONS` → `OptionsContext`
- `SAVE` → `SaveContext`
- `CHALLENGE` → `ChallengeContext`
- `GFXTEST` → `GfxTestContext`
- `GAMEOVER` → kein eigener Context in `StateUnion`

State→BG-Modus:
- `TITLE` → `BG_MODE_MENU`
- `SELECT` → `BG_MODE_MENU`
- `OPTIONS` → `BG_MODE_MENU`
- `SOUNDTEST` → `BG_MODE_MENU`
- `HIGHSCORE` → `BG_MODE_MENU`
- `GAMEOVER` → `BG_MODE_MENU`
- `SAVE` → `BG_MODE_MENU`
- `GFXTEST` → `BG_MODE_MENU`
- `CHALLENGE` → `BG_MODE_SPACE`
- `GAME` → `BG_MODE_RIISTAR`
- `VS` → `BG_MODE_CLUB`

Boot (`src/main.c`):
1. `JOY_init()`
2. `sctx = MEM_alloc(sizeof(StateUnion))`
3. `memset(sctx,0,sizeof(StateUnion))`
4. PAL/NTSC Bildschirmhöhe setzen
5. Font laden, Paletten schwarz
6. `initHighscores()`
7. `initStateMachine()`
8. `menu_bg_init()`
9. `start_boot_sequence()` → Boot-Fade auf BG
10. `SOUND_init()`
11. `SOUND_playMusic()`
12. Start in `STATE_SAVE` mit `config.sramop = SRAM_INIT`

Frame-Loop (`src/main.c`):
1. `joyState = JOY_readJoypad(JOY_1)`
2. Bei State-Wechsel: Fade-Out → `cleanup` → `memset(sctx)` → `init` → `init_draw` → Fade-In
3. `states[currentState].update()`
4. `states[currentState].draw()`
5. `menu_bg_update()`
6. `update_ui_fade_freeze()`
7. `lastJoyState = joyState`
8. `SYS_doVBlankProcess()`

Fade-Policy:
- Fullscreen-Fade bei `TITLE↔GAME` und `TITLE→CHALLENGE`
- UI-Fade für Menü-/Substates
- `menu_bg_set_palette_frozen(TRUE/FALSE)` schützt BG-Palette während UI-Fade
- `game` macht eigenen Start-Fade; main überspringt globalen Fade-In beim Eintritt in `STATE_GAME`

## 4. SPEICHER / GLOBALCONFIG / SAVE
`StateUnion` einmalig allokiert, danach nur wiederverwendet
- State-Daten niemals separat freigeben
- Jeder State bindet lokalen `ctx = &sctx-><slot>`
- State-Wechsel setzt komplette Union auf 0

`GlobalConfig`:
- Persistenter Teil: `Serializable serializable`
- Anonymer Mirror-Struct für Direktzugriff (`config.speedLevel` statt `config.serializable.speedLevel`)
- Nicht persistent:
  - `RuntimeConfig runtime`
  - `GameState preferredState`
  - `SramOp sramop`

`RuntimeConfig`:
- `gameMode` → `GAME_MODE_FREEGAME` oder `GAME_MODE_CHALLENGE`
- `challengeLevelId`
- `challengeResult` → `NONE SUCCESS FAIL`

Systemflags (`config.flags`):
- `FLAG_SHADOW`
- `FLAG_HOLD`
- `FLAG_NEXT`
- `FLAG_SOUND`
- `FLAG_MUSIC`
- `FLAG_BG`
- `FLAG_DEBUG`

SRAM-Layout:
- `SAVE_MAGIC = 0x54455452`
- `SAVE_VERSION = 0x0000014C`
- `ADDR_MAGIC = 0x00`
- `ADDR_VERSION = 0x04`
- `ADDR_OPTIONS = 0x10`
- SRAM-Fenster: 512 Bytes

Persistiert in `Serializable`:
- `currentScore`
- `playerName[4]`
- `randMode`
- `speedLevel`
- `garbageFreq`
- `itemMode`
- `flags`
- `thresholdLRInitial`
- `thresholdLRRepeat`
- `thresholdSD`
- `highscores[10]`
- `challenge_unlocked[4]`
- `challenge_cleared[4]`

Nicht persistent:
- alle State-Contexts
- alle Timer/Cursor/Animationen
- `runtime.gameMode`, `runtime.challengeLevelId`, `runtime.challengeResult`
- `preferredState`, `sramop`

Save-Flow:
- Boot: `STATE_SAVE + SRAM_INIT` → `save_init()` → `save_load()` oder `save_clear()`
- Save: beliebiger State setzt `config.sramop = SRAM_SAVE`, `config.preferredState = STATE_X`, `currentState = STATE_SAVE`
- `save_execute()` schreibt Header + gesamten gepackten `Serializable`-Block byteweise
- `save_verify()` Pflicht; Bytevergleich RAM vs SRAM
- Bei Fehler: Save-State bleibt sichtbar, User quittiert

Defaults (`save_clear()`):
- `playerName = ABC`
- `speedLevel = 3`
- `garbageFreq = 3`
- `itemMode = 1`
- `flags = SHADOW|HOLD|NEXT|MUSIC|SOUND|BG`
- `thresholdLRInitial = 6`
- `thresholdLRRepeat = 2`
- `thresholdSD = 3`
- Challenge-Bitfelder 0
- Top10 mit PET/SGK/CPU/VDP/ACE/SKY/DAN/EVA/MAX/JOE

## 5. GETEILTE MANAGER
Audio:
- nur `SOUND_init()` lädt Treiber: `Z80_loadDriver(Z80_DRIVER_XGM2, 0)`
- `SoundEntry` aligned(2)
- `sfx_bank[100]` mappt `WAV_001..WAV_099`
- `SOUND_play(event)` → `XGM2_playPCM(..., SOUND_PCM_CH_AUTO)`
- Sound nur wenn `FLAG_SOUND` gesetzt
- Musik nur wenn `FLAG_MUSIC` gesetzt
- `SOUND_playMusic()` startet den Standardtrack aus `res/music.res`
- `SOUND_playMusicById(id)` startet einen eingebetteten Musiktrack numerisch (Sound-Test)
- `res/music.res` verwendet für Musik `XGM2`-Ressourcen aus `.vgm`-Quellen; keine rohen `BIN`-VGMs und kein alter `XGM`-Pfad mit `Z80_DRIVER_XGM2`
- `SOUND_getMusicCount()` / `SOUND_getMusicName(id)` versorgen die Sound-Test-UI
- `SOUND_stopMusic()` → `XGM2_stop()`

Wichtige `SoundEvent` IDs:
- `SND_MENU_SELECT=53`
- `SND_MOVE=21`
- `SND_ROTATE=22`
- `SND_SOFT_DROP=15`
- `SND_HARD_DROP=16`
- `SND_PIECE_LOCK=18`
- `SND_LINE_CLEAR=91`
- `SND_TETRIS=36`
- `SND_LEVEL_UP=90`
- `SND_GAME_OVER=40`
- `SND_HOLD=13`
- `SND_COMBO=52`
- `SND_GARBAGE=99`
- `SND_GOOD_ITEM=15`
- `SND_ALERT=87`
- `SND_BAD_ITEM=15`
- `SND_RESET=36`

Sprites:
- Autorität: `inc/sprite.h` + `src/sprite.c`
- `gameSprites[4]` → `INDEX_PIECE SHADOW NEXT HOLD`
- `DustParticle dustParticles[DUST_SLOT_COUNT]`
- zentrale Engine-Init in `sprites_init()`
- pro Frame `sprites_update()`
- finaler Flush in main via `SPR_update()`

`GameSprite` Felder:
- `Sprite* vdpSprite`
- `s16 x y offsetX offsetY`
- `u16 frame animation animTimer stateTimer attr`
- `s16 animDir`
- `u8 type padding`

Sprite-Typen:
- `SPRITE_TYPE_NOROTATE`
- `SPRITE_TYPE_SKULL`
- `SPRITE_TYPE_SPIRAL`
- `SPRITE_TYPE_SPEED`
- `SPRITE_TYPE_DUST`

Partikel:
- Dust + Explosion teilen Slotpool
- DUST-Konstanten in `sprite.h` sind kanonisch
- Cleanup typischerweise durch Offscreen-Position `(-128,-128)` statt Zerstörung

BG-Manager:
- `menu_bg` ist exklusiver Besitzer von `BG_B`
- Speicherarchitektur: gemeinsamer Basiszustand + mode-spezifische Union (RIISTAR/CLUB), keine Vollallokation aller Mode-Daten gleichzeitig
- Modi: `NONE MENU SPACE RIISTAR CLUB`
- `menu_bg_set_mode()` = normaler Wechsel
- `menu_bg_set_mode_instant()` = harter Sofortwechsel
- `menu_bg_set_active()` = Legacy-Wrapper, in neuem Code vermeiden
- `menu_bg_set_palette_frozen()` = notwendig während UI-Fades
- Detailliertes Subsystem-Regelwerk: `.github/copilot-instructions-menu-bg.md`
- Riistar-spezifisch:
  - `menu_bg_riistar_set_stack_top(topRow)`
  - `menu_bg_riistar_pulse(pulseType)`

GFX:
- nur zwei explizit exportierte Custom-Tiles: `tile_skull[8]`, `tile_heart[8]`
- `gfx_load_tiles(u16 offset)` lädt projektspezifische Block-/Ghost-/Item-Tiles
- `UI_init_fonts_and_palettes()` ist Standard-UI-Init vieler Menü-States
- `SPR_init()` reserviert 420 Tiles
- `menu_bg.c` rechnet diese Reserve explizit mit ein
- `gfxtest` nutzt `TILE_USER_INDEX` als Start

Rendering-Autorität:
- Board/HUD/Preview/Hold/GoalPanel → `game_view.c`
- BG_B-Hintergründe → `menu_bg.c`
- Hardware-Sprites/Effekt-Sprites → `sprite.c`

## 6. GETEILTE GAMEPLAY-SYSTEME
Singleplayer-Schichten:
- `game.c` → State-Orchestrierung, Tickflow, BG-Mode, sprite hooks
- `game_controls.c` → Input, DAS, Rotation, Hold, Hard/Softdrop
- `game_logic.c` → Boardmutation, Bag, Spawn, Lock, Line-Clear, Items, Garbage, Effekt-Timer
- `game_view.c` → Rendering, Tilecache, UI, Goal-/Quest-Panel
- `conditions.c` → `gameConditions` aus Select/Challenge erzeugen
- `quests.c` → Tutorial-Eingabeabfolge

`GameContext` Kernfelder:
- Scoring: `score lastScore`
- Progression: `level lastLevel startLevel linesTotal`
- Piece-State: `pieceX pieceY ghostY type rotation nextType holdType`
- DAS/Timing: `moveTimer dasTimer dasDir dasNextThreshold clearTimer garbageTimer garbageNextThreshold`
- Items/Effekte: `itemSpawnCounter itemSlot itemType badEffectTimer activeBadEffect forcedPieceType`
- Board: `board[200] clearingLineBackup[200] clearingLineMask`
- RNG: `bag[7] bagIndex`
- Kommentar: `lastComment[20] commentTimer`

Boardmodell:
- 10×20 → 200 Zellen
- Index-Makro: `x + ((y<<3)+(y<<1))`
- Inline-Zugriff über `set_board_tile/get_board_tile`
- `boardFlags` enthält `GF_NEEDS_DRAW` + Pending-Line-Bits

GameFlags (`ctx->flags`):
- `GF_CAN_HOLD`
- `GF_B2B_ACTIVE`
- `GF_REVERSED`
- `GF_ROT_LOCKED`
- `GF_HOLD_LOCKED`
- `GF_NEXT_HIDDEN`
- `GF_HEART_TRIG`
- `GF_SKULL_TRIG`
- `GF_CLEARING`

RuleFlags:
- `GC_RULE_ALLOW_SHADOW`
- `GC_RULE_ALLOW_HOLD`
- `GC_RULE_SHOW_NEXT`
- `GC_RULE_DEBUG_UI`
- `GC_RULE_RANDOM_CHAOS`
- `GC_RULE_ITEMS_ENABLED`
- `GC_RULE_ITEMS_GOOD_ONLY`
- `GC_RULE_ITEMS_BAD_ONLY`

GoalFlags:
- `GC_GOAL_SCORE`
- `GC_GOAL_LINES`
- `GC_GOAL_HEARTS`
- `GC_GOAL_SKULLS`
- `GC_GOAL_SURVIVE_TIME`
- `GC_GOAL_SURVIVE_LINES`
- `GC_GOAL_PIECE_LIMIT`
- `GC_GOAL_CLEAN_BOARD`
- `GC_GOAL_TUTORIAL_QUEST`
- `GC_GOAL_DOUBLES`
- `GC_GOAL_CLEARS`
- `GC_GOAL_TETRISES`

Select→Conditions:
- Flags spiegeln Select-Konfiguration
- `randMode != 0` aktiviert `GC_RULE_RANDOM_CHAOS`
- `itemMode` 1/2/3 aktiviert Items / good-only / bad-only

Items:
- `ITEM_ID_HEART = 10`
- `ITEM_ID_SKULL = 11`
- `ITEM_ID_NONE = 0`
- `ITEM_RATIO_HEART = 30`
- Spawn-Rate: `ITEM_SPAWN_RATE_MIN 2`, `ITEM_SPAWN_RATE_MAX 4`
- `itemMode`: `0 None` · `1 All` · `2 Good` · `3 Bad`

Bad/Board-Effekte:
- `EFFECT_FULLSPEED`
- `EFFECT_SAME_TILES`
- `EFFECT_REVERSED`
- `EFFECT_NO_ROTATE`
- `EFFECT_HOLD_LOCK`
- `EFFECT_HIDE_NEXT`
- `EFFECT_I_RAIN`
- `EFFECT_FREEZE`
- `EFFECT_MULTIPLIER`
- `EFFECT_RAINBOW`
- `EFFECT_SHADOW_BOARD`

Effektdauer:
- zeitbasiert: `NO_ROTATE REVERSED HOLD_LOCK HIDE_NEXT SHADOW FREEZE` = 300 Ticks
- spawnbasiert: `FULLSPEED SAME_TILES I_RAIN` = 5 Spawns

## 7. TITLE / ENTRY
`src/states/title.c`:
- `TitleContext.phase`: `PHASE_BLINK` oder `PHASE_MENU`
- Blinkphase: „PRESS START TO PLAY“ blinkt; Idle 420 Frames → `STATE_HIGHSCORE`
- Menüeintrag-Reihenfolge:
  1. `CHALLENGE`
  2. `FREE GAME`
  3. `VS STATE`
  4. `OPTIONS`
  5. `SOUND TEST`
  6. `GFX TEST`
- `BUTTON_START` im Menü wechselt State direkt
- `BUTTON_B` verlässt Menü zurück zu Blinkphase
- `title_init()` ruft `menu_bg_init()` erneut; existierendes Projektverhalten, nicht ungefragt umbauen

## 8. FREE GAME / SELECT / OPTIONS
`SELECT` (`src/states/game_select.c`):
- Nameingabe: 3 Zeichen A-Z, `A` springt zum nächsten Zeichen
- Parameter:
  - `randMode`: `0 Fair`, `1 Chaos`
  - `speedLevel`: `0..9`
  - `garbageFreq`: `0..9`
  - `itemMode`: `0 None`, `1 All`, `2 Good`, `3 Bad`
  - Flags: Shadow/Hold/Next
- Bei Start:
  - Werte nach `config` kopieren
  - `config.runtime.gameMode = GAME_MODE_FREEGAME`
  - `config.runtime.challengeLevelId = 255`
  - `config.runtime.challengeResult = NONE`
  - `game_conditions_set_from_select(ctx)` vor State-Wechsel ausführen
  - Bei Änderung: erst `STATE_SAVE`, sonst direkt `STATE_GAME`

`OPTIONS` (`src/states/options.c`):
- Toggelt `FLAG_MUSIC`, `FLAG_SOUND`, `FLAG_BG`, `FLAG_DEBUG`
- Sensibility-Tripel:
  - `thresholdLRInitial`
  - `thresholdLRRepeat`
  - `thresholdSD`
- Row 5 `SYSTEM`:
  - `RELOAD OPTIONS` → `STATE_SAVE + SRAM_LOAD + preferredState=STATE_OPTIONS`
  - `RESET DEFAULTS` → nur RAM-Reset der Optionswerte, kein direkter SRAM-Write
- `START` → Save + Return Title
- `B` → Return Title ohne Save
- `FLAG_DEBUG` schaltet `SYS_showFrameLoad()`

## 9. SINGLEPLAYER
Zufall:
- Standard: 7-Bag (`bag[7]`)
- `randMode=Chaos` setzt Random-Chaos-Flag, weitere Logik in `conditions.c`/`game_logic.c`

Routing bei Spielende:
- Freegame → `STATE_GAMEOVER`
- Challenge → `config.runtime.challengeResult = SUCCESS/FAIL`, dann zurück nach `STATE_CHALLENGE`

## 10. CHALLENGE-SYSTEM
`ChallengeContext`:
- 16×8 Grid = 128 Level
- `cursor_x cursor_y current_level_id`
- `holdDir holdTimer holdNextThreshold` → D-Pad-Repeat im Grid
- `frontier_open[4]` → 128-bit Frontier

Challenge-Levelraum (`inc/states/challenge.h`):
- `CHALLENGE_LEVEL_COUNT = 128`
- Voller Enum `ChallengeLevelId` mit benannten Leveln; Header ist kanonische Levelliste
- Tutorial-Einstieg: `CHALLENGE_TUTORIAL_ENTRY_ID = 2*16`

Challenge-Flow:
- `challenge_init()` positioniert Cursor / verarbeitet Ergebnisrückkehr
- `BUTTON_START` auf freigeschaltetem Level:
  - `config.runtime.gameMode = GAME_MODE_CHALLENGE`
  - `config.runtime.challengeLevelId = level_id`
  - `config.runtime.challengeResult = NONE`
  - `game_conditions_set_for_challenge_level(level_id)`
  - `currentState = STATE_GAME`
- `BUTTON_C` → Save-Progress über `STATE_SAVE`, Rückkehr `STATE_TITLE`

Challenge-Persistenz:
- `challenge_unlocked[4]` / `challenge_cleared[4]` in `Serializable`
- `challenge_mark_cleared()` setzt cleared-bit und öffnet Nachbarn

Tutorial/Quest:
- `QUEST_ACT_MOVE_LR`
- `QUEST_ACT_ROTATE_AB`
- `QUEST_ACT_SOFTDROP`
- `QUEST_ACT_HOLD_STORE`
- `QUEST_ACT_HARDDROP`
- `QUEST_ACT_HOLD_RECALL`
- `QUEST_TUTORIAL_STAGE_COUNT = 6`

## 11. VS-MODE
`VsContext` = 2× `GameContext` + Input/AI/Match-State
- `left` / `right`
- `joy1 joy1Last joy2 joy2Last`
- `leftDead rightDead`
- `rightAiEnabled`
- `leftNeedsRedraw rightNeedsRedraw`
- `leftItemMode rightItemMode`
- `leftGarbagePending rightGarbagePending`
- `leftGameOverAnimRow rightGameOverAnimRow`
- `matchOver winnerSide`
- `leftEventText[24] rightEventText[24]`

AI-Felder:
- `rightAiState`
- `rightAiTargetX rightAiTargetY rightAiEntryX`
- `rightAiTargetRot`
- `rightAiPlannedType`
- `rightAiBestScore`
- `rightAiPulseTimer`
- `rightAiThinkBudget`
- `rightAiActionDelay`

VS-Module:
- `src/states/vs_state.c` → VS-Orchestrierung, Boards, Input, Attacks, Draw, Effect-Sync
- `src/states/vs/brain.c` → CPU-Planung / Bitboard-Simulation / Lookahead mit `nextType`

VS-Spezifika:
- rechter Spieler = CPU
- `vs_brain_update_player()` steuert KI-Spieler zentral
- `vs_sync_effect_sprites()` koppelt Spriteeffekte an aktive BadEffects beider Boards
- BG-Modus für VS: `BG_MODE_CLUB`

## 12. AUX-STATES
`GAMEOVER`:
- zeigt `config.playerName` + `config.currentScore`
- `START`/`A` → `check_and_update_highscore()` → `STATE_HIGHSCORE`

`HIGHSCORE`:
- zeigt Top10 aus `config.highscores`
- neue Einträge über `isNew`
- bei Exit: ggf. Save, sonst Title
- `highscore_cleanup()` löscht alle `isNew` Marker

`SOUNDTEST`:
- Sound-ID `1..99`
- `LEFT/RIGHT` wechseln ID
- `A` spielt `SOUND_play((SoundEvent)id)`
- `START` → Title

`GFXTEST`:
- lädt Gameplay-Tiles über `gfx_load_tiles(TILE_USER_INDEX)`
- rendert zufällige 7-Bag-Piece-Layouts auf BG_A
- `START` → Title

## 13. KNOWN PATTERNS / NICHT UNGEFRAGT "AUFRÄUMEN"
- `src/main.c` enthält explizite `game_*`-Prototypen als Build-Workaround; nicht blind entfernen
- `title_init()` ruft `menu_bg_init()` erneut; existierendes Verhalten nicht reflexartig deduplizieren
- `SOUND_playMusic()` ist bewusst stub-artig; nicht stillschweigend Track-Handling erfinden
- README spricht von Features/Leaks/Roadmap; keine Architekturentscheidung auf README stützen
- `res/*.h` und `res/*.res` als Ressourcenebene behandeln; nicht ohne Ressourcenpipeline umstrukturieren
- Bei Projektänderungen immer Autoritätsdatei pflegen: Typen in Header, Verhalten in zugehörigem Modul