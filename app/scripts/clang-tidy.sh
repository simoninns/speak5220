#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Runs clang-tidy with app/.clang-tidy (application plan §1.3), using the
# compile_commands.json of a configured build directory.
#
# Usage: clang-tidy.sh [-p BUILD_DIR] [FILE...]
# BUILD_DIR defaults to build/app. With no files, checks every .cc under
# app/src and app/tests (headers are checked through the files that include
# them). Run from the repository root.

set -euo pipefail

build_dir=build/app
if [[ ${1:-} == -p ]]; then
  build_dir=$2
  shift 2
fi

if [[ ! -f $build_dir/compile_commands.json ]]; then
  cat <<MESSAGE
clang-tidy needs a configured build directory, and there is no
$build_dir/compile_commands.json. Create one with:

  cmake -S app -B build/app -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build build/app

then commit again. (The build creates Qt's generated headers, which
clang-tidy also needs.)
MESSAGE
  exit 1
fi

if [[ $# -eq 0 ]]; then
  mapfile -t files < <(find app/src app/tests -type f -name '*.cc' | sort)
else
  files=("$@")
fi
[[ ${#files[@]} -gt 0 ]] || exit 0

# Runs clang-tidy on one file, dropping its progress and suppressed-warning
# counts from stderr so that only diagnostics are shown.
tidy_one() {
  clang-tidy -p "$1" --quiet "$2" 2> >(
    grep -vE '^[0-9]+ warnings? generated\.$|^\[[0-9]+/[0-9]+\] Processing file' >&2
  )
}
export -f tidy_one

jobs=$(nproc 2>/dev/null || echo 4)
# The single quotes are intended: the child bash expands $0 and $1.
# shellcheck disable=SC2016
printf '%s\0' "${files[@]}" |
  xargs -0 -n 1 -P "$jobs" bash -c 'tidy_one "$0" "$1"' "$build_dir"
