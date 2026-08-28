#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

cmake -B build -DCMAKE_BUILD_TYPE=Release "$@"
cmake --build build --config Release
cmake --install build

echo ""
echo "Installed: ~/.clap/MambaCompressor.clap"
echo "Rescan plugins in your DAW (Bitwig / Reaper / etc.) to pick it up."
