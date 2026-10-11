#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Formats the application's sources in place: clang-format, ruff format,
# nixfmt and gersemi (application plan §1.3). The "format" CMake target runs
# this. Run from the repository root.

set -euo pipefail

mapfile -t cpp_files < <(find app -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
mapfile -t python_files < <(find app -type f -name '*.py' | sort)
mapfile -t cmake_files < <(find app -type f \( -name CMakeLists.txt -o -name '*.cmake' \) | sort)

if [[ ${#cpp_files[@]} -gt 0 ]]; then
  clang-format -i "${cpp_files[@]}"
fi
if [[ ${#python_files[@]} -gt 0 ]]; then
  ruff format "${python_files[@]}"
fi
nixfmt flake.nix
gersemi --in-place "${cmake_files[@]}"
