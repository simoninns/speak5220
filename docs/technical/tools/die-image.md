# Die image

`die.py` reads the TMS5220NL die photograph ([Die analysis](../die-analysis/index.md)). The image goes in `investigation/local/die_image/` (git-ignored), along with everything made from it. All positions are full-resolution pixels, origin top left.

```
python investigation/tools/die.py cache                   # decode the 85 MB JPEG once, to a raw memmap (809 MB)
python investigation/tools/die.py tiles                   # Deep Zoom pyramid and viewer.html (114 MB)
python -m http.server -d investigation/local/die_image/tiles   # then open http://localhost:8000/viewer.html
python investigation/tools/die.py crop 16500 4300 1300 1600 --scale 0.75 --out p.png
python investigation/tools/die.py crop --cell pad21       # a record of cells.json, with a margin
python investigation/tools/die.py annotate                # redraw docs/technical/die-analysis/img/fig_floorplan.jpg
python investigation/tools/die.py tiles --viewer-only     # refresh the viewer's overlay after editing cells.json
python investigation/tools/die.py rom                     # read the parameter and chirp ROMs (die-analysis/roms.md)
python investigation/tools/die.py copies                  # the multiplier cell against its copies, and their average
python investigation/tools/die.py lowend                  # each stage's end rows against its middle rows (die-analysis/multiplier-low-end.md)
python investigation/tools/die.py summer                  # block B's rows, and the MR lines from B to stage 0 (die-analysis/adder-and-delay-stack.md)
```

The ROM reads are described in [ROMs](../die-analysis/roms.md), the low-end comparison in [Multiplier low end](../die-analysis/multiplier-low-end.md), and block B in [Adder and delay stack](../die-analysis/adder-and-delay-stack.md).

The viewer draws every record of [`cells.json`](../die-analysis/data/cells.json) (`docs/technical/die-analysis/data/cells.json`) and shows the image coordinates under the mouse, ready to copy into a new record.
