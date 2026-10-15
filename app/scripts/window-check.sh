#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The by-hand check of every window and dialog: renders the main window's
# tabs, the Optimise window, and the Export, Preferences and About dialogs,
# in light and dark, at 1x and 2x pixel density, to PNG files to be looked
# at for unreadable text, colours of the wrong scheme, and blurred or
# clipped drawing.
#
# Usage: window-check.sh [BUILD_DIR]
# BUILD_DIR defaults to build/app (the GUI functional tests must be built).
# The images go to BUILD_DIR/project-output/window-check/. Run from the
# repository root:
#
#   nix develop .#app --command app/scripts/window-check.sh

set -euo pipefail

build_dir=${1:-build/app}
tests=$build_dir/tests/gui/speak5220_gui_functional_tests

if [[ ! -x $tests ]]; then
  echo "window-check: $tests not found; build it with:" >&2
  echo "  cmake --build $build_dir --target speak5220_gui_functional_tests" >&2
  exit 2
fi

output_dir=$build_dir/project-output/window-check
mkdir -p "$output_dir"
SPEAK5220_WINDOW_CHECK_DIR=$output_dir QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-offscreen} \
  "$tests" --gtest_also_run_disabled_tests \
  --gtest_filter='WindowCheck.*'
echo "window-check: images in $output_dir"
