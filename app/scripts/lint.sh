#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs every read-only check on all files, and reports each one that
# fails. The "lint" CMake target runs this.
#
# Usage: lint.sh [BUILD_DIR]
# BUILD_DIR (default build/app) supplies compile_commands.json for
# clang-tidy. Run from the repository root.

set -uo pipefail

build_dir=${1:-build/app}
script_dir=$(dirname "${BASH_SOURCE[0]}")

mapfile -t cpp_files < <(find app -type f \( -name '*.cpp' -o -name '*.h' \) | sort)
mapfile -t python_files < <(find app -type f -name '*.py' | sort)
mapfile -t shell_files < <(find app -type f -name '*.sh' | sort)
mapfile -t cmake_files < <(find app -type f \( -name CMakeLists.txt -o -name '*.cmake' \) | sort)

failed=()

# Runs one check, printing its name, and records it if it fails.
run_check() {
  local name=$1
  shift
  echo "== $name"
  if ! "$@"; then
    failed+=("$name")
  fi
}

run_check check-spdx "$script_dir/check-spdx.sh"
run_check check-header-guards "$script_dir/check-header-guards.sh"
run_check clang-format "$script_dir/clang-format-check.sh" "${cpp_files[@]}"
run_check cpplint cpplint --quiet "${cpp_files[@]}"
run_check clang-tidy "$script_dir/clang-tidy.sh" -p "$build_dir"
if [[ ${#python_files[@]} -gt 0 ]]; then
  run_check ruff ruff check "${python_files[@]}"
  run_check ruff-format ruff format --check "${python_files[@]}"
fi
run_check shellcheck shellcheck "${shell_files[@]}"
run_check nixfmt nixfmt --check flake.nix
run_check gersemi gersemi --check "${cmake_files[@]}"
run_check actionlint actionlint

if [[ ${#failed[@]} -gt 0 ]]; then
  echo "lint: failed: ${failed[*]}"
  exit 1
fi
echo "lint: all checks passed"
