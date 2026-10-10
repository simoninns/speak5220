# Preparation

The groundwork for reading the image: where it comes from and its licence, the tools and coordinates, the scale and the process, and the bond pads. The pads are matched to pins on the [floorplan](floorplan.md#pin-map) page, from the pads' drivers.

## Source and licence

| | |
|---|---|
| Page | [siliconpr0n: mcmaster:ti:tms5220nl](https://siliconpr0n.org/archive/doku.php?id=mcmaster:ti:tms5220nl). The original 2013 page was `digshadow:ti:tms5220nl` |
| Author | John McMaster ("digshadow"), decapped and imaged in April 2013. Published emulator sources credit him and Lord Nightmare with checking the LPC and chirp tables against this decap |
| Licence | CC BY 4.0 (CC BY 3.0 on the 2013 page). Any figure made from the image must credit "John McMaster, siliconpr0n, CC BY 4.0" |
| Part | TMS5220NL, package marked **DEP 8319 PHILIPPINES**. Chips 1 and 4 are DEP 8423, a year later |
| Image | Top metal only (`mz`), Mitutoyo 20× objective (`mit20x`), one stitched JPEG, 18,074 × 14,916 px, SHA-256 `42e5707a…6858` ([the image](index.md#the-image)) |

The site sits behind Cloudflare and refuses scripted fetches, so these details were read from Wayback Machine copies of the wiki pages.

**No delayered image of the TMS5220NL exists** in the archive. The same archive has top-metal images of related dies, all at 20×:

| Die | Marking | siliconpr0n map | Use in this analysis |
|---|---|---|---|
| TMS5220CNL | — | `/map/ti/tms5220cnl/mz_mit20x/` (15,931 × 12,936) | The later revision. If its multiplier differs, compare the two ([lowest-order arithmetic](lowest-order-arithmetic.md#56-more-parts)) |
| TMS5200NL | `T0285E 2501E`, 6,166 × 5,121 µm | `/map/ti/tms5200nl/mz_mit20x/` | Same die family (T0285E). Its page says the "(C) 1978" mark, which was in metal, is "etched off": the mark, not the image. The map is titled "TMS5200 top metal @ Mit20x" and its multiplier is fully metallised, so it is top metal like this one. Its layout matches this die's, so it can stand in where this image is damaged ([reference cell](reference-cell.md#gates-and-contacts)) |
| TMS5100NL | `TO280B 0281 A (C)3-78`, 5,288 × 5,388 µm | `/map/ti/tms5100nl/mz_mit20x/` | The filter of US 4,335,277 (target T1). Its multiplier is the patent's, so a cell that differs between the two dies is the first suspect for T1 |
| TMC0280FNL (CD2801) | — | `/map/ti/tmc0280fnl/mz_mit20x/` | The Speak & Spell chip |
| TMS5111NLL | `TMS5100AJ` | `/map/ti/tms5111nll/mz_mit20x/` | — |

No pad diagram, floorplan or circuit trace of any of these dies has been published. Published emulator notes cover only the ROM tables and a few behaviours.

**The ROMs have been read once already.** The 5220's LPC and chirp tables in published emulator sources are said to match the decap. Reading the parameter ROM is still worth doing as a test of the method ([ROMs](roms.md)), but a match tells us nothing new. A mismatch with [datasheet §8.1](../datasheet/coded-parameters.md) would be news.

## Tools and coordinates

`investigation/tools/die.py` (see [Tools](../tools/die-image.md), "Die image"):

| Command | What it does |
|---|---|
| `die.py cache` | Decodes the JPEG once into `investigation/local/die_image/die.rgb`, a raw 809 MB memmap. Every other command reads that, so a crop takes milliseconds instead of a 6 s decode |
| `die.py tiles` | Builds a Deep Zoom pyramid in `investigation/local/die_image/tiles/`: 5,648 tiles, 114 MB, levels 0–15. It also writes `viewer.html` (OpenSeadragon), which overlays `cells.json` and shows the image coordinates under the mouse. Serve it with `python -m http.server -d investigation/local/die_image/tiles`, because browsers block tile loads from `file://`. `--viewer-only` rewrites the overlay after `cells.json` changes |
| `die.py crop X Y W H [--scale S]` | Crops a region. `--cell NAME` crops a record of `cells.json`, with a margin |
| `die.py annotate` | Draws `cells.json` over the whole die. It makes `fig_floorplan.jpg` ([floorplan figure](index.md#floorplan)) |

Further commands read the ROMs (`rom`, [ROMs](roms.md)), average the copies of the multiplier cell (`copies`, [reference cell](reference-cell.md)), compare the multiplier's end rows (`lowend`, [multiplier low end](multiplier-low-end.md)) and compare block B's rows and trace the MR lines (`summer`, [adder and delay stack](adder-and-delay-stack.md)).

**Coordinates.** All positions are full-resolution pixels, origin top left, x right and y down. [`cells.json`](data/cells.json) holds one record per traced object: `name`, `label`, `kind` (block, pad, cell, feature), `box` as `[x, y, w, h]`, and `notes`. Pads also carry `side`, `bond`, `pin`, `pin_name` and `pin_status`.

**Orientation.** The die text (`TO285E`, `5220`, `© 11-78`) reads normally, so the image is the die seen from above, not mirrored.

## Scale

| Measure | Pixels | At 0.341 µm/px |
|---|---|---|
| Die, edge to edge in the image | 18,074 × 14,916 | 6.16 × 5.09 mm |
| Metal bus pitch (two bundles, FFT) | 35.3, 35.6 | 12.1 µm |
| Parameter ROM (C) cell | 36.3 × 31.9 | 12.4 × 10.9 µm |
| Datapath bit pitch (A, B) | 452 | 154 µm |
| Datapath column group (A) | ~1,080 | ~370 µm |
| Bond pad opening | 340–360 | 116–123 µm |

**Scale: 0.341 µm/px (±2 %).** It comes from the published size of the TMS5200NL die, 6,166 × 5,121 µm. That is the same T0285E design, imaged the same way: 6,166 / 18,074 = 0.341 and 5,121 / 14,916 = 0.343. The two images' aspect ratios agree to 0.6 %. The checks are consistent with it:

- a 12 µm metal pitch fits the 5–6 µm rules of a 1978 TI process;
- a 120 µm pad fits pads of that time (4–5 mil).

The ±2 % covers the difference between the two axes and the doubt about whether the TMS5200 figure includes the scribe lane. This is accurate enough for W/L ratios (targets T4 and T6), which do not depend on the absolute scale.

**Process.** The datasheet gives the supplies as `VSS` = +5 V (substrate) and `VDD` = −5 V, with ratings down to −20 V from `VSS`. That is the convention of a **P-channel** MOS process. In P-channel logic, a precharged node is pulled towards `VDD` and discharged to `VSS`. Any trace of a dynamic node (T4) has to be read with the polarities this way round.

## Pads

The die has **29 bond pads**, and every one carries a bond mark: 11 still hold a gold ball, and 18 show a torn wire. The package has 28 pins, so **one pin is bonded to two pads**. On a supply pin that is common practice.

The pads are numbered **P1–P29 counterclockwise from the top-left corner** (see the [floorplan figure](index.md#floorplan)). That is the direction in which pin numbers rise: the die faces up, the image is not mirrored, and bond wires cannot cross. So pin *n* + 1 is always on the next pad counterclockwise from pin *n*, apart from the one doubled pin. The whole pin map therefore comes down to two numbers: the pad of pin 1, and where the doubled pin is.

| Pad | Side | Centre (x, y) | Bond mark |
|---|---|---|---|
| P1 | left | 598, 925 | torn wire |
| P2 | left | 600, 3,152 | torn wire |
| P3 | left | 588, 4,345 | torn wire |
| P4 | left | 591, 6,421 | torn wire |
| P5 | left | 554, 7,146 | ball |
| P6 | left | 531, 8,927 | ball |
| P7 | left | 552, 10,091 | ball |
| P8 | left | 564, 11,534 | ball |
| P9 | left (corner) | 518, 14,224 | torn wire |
| P10 | bottom | 1,226, 14,461 | ball |
| P11 | bottom | 7,520, 14,326 | torn wire |
| P12 | bottom | 8,524, 14,407 | torn wire |
| P13 | bottom | 9,273, 14,336 | torn wire |
| P14 | bottom | 13,805, 14,332 | torn wire |
| P15 | bottom | 14,623, 14,353 | torn wire |
| P16 | right (corner) | 17,120, 14,258 | ball |
| P17 | right | 17,042, 11,902 | ball |
| P18 | right | 17,095, 11,074 | torn wire |
| P19 | right | 17,126, 10,539 | ball |
| P20 | right | 16,975, 5,622 | ball |
| P21 | right | 16,976, 4,594 | torn wire |
| P22 | right | 16,983, 1,520 | torn wire |
| P23 | right | 16,977, 949 | torn wire |
| P24 | top | 11,044, 588 | torn wire |
| P25 | top | 10,243, 582 | ball |
| P26 | top | 8,594, 577 | ball |
| P27 | top | 3,582, 552 | torn wire |
| P28 | top | 2,738, 553 | torn wire |
| P29 | top | 1,531, 546 | torn wire |

The boxes in `cells.json` are the passivation openings, found automatically. Those of P9, P12 and P6 come out larger than the true opening because neighbouring metal merges into the detection.

### What the pads show on their own

These observations come from the pads and their surroundings alone. The [floorplan](floorplan.md#pin-map) settles which of them hold.

- **P20 and P21 are a mirrored pair.** Each pad has its own strap into one half of a symmetrical structure: four large squares (diffusion contacts) and small devices between them. The two halves are not joined. It looks like input protection, and the only adjacent pair of pure inputs on the chip is `WS`/`RS` (pins 27, 28). But adjacent pads are often laid out as mirror images whatever their type, so this is weak evidence. (They are `M0`/`M1`.)
- **Block G's very wide, interdigitated transistors** suggest the DAC (`SPEAKER`) or the clock drivers. G runs along the bottom-left, from P9/P10 to just short of P11–P12. (They are the clock drivers.)
- **Block E is not a DAC.** It is a decoder row over an array of small, regular cells.
- **The large metal plate** at the top, between P27 and P26 (x 4,400–8,500, y 250–900), is the largest single piece of metal on the die. It may be a capacitor, which would fit the RC oscillator behind `OSC` ([datasheet §11.2](../datasheet/pins.md)), or part of a supply bus. (Its purpose is still unknown: `OSC` is P11.)
- **Nine identical large drivers** stand in the right-hand column between P20 and P16, not attached to any pad of their own (block H). They look like the D0–D7 bus drivers, or the VSM outputs.
- **The metal ring at the die edge** is in contact with the frames of P9 and P12 in a thresholded metal map. On a P-channel die the edge ring is normally the substrate, `VSS`. But at full resolution P9 leaves through a narrow signal strap, not a supply strap, so the map may have merged neighbouring lines.

### Two pin orders that the pads alone allow

Counting from the pads alone, two pin orders fit, and each conflicts with one observation:

| | **Order 1: `WS`/`RS` = P20/P21** | **Order 2: `SPEAKER` beside G** |
|---|---|---|
| Rule | P20 = 27 (`WS`), P21 = 28 (`RS`) | P12 = 8 (`SPEAKER`) |
| Pin 1 (D7) | P22 | P5 |
| `VDD`, `VSS` (4, 5) | P25, P26: the adjacent ball-bonded pair at the top, beside the large plate | P8, P9 |
| `OSC` (6) | P27, next to the large plate | P10 |
| `SPEAKER` (8) | P29, top-left, far from G | P12, at the right-hand end of G |
| `VREF` (11) | P3 | P16 |
| D0–D2 (14–12) | P6–P4 (left) | P19–P17, beside the drivers in H |
| `WS`, `RS` | P20, P21 (the mirrored pair) | P3, P4 |
| Doubled pad | One pad left over among P19/P20, an unlikely pin to double | P13, beside `SPEAKER` |
| Against it | `SPEAKER` far from the only large output stage | P8/P9 do not look like supply pads (P9 has a narrow strap). The mirrored pair would be `M0`/`M1` |

Neither fits everything. Three traces were set to decide between them:

1. **The supply rails.** Follow the two widest rails of the datapath (A, B) to their pads. They are `VDD` and `VSS` (pins 4, 5), and they fix the offset.
2. **Block G's output.** Follow the drains of G's large transistors to a pad. If G is the DAC, that pad is `SPEAKER`.
3. **The doubled pin.** Find two adjacent pads joined by metal. Candidates: P12/P13, P25/P26, P19/P20.

The pin map that holds comes from the pads' drivers instead. It is **Order 2 shifted by one pad**: P6 = pin 1, P10 = `VSS`, P13 = `SPEAKER`, P16 = `VREF`, and the mirrored pair P20/P21 are `M0`/`M1`. The extra pad is P5, a second `VSS` bond on the ring. The eight identical output drivers fall exactly on D0–D7, which decides it ([floorplan](floorplan.md#pin-map)).
