#!/bin/sh
# Builds Roadkill on macOS or Linux: ./scripts/build.sh [debug]
# Needs CMake and SDL2 (macOS: brew install cmake sdl2; Debian / Ubuntu: apt install cmake
# libsdl2-dev; Fedora: dnf install cmake SDL2-devel; Arch: pacman -S cmake sdl2).
set -e
cd "$(dirname "$0")/.."
type=Release
[ "$1" = debug ] && type=Debug
for tool in cmake cc; do
    command -v $tool >/dev/null 2>&1 || { echo "roadkill: $tool not found (see the top of scripts/build.sh)"; exit 1; }
done
jobs=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)
gen=""
[ ! -f build/CMakeCache.txt ] && command -v ninja >/dev/null 2>&1 && gen="-G Ninja"
cmake -S . -B build $gen -DCMAKE_BUILD_TYPE=$type
cmake --build build -j "$jobs"
echo
echo "Built build/roadkill. Put your Twisted Metal 2 PC disc files in disc/files/ (or set TM2_DATA),"
echo "then run ./build/roadkill"
