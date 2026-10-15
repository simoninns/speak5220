#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Builds a project of real speech for trying the GUI by hand: the 33 CMU
# ARCTIC prompts (a0001 to a0033) of the speech corpus recordings
# (investigation/captures/phase11/inputs/recordings/, CC BY-SA 4.0), each
# added as a phrase with the prompt's text (from the recordings'
# index.csv) as its description, and encoded with the default options.
# Open it in speak5220 to compare the original and encoded audio. The project is
# derived from committed files, so it is generated, not committed
# (TESTING.md); running the script again replaces it.
#
# Usage: arctic-project.sh [BUILD_DIR [OUTPUT_DIR]]
# BUILD_DIR defaults to build/app (speak5220-cli must be built); the
# project is written to OUTPUT_DIR/arctic.s5proj, OUTPUT_DIR defaulting to
# BUILD_DIR/test-projects. The corpus WAVs are written to
# BUILD_DIR/generated-corpus. Run from the repository root in the default
# Nix shell, which has the Python packages the corpus step needs:
#
#   nix develop --command app/scripts/arctic-project.sh

set -euo pipefail

build_dir=${1:-build/app}
output_dir=${2:-$build_dir/test-projects}
script_dir=$(dirname "${BASH_SOURCE[0]}")
cli=$build_dir/src/cli/speak5220-cli

if [[ ! -x $cli ]]; then
  echo "arctic-project: $cli not found; build it with:" >&2
  echo "  cmake --build $build_dir --target speak5220-cli" >&2
  exit 2
fi

recordings=$script_dir/../../investigation/captures/phase11/inputs/recordings
corpus=$build_dir/generated-corpus
python3 "$script_dir/generate_corpus.py" --output "$corpus"

# Each ARCTIC recording's file stem and prompt text, tab-separated, in
# index order.
prompts=$(python3 - "$recordings/index.csv" <<'PYTHON'
import csv
import sys

with open(sys.argv[1], newline="", encoding="utf-8") as index:
    for row in csv.DictReader(index):
        if row["id"].startswith("arctic_"):
            print(row["file"].removesuffix(".flac") + "\t" + row["text"])
PYTHON
)
if [[ -z $prompts ]]; then
  echo "arctic-project: no ARCTIC prompts in $recordings/index.csv" >&2
  exit 1
fi

mkdir -p "$output_dir"
project=$output_dir/arctic.s5proj
rm -f "$project" "$project~"
"$cli" new "$project" --name ARCTIC
count=0
while IFS=$'\t' read -r stem text; do
  "$cli" add "$project" "$corpus/$stem.wav" --description "$text"
  count=$((count + 1))
done <<<"$prompts"
"$cli" encode "$project"
echo "arctic-project: $count phrases in $project"
