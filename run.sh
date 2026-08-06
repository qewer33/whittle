#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"$ROOT/build.sh"

TARGET="$(ls "$ROOT/build"/*.3dsx 2>/dev/null | head -1)"
if [ -z "$TARGET" ]; then
    echo "No .3dsx found in build/" >&2
    exit 1
fi

AZAHAR="${AZAHAR:-$HOME/Applications/azahar.AppImage}"

if [ ! -f "$AZAHAR" ]; then
    echo "Emulator not found at: $AZAHAR (set AZAHAR=/path/to/azahar.AppImage)" >&2
    exit 1
fi

echo "Launching $TARGET in Azahar"
"$AZAHAR" "$TARGET" >/dev/null 2>&1 &
