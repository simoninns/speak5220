#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Restores the encoder's test corpus into the gitignored local/corpus/.
# The corpus (8 kHz 16-bit mono FLAC: real speech in recordings/, Piper
# voices in tts/, each with its index.csv and licences in README.md) is not
# in the working tree; it is taken from the repository's history, from
# investigation/captures/phase11/inputs/ at commit c537c1d, the last
# commit that has it. generate_corpus.py, corpus-check.sh, arctic-project.sh
# and golden/encoder.py read it from there.
#
# Usage: fetch-corpus.sh [DIR]   (default local/corpus, from the repository root)

set -euo pipefail

commit=c537c1d
source=investigation/captures/phase11/inputs
dir=${1:-local/corpus}

if [[ -e $dir ]]; then
  echo "fetch-corpus: $dir already exists; remove it to fetch again" >&2
  exit 1
fi
mkdir -p "$dir"
git archive "$commit" "$source" | tar -x -C "$dir" --strip-components=4
echo "fetch-corpus: $(find "$dir" -name '*.flac' | wc -l | tr -d ' ') files in $dir"
