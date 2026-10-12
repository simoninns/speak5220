#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Checks the BBC Micro discs on an emulated BBC Micro (plan Phase 10,
# TESTING.md): exports the bbc fixture suite with run-projects.sh, then
# boots each disc in MAME's BBC Micro Model B with the Acorn speech system
# and Acorn DFS, and drives the player (app/scripts/bbc/emulator_check.py
# says what is checked). Run by hand, in the default Nix shell, which has
# the Python packages it needs; MAME is taken from PATH, or else from
# nixpkgs with "nix shell".
#
# Usage: bbc-check.sh [BUILD_DIR [ROMS]]
# BUILD_DIR defaults to build/app (speak5220-cli must be built). ROMS is
# MAME's ROM directory, default local/mame/roms (git-ignored): the bbcb,
# saa5050 and bbc_acorn8271 sets, which are not part of the repository.
# The recordings, logs and summary go to BUILD_DIR/bbc-check/. Exits 1 if
# a check fails. Run from the repository root:
#
#   nix develop --command app/scripts/bbc-check.sh

set -euo pipefail

build_dir=${1:-build/app}
roms=${2:-local/mame/roms}
script_dir=$(dirname "${BASH_SOURCE[0]}")

if [[ ! -d $roms ]]; then
  echo "bbc-check: no MAME ROMs in $roms (see app/scripts/bbc/emulator_check.py)" >&2
  exit 2
fi

"$script_dir/run-projects.sh" bbc "$build_dir"
python3 "$script_dir/bbc/emulator_check.py" --build-dir "$build_dir" --roms "$roms"
