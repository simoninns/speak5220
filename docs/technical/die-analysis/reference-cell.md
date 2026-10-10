# The multiplier reference cell

One complete multiplier cell in the middle of the array, **checked against the patent rather than traced blind**. The cell at stage 2 (column group 3) and bit row 7 is mapped gate by gate and contact by contact, the transistors of the patent's cell (US 4,335,277 FIG. 10c) are placed on them using the diffusion only where it can be seen, and every visible gate and contact is checked to be used, every patent transistor to have a gate, and nothing visible to contradict the patent's connections. This is done on several copies in this image and in the TMS5200NL image, so that a smudge on one copy is not read as a feature. The result is the **reference cell**, against which the [multiplier's low end](multiplier-low-end.md) is compared. Had the patent's cell not fitted, the misfit would have had to be explained first.

In short:

- **The cell is identical in all regular copies in both chips,** and averaging 22 copies reveals the diffusion.
- **Its one two-row link is the patent's Σ(x−2),** and it confirms row 14 as the LSB.
- **The LSB row is the middle cell with its channel tied off,** not a separate cell.
- **24 gates and 34 contacts** are placed and counted (37 contacts with three large windows that are probably wide contacts), which fits the patent's precharged cell.
- **The connections through diffusion under the metal cannot be read,** so the cell is *consistent with* the patent at the level of its links, count and style, not proven gate for gate.

Positions are in full-resolution pixels ([preparation](preparation.md#tools-and-coordinates)). The record is `ref_cell` in [`cells.json`](data/cells.json).

## Why a blind trace fails on one copy

The cell at stage 2 (column group 3, x 4,000–5,260) and bit row 7 (y 8,446–8,898) has a block of random logic on each side of a vertical routing channel. To reduce it to logic, every transistor's source and drain must be known. In a single copy:

- the diffusion shows only as a faint difference in the orange of the field oxide;
- near metal, that difference is lost in the metal's halo, in the speckle of the metal surface and in the JPEG blocks.

The [ROMs](roms.md) could be read because their diffusion is a regular grid of strips. The random logic has no such grid, so a netlist drawn from one copy would be a guess.

## The copies and their average

`python investigation/tools/die.py copies` matches the cell (green channel, normalised correlation, ±40 px) against every group and row of A, in this image and in the TMS5200NL's:

| Column group | TMS5220NL, rows 1–13 | Row 14 | TMS5200NL, rows 1–13 | Row 14 |
|---|---|---|---|---|
| 1 | 0.52–0.61 | 0.41 | 0.46–0.51 | 0.33 |
| 2 | 0.76–0.80 | 0.64 | 0.66–0.69 | 0.53 |
| 3 | 0.87–0.90 | 0.71 | 0.70–0.73 | 0.59 |
| 4 | 0.85–0.88 | 0.69 | 0.70–0.72 | 0.60 |
| 5 | 0.30–0.37 | 0.33 | 0.25–0.29 | 0.29 |

The TMS5200NL's scores are lower because its photograph differs in colour and focus, but the pattern is the same in both chips:

- **Groups 3 and 4 are identical** in every row. These are the regular stages.
- **Group 2 is nearly identical.** Its differences are small and scattered over the left-hand logic: a few features, not a different circuit.
- **Groups 1 and 5 are different cells.** They are the two end stages, as the [floorplan](floorplan.md#bit-rows-and-word-length) found.
- **Row 14 differs in every group** ([the end rows](#the-end-rows)).

**Averaging reveals the diffusion.** The 22 regular copies (groups 3–4, rows 2–12) were aligned on their best match and averaged. The speckle and JPEG noise average out, and the field oxide and the diffusion become two distinct tones with sharp edges. The diffusion is the more saturated orange: on the parameter ROM, whose strips are known ([ROMs](roms.md#organisation)), (R − G)/R is 0.50–0.51 on the strips and 0.44–0.46 between them. The command writes the average and a map of (R − G)/R to `investigation/local/die_image/` (`cell_average.png`, `cell_average_diffusion.png`).

![One copy, the average of 22 copies, and the two-row link](img/fig_ref_cell.jpg)
*Figure: top, one copy of the reference cell. Middle: the average of 22 copies, in which the diffusion shows as the lighter orange. Bottom: one net of the routing channel (red), which runs from row 6 to row 8. Die image: John McMaster, siliconpr0n, CC BY 4.0.*

The average shows **107 dark-rimmed features** (gate windows and contacts). Their interiors do not split cleanly into the two kinds, and the diffusion still cannot be seen under metal, so the transistor-by-transistor fit is not complete ([gates and contacts](#gates-and-contacts)).

## Checks against the patent's cell

The patent's middle cell is B-2 (FIG. 10c). It has:

- a Booth select (+1, −1, +2, −2 lines choosing MRx, its complement, 2MRx or its complement);
- an exclusive-OR with the incoming partial sum Σx;
- a precharged carry, C_IN from the bit below and C_OUT to the bit above;
- a second exclusive-OR, clocked on φ2 and φ3, giving the output Σ(x−2) to the next stage two bits lower;
- two clocked inverters (φ4, φ3) that pass MRx on to the next stage.

Its links therefore span a known number of bits:

| Link | Bits spanned | Found on the die |
|---|---|---|
| Σ(x−2), to the next stage | **2** | **One channel net per cell spans exactly two rows**: from a contact in row *r* to a contact in row *r* + 2, about 820 px long, stepping one track. It is identical in every row of groups 1–4 (12–13 per group), and **none starts in row 13 or 14**. Group 5 has none |
| C_IN, C_OUT, and the MR pair passed to the neighbour | 1 | 13–16 channel nets per group spanning one row (360 px, or 452 px) |
| +1, −1, +2, −2 and the clocks | the whole stage | 8 metal lines run straight through all 14 rows of each group |
| MRx to the next stage | 0 (same bit) | Horizontal lines leave each cell sideways. Not traced across the groups |

These were found with gap-bridging labels of the channel's metal (closing by 11 px vertically, so that diffusion edges under the metal do not cut the lines). The checks pass:

1. **The array is 5 stages × 14 bits** of one repeated cell, with two different end stages.
2. **The only link that spans two bits exists once per cell,** as Σ(x−2) needs. Two bits fall off per stage: no two-row link leaves rows 13 or 14.
3. **The links run downwards, so row 14 is the LSB.** This confirms the reading of the [floorplan](floorplan.md#bit-rows-and-word-length) by an independent route: the bits that are dropped are at the bottom.
4. **Nothing in the cell contradicts the patent's cell** at this level.

**Result: the cell is consistent with the patent's B-2 cell at the level of its links.** It is the reference cell for the [low end](multiplier-low-end.md), labelled *consistent with the die* ([ways of working](index.md#ways-of-working)). For ordinary operands its logic is the multiplier form of [datasheet §8.8](../datasheet/lattice-filter.md), which the vectors confirm ([lowest-order arithmetic](lowest-order-arithmetic.md#1-what-a-candidate-must-explain), constraint 1).

## The end rows

The patent uses different cells at the ends of each stage: B-1 at the multiplicand's LSB (MR0) and B-3 at its MSB (MR13). Differences between the average and the rows at the ends (24 px blocks, mean difference above 30 grey levels):

- **Row 1 does not differ** from the middle cell anywhere.
- **Row 14 differs only in the channel and below it.** Its logic on both sides is the same as the middle cell's. In the channel, the lines stop above y ≈ 11,950, several ending in contacts. A wide metal bar (x ≈ 4,520–4,950, y ≈ 12,000–12,040 in group 3) runs below them, and no other row has it.

So the chip does not use a separate LSB cell in this array: the patent's separate B-1 and B-3 cells are not used. The reading that fits is that **the LSB cells are the middle cell with their inputs from the bit below (C_IN, and the neighbour's MR pair) tied off at the bottom of the channel**. The MSB handling is presumably in the header row above row 1, which the [floorplan](floorplan.md#bit-rows-and-word-length) found to differ. What the tie-offs are tied to (`VSS`, `VDD` or a clock) decides how the lowest bits are rounded, which is the question of T1. The [multiplier low end](multiplier-low-end.md#bit-0-inputs-tied-off) page compares the low-end cells with this reference cell and scores the readings.

## Gates and contacts

**The TMS5200NL average is clearer.** The same 22 copies averaged in the TMS5200NL image show the diffusion edges as crisp dark-orange outlines, sharper than in this image (it aligns with the TMS5220NL average to within 2 px). Every feature below was classified on it.

**Gates and contacts can be told apart by shape.** The delay stack gives labelled examples, because each of its jumpers carries one contact (on a strip's output) and one gate (on the next strip) ([floorplan](floorplan.md#the-delay-stack)). Averaged over its 14 bit rows:

- **contacts are square or wide:** inner opening 21 × 15 px;
- **gates are narrow windows lying across a strip:** 13–14 px across the channel, 27 px long.

Their colour and rim darkness are the same, so shape is the only test. In the reference cell, gates 50 and 51 show the pattern clearly: two narrow windows flank a strip of diffusion that runs between them, and their metal ends in square contacts (40, 78 and 79).

![Gates and contacts of the reference cell](img/fig_ref_cell_features.jpg)
*Figure: average of 22 copies in the TMS5200NL image. Red: gate windows. Green: contacts. Not marked: the routing channel's contacts ([checks](#checks-against-the-patents-cell)), and three large rimmed rectangles on the right (x ≈ 1,050–1,200), read by eye as probably wide contacts. Die image: John McMaster, siliconpr0n, CC BY 4.0.*

| | Left logic (x 0–410) | Right logic (x 950–1,260) | Cell |
|---|---|---|---|
| Gates | 15 | 9 | **24** |
| Contacts | 17 | 17 + 3 large (probably) | **37** |

**Against the patent's cell.** B-2 has about 39 transistors if every gate has a static load. That falls to about 25 if its precharged nodes (φ3) have no static loads. The cell's 24 gates fit the second count. The three large windows were read by eye as probably wide contacts, not loads. Two further observations fit the patent's style of logic:

- **The stage-wide lines carry contacts, not gates.** The Booth selects and clocks reach their gates through short diffusion links to small metal pieces, such as the piece that joins contacts 40 and 78 to gate 50. This is the precharged, conditionally discharged logic the patent describes.
- **The gates come in mirrored pairs** (50/51 with contacts 78/79), as the patent's paired true and complement paths need.

**What is not readable: the connections through diffusion.** Under metal, the diffusion shows only as outlines. Which outlined regions are diffusion, and so which gate's source and drain join which node, is a judgement at almost every site. A netlist drawn from it would be a guess, and the rule is not to trust one for T1 ([ways of working](index.md#ways-of-working)). So the fit stops at placing the transistors: the gates and contacts are placed and counted, and the cell is *consistent with the patent* in count, style and links ([checks](#checks-against-the-patents-cell)). It is not proven gate for gate.

**The TMS5200NL image is top metal too.** The map is titled "TMS5200 top metal @ Mit20x", and its multiplier cells are fully metallised. Its layout matches this die's, at a ~45 px lower offset, so it is a second copy of every cell. It is kept in `investigation/local/die_image/tms5200nl/`, downloaded from the siliconprawn.org map tiles (zoom 6, 73 × 60 tiles of 250 px, 18,250 × 15,000 px; the download needs a browser's Cloudflare cookie). `die.py` reads it from the tiles.

## Open questions

- **The cell gate for gate.** The cell count (24 gates, 37 contacts with the three probable wide contacts; 34 without) and the links fit the patent, but the diffusion connections cannot be read. Only a delayered image can settle it:
  - **a delayered TMS5220,** from a dead chip (chip 2 or 3), or from John McMaster if one exists;
  - **a delayered sibling.** The TMS5100 (TMC0281) uses the same lattice-filter design (patent US 4,209,844). If siliconprawn has a delayered image of it, its multiplier cell could be compared with this one by shape.
- **The MSB handling** in the header row above row 1 has not been read.
- **The MRx lines to the next stage** leave each cell sideways and have not been traced across the groups.
