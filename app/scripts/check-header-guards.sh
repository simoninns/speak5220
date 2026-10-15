#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks header guards: SPEAK5220_ followed by the header's path from
# app/src/ (or from app/ for headers outside src/), upper case, with every
# other character replaced by "_", and a trailing "_".
# Example: app/src/core/lpc/frame_codec.h -> SPEAK5220_CORE_LPC_FRAME_CODEC_H_
#
# cpplint's own guard check cannot add the SPEAK5220_ prefix, so it is
# disabled in app/CPPLINT.cfg and this script replaces it.
#
# Usage: check-header-guards.sh [FILE...]
# With no arguments, checks every .h under app/. Run from the repository root.

set -euo pipefail

if [[ $# -eq 0 ]]; then
  mapfile -t candidates < <(find app -type f -name '*.h' | sort)
else
  candidates=("$@")
fi

failures=0
checked=0
for path in "${candidates[@]}"; do
  path=${path#./}
  [[ $path == app/*.h ]] || continue
  checked=$((checked + 1))

  relative=${path#app/}
  relative=${relative#src/}
  expected="SPEAK5220_$(tr '[:lower:]' '[:upper:]' <<<"$relative" | tr -c 'A-Z0-9\n' '_')_"

  if ! grep -qx "#ifndef $expected" "$path" ||
    ! grep -qx "#define $expected" "$path" ||
    ! grep -qx "#endif  // $expected" "$path"; then
    echo "$path: expected header guard $expected (#ifndef, #define and '#endif  // $expected')"
    failures=$((failures + 1))
  fi
done

if [[ $failures -gt 0 ]]; then
  echo "check-header-guards: $failures problem(s)"
  exit 1
fi
echo "check-header-guards: $checked file(s) OK"
