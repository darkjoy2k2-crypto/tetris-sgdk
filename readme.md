# Tetris Vibe SGDK - See what AI can do for you

A fully functional Tetris clone for the **Sega Mega Drive / Genesis**, inspired by *Tetris Party (Wii)*. 

This project serves as a technical demonstration of the synergy between low-level hardware programming (SGDK 2.1.1) and AI-assisted development (Google Gemini).

---

## 💻 Development Insight: AI Collaboration

The majority of this project’s logic and optimization was developed through iterative interaction with **Gemini**. The focus was on leveraging AI to solve platform-specific challenges:

* **Prompt Engineering:** Utilizing precise instructions to enforce Motorola 68000 memory alignment rules, preventing "Address Errors."
* **Low-Level Optimization:** Implementing bitshift-based math to replace costly division/multiplication cycles for u16/s16 operations.
* **Hardware Architecture:** Designing a robust Game State Machine and decoupling core logic (`game_logic.c`) from the VDP rendering engine (`game_view.c`).

### Technical Implementation (SGDK 2.1.1)
* **Math Engine:** Heavy use of fixed-point arithmetic (`FIX16`, `F16_mul`, `F16_div`) for physics and timing.
* **Dynamic Context Management:** Utilizing `MEM_alloc` for game state persistence to minimize the global RAM footprint.
* **VDP Management:** Custom background system featuring a "Tetromino Rain" animation using plane scrolling and tile-map manipulation.

---

## 🎮 Controls

The game features a streamlined control scheme optimized for the original 3-button and 6-button pads. (Pause functionality is currently omitted to maintain gameplay intensity).

| Button | Action |
| :--- | :--- |
| **D-Pad Left/Right** | Move Piece |
| **D-Pad Down** | Soft Drop |
| **D-Pad Up** | Hard Drop |
| **Button A / B** | Rotate Piece (Clockwise/Counter-Clockwise) |
| **Button C** | Hold Function |
| **Start** | (Unassigned / Debug) |

---

## 🚀 Game Features

* **Dynamic Backgrounds:** Features a specialized "Tetromino Rain" visualizer running on background planes.
* **Curse Engine:** Randomized status effects including "No-Rotate," "Speed-Skulls," and "Reversed Controls."
* **7-Bag RNG:** Industry-standard 7-bag randomizer to prevent piece droughts and ensure fair play.
* **Fully Customizable:** In-game menus allow adjustment of every gameplay aspect, including item ratios, drop speeds, and gravity.
* **Ghost Piece:** Real-time projection of the piece's landing position for precision play.
* **Highscore System:** Persistent session-based highscore list with support for up to 10 entries.

---

## 📝 TODOs / Roadmap

- [ ] **Stability:** Resolve current memory leak/crash issues introduced during Sprite-Engine integration.
- [ ] **Logic Fix:** Debug the "Next Tile" synchronization error (VDP vs. Logic mismatch).
- [ ] **Enhanced Audio:** Full integration of FM-synth tracks and PCM samples for line clears.
- [ ] **Progression:** Add dynamic stage backgrounds and difficulty scaling per level.
- [ ] **Multiplayer:** Implement a "Versus CPU" or 2-Player competitive mode.

---

## 📖 Documentation & Wiki

For deep-dives into the memory layout, alignment strategies, and specific AI prompts used:
> [👉 View the Project Wiki (Placeholder)](https://github.com/darkjoy2k2-crypto/tetris-vibe/wiki)

---

## 📜 Credits
* **Lead Development:** Gemini (AI) & darkjoy2k2.
* **Sound Assets:** Sound effects sourced from *Puyo Puyo*. Developed by **Compile**, created by **Einosuke Nagao**.
* **SDK:** SGDK by **Stephane Dallongeville**.
