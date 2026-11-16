#!/usr/bin/env sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=$(mktemp "${TMPDIR:-/tmp}/openplotter-firmware-tests.XXXXXX")
trap 'rm -f "$OUT"' EXIT

if ! command -v c++ >/dev/null 2>&1; then
  echo "A C++ compiler is required to run firmware unit tests (c++ not found)." >&2
  exit 127
fi

c++ -std=c++17 -Wall -Wextra -Werror -Wno-unused-parameter \
  -ffunction-sections -fdata-sections \
  -I"$ROOT" \
  "$ROOT/test/native/test_firmware.cpp" \
  "$ROOT/src/gcode/parser.cpp" \
  -Wl,--gc-sections \
  -o "$OUT"

"$OUT"
