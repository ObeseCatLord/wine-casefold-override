#!/usr/bin/env bash
set -Eeuo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
TEST_LIBRARY="${TEST_LIBRARY:-$BUILD_DIR/libwinecasefold.so}"
TEST_BINARY="${TEST_BINARY:-$BUILD_DIR/test_casefold_override}"
TEST_LIBRARY="$(realpath -e "$TEST_LIBRARY")"
TEST_BINARY="$(realpath -e "$TEST_BINARY")"
TMP_DIR="$(mktemp -d /tmp/wine-casefold-test.XXXXXX)"
trap 'rm -rf -- "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/root/child" "$TMP_DIR/root-sibling" "$TMP_DIR/outside"

plain="$($TEST_BINARY \
  "$TMP_DIR/root/child" "$TMP_DIR/root-sibling" "$TMP_DIR/outside")"
if grep -q 'casefold=1' <<<"$plain"; then
  printf 'unexpected native casefold flag in test filesystem\n%s\n' "$plain" >&2
  exit 1
fi

scoped="$(
  TEST_FAULT_SAFE_OUTPUT=1 \
  WINE_CASEFOLD_ROOTS="$TMP_DIR/root/" \
  LD_PRELOAD="$TEST_LIBRARY" \
  "$TEST_BINARY" \
    "$TMP_DIR/root/child" "$TMP_DIR/root-sibling" "$TMP_DIR/outside"
)"

mapfile -t lines <<<"$scoped"
[[ "${lines[0]}" == *'casefold=1' ]] || {
  printf 'configured child was not treated as casefolded\n%s\n' "$scoped" >&2
  exit 1
}
[[ "${lines[1]}" == *'casefold=0' ]] || {
  printf 'boundary sibling was incorrectly treated as casefolded\n%s\n' "$scoped" >&2
  exit 1
}
[[ "${lines[2]}" == *'casefold=0' ]] || {
  printf 'outside directory was incorrectly treated as casefolded\n%s\n' "$scoped" >&2
  exit 1
}

relative="$(
  cd "$TMP_DIR/root/child"
  WINE_CASEFOLD_ROOTS="$TMP_DIR/root" \
  LD_PRELOAD="$TEST_LIBRARY" \
  "$TEST_BINARY" .
)"
[[ "$relative" == *'casefold=1' ]] || {
  printf 'relative descriptor path was not recognized\n%s\n' "$relative" >&2
  exit 1
}

printf 'wine-casefold override tests passed\n'
