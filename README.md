# Heads-Up No-Limit Texas Hold'em Engine, CFR Solver & Web Arena

A complete C++17 Heads-Up No-Limit Texas Hold'em game engine, Monte Carlo Counterfactual Regret Minimization (MCCFR) equilibrium solver, simulation arena, and interactive web application built with Next.js 16, React 19, Tailwind CSS v4, and Bun.

---

## Table of Contents

1. [Architecture and Components](#architecture-and-components)
2. [Prerequisites and Build](#prerequisites-and-build)
3. [Running the Web Arena](#running-the-web-arena)
4. [Web Interface Features](#web-interface-features)
5. [Backend API Reference](#backend-api-reference)
6. [Interactive CLI Play](#interactive-cli-play)
7. [Training the CFR Solver](#training-the-cfr-solver)
8. [Strategy Inspection](#strategy-inspection)
9. [Bot Simulations and Duels](#bot-simulations-and-duels)
10. [Multi-Version Arena Benchmarks](#multi-version-arena-benchmarks)
11. [Watching Bot Matches](#watching-bot-matches)
12. [Production Deployment Setup](#production-deployment-setup)
13. [Project Layout and Testing](#project-layout-and-testing)

---

## Architecture and Components

The repository consists of modular C++ libraries, standalone command-line tools, and an independent web interface:

- `poker_core`: Card encoding (`Card`), deck manipulation with Fisher-Yates shuffling (`Deck`), and reproducible bounded Mersenne Twister RNG (`Rng`).
- `poker_eval`: 5-to-7 card hand evaluator scoring hands into standard ranks (High Card to Straight Flush) and tie-breaking values.
- `poker_engine`: State machine (`GameState`) enforcing Heads-Up No-Limit Texas Hold'em rules, blinds rotation (50/100 default), street transitions, legal action validation, and zero-sum terminal payoffs. Supports arbitrary and asymmetric starting stacks.
- `poker_ai`: Monte Carlo equity calculator (`equity_vs_random`), opponent range discounting, card bucketing (12 canonical clusters per street), and CFR solvers (Kuhn, Leduc, and Hold'em MCCFR).
- `poker_agents`: Agent interfaces and implementations:
  - `RuleAgent`: Preflop charts and postflop texture heuristics.
  - `EquityAgent`: Monte Carlo simulation agent (2,000 samples) comparing hand equity against pot odds with dynamic opponent range narrowing.
  - `CfrAgent`: Equilibrium agent executing pre-trained Counterfactual Regret Minimization strategies with fallback telemetry for unseen info-sets.
  - `RandomAgent`: Uniform random baseline agent.
  - `HumanCliAgent`: Text-based terminal interface for interactive play.
- `poker_cli`: Command-line executable supporting human play, statistical duels (`--sim`), round-robin strategy tournaments (`--arena`), and strategy file inspection (`--info`).
- `holdem_train`: External-sampling Monte Carlo CFR training executable with periodic snapshot checkpointing.
- `poker_server`: POSIX HTTP server interfacing `GameState` and AI agents with the web application over REST/JSON.
- `frontend`: Minimalist web interface built with Next.js 16 (React 19), Tailwind CSS v4, and Bun.

---

## Prerequisites and Build

### System Requirements

- C++17 compiler (`g++` 9+ or `clang++` 10+)
- CMake 3.16+
- Bun 1.1+ (for frontend)
- Linux or macOS

### Build Targets

```bash
# Configure build directory
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Build all binaries
cmake --build build -j$(nproc)

# Run test suite (106 GoogleTests covering cards, evaluator, engine, equity, CFR solvers)
ctest --test-dir build --output-on-failure
```

Built executables are placed in `build/`:
- `build/poker_cli`
- `build/holdem_train`
- `build/poker_server`
- `build/kuhn_solver`
- `build/leduc_solver`

---

## Running the Web Arena

### Single Command (Recommended)

Run the launch script from the project root:

```bash
./run_app.sh
```

This compiles `poker_server` (if unbuilt), launches the C++ backend daemon on port 8085, and starts the Next.js development server on `http://localhost:3000`.

### Manual Startup

1. **Start the C++ backend server**:
   ```bash
   ./build/poker_server 8085
   ```

2. **Start the Next.js frontend with Bun**:
   ```bash
   cd frontend
   bun install
   bun run dev -p 3000
   ```
   Open `http://localhost:3000` in a browser.

3. **Production build**:
   ```bash
   cd frontend
   bun run build
   bun run start -p 3000
   ```

---

## Web Interface Features

- **Bot Selection**: Switch dynamically between RuleBot, EquityBot, and Hold'em CFR.
- **Hold'em CFR Version Switcher**: Scans the `versions/` directory and exposes trained checkpoints (e.g., `run1`, `run2`, and `run4` checkpoints up to 10M iterations) with metadata.
- **Hold'em Fallback Telemetry**: Displays the count and percentage of unvisited info-sets encountered during a match, tracking model coverage against human betting lines.
- **30-Second Shot Clock**: Enforces a 30-second decision timer. When the timer expires:
  - Automatically checks if check is free (`toCall == 0`).
  - Automatically folds if facing a bet (`toCall > 0`).
  - Visual warning color changes and audio ticking alerts occur in the final 5 seconds.
- **BotCheck Mode (Fold Inspector)**: A side panel that reveals the bot's actual mucked hole cards and evaluated hand rank whenever either player folds, allowing review of bluffs and fold decisions.
- **Persistent Stacks & Auto-Rebuy**:
  - `Persistent` (Default): Players start with 10,000 chips; chip balances persist across hands. If a player drops to or below the big blind (<= 100 chips), an automatic rebuy replenishes them to 10,000 chips.
  - `Reset 10k`: Resets both players to 10,000 chips at the start of each round.
- **Sequential Community Card Dealing**: Flop, turn, and river cards reveal one-by-one with realistic 240ms delays and card-slide sound effects.
- **Discrete Engine-Compatible Sizing**: Buttons strictly mirror the engine's legal action set (`Fold`, `Check/Call`, discrete pot-fraction raises, `All-In`, `Next Hand`).
- **Web Audio API Sound Effects**: Synthesized audio for card deals, chip betting, table knock checks, fold swishes, dramatic all-in tones, and win chimes (100% offline, zero audio assets).
- **Four-Color Deck**: Toggle between high-contrast 4-color suits (Black Spades, Red Hearts, Blue Diamonds, Green Clubs) and standard 2-color decks.
- **Dedicated Hand History Drawer**: Slide-over drawer accessible via the header or hotkey `H` (`Esc` to close).

### Keyboard Controls (Web)

- `F`: Fold
- `Space` or `C`: Check / Call
- `A`: All-In
- `1`, `2`: Sized raise options
- `H`: Toggle Hand History Log
- `Space` or `Enter`: Deal next hand when terminal

---

## Backend API Reference

The C++ server (`poker_server`) runs on port 8085 and exposes the following endpoints:

### `GET /api/bots`
Returns available agent types and scans the `versions/` directory for CFR model files.

**Response:**
```json
{
  "bots": [
    { "id": "rule", "name": "RuleBot", "type": "rule", "description": "Preflop chart + postflop heuristics" },
    { "id": "equity", "name": "EquityBot", "type": "equity", "description": "Monte Carlo equity simulator vs pot odds" },
    {
      "id": "cfr",
      "name": "Hold'em (CFR)",
      "type": "cfr",
      "description": "Counterfactual Regret Minimization equilibrium strategy",
      "versions": [
        {
          "id": "versions/run4.cfr",
          "filename": "run4.cfr",
          "name": "run4.cfr",
          "iterations": 10000000,
          "infoSets": 5540,
          "raiseMenu": "1x pot, all-in",
          "stack": "100bb"
        }
      ]
    }
  ]
}
```

### `POST /api/game/new`
Initializes a new match session and deals the first hand.

**Request:**
```json
{
  "bot": "cfr",
  "version": "versions/run4.cfr",
  "autoRebuy": false
}
```

**Response:**
```json
{
  "sessionId": "1731170000",
  "round": 1,
  "street": "preflop",
  "pot": 150,
  "currentBet": 100,
  "toCall": 50,
  "button": 0,
  "heroStack": 9950,
  "botStack": 9900,
  "heroCards": ["Ah", "Kd"],
  "botCards": ["??", "??"],
  "boardCards": [],
  "isTerminal": false,
  "legalActions": [
    { "action": "fold", "label": "Fold", "amount": 0 },
    { "action": "call", "label": "Call 50", "amount": 50 },
    { "action": "raise", "label": "Raise to 300", "amount": 300 },
    { "action": "allin", "label": "All-in (10000)", "amount": 10000 }
  ],
  "history": [
    { "type": "blind", "player": "hero", "amount": 50, "text": "Hero posts small blind 50" },
    { "type": "blind", "player": "bot", "amount": 100, "text": "Bot posts big blind 100" }
  ]
}
```

### `POST /api/game/action`
Applies a human action and advances the state through bot responses and street transitions until the next human decision point or terminal showdown.

**Request:**
```json
{
  "action": "call",
  "amount": 50
}
```

**Response:** Returns updated game state JSON including `events` generated during the turn, newly dealt `boardCards`, and updated `legalActions`.

### `POST /api/game/next`
Advances to the next hand, rotates the dealer button, and preserves or resets stacks based on `autoRebuy`.

**Request:**
```json
{
  "autoRebuy": false
}
```

### `POST /api/game/toggle_rebuy`
Toggles between persistent stack mode and per-hand 10,000 reset mode without resetting the active hand.

**Response:**
```json
{
  "autoRebuy": true,
  "mode": "reset_each_hand"
}
```

### `GET /api/game/state`
Returns the current session state without modifying it.

---

## Interactive CLI Play

Play directly in the terminal against any bot implementation:

```bash
# Play against default RuleBot
./build/poker_cli

# Play against specific bots
./build/poker_cli --bot rule
./build/poker_cli --bot random
./build/poker_cli --bot equity

# Play against a trained CFR strategy
./build/poker_cli --bot cfr --strategy holdem.cfr
./build/poker_cli --bot cfr --strategy versions/run4.cfr

# Play with a fixed seed for reproducible deals
./build/poker_cli --bot cfr --strategy versions/run4.cfr --seed 42
```

### CLI Terminal Controls

- `1`, `2`, `3`...: Select action by index
- `f`: Fold
- `k` or `c`: Check / Call
- `a`: All-In
- `q`: Quit match

---

## Training the CFR Solver

The `holdem_train` binary executes external-sampling Monte Carlo Counterfactual Regret Minimization.

```bash
# Quick 2-minute test run (prints iterations/sec and info set counts; saves holdem.cfr)
./build/holdem_train --seconds 120

# 20-minute run with custom output filename
./build/holdem_train --seconds 1200 --out holdem_20min.cfr

# Train for a fixed iteration count instead of a timer
./build/holdem_train --iters 200000 --out holdem_200k.cfr

# Long background run with automatic snapshots every 30 minutes
mkdir -p versions
nohup ./build/holdem_train --seconds 28800 --snapshot-sec 1800 --out versions/run1.cfr > train.log 2>&1 &
tail -f train.log

# Stop running background training gracefully (saves automatically every 10 minutes)
pkill -f holdem_train

# Train on macOS without system sleep
caffeinate -i ./build/holdem_train --seconds 10800 --snapshot-sec 1800 --out versions/run1.cfr

# Training with expanded raise menu (0.5x pot, 1x pot, all-in)
./build/holdem_train --seconds 3600 --sizes 2 --out holdem_sizes2.cfr

# Display all command options
./build/holdem_train --help
```

### Training CLI Flags

- `--iters N`: Maximum CFR training iterations.
- `--seconds S`: Maximum training time in seconds.
- `--snapshot-sec S`: Save periodic numbered snapshots (`<file>_<iterations>.cfr`) every `S` seconds.
- `--out FILE`: Target filepath for strategy output (default: `holdem.cfr`).
- `--sizes 1|2`: Raise sizing abstraction (1 = 1x pot, all-in; 2 = 0.5x pot, 1x pot, all-in).
- `--buckets B`: Canonical card clusters per street (default: 12).
- `--stack-bb N`: Starting stack in big blinds (default: 100).
- `--seed N`: RNG seed for reproducibility.

---

## Strategy Inspection

Examine metadata, iteration counts, info-set table sizes, and configuration headers inside strategy files:

```bash
# Inspect a single file
./build/poker_cli --info holdem.cfr

# Inspect all version snapshots
./build/poker_cli --info versions/*.cfr
```

Example output:
```text
versions/run4.cfr
  trained: 10000000 iterations
  info sets: 5540
  stack: 100bb, blinds 50/100
  raise menu: 1x pot, all-in
  card buckets per street: 12
```

---

## Bot Simulations and Duels

Evaluate bot win rates over large sample sizes without rendering. Results are reported in `bb/hand` and `bb/100` from Hero's perspective with 95% confidence intervals.

Format: `./build/poker_cli --sim HANDS --hero X --bot Y`

```bash
# Baseline simulations
./build/poker_cli --sim 20000 --hero rule --bot random
./build/poker_cli --sim 20000 --hero equity --bot random
./build/poker_cli --sim 20000 --hero equity --bot rule
./build/poker_cli --sim 20000 --hero rule --bot equity

# CFR evaluations against heuristics (3,000 - 5,000 hands recommended when EquityBot is involved)
./build/poker_cli --sim 5000 --hero cfr --bot random --strategy versions/run4.cfr
./build/poker_cli --sim 5000 --hero cfr --bot rule --strategy versions/run4.cfr
./build/poker_cli --sim 3000 --hero cfr --bot equity --strategy versions/run4.cfr
./build/poker_cli --sim 3000 --hero equity --bot cfr --strategy versions/run4.cfr

# Duplicate format (--dup): Deals every hand twice with swapped hole cards and positions to cancel variance
./build/poker_cli --sim 10000 --dup --hero equity --bot rule
./build/poker_cli --sim 10000 --dup --hero cfr --bot equity --strategy versions/run4.cfr
```

---

## Multi-Version Arena Benchmarks

Compare different CFR training runs or checkpoint snapshots against each other in duplicate format:

```bash
# Compare two specific checkpoints
./build/poker_cli --sim 10000 --dup --hero cfr --bot cfr --strategy versions/run1.cfr --strategy2 versions/run4.cfr

# Round-robin tournament across all checkpoints in versions/ (4,000 duplicate hands per pair)
./build/poker_cli --arena versions/run4_*.cfr

# Benchmark specific models with custom hand counts
./build/poker_cli --arena holdem.cfr versions/run1.cfr versions/run4.cfr --sim 6000
```

---

## Watching Bot Matches

Observe bot decisions hand-by-hand in the terminal:

```bash
# Watch 5 hands between EquityBot and RuleBot
./build/poker_cli --watch 5 --hero equity --bot rule --seed 1

# Watch CFR Bot against RuleBot
./build/poker_cli --watch 10 --hero cfr --bot rule --strategy versions/run4.cfr

# Step-by-step mode (press Enter to advance, q to stop)
./build/poker_cli --watch 20 --step --hero cfr --bot equity --strategy versions/run4.cfr

# Log detailed hand action to a file
./build/poker_cli --watch 200 --hero cfr --bot rule --strategy versions/run4.cfr > hands.txt
```

---

## Production Deployment Setup

For hosting the web application on a Linux server (Ubuntu/Debian VPS), the optimal architecture decouples the C++ backend daemon from the Next.js frontend, managed by systemd and unified behind Nginx.

### Architecture

```text
[ Client Browser ]
        │
     HTTPS (443)
        ▼
   [ Nginx Reverse Proxy ]
        ├── /api/*  ──► [ C++ poker_server daemon (127.0.0.1:8085) ]
        └── /*      ──► [ Next.js Bun App (127.0.0.1:3000) ]
```

### 1. Build Binaries and Assets

```bash
# Build C++ backend
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target poker_server -j$(nproc)

# Build Next.js frontend with Bun
cd frontend
bun install --frozen-lockfile
bun run build
cd ..
```

### 2. Systemd Service Definitions

#### Backend Service (`/etc/systemd/system/poker-server.service`)
```ini
[Unit]
Description=Poker Engine C++ Backend Server
After=network.target

[Service]
Type=simple
User=poker
WorkingDirectory=/opt/poker_bot_ary
ExecStart=/opt/poker_bot_ary/build/poker_server 8085
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

#### Frontend Service (`/etc/systemd/system/poker-frontend.service`)
```ini
[Unit]
Description=Poker Arena Next.js Frontend
After=network.target poker-server.service

[Service]
Type=simple
User=poker
WorkingDirectory=/opt/poker_bot_ary/frontend
Environment=NODE_ENV=production
Environment=PORT=3000
ExecStart=/usr/local/bin/bun run start -p 3000
Restart=always
RestartSec=3

[Install]
WantedBy=multi-user.target
```

Enable and start services:
```bash
sudo systemctl daemon-reload
sudo systemctl enable --now poker-server.service
sudo systemctl enable --now poker-frontend.service
```

### 3. Nginx Reverse Proxy Configuration

Place the following configuration in `/etc/nginx/sites-available/poker.conf`:

```nginx
server {
    listen 80;
    server_name poker.yourdomain.com;

    # C++ Backend API
    location /api/ {
        proxy_pass http://127.0.0.1:8085/api/;
        proxy_http_version 1.1;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
    }

    # Next.js Frontend
    location / {
        proxy_pass http://127.0.0.1:3000;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection 'upgrade';
        proxy_set_header Host $host;
        proxy_cache_bypass $http_upgrade;
    }
}
```

Enable the site and reload Nginx:
```bash
sudo ln -s /etc/nginx/sites-available/poker.conf /etc/nginx/sites-enabled/
sudo nginx -t
sudo systemctl reload nginx
```

---

## Project Layout and Testing

```text
.
├── CMakeLists.txt            # CMake build definitions (C++17)
├── run_app.sh                # Launcher script for backend server + Next.js frontend
├── versions/                 # Pre-trained Hold'em CFR strategy checkpoints
├── src/
│   ├── core/                 # Card, Deck, Rng implementations
│   ├── eval/                 # 5-to-7 card hand evaluator and preflop classification
│   ├── engine/               # GameState, Action, and GameConfig state machine
│   ├── ai/                   # Equity calculator, Bucketer, MCCFR solver, Strategy tables
│   ├── agents/               # RuleAgent, EquityAgent, CfrAgent, RandomAgent, HumanCliAgent
│   └── app/
│       ├── main.cpp          # poker_cli implementation
│       ├── holdem_train_main.cpp # holdem_train binary
│       ├── poker_server.cpp  # C++ REST API server
│       ├── kuhn_main.cpp     # Kuhn poker solver binary
│       └── leduc_main.cpp    # Leduc poker solver binary
├── tests/                    # 106 GoogleTests for core, engine, evaluator, AI, and CFR
└── frontend/                 # Next.js 16 (React 19) web application
    ├── app/                  # App router, page views, and API proxy routes
    ├── components/           # PokerTable, BettingControls, CardComponent, BotCheckPanel
    ├── lib/                  # Web Audio synthesizer and TypeScript interfaces
    └── package.json          # Bun package manifest
```

### Running Automated Tests

All C++ test cases can be executed via CTest:

```bash
ctest --test-dir build --output-on-failure
```

Test categories:
- `cards_tests`: Card encoding, string parsing, and deck shuffling.
- `eval_tests`: Exact 5-card, 6-card, and 7-card evaluator comparisons and classifications.
- `engine_tests`: GameState rules, blinds posting, pot calculation, street progression, and stack conservation.
- `equity_tests`: Monte Carlo equity computation against uniform and weighted ranges.
- `cfr_tests`: Kuhn and Leduc CFR convergence checks and Nash equilibrium validation.
