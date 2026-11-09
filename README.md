################ PROJECT POKER: RUN COMMANDS ################
# hero = seat 0, bot = seat 1. Results are always from the HERO's side.
# OLD.cfr / NEW.cfr / FILE.cfr = your strategy files. Replace with real names.

# ---------- BUILD AND TEST ----------
cmake -S . -B build
cmake --build build 2>&1 | tail -30
ctest --test-dir build --output-on-failure

# ---------- TRAIN THE CFR BOT ----------
# 2 minute test run (shows it/s and info-set count), saves holdem.cfr
./build/holdem_train --seconds 120

# 20 minute run
./build/holdem_train --seconds 1200 --out holdem_20min.cfr

# fixed number of iterations instead of time
./build/holdem_train --iters 200000 --out holdem_200k.cfr

# long run WITH VERSION SNAPSHOTS every 30 min (Linux, keeps running after closing terminal)
mkdir -p versions
nohup ./build/holdem_train --seconds 28800 --snapshot-sec 1800 --out versions/run1.cfr > train.log 2>&1 &
tail -f train.log

# stop a running training (autosaves every 10 min, so little is lost)
pgrep holdem_train
kill PID_NUMBER

# long run on Mac (prevents sleep; keep lid open and plugged in)
mkdir -p versions
caffeinate -i ./build/holdem_train --seconds 10800 --snapshot-sec 1800 --out versions/run1.cfr

# bigger raise menu (half pot + pot + all-in), slower to train
./build/holdem_train --seconds 3600 --sizes 2 --out holdem_sizes2.cfr

# other training options: --buckets 12 --seed 1 --stack-bb 100

# ---------- LOOK INSIDE STRATEGY FILES ----------
./build/poker_cli --info holdem.cfr
./build/poker_cli --info versions/run1_*.cfr

# ---------- YOU vs A BOT ----------
./build/poker_cli
./build/poker_cli --bot rule
./build/poker_cli --bot random
./build/poker_cli --bot equity
./build/poker_cli --bot cfr --strategy holdem.cfr
./build/poker_cli --bot cfr --strategy versions/run1_000012000.cfr
./build/poker_cli --bot cfr --strategy FILE.cfr --seed 42
# controls: number = pick option, f = fold, k or c = check/call, a = all-in, q = quit

# ---------- BOT vs BOT: STATISTICS (--sim) ----------
# format: ./build/poker_cli --sim HANDS --hero X --bot Y
# bots: rule  random  equity  cfr
./build/poker_cli --sim 20000 --hero rule --bot random
./build/poker_cli --sim 20000 --hero equity --bot random
./build/poker_cli --sim 20000 --hero equity --bot rule
./build/poker_cli --sim 20000 --hero rule --bot equity
./build/poker_cli --sim 5000 --hero cfr --bot random --strategy FILE.cfr
./build/poker_cli --sim 5000 --hero cfr --bot rule --strategy FILE.cfr
./build/poker_cli --sim 3000 --hero cfr --bot equity --strategy FILE.cfr
./build/poker_cli --sim 3000 --hero equity --bot cfr --strategy FILE.cfr
./build/poker_cli --sim 3000 --hero rule --bot cfr --strategy FILE.cfr
# same deals every time: add --seed 1
# equity bot is slow, so use 3000 to 5000 hands when it is involved

# duplicate format (each deal played twice, cards and seats swapped = much less luck)
./build/poker_cli --sim 10000 --dup --hero equity --bot rule
./build/poker_cli --sim 10000 --dup --hero cfr --bot equity --strategy FILE.cfr

# ---------- VERSION vs VERSION ----------
# stats: hero uses --strategy, bot uses --strategy2
./build/poker_cli --sim 10000 --dup --hero cfr --bot cfr --strategy OLD.cfr --strategy2 NEW.cfr

# hand by hand
./build/poker_cli --watch 10 --hero cfr --bot cfr --strategy OLD.cfr --strategy2 NEW.cfr

# every file against every other (duplicate format, 4000 hands per pairing by default)
./build/poker_cli --arena versions/run1_*.cfr
./build/poker_cli --arena OLD.cfr NEW.cfr --sim 6000
./build/poker_cli --arena holdem.cfr holdem_20min.cfr versions/run1.cfr --sim 6000

# ---------- WATCH BOTS PLAY HAND BY HAND (--watch) ----------
./build/poker_cli --watch 5 --hero equity --bot rule --seed 1
./build/poker_cli --watch 10 --hero rule --bot random
./build/poker_cli --watch 10 --hero cfr --bot rule --strategy FILE.cfr
./build/poker_cli --watch 10 --hero cfr --bot equity --strategy FILE.cfr
# pause after every hand (Enter = next, q = stop)
./build/poker_cli --watch 20 --step --hero cfr --bot equity --strategy FILE.cfr
# save a long watch to a file instead of the screen
./build/poker_cli --watch 200 --hero cfr --bot rule --strategy FILE.cfr > hands.txt

# ---------- HELP ----------
./build/poker_cli --help
./build/holdem_train --help
