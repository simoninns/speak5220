#!/usr/bin/env bash
# Phase 11 bench run: every set, 3 takes per path, standard procedure
cd "$(dirname "$0")/../.."
for s in hand phrom rec tts2 tts1; do
    echo "=== $s $(date -Is)"
    python3 tools/run.py captures/phase11/data/$s.json all --takes 3 --path both \
        --out captures/phase11/takes --zip-only --resume --timeout-ms 30000
    echo "=== $s done $? $(date -Is)"
done
