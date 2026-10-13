#!/usr/bin/env bash
# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Generates the application's icon set and the About box's logo in
# app/resources/ from the artwork in assets/logo/ (assets/README.md). Run
# it by hand after changing the artwork, and commit what it writes:
#
#   - icons/hicolor/SIZExSIZE/apps/io.github.simoninns.speak5220.png, the
#     Linux icon theme sizes, installed with the .desktop file;
#   - speak5220.ico (Windows), speak5220.icns (macOS);
#   - speak5220-icon.png, the window icon, built into the executable;
#   - speak5220-logo.png and speak5220-logo@2x.png, the About box's logo.
#
# The tools come from nixpkgs. Run from the repository root:
#
#   nix shell nixpkgs#librsvg nixpkgs#imagemagick nixpkgs#libicns \
#     --command app/scripts/generate-icons.sh

set -euo pipefail

icon_svg=assets/logo/speak5220-icon.svg
logo_svg=assets/logo/speak5220-logo.svg
out=app/resources
app_id=io.github.simoninns.speak5220
# The logo's width in the About box, in device-independent pixels.
logo_width=320

for tool in rsvg-convert magick png2icns; do
  if ! command -v "$tool" >/dev/null; then
    echo "generate-icons.sh: $tool not found (see the usage above)" >&2
    exit 1
  fi
done

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

png() {
  rsvg-convert --width "$2" --height "$2" --output "$3" "$1"
}

for size in 16 24 32 48 64 128 256 512; do
  png "$icon_svg" "$size" "$work/$size.png"
  mkdir -p "$out/icons/hicolor/${size}x${size}/apps"
  cp "$work/$size.png" "$out/icons/hicolor/${size}x${size}/apps/$app_id.png"
done
png "$icon_svg" 1024 "$work/1024.png"

cp "$work/256.png" "$out/speak5220-icon.png"
magick "$work/16.png" "$work/24.png" "$work/32.png" "$work/48.png" \
  "$work/64.png" "$work/128.png" "$work/256.png" "$out/speak5220.ico"
png2icns "$out/speak5220.icns" "$work/16.png" "$work/32.png" \
  "$work/128.png" "$work/256.png" "$work/512.png" "$work/1024.png" >/dev/null

rsvg-convert --width "$logo_width" --keep-aspect-ratio \
  --output "$out/speak5220-logo.png" "$logo_svg"
rsvg-convert --width $((logo_width * 2)) --keep-aspect-ratio \
  --output "$out/speak5220-logo@2x.png" "$logo_svg"
