#!/usr/bin/env python3
"""
    die.py

    Tools for reading the TMS5220NL die image (docs-tech/die_analysis/).
    Every position is in full-resolution pixels of the image, origin top
    left, x to the right and y down.

      python tools/die.py cache                     # decode the JPEG once, to a raw memmap
      python tools/die.py tiles                     # Deep Zoom pyramid and viewer.html
      python tools/die.py crop X Y W H [--scale S] [--out F]
      python tools/die.py crop --cell NAME          # a record of cells.json, with a margin
      python tools/die.py annotate [--out F]        # the overview with cells.json drawn on it

    The image and everything made from it stay under local/die_image/
    (git-ignored): the JPEG is 85 MB, its raw cache 809 MB and the tiles
    114 MB. The cell records (docs-tech/die_analysis/cells.json) and
    the figures made from them are committed.

    This file is part of speak5220.
    SPDX-License-Identifier: GPL-3.0-or-later
"""

import argparse
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

Image.MAX_IMAGE_PIXELS = None

ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), '..'))
LOCAL = os.path.join(ROOT, 'local', 'die_image')
JPEG = os.path.join(LOCAL, 'ti_tms5220nl_mcmaster_mz_mit20x.jpg')
RAW = os.path.join(LOCAL, 'die.rgb')
TILES = os.path.join(LOCAL, 'tiles')
DOCS = os.path.join(ROOT, 'docs-tech', 'die_analysis')
CELLS = os.path.join(DOCS, 'cells.json')

WIDTH, HEIGHT = 18074, 14916
TILE, OVERLAP = 256, 1

# Colour per kind of record in the annotated overview.
KIND_COLOUR = {
    'block': '#00e5ff',
    'pad': '#ff3df5',
    'cell': '#ffea00',
    'feature': '#7cff4d',
}


def image():
    """The die as an (H, W, 3) uint8 array: the raw cache if it exists,
    otherwise the JPEG (slow: about 10 s per decode)."""
    if os.path.exists(RAW):
        return np.memmap(RAW, dtype=np.uint8, mode='r', shape=(HEIGHT, WIDTH, 3))
    print('die.py: no raw cache, decoding the JPEG (run "die.py cache" once)',
          file=sys.stderr)
    return np.asarray(Image.open(JPEG).convert('RGB'))


def load_cells():
    with open(CELLS) as f:
        return json.load(f)


def cmd_cache(args):
    im = Image.open(JPEG).convert('RGB')
    assert im.size == (WIDTH, HEIGHT), im.size
    a = np.memmap(RAW, dtype=np.uint8, mode='w+', shape=(HEIGHT, WIDTH, 3))
    a[:] = np.asarray(im)
    a.flush()
    print(f'wrote {RAW} ({os.path.getsize(RAW) / 1e6:.0f} MB)')


VIEWER = """<!doctype html>
<html><head><meta charset="utf-8"><title>TMS5220NL die</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/openseadragon/4.1.0/openseadragon.min.js"></script>
<style>
html,body{margin:0;height:100%;background:#111;color:#ddd;font:13px sans-serif}
#v{position:absolute;inset:0}
#xy{position:absolute;left:8px;bottom:8px;background:#000a;padding:4px 8px;z-index:9}
.lbl{color:#fff;background:#000a;font:12px sans-serif;padding:1px 3px;white-space:nowrap}
</style></head><body>
<div id="v"></div><div id="xy">x, y</div>
<script>
const W=@W@, H=@H@, cells=@CELLS@, colour=@COLOUR@;
const v=OpenSeadragon({id:'v',prefixUrl:'https://cdnjs.cloudflare.com/ajax/libs/openseadragon/4.1.0/images/',
  tileSources:'die.dzi',maxZoomPixelRatio:4,showNavigator:true,gestureSettingsMouse:{clickToZoom:false}});
v.addHandler('open',()=>{
  for(const c of cells){
    const [x,y,w,h]=c.box, col=colour[c.kind]||'#fff';
    const r=document.createElement('div');
    r.style.cssText=`border:2px dashed ${col};box-sizing:border-box`;
    r.title=c.name+(c.notes?': '+c.notes:'');
    v.addOverlay({element:r,location:v.viewport.imageToViewportRectangle(x,y,w,h)});
    const l=document.createElement('div'); l.className='lbl'; l.textContent=c.name; l.style.borderLeft=`3px solid ${col}`;
    v.addOverlay({element:l,location:v.viewport.imageToViewportCoordinates(x,y),placement:'BOTTOM_LEFT'});
  }
});
// Image coordinates under the mouse, to copy into cells.json.
new OpenSeadragon.MouseTracker({element:v.container,moveHandler:e=>{
  const p=v.viewport.viewerElementToImageCoordinates(e.position);
  document.getElementById('xy').textContent=`x ${Math.round(p.x)}, y ${Math.round(p.y)}`;
}});
</script></body></html>
"""


def write_viewer():
    cells = load_cells() if os.path.exists(CELLS) else []
    with open(os.path.join(TILES, 'viewer.html'), 'w') as f:
        f.write(VIEWER.replace('@W@', str(WIDTH)).replace('@H@', str(HEIGHT))
                .replace('@CELLS@', json.dumps(cells))
                .replace('@COLOUR@', json.dumps(KIND_COLOUR)))


def cmd_tiles(args):
    """A Deep Zoom (DZI) pyramid: level L is the image scaled by
    2^(L - top), cut into 256 px JPEG tiles with a 1 px overlap."""
    if args.viewer_only:
        write_viewer()
        print(f'wrote {TILES}/viewer.html')
        return
    top = math.ceil(math.log2(max(WIDTH, HEIGHT)))
    files = os.path.join(TILES, 'die_files')
    os.makedirs(files, exist_ok=True)
    level_im = Image.fromarray(np.asarray(image()))
    for level in range(top, -1, -1):
        w, h = level_im.size
        d = os.path.join(files, str(level))
        os.makedirs(d, exist_ok=True)
        for ty in range(math.ceil(h / TILE)):
            for tx in range(math.ceil(w / TILE)):
                x0 = max(tx * TILE - OVERLAP, 0)
                y0 = max(ty * TILE - OVERLAP, 0)
                x1 = min((tx + 1) * TILE + OVERLAP, w)
                y1 = min((ty + 1) * TILE + OVERLAP, h)
                level_im.crop((x0, y0, x1, y1)).save(
                    os.path.join(d, f'{tx}_{ty}.jpg'), quality=90)
        print(f'level {level}: {w} x {h}')
        level_im = level_im.resize((max(1, (w + 1) // 2), max(1, (h + 1) // 2)),
                                   Image.LANCZOS)
    with open(os.path.join(TILES, 'die.dzi'), 'w') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n'
                '<Image xmlns="http://schemas.microsoft.com/deepzoom/2008" '
                f'Format="jpg" Overlap="{OVERLAP}" TileSize="{TILE}">'
                f'<Size Width="{WIDTH}" Height="{HEIGHT}"/></Image>\n')
    write_viewer()
    print(f'wrote {TILES}/die.dzi and viewer.html')


def cmd_crop(args):
    if args.cell:
        cell = next((c for c in load_cells() if c['name'] == args.cell), None)
        if cell is None:
            sys.exit(f'die.py: no cell named {args.cell!r} in {CELLS}')
        x, y, w, h = cell['box']
        m = args.margin
        x, y, w, h = x - m, y - m, w + 2 * m, h + 2 * m
        out = args.out or f'{args.cell}.png'
    else:
        if args.box is None or len(args.box) != 4:
            sys.exit('die.py crop: give X Y W H, or --cell NAME')
        x, y, w, h = args.box
        out = args.out or f'crop_{x}_{y}_{w}_{h}.png'
    x0, y0 = max(x, 0), max(y, 0)
    x1, y1 = min(x + w, WIDTH), min(y + h, HEIGHT)
    im = Image.fromarray(np.asarray(image()[y0:y1, x0:x1]))
    if args.scale != 1:
        im = im.resize((max(1, round(im.width * args.scale)),
                        max(1, round(im.height * args.scale))), Image.LANCZOS)
    im.save(out)
    print(f'wrote {out}: x {x0}..{x1}, y {y0}..{y1}, {im.width} x {im.height} px')


def cmd_annotate(args):
    """The whole die scaled to --width, with every record of cells.json
    (or only those of --kind) drawn on it and labelled."""
    a = image()
    s = args.width / WIDTH
    step = max(1, int(1 / s / 2))     # decimate before resizing: much faster
    im = Image.fromarray(np.asarray(a[::step, ::step]))
    im = im.resize((args.width, round(HEIGHT * s)), Image.LANCZOS)
    d = ImageDraw.Draw(im)
    try:
        from matplotlib import font_manager
        font = ImageFont.truetype(
            font_manager.findfont('DejaVu Sans:bold'), args.font)
    except (ImportError, OSError):
        font = ImageFont.load_default()
    for c in load_cells():
        if args.kind and c['kind'] not in args.kind:
            continue
        x, y, w, h = (v * s for v in c['box'])
        col = KIND_COLOUR.get(c['kind'], '#ffffff')
        d.rectangle((x, y, x + w, y + h), outline=col, width=args.line)
        label = c.get('label', c['name'])
        if c.get('pin_name'):
            label += ' ' + c['pin_name'] + ('?' if c.get('pin_status') == 'working' else '')
        tx, ty = x + w + 4, y
        if c['kind'] == 'pad' and x > im.width / 2:
            tx = x - 4 - d.textlength(label, font=font)
        alt = (args.font + 4) * (int(c['name'][3:]) % 2) if c['kind'] == 'pad' else 0
        if c['kind'] == 'pad' and y > im.height * 0.85:
            tx, ty = x, y - args.font - 6 - alt
        elif c['kind'] == 'pad' and y < im.height * 0.06 and im.width * 0.04 < x < im.width * 0.9:
            tx, ty = x, y + h + 4 + alt
        elif c['kind'] != 'pad':
            tx, ty = x + 6, y + 4
        bb = d.textbbox((tx, ty), label, font=font)
        d.rectangle(bb, fill=(0, 0, 0))
        d.text((tx, ty), label, fill=col, font=font)
    out = args.out or os.path.join(DOCS, 'fig_floorplan.jpg')
    im.convert('RGB').save(out, quality=88)
    print(f'wrote {out}')


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n\n')[1].strip(),
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest='cmd', required=True)
    sub.add_parser('cache', help='decode the JPEG once, to a raw memmap')
    t = sub.add_parser('tiles', help='Deep Zoom pyramid and viewer.html')
    t.add_argument('--viewer-only', action='store_true',
                   help='rewrite viewer.html (after editing cells.json)')
    c = sub.add_parser('crop', help='a region at full or reduced scale')
    c.add_argument('box', nargs='*', type=int, metavar='X Y W H')
    c.add_argument('--cell', help='crop a record of cells.json')
    c.add_argument('--margin', type=int, default=100)
    c.add_argument('--scale', type=float, default=1.0)
    c.add_argument('--out')
    a = sub.add_parser('annotate', help='the overview with cells.json drawn on it')
    a.add_argument('--width', type=int, default=2000)
    a.add_argument('--kind', nargs='*', help='only these kinds (block, pad, cell, feature)')
    a.add_argument('--font', type=int, default=20)
    a.add_argument('--line', type=int, default=3)
    a.add_argument('--out')
    args = p.parse_args()
    {'cache': cmd_cache, 'tiles': cmd_tiles, 'crop': cmd_crop,
     'annotate': cmd_annotate}[args.cmd](args)


if __name__ == '__main__':
    main()
