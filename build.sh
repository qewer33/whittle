#!/usr/bin/env bash
set -euo pipefail

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITARM="${DEVKITARM:-$DEVKITPRO/devkitARM}"
export PATH="$DEVKITPRO/tools/bin:$DEVKITARM/bin:$PATH"

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build"

if [ ! -f "$BUILD/CMakeCache.txt" ]; then
    cmake -B "$BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$DEVKITPRO/cmake/3DS.cmake" "$ROOT"
fi

cmake --build "$BUILD"

TARGET="$(ls "$BUILD"/*.3dsx 2>/dev/null | head -1)"
if [ -n "$TARGET" ]; then
    echo "Built: $TARGET"
else
    echo "Build finished, but no .3dsx found"
fi
