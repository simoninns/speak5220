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
      python tools/die.py rom                       # read the parameter and chirp ROMs
      python tools/die.py copies                    # the multiplier cell against its copies

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

# The TMS5200NL image (siliconprawn), kept as its 250 px map tiles. Its
# layout is the same as the TMS5220NL's, at a ~45 px lower offset.
TILES_5200 = os.path.join(LOCAL, 'tms5200nl', 'tiles')
TILE_5200 = 250
OFFSET_5200 = (0, 45)

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


# The two mask ROMs (phase2.md). Gate lines are vertical metal; a stored
# transistor is a thin-oxide window in the metal over the gap between two
# horizontal diffusion strips. Positions were measured from the image.
PROM_COLS = [12494, 12532, 12567, 12602, 12637, 12675, 12711, 12746, 12782,
             12820, 12856, 12892, 12927, 12966, 13002, 13037, 13072, 13112,
             13147, 13183, 13218, 13257, 13293, 13328, 13363, 13403, 13439,
             13474, 13510, 13549, 13584, 13619, 13655, 13693, 13728, 13764,
             13799, 13839, 13874, 13909, 13945, 13984, 14019, 14055, 14090,
             14129, 14165, 14201, 14236, 14275, 14310, 14346, 14382, 14421,
             14457, 14492, 14527, 14566, 14602, 14637, 14673, 14712, 14747,
             14782, 14818, 14857, 14892, 14928, 14963, 15005, 15044, 15083,
             15122]
PROM_STRIP0, PROM_PITCH = 11537.57, 31.8555   # diffusion strip k at y0 + k * pitch
# Physical column -> (table, block of four indices). Columns 21, 30, 39, 44,
# 49 and 54 hold a filler pattern; 69-72 are the gap selectors.
PROM_LAYOUT = {}
for _t, _cols in [('energy', [1, 2, 3, 4]),
                  ('pitch', [17, 18, 19, 20, 13, 14, 15, 16, 9, 10, 11, 12, 5, 6, 7, 8]),
                  ('k1', [26, 27, 28, 29, 22, 23, 24, 25]),
                  ('k2', [35, 36, 37, 38, 31, 32, 33, 34]),
                  ('k3', [41, 40, 42, 43]), ('k4', [45, 46, 47, 48]),
                  ('k5', [50, 51, 52, 53]), ('k6', [55, 56, 57, 58]),
                  ('k7', [59, 60, 61, 62]), ('k8', [63, 64]), ('k9', [65, 66]),
                  ('k10', [67, 68])]:
    for _b, _c in enumerate(_cols):
        PROM_LAYOUT[_c] = (_t, _b)
# Sites where the metal is damaged or covered, so the window cannot be seen.
PROM_UNREADABLE = {(57, 11), (57, 12), (57, 13), (57, 14), (61, 26)}
# Sites under a dark particle, which the automatic reading takes for a
# window. Each was read by eye at full resolution: no window.
PROM_BY_EYE = {(18, 6): 0, (42, 26): 0, (59, 36): 0, (60, 26): 0, (61, 24): 0}

CHIRP_COLS = [10827, 10862, 10898, 10937, 10972, 11008, 11044, 11083, 11118,
              11154, 11190, 11228, 11264]          # the 13 data columns
CHIRP_SEL_COLS = [11301, 11337, 11375, 11411]
# Diffusion strips as (top edge, bottom edge): four bands of nine strips.
CHIRP_STRIPS = [(9006, 9016), (9038, 9048), (9070, 9080), (9102, 9112),
                (9134, 9154), (9176, 9186), (9208, 9218), (9240, 9250),
                (9272, 9281), (9304, 9313), (9336, 9345), (9367, 9377),
                (9399, 9408), (9431, 9451), (9474, 9483), (9505, 9515),
                (9537, 9546), (9569, 9578), (9600, 9610), (9632, 9642),
                (9664, 9674), (9696, 9706), (9728, 9748), (9770, 9780),
                (9802, 9812), (9834, 9844), (9866, 9875), (9898, 9908),
                (9930, 9939), (9961, 9971), (9993, 10003), (10025, 10046),
                (10068, 10077), (10099, 10109), (10132, 10141), (10163, 10172)]
CHIRP_BLOCK_COLS = [6, 5, 8, 7, 10, 9]  # data column of address blocks 0-5


def window_score(a, xc, ya, yb):
    """Fraction of dark pixels (red < 175) on a gate line between two
    strips: high where the metal steps down into a thin-oxide window."""
    return float((a[ya:yb, xc - 16:xc + 17, 0] < 175).mean())


def two_means(v):
    t = float(np.median(v))
    for _ in range(50):
        lo, hi = v[v < t], v[v >= t]
        if len(lo) == 0 or len(hi) == 0:
            break
        t = (lo.mean() + hi.mean()) / 2
    return t


def cmd_rom(args):
    """Read both ROMs, write bit and word tables, compare with the tables
    read through PROM OUT (parameter ROM) and Table 8-7 (chirp)."""
    import csv
    a = image()
    strip = lambda k: PROM_STRIP0 + PROM_PITCH * k
    # Parameter ROM: ten groups of five strips; gaps 5g+1..5g+4 hold bit g.
    S = np.zeros((50, len(PROM_COLS)))
    for c, xc in enumerate(PROM_COLS):
        for j in range(50):
            S[j, c] = window_score(a, xc, int(strip(j) + 7), int(strip(j + 1) - 7))
    bits = np.zeros(S.shape, bool)
    for c in range(len(PROM_COLS)):
        rows = [j for j in range(50) if j % 5]
        bits[:, c] = S[:, c] > two_means(S[rows, c])
    for (c, j), v in PROM_BY_EYE.items():
        bits[j, c] = bool(v)
    ref = {}
    with open(os.path.join(ROOT, 'captures', 'promout', 'chip1', 'rom.csv')) as f:
        for r in csv.DictReader(f):
            v = int(r['read'])
            ref[(r['parameter'], int(r['index']))] = (v // 4 if r['parameter'] == 'pitch' else v)
    rows_out, bad = [], 0
    for c, (t, b) in sorted(PROM_LAYOUT.items(), key=lambda kv: (kv[1][0], kv[1][1])):
        for r in range(4):
            i = 3 - r                  # index 4b+r is in gap 5g+1+(3-r)
            js = [5 * g + 1 + i for g in range(10)]
            w = sum(int(bits[j, c]) << g for g, j in enumerate(js))
            exp = ref[(t, 4 * b + r)] & 1023
            diff = [g for g in range(10) if (w ^ exp) >> g & 1]
            unread = [g for g, j in enumerate(js) if (c, j) in PROM_UNREADABLE]
            unexplained = [g for g in diff if g not in unread]
            bad += len(unexplained)
            val = w - 1024 if (t.startswith('k') and w >= 512) else w
            rows_out.append([t, 4 * b + r, c, PROM_COLS[c], val, exp if exp < 512 or not t.startswith('k') else exp - 1024,
                             ' '.join(map(str, diff)) or '-', ' '.join(map(str, unread)) or '-'])
    out = os.path.join(DOCS, 'rom_parameters.csv')
    with open(out, 'w', newline='') as f:
        wr = csv.writer(f)
        wr.writerow(['table', 'index', 'column', 'x', 'die', 'promout', 'differing_bits', 'unreadable_bits'])
        wr.writerows(rows_out)
    print(f'parameter ROM: {len(rows_out)} words, {len(rows_out) * 10} bits; '
          f'{sum(r[6] != "-" for r in rows_out)} words differ from PROM OUT '
          f'({len(PROM_BY_EYE)} sites read by eye), {bad} differing bits not on an '
          f'unreadable site; wrote {out}')

    # Chirp ROM: four bands of nine strips; band b's gaps 9b+u (upper half)
    # and 9b+7-u (lower half) hold address 4*block+u; window = 0.
    n = len(CHIRP_STRIPS)
    S = np.zeros((n - 1, len(CHIRP_COLS)))
    for c, xc in enumerate(CHIRP_COLS):
        for j in range(n - 1):
            S[j, c] = window_score(a, xc, CHIRP_STRIPS[j][1] + 2, CHIRP_STRIPS[j + 1][0] - 2)
    data = [j for j in range(n - 1) if j not in (8, 17, 26)]
    t = two_means(S[data].ravel())
    win = S > t
    table = [0, 3, 15, 40, 76, 108, 113, 80, 37, 38, 76, 68, 26, 50, 59, 19, 55,
             26, 37, 31, 29] + [0] * 31
    words = {}
    for c in range(len(CHIRP_COLS)):
        for u in range(4):
            v = 0
            for bnd in range(4):
                for h in range(2):
                    j = 9 * bnd + (u if h == 0 else 7 - u)
                    v |= int(not win[j, c]) << (7 - (2 * bnd + h))
            words[(c, u)] = v - 256 if v >= 128 else v
    zero_cols = [c for c in range(len(CHIRP_COLS)) if c not in CHIRP_BLOCK_COLS]
    out = os.path.join(DOCS, 'rom_chirp.csv')
    agree = 0
    with open(out, 'w', newline='') as f:
        wr = csv.writer(f)
        wr.writerow(['address', 'column', 'x', 'die', 'table_8_7'])
        for addr in range(52):
            blk, u = divmod(addr, 4)
            if blk < len(CHIRP_BLOCK_COLS):
                c = CHIRP_BLOCK_COLS[blk]
                v, cs, xs = words[(c, u)], str(c), str(CHIRP_COLS[c])
            else:      # blocks 6-12: the seven all-zero columns, order not known
                vs = {words[(c, u)] for c in zero_cols}
                v = vs.pop() if len(vs) == 1 else None
                cs, xs = 'one of ' + ' '.join(map(str, zero_cols)), '-'
            agree += v == table[addr]
            wr.writerow([addr, cs, xs, v, table[addr]])
    margin = (S[data][~win[data]].max(), S[data][win[data]].min())
    print(f'chirp ROM: 52 words of 8 bits, {agree} equal Table 8-7; '
          f'threshold {t:.3f}, closest scores {margin[0]:.3f} (no window) and '
          f'{margin[1]:.3f} (window); wrote {out}')


def crop_5200(x, y, w, h):
    """A region of the TMS5200NL image, assembled from its map tiles."""
    out = np.zeros((h, w, 3), np.uint8)
    T = TILE_5200
    for ty in range(y // T, (y + h - 1) // T + 1):
        for tx in range(x // T, (x + w - 1) // T + 1):
            f = os.path.join(TILES_5200, f'{tx}_{ty}.jpg')
            if not os.path.exists(f):
                continue
            t = np.asarray(Image.open(f).convert('RGB'))
            X, Y = tx * T, ty * T
            xa, xb = max(x, X), min(x + w, X + t.shape[1])
            ya, yb = max(y, Y), min(y + h, Y + t.shape[0])
            if xa < xb and ya < yb:
                out[ya - y:yb - y, xa - x:xb - x] = t[ya - Y:yb - Y, xa - X:xb - X]
    return out


# Block A: five column groups of 1,260 px, 14 bit rows of 452 px.
A_GROUPS = [1480, 2740, 4000, 5260, 6520]
A_ROW1, A_ROW, A_COL = 5734, 452, 1260


def best_match(img, tpl):
    """Normalised cross-correlation of tpl over img (green channel, at
    half resolution); the best score and its offset in img."""
    from scipy.signal import fftconvolve
    t = tpl[::2, ::2].astype(float)
    t = (t - t.mean()) / t.std()
    I = img[::2, ::2].astype(float)
    num = fftconvolve(I, t[::-1, ::-1], 'valid')
    ones = np.ones_like(t)
    s1 = fftconvolve(I, ones, 'valid')
    s2 = fftconvolve(I * I, ones, 'valid')
    var = np.maximum(s2 - s1 * s1 / t.size, 1e-9)
    ncc = num / np.sqrt(var) / np.sqrt((t * t).sum())
    k = np.unravel_index(ncc.argmax(), ncc.shape)
    return float(ncc[k]), 2 * int(k[1]), 2 * int(k[0])


def cmd_copies(args):
    """Match the reference multiplier cell (group 3, row 7) against every
    group and row of A, in this image and the TMS5200NL's. Print the
    scores, and save the average of the regular copies (groups 3-4, rows
    2-12) with its diffusion map: averaging removes the speckle that hides
    the diffusion in a single copy."""
    a = image()
    W, H = A_COL, A_ROW
    rx, ry = A_GROUPS[2], A_ROW1 + 6 * A_ROW
    ref = np.asarray(a[ry:ry + H, rx:rx + W, 1])
    found = []
    for name in ('5220', '5200'):
        print(f'{name}: score of each row 1-14 against the reference cell')
        for g, gx in enumerate(A_GROUPS, 1):
            scores = []
            for r in range(1, 15):
                y = A_ROW1 + A_ROW * (r - 1)
                if name == '5220':
                    m = 40
                    X, Y = gx - m, y - m
                    img = np.asarray(a[Y:Y + H + 2 * m, X:X + W + 2 * m, 1])
                else:
                    m = 120
                    X, Y = gx + OFFSET_5200[0] - m, y + OFFSET_5200[1] - m
                    img = crop_5200(X, Y, W + 2 * m, H + 2 * m)[..., 1]
                if name == '5220' and (gx, y) == (rx, ry):
                    s, dx, dy = 1.0, m, m
                else:
                    s, dx, dy = best_match(img, ref)
                scores.append(s)
                found.append((name, g, r, s, X + dx, Y + dy))
            print(f'  group {g}: ' + ' '.join(f'{s:.2f}' for s in scores))
    regular = [f for f in found if f[0] == '5220' and f[1] in (3, 4) and 2 <= f[2] <= 12]
    avg = np.mean([np.asarray(a[y:y + H, x:x + W]).astype(float)
                   for _, _, _, _, x, y in regular], 0)
    os.makedirs(LOCAL, exist_ok=True)
    out = args.out or os.path.join(LOCAL, 'cell_average.png')
    Image.fromarray(np.clip(avg, 0, 255).astype(np.uint8)).save(out)
    # Diffusion is the more saturated orange: (R - G) / R is 0.50-0.51 on
    # the parameter ROM's strips against 0.44-0.46 between them.
    R, G = avg[..., 0], avg[..., 1]
    sat = (R - G) / (R + 1)
    v = sat[G <= 165]
    lo, hi = np.percentile(v, [5, 95])
    grey = np.clip((sat - lo) / (hi - lo), 0, 1) * 255
    rgb = np.stack([grey] * 3, -1)
    rgb[G > 165] = (60, 100, 200)
    sat_out = out.replace('.png', '_diffusion.png')
    Image.fromarray(rgb.astype(np.uint8)).save(sat_out)
    print(f'averaged {len(regular)} copies; wrote {out} and {sat_out}')


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
    sub.add_parser('rom', help='read the parameter and chirp ROMs')
    k = sub.add_parser('copies', help='the multiplier cell against its copies, and their average')
    k.add_argument('--out', help='the averaged cell (default local/die_image/cell_average.png)')
    args = p.parse_args()
    {'cache': cmd_cache, 'tiles': cmd_tiles, 'crop': cmd_crop,
     'annotate': cmd_annotate, 'rom': cmd_rom, 'copies': cmd_copies}[args.cmd](args)


if __name__ == '__main__':
    main()
