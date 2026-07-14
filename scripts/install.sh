#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_DIR="${WINE_CASEFOLD_INSTALL_DIR:-$HOME/.local/lib/wine-casefold-override}"

make -C "$ROOT_DIR" test
install -d "$INSTALL_DIR/lib"
install -m 0755 "$ROOT_DIR/build/libwinecasefold.so" \
  "$INSTALL_DIR/lib/libwinecasefold.so"

if make -C "$ROOT_DIR" test32; then
  install -d "$INSTALL_DIR/lib32"
  install -m 0755 "$ROOT_DIR/build/libwinecasefold32.so" \
    "$INSTALL_DIR/lib32/libwinecasefold.so"
else
  printf '%s\n' \
    'warning: 32-bit build failed; install multilib C development packages and rerun' >&2
fi

printf 'installed wine-casefold-override in %s\n' "$INSTALL_DIR"
