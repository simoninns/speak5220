# PROM corpus

The aim is to cover **every original speech PROM that can be found**. This page describes where the original speech data comes from, how it was inventoried and checked, and how it is captured. The inventory itself, with every image's hashes, word counts and format, is the [PROM inventory](prom-inventory.md), written by `investigation/tools/promimport.py`.

## What was looked for

- **Acorn PHROM A** (Kenneth Kendall voice). In the repo as `investigation/firmware/src/romdata_acorn.h`, from TMS6100-Emulator.
- **Acorn PHROM US** (American voice). It is TI's VM61002, which was found.
- **Other TMS6100 / VSM dumps** that are archived publicly (for example for TI products), checked for whether they target the TMS5220 or the TMS5200/5100. Older chips use different coefficient tables and frame formats. Those dumps still play on a 5220, but they sound wrong. That is still a useful test, as long as it is labelled clearly.
- **Speak External data from other TMS5220 hosts.** Arcade boards (for example several Atari games) and Apple II and similar speech cards stored LPC data in CPU ROM rather than in a VSM. It can be replayed through Speak External ([controlled stimulus](stimulus.md)).

## Known sources

PROM images:

- **TMS6100 Multi-PHROM Emulator for the BBC Micro** (eRadiofan): <https://github.com/eRadiofan/TMS6100-Multi-PHROM-Emulator-for-the-BBC-Micro/tree/main/Firmware/Common/Src>. Several PHROM images built into its firmware sources.
- **TMS6100-Emulator** (simoninns): <https://github.com/simoninns/TMS6100-Emulator/tree/master/Firmware/tms6100>. The source of the Acorn PHROM A data in `romdata_acorn.h`, and of a full VM61002 dump.
- **Talkie** (Peter Knight): <https://github.com/going-digital/Talkie>. An Arduino LPC library whose vocabularies are words copied from TI and Acorn PROMs, including 232 words of the VM61003 and VM61005, which are not otherwise available.
- **Star Wars sound ROM disassembly** (nmikstas): <https://github.com/nmikstas/star-wars-arcade-audio>. The game's 23 Speak External speech blocks.
- **Published ROM hash lists:** the reference hashes of the dumps, and the list of systems that used a VSM. The hash each image is checked against is recorded in its manifest.

Information:

- **TI Portable Speech Lab** (Stuart Conner): <https://www.stuartconner.me.uk/ti_portable_speech_lab/ti_portable_speech_lab.htm>. TI's speech development system, which is background for the [authoring guide](../datasheet/authoring.md).

## The inventory

`investigation/tools/promimport.py` fetches each source file at a pinned commit and rebuilds the image from it. It checks each image against its published reference hash, and reads the word table from the image's own index, cross-checked against the source's published word list. It identifies the frame format from the data, and writes the manifests (`investigation/proms/<name>.json`) and the [PROM inventory](prom-inventory.md) page. The images go in `investigation/proms/images/`, which is git-ignored: original ROM images stay out of the repo unless their redistribution status is clear, and the repo holds only the manifests and hashes. The Acorn PHROM A is the exception; its image is in the firmware.

The inventory has eight manifests:

- **Dumps, matching their reference hashes byte for byte:**
    - `acorn_phrom_a` (CM62024);
    - `vm61002` (Acorn's US PHROM);
    - `vm61004` (military vocabulary);
    - `vm71003a` (female talking clock, 4 KB);
    - `ti99_speech` (CD2325A + CD2326A, 32 KB across banks 0 and 1, written for the TMS5200).
- **Assembled from original words:**
    - `starwars_speech`, the arcade game's 23 Speak External blocks;
    - `vm61003_words` and `vm61005_words`, 116 words each, from Talkie.
- **Known but not held:** the complete VM61003 and VM61005 images, Exelvision's CM62312 (TMS5220C), and Speak External data in other games' program ROMs.
- **Left out:** VSMs for the TMS5100/TMS5110 family, whose frame format differs.

**Multi-bank images.** An image may span several 16 KB banks. `run.py` uploads every bank, and the tools find words by their offset from the image's first bank (`prom.base`). The Pico holds 4 banks at once, so before uploading, `run.py` unloads banks that belong to other PROMs, and the self-test starts from empty banks.

### What the inventory showed

- **Every copy found is a good dump.** The two Acorn copies are identical to the image in this repo. The eRadiofan copies stop after their last word, and padded with zeros they match the reference dumps exactly.
- **Talkie's words are verbatim.** All 766 of its words that come from a held image match that image byte for byte. On that basis its VM61003 and VM61005 words are taken as original.
- **Every held image is in the TMS5200/TMS5220 frame format.** Every word in all eight parses to a stop code, and none runs into the next word. Read with the TMS5100's 5-bit pitch field instead, 2–123 words per PROM run into the next one. The data cannot separate the TMS5200 from the TMS5220, which share a frame format. TI-99's target, the TMS5200, comes from the product.
- **Four word names in `romdata_acorn.h` are wrong.** The Acorn PHROM stores each word's name next to its data. Those names correct words 135 (-TEEN), 137 (-TY), 138 (-Z) and 230 (O). The list's numbers and addresses all match the PHROM's pointer table, including word 272 (TRY), which the list marked as unverified.
- **The image formats** are documented in the [PROM inventory](prom-inventory.md#image-formats): Acorn's header, pointer table and stored names; TI's indexed and unindexed layouts; and the TI-99's binary search tree. In TI's own images, every word is followed by one spare byte after the byte holding its stop code. This is material for the [authoring guide](../datasheet/authoring.md).
- **For Speak External replays,** the `starwars_speech` manifest records how many bytes the game sends for each block (`stream_bytes`), including the bytes after the stop code.
- **The 4 KB VM71003A** is served padded with zeros. How a real 4 KB VSM answers addresses above 4 KB is not known ([known gaps](../datasheet/gaps.md#a-4-kb-vsm-above-4-kb)).

The first captures of the inventory (at least one word from every PROM, with the standard procedure, through both paths for TI-99, Star Wars and the VM61005 words) all passed every check. That includes TI-99's LINE, which runs from the first VSM into the second. Of the 22 takes, one TI-99 take was accepted one slot early and failed `start_slot`; its run was repeated, and every take in the repeat passed. Those captures also found:

- **The DAC model fails at the rails.** TI-99 speech is loud on the TMS5220, and LINE uses the whole 10-bit `I/O` range. Codes beyond −128…+127 clip to the rail values. Codes −128 and +127 sit about 10 mV and 6 mV from the bit-weight model ([datasheet §8.2](../datasheet/dac-and-audio.md)) [p6-01 to p6-08]. So the DAC check (`process.py` version 3) judges the fit, the residual and the code recovery on codes inside ±119, and reports the rail samples separately.
- **Larger upsets in TI-99 words.** Within a path, takes differed by up to 15 codes, from as early as sample 3 of speech, against 3 codes for the Acorn word in the [repeatability](work/repeatability.md) captures. The two LINE takes through Speak External were bit-identical. Loud words may be the quickest way to study upsets.

Evidence: p6-01 to p6-08, TI-99's MORE and LINE, two takes per path ([evidence index](../datasheet/evidence.md)).

## Words and the address sweep

For each image, the word table gives the entry points. A **sweep of byte addresses**, not just the word starts, exercises the frame parser with data no encoder would write: starting mid-word produces garbage. It is not deterministic: such data can drive the filter far outside the range of speech, where the smallest upset grows to the full output range ([the Acorn PHROM A reference set](work/acorn-phrom-a.md)). The sweep leaves out addresses whose data reaches the end of the image without a stop code, because those read on into an empty bank (`prom.sweepPoints`).

- The Acorn PHROM A is swept at every address ([the Acorn PHROM A reference set](work/acorn-phrom-a.md)).
- The other dumps are swept at a sample of addresses, and the assembled images are not swept, because their bytes are original only word by word ([the remaining PROM corpora](work/prom-corpora.md)).

## Use for authoring

The corpus also feeds the [authoring guide](../datasheet/authoring.md). Analysing how the original PROMs were encoded (frame rates, use of repeat frames, energy and pitch ranges, word table and header layout) shows what real speech data looks like and what the original tools produced (`investigation/tools/promstats.py`, `investigation/captures/phase11/promstats/`).

## Open work

Version 1.x of the inventory would add the complete VM61003 and VM61005, the CM62312, and Speak External data from other hosts, if they can be found.
