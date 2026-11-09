#!/usr/bin/env bash
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$DIR"

echo "=================================================="
echo "  Hold'em 1v1 Poker Arena (User vs. Bots)"
echo "=================================================="

# Ensure C++ server binary is built
if [ ! -f "build/poker_server" ]; then
    echo ">> Building C++ poker engine server..."
    cmake -B build -S .
    cmake --build build --target poker_server -j$(nproc)
fi

# Kill any existing server on 8085
fuser -k 8085/tcp 2>/dev/null || true

# Start C++ poker server on port 8085
echo ">> Starting C++ poker server on port 8085..."
./build/poker_server 8085 &
SERVER_PID=$!

cleanup() {
    echo ""
    echo ">> Stopping servers..."
    kill $SERVER_PID 2>/dev/null || true
    fuser -k 8085/tcp 2>/dev/null || true
    exit 0
}
trap cleanup SIGINT SIGTERM EXIT

sleep 0.5

# Start Next.js frontend with Bun
echo ">> Starting Next.js frontend with Bun on http://localhost:3000 ..."
cd frontend
bun run dev -p 3000
