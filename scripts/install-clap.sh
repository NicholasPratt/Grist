#!/usr/bin/env bash
set -euo pipefail

# Build + install Grist.clap into a user CLAP plugin folder.
#
# Usage:
#   scripts/install-clap.sh            # build + copy to default folder
#   scripts/install-clap.sh --clean    # clean build first
#   scripts/install-clap.sh --dest ~/.clap
#
# Environment:
#   CLAP_PATH  If set, used as destination (first entry if ':' separated).

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PLUGIN_SRC="$ROOT_DIR/bin/Grist.clap"

CLEAN=0
DEST=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --clean) CLEAN=1; shift ;;
    --dest) DEST="$2"; shift 2 ;;
    -h|--help)
      sed -n '1,120p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown arg: $1" >&2
      exit 2
      ;;
  esac
done

# Pick destination
if [[ -z "$DEST" ]]; then
  if [[ -n "${CLAP_PATH:-}" ]]; then
    DEST="${CLAP_PATH%%:*}"
  elif [[ -d "$HOME/.clap" ]]; then
    DEST="$HOME/.clap"
  else
    # Common user-level location
    DEST="$HOME/.local/share/clap"
  fi
fi

mkdir -p "$DEST"

echo "==> Building Grist (CLAP)"
if [[ $CLEAN -eq 1 ]]; then
  make -C "$ROOT_DIR/plugins/Grist" clean
fi
make -C "$ROOT_DIR/plugins/Grist" -j

if [[ ! -e "$PLUGIN_SRC" ]]; then
  echo "Build succeeded but plugin not found at: $PLUGIN_SRC" >&2
  exit 1
fi

PLUGIN_DEST="$DEST/Grist.clap"

echo "==> Installing to: $PLUGIN_DEST"
# Copy as a directory bundle if applicable, otherwise file copy.
if [[ -d "$PLUGIN_SRC" ]]; then
  rm -rf "$PLUGIN_DEST"
  cp -a "$PLUGIN_SRC" "$PLUGIN_DEST"
else
  cp -a "$PLUGIN_SRC" "$PLUGIN_DEST"
fi

echo "==> Done"
echo "    If your host doesn't see it yet, rescan CLAP plug-ins."
