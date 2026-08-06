#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

FTP_HOST="${FTP_HOST:-192.168.1.44}"
FTP_PORT="${FTP_PORT:-5000}"
DEST_DIR="${DEST_DIR:-/3ds/whittle}"

TARGET_3DSX="$(ls "$ROOT"/build/*.3dsx 2>/dev/null | head -1)"
if [ -z "$TARGET_3DSX" ]; then
    echo "No .3dsx found in build/, run ./build.sh first" >&2
    exit 1
fi

echo "Uploading $(basename "$TARGET_3DSX") to ftp://$FTP_HOST:$FTP_PORT$DEST_DIR/"
curl -sS --fail --ftp-create-dirs \
    --connect-timeout 5 --max-time 60 \
    -u anonymous: \
    -T "$TARGET_3DSX" \
    "ftp://$FTP_HOST:$FTP_PORT$DEST_DIR/$(basename "$TARGET_3DSX")"

echo "Done, launch it from Homebrew Launcher"
