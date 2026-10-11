#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks C++ formatting without changing anything (application plan §1.3).
# On failure, prints the command that fixes the files.
#
# Usage: clang-format-check.sh [FILE...]
# With no arguments, checks every .cpp and .h under app/. Run from the
# repository root.

set -euo pipefail

if [[ $# -eq 0 ]]; then
  mapfile -t files < <(find app -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
else
  files=("$@")
fi
[[ ${#files[@]} -gt 0 ]] || exit 0

if ! clang-format --dry-run -Werror "${files[@]}"; then
  echo
  echo "Formatting check failed. To fix it, run:"
  echo "  clang-format -i ${files[*]}"
  echo "or, for every file: cmake --build build/app --target format"
  exit 1
fi
