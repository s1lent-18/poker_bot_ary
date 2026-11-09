# Heads-Up Poker Frontend (1 User vs 1 Bot)

A minimalist, high-performance web frontend for Heads-Up No-Limit Texas Hold'em built with **Next.js (React 19)**, **Tailwind CSS v4**, and **Bun**, powered directly by the native C++17 poker engine and CFR solver.

---

## Features

- **10,000 Chip Starting Stack**: Both player and bot start with 10,000 chips (100 BB at 50/100 blinds), resetting each hand just like the CLI interactive mode.
- **Bot Opponent Options**:
  - **RuleBot**: Fast heuristic-based bot using preflop hand charts and postflop made-hand texture evaluation.
  - **EquityBot**: Monte Carlo simulation bot (2,000 samples) comparing equity vs. pot odds and opponent range narrowing.
  - **Holden (CFR Bot)**: Equilibrium solver bot using Counterfactual Regret Minimization.
- **Holden Checkpoint Switcher**:
  - Automatically discovers all trained `.cfr` checkpoints in the `versions/` folder (from `run1` & `run2` up to `run4` with 10M iterations).
  - Displays checkpoint iteration counts and info set sizes.
- **Holden Fallback Telemetry**:
  - Live metric showing when Holden encounters info sets that were unvisited during training (`fallbacks / decisions` and `%`).
- **Minimalist, Non-AI Aesthetic**:
  - No flashy gradients or generic AI glassmorphism.
  - Solid, clean casino forest green felt (`#143322`), crisp flat borders, and monospace numbers.
- **Synthesized Web Audio Effects**:
  - Card dealing, chip betting, check table-knocking, fold swishing, all-in dramatic tone, and warm win chimes generated on the fly via the Web Audio API (zero external assets, 100% offline).
  - Mute toggle available in the top bar.
- **Full Betting Controls & Keyboard Shortcuts**:
  - `F`: Fold
  - `Space` or `C`: Check / Call
  - `A`: All-In
  - `Enter` or `Space`: Deal next hand when terminal
  - Quick-raise buttons: `Min`, `½ Pot`, `1x Pot` (Holden's trained raise sizing), and `All-In` slider.
- **4-Color Deck Option**:
  - Toggle between 4-color suits (♠ Black, ♥ Red, ♦ Blue, ♣ Green) and classic 2-color suits.
- **Collapsible Hand History Log**:
  - Complete chronological record of hand actions, board reveals, and showdown payoffs.

---

## How to Run

### Single Command (Recommended)
Run the launcher script from the repository root:

```bash
./run_app.sh
```

This compiles `poker_server` (if not already compiled), starts the C++ server on port `8085`, and launches the Next.js frontend with Bun on `http://localhost:3000`.

### Manual Start

1. **Build & run the C++ backend server**:
   ```bash
   cmake -B build -S .
   cmake --build build --target poker_server -j$(nproc)
   ./build/poker_server 8085
   ```

2. **Run the Next.js frontend with Bun**:
   ```bash
   cd frontend
   bun run dev -p 3000
   ```
   Open [http://localhost:3000](http://localhost:3000) in your browser.
