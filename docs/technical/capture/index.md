# Reference capture

The capture work maps exactly how the TMS5220 turns its input into its output, on real hardware. Its results are a **new TMS5220 datasheet** and a **reference data set** built from every available original speech PROM.

The new datasheet is based on TI's original ([TMS5220 data manual](../sources/tms5220-data-manual/index.md)). It keeps the original's structure and fills every gap, so that someone working from it alone can:

1. **Author new phrases:** encode LPC frames that the chip plays exactly as intended.
2. **Author new speech PROMs:** build VSM/PHROM images, with word tables, that work on real hardware.
3. **Emulate the chip exactly:** write an emulator that matches the original TMS5220 sample for sample, and matches its host and VSM interface timing.

"Sounds right" is not enough. For every capture we record which bits the chip consumed, when it consumed them, and which sample values it produced.

## What the capture work produces

### The reference datasheet

The [reference datasheet](../datasheet/index.md) follows the original's section order, so readers of the original can find things, and it marks which content is new or corrected. Its figures are generated from the captured data, and every claim cites the captures that prove it ([evidence index](../datasheet/evidence.md)).

| Section | What it adds to the original | Where it comes from |
|---|---|---|
| [System clock](../datasheet/clock.md) (§2, App. A) | The real `OSC` drive requirement: a rail-to-rail swing from −5 V to +5 V, an external-clock circuit, and the relationship between `fosc`, ROMCLK and the sample rate | [Bench hardware](bench-hardware.md) |
| [CPU interface](../datasheet/cpu-interface.md), [I/O structure](../datasheet/io-structure.md), [commands](../datasheet/commands.md) (§3, §5, §6, App. B, C) | Measured cycle timing: READY, `INT`, FIFO thresholds, the delay from Speak to first fetch, Reset behaviour. Corrections to the original | Protocol tests ([stimulus](stimulus.md)) |
| [VSM interface](../datasheet/vsm-interface.md) (§4) | The exact command and bit sequences, read-ahead per frame, address wrap and bank rules | VSM trace (layer 1), synthetic VSM images |
| Speech synthesis (§8) | The bit-exact frame format and parse rules: repeat frames, silence, stop code, unvoiced frames with fewer K parameters | VSM trace plus Speak External vectors |
| [Parameter tables](../datasheet/coded-parameters.md) (§8.1, coding table) | The energy, pitch and K1–K10 values **as the chip actually uses them**, not just the nominal coded values | Single-parameter sweeps |
| [Excitation](../datasheet/excitation-interpolation.md) (new) | The chirp table and the noise generator (LFSR polynomial, seed, period) | Voiced and unvoiced sweeps |
| [Interpolation](../datasheet/excitation-interpolation.md) (new) | The step size and timing per interpolation period, and when interpolation is inhibited | Interpolation vectors |
| [Lattice filter](../datasheet/lattice-filter.md) (new) | Bit widths, the order of operations, multiplier truncation and rounding, overflow and clipping behaviour | Layer 2 samples compared with candidate models |
| [Output](../datasheet/dac-and-audio.md) (§8.2, §8.3, [§8.4](../datasheet/serial-output.md)) | The `T11` and `I/O` serial output format, and the DAC transfer function (code to current, linearity, hold timing) | Layers 2 and 3 |
| [Pin functions](../datasheet/pins.md) (§11.2) | What `T11`, `I/O`, `PROM OUT` and `TEST` actually do | [Bench bring-up](work/bench-bring-up.md), [known gaps](../datasheet/gaps.md#the-test-mode) |
| [Authoring guide](../datasheet/authoring.md) (new) | How to encode speech into frames, the frame and bit budget, VSM image layout, word table and PHROM header formats, worked examples | [Corpus analysis](prom-corpus.md) and authoring tests |
| [Reference algorithm](../datasheet/reference-algorithm.md) (new) | Complete pseudocode for the synthesis path, from frame bits to output sample | All of the above |

The original datasheet is TI's copyright. The new datasheet is written fresh: it follows the original's structure and cites it, but does not copy its text or figures wholesale.

### The PROM reference data set

For **every original speech PROM that can be found** ([PROM corpus](prom-corpus.md)):

- **An inventory entry:** name, file hash, provenance, size, target chip (5220 or 5100/5200), and the word table ([PROM inventory](prom-inventory.md)).
- **Decoded content for every word:** its address, its length in bits and frames, and every frame's parameters as readable text.
- **The captured output for every word**, in both forms described below.
- **A browsable index page per PROM:** the word list, decoded frames, and links to the audio.

### Audio and capture data

Each capture is kept in two forms ([data layout](data-layout.md)):

- **The original capture format:** the raw PicoScope streams and the raw VSM trace, exactly as recorded and never modified afterwards. Everything else is regenerated from these by scripts, so any later improvement to the decoding can be rerun over the whole corpus.
- **Readable formats:** lossless WAV files (a bit-exact sample WAV at the chip's native 8 kHz, plus a 48 kHz listening version), and CSV/JSON for samples and frames. FLAC is acceptable for archiving because it is lossless. Nothing lossy (MP3, AAC) is used anywhere in the data set.

### Test vectors and tools

The synthetic stimulus files ([stimulus](stimulus.md)), the frame [encoder](../encoder/index.md) and decoder, the [reference emulator](../emulator/index.md), the capture runner and the post-processing scripts ([tools](../tools/index.md)) are all published, so anyone can reproduce or extend the captures.

## Acceptance tests

The datasheet is judged complete against three tests:

1. **An emulator written from the datasheet alone** reproduces every capture in the data set within the tolerance below (layer 2), and matches the VSM trace timing exactly (layer 1). Where the chip carries state between utterances ([observables](observables.md), [datasheet §7](../datasheet/power-up.md)), the emulator is given the documented starting state. See [reference emulator validation](work/reference-emulator.md) and [the remaining PROM corpora](work/prom-corpora.md).
2. **New speech data generated by this project** plays on the real TMS5220 as documented. These are new phrases that are in no original PROM, built with the project's own encoder from the datasheet alone. They are delivered both as a new VSM/PHROM image and as Speak External streams. The emulator's output is predicted and recorded *before* the data is played on the chip. The chip's captured output must then match that prediction within the tolerance below ([confirmation with new speech data](stimulus.md#confirmation-with-new-speech-data), [new speech data](work/new-speech-data.md)).
3. **Re-encoding original data gives back the original bits.** Decoding each original PROM word to frames with the documented frame format, then re-encoding it, reproduces the original bit stream ([new speech data](work/new-speech-data.md#the-round-trip)).

Anything that fails any of these tests is a gap in the datasheet.

### The tolerance

The tests ask for bit-for-bit agreement. The chip's lowest-order arithmetic has not been found, and part of it is marginal: the chip itself changes it slightly between days ([datasheet §8.8](../datasheet/lattice-filter.md)). So layer 2 is judged against a tolerance instead. **The tolerance is observed, not proven.** It is set from the agreement measured between two chips of one date code and between a chip's own takes, at the close of the [reference emulator validation](work/reference-emulator.md). Nothing shows that it bounds the chip's behaviour on data not yet captured, on other parts or date codes, or at other temperatures and supplies. It must be revisited if the arithmetic is found, or if new captures fall outside it.

- **Layer 1 (VSM timing):** exact.
- **Layer 2 (I/O codes), per vector set or PROM, against chip 1:**
  - The measure is the median, over the set's takes, of the signal-to-difference ratio (SDR): the chip's code power over the power of the difference, over the speech window.
  - The emulator's median SDR may be at most **7 dB** below the reference agreement. That is the median SDR of a second chip against chip 1 where one has been captured, otherwise of chip 1's own takes. Agreement above **40 dB** counts as 40 dB.
  - 7 dB is the largest gap observed on speech-like data in the reference emulator validation (6.2 dB, the pitch vectors), rounded up. The typical difference there is one code.
- **Checked by** `investigation/tools/variance.py`. Its `summary.json` gives the result per set ("tolerance").

Where the tests stand: test 1 holds within the tolerance on every PROM and on the speech-like vector sets ([the remaining PROM corpora](work/prom-corpora.md)); tests 2 and 3 pass on all the new speech ([new speech data](work/new-speech-data.md)). The sets outside the tolerance against chip 1 alone are the special frames, the loud and random vectors and the hand-designed new speech; none of them has been captured on a second chip ([known gaps](../datasheet/gaps.md#other-chips-and-the-tms5220c)).

## How the pages fit together

The method:

- [Observables and capture layers](observables.md): what can be seen on the chip, and the three layers recorded for every utterance.
- [Using the oscilloscope](oscilloscope.md): wiring, resolution and mode of the PicoScope 3405D.
- [Bench hardware](bench-hardware.md): the board changes that make the measurements possible.
- [Data layout](data-layout.md): the directory layout, file formats and where the data is stored.
- [Automation](automation.md): the host, the Pico and the scope working together.
- [PROM corpus](prom-corpus.md): the original speech data, its sources and how it is covered; the generated [PROM inventory](prom-inventory.md).
- [Controlled stimulus](stimulus.md): synthetic vectors, synthetic VSM images, timing tests and new speech data.
- [From captures to the datasheet](analysis.md): how a capture becomes a datasheet statement.
- [Open questions](open-questions.md): the open questions about the method and the data set. What is not known about the chip is in the datasheet's [known gaps](../datasheet/gaps.md).

The [work](work/index.md) pages describe each piece of measurement work, with its findings and evidence.

The hardware itself is described in [hardware](../hardware/index.md), and the host tools in [tools](../tools/index.md).
