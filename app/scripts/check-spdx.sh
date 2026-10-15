#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks the SPDX headers of AGENTS.md §4.2:
#   - every source file carries both SPDX lines;
#   - a C++ file's "File:" line matches its filename exactly.
#
# Usage: check-spdx.sh [FILE...]
# With no arguments, checks every source file under app/ (with the
# packaging files and the desktop metadata), flake.nix and the
# workflows in .github/workflows/. With arguments (as the pre-commit hook
# passes them), checks those of the given files that are source files.
# Run from the repository root.

set -euo pipefail

license_line='SPDX-License-Identifier: GPL-3.0-or-later'
copyright_pattern='SPDX-FileCopyrightText: \(c\) [0-9]{4}'

# Succeeds if the path is a source file that needs a header.
is_source_file() {
  local path=$1 name
  name=$(basename "$path")
  case "$path" in
  # The BBC Micro player's BASIC source carries its header in REM lines;
  # the reference programs under app/golden/ are data.
  app/src/*.bas) return 0 ;;
  # The Flatpak manifest and the vcpkg triplets
  app/packaging/*) return 0 ;;
  app/*)
    case "$name" in
    # YAML under app/ (projects, golden data) is data, not source.
    *.cpp | *.h | *.cmake | CMakeLists.txt | *.nix | *.sh | *.py | *.lua | *.ui | *.qrc) return 0 ;;
    *.desktop | *.metainfo.xml) return 0 ;;
    *) return 1 ;;
    esac
    ;;
  flake.nix) return 0 ;;
  .github/workflows/*.yml | .github/workflows/*.yaml) return 0 ;;
  *) return 1 ;;
  esac
}

if [[ $# -eq 0 ]]; then
  mapfile -t candidates < <(
    {
      find app -type f -not -path '*/__pycache__/*'
      echo flake.nix
      find .github/workflows -type f
    } | sort
  )
else
  candidates=("$@")
fi

failures=0
checked=0
for path in "${candidates[@]}"; do
  path=${path#./}
  is_source_file "$path" || continue
  checked=$((checked + 1))

  # Headers belong at the top of the file.
  head_text=$(head -n 12 "$path")
  if ! grep -qF "$license_line" <<<"$head_text"; then
    echo "$path: missing '$license_line' in the first 12 lines"
    failures=$((failures + 1))
  fi
  if ! grep -qE "$copyright_pattern" <<<"$head_text"; then
    echo "$path: missing 'SPDX-FileCopyrightText: (c) <year> <holder>' in the first 12 lines"
    failures=$((failures + 1))
  fi

  case "$path" in
  *.cpp | *.h)
    name=$(basename "$path")
    file_line=$(grep -m1 -E '^ \* File: ' <<<"$head_text" || true)
    declared=$(sed -E 's/^ \* File: +//; s/[[:space:]]+$//' <<<"$file_line")
    if [[ -z $file_line ]]; then
      echo "$path: missing ' * File:' line in the header block"
      failures=$((failures + 1))
    elif [[ $declared != "$name" ]]; then
      echo "$path: 'File: $declared' does not match the filename '$name'"
      failures=$((failures + 1))
    fi
    ;;
  esac
done

if [[ $failures -gt 0 ]]; then
  echo "check-spdx: $failures problem(s); see AGENTS.md §4.2 for the header templates"
  exit 1
fi
echo "check-spdx: $checked file(s) OK"
