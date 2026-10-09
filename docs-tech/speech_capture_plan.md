# TMS5220 Reference Capture Plan

This is a high-level plan for mapping exactly how the TMS5220 turns its input into its output, and for publishing the result as a **new TMS5220 datasheet** plus a **reference data set** built from every available original speech PROM.

The new datasheet is based on TI's original (transcribed in `tms5220_datasheet/`). It keeps the original's structure and fills every gap, so that someone working from it alone can:

1. **Author new phrases:** encode LPC frames that the chip plays exactly as intended.
2. **Author new speech PROMs:** build VSM/PHROM images, with word tables, that work on real hardware.
3. **Emulate the chip exactly:** write an emulator that matches the original TMS5220 sample for sample, and matches its host and VSM interface timing.

"Sounds right" is not enough. For every capture we record which bits the chip consumed, when it consumed them, and which sample values it produced.

## 1. Deliverables

### 1.1 The new datasheet

It lives in `docs-tech/tms5220_reference_datasheet/`, in Markdown, with figures generated from the captured data. It follows the original's section order, so readers of the original can find things, and it marks which content is new or corrected.

| Section | What it adds to the original | Where it comes from |
|---|---|---|
| System clock (§2, App. A) | The real `OSC` drive requirement: a rail-to-rail swing from −5 V to +5 V, an external-clock circuit, and the relationship between `fosc`, ROMCLK and the sample rate | Bench work so far (§5) |
| CPU interface, I/O structure, commands (§3, §5, §6, App. B, C) | Measured cycle timing: READY, `INT`, FIFO thresholds, the delay from Speak to first fetch, Reset behaviour. Any corrections to the original | Protocol tests (§9.1, §9.3) |
| VSM interface (§4) | The exact command and bit sequences, read-ahead per frame, address wrap and bank rules | VSM trace (layer 1), synthetic VSM images (§9.2) |
| Speech synthesis (§8) | The bit-exact frame format and parse rules: repeat frames, silence, stop code, unvoiced frames with fewer K parameters | VSM trace plus Speak External vectors |
| Parameter tables (§8.1, coding table) | The energy, pitch and K1–K10 values **as the chip actually uses them**, not just the nominal coded values | Single-parameter sweeps (§9.1) |
| Excitation (new) | The chirp table and the noise generator (LFSR polynomial, seed, period) | Voiced and unvoiced sweeps |
| Interpolation (new) | The step size and timing per interpolation period, and when interpolation is inhibited | Interpolation vectors |
| Lattice filter (new) | Bit widths, the order of operations, multiplier truncation and rounding, overflow and clipping behaviour | Layer 2 samples compared with candidate models |
| Output (§8.2, §8.3) | The `T11` and `I/O` serial output format, and the DAC transfer function (code to current, linearity, hold timing) | Layers 2 and 3 |
| Pin functions (§11.2) | What `T11`, `I/O`, `PROM OUT` and `TEST` actually do | Phase 1, §11 |
| Authoring guide (new) | How to encode speech into frames, the frame and bit budget, VSM image layout, word table and PHROM header formats, worked examples | Corpus analysis (§8) and authoring tests |
| Reference algorithm (new) | Complete pseudocode for the synthesis path, from frame bits to output sample | All of the above |

The original datasheet is TI's copyright. The new datasheet is written fresh: it follows the original's structure and cites it, but does not copy its text or figures wholesale.

### 1.2 The PROM reference data set

For **every original speech PROM that can be found** (§8):

- **An inventory entry:** name, file hash, provenance, size, target chip (5220 or 5100/5200), and the word table.
- **Decoded content for every word:** its address, its length in bits and frames, and every frame's parameters as readable text.
- **The captured output for every word**, in both forms described in §1.3.
- **A browsable index page per PROM:** the word list, decoded frames, and links to the audio.

### 1.3 Audio and capture data

Each capture is published in two forms:

- **The original capture format:** the raw PicoScope streams and the raw VSM trace, exactly as recorded and never modified afterwards. Everything else is regenerated from these by scripts, so any later improvement to the decoding can be rerun over the whole corpus.
- **Readable formats:** lossless WAV files (a bit-exact sample WAV at the chip's native 8 kHz, plus a 48 kHz listening version), and CSV/JSON for samples and frames. FLAC is acceptable for archiving because it is lossless. Nothing lossy (MP3, AAC) is used anywhere in the data set.

### 1.4 Test vectors and tools

The synthetic stimulus files (§9), the frame encoder and decoder, the capture runner and the post-processing scripts are all published, so anyone can reproduce or extend the captures.

### 1.5 When it is finished

The datasheet is complete when all three of these hold:

1. **An emulator written from the datasheet alone** reproduces every capture in the data set within the tolerance below (layer 2), and matches the VSM trace timing exactly (layer 1). Where the chip carries state between utterances (§2, §11), the emulator is given the documented starting state.
2. **New speech data generated by this project** plays on the real TMS5220 as documented. These are new phrases that are in no original PROM, built with the project's own encoder from the datasheet alone. They are delivered both as a new VSM/PHROM image and as Speak External streams. The emulator's output is predicted and recorded *before* the data is played on the chip. The chip's captured output must then match that prediction within the tolerance below (§9.4).
3. **Re-encoding original data gives back the original bits.** Decoding each original PROM word to frames with the documented frame format, then re-encoding it, reproduces the original bit stream.

Anything that fails any of these tests is a gap in the datasheet.

**The tolerance (2026-10-08, Phase 9).** The tests asked for bit-for-bit agreement. The chip's lowest-order arithmetic could not be found, and part of it is marginal: the chip itself changes it slightly between days (datasheet §8.8). So layer 2 is judged against a tolerance instead. **The tolerance is observed, not proven.** It is set from the agreement measured at the close of Phase 9, between two chips of one date code and between a chip's own takes. Nothing shows that it bounds the chip's behaviour on data not yet captured, on other parts or date codes, or at other temperatures and supplies. It must be revisited if the arithmetic is found, or if new captures fall outside it.
- **Layer 1 (VSM timing):** exact, as before.
- **Layer 2 (I/O codes), per vector set or PROM, against chip 1:**
  - The measure is the median, over the set's takes, of the signal-to-difference ratio (SDR): the chip's code power over the power of the difference, over the speech window.
  - The emulator's median SDR may be at most **7 dB** below the reference agreement. That is the median SDR of a second chip against chip 1 where one has been captured, otherwise of chip 1's own takes. Agreement above **40 dB** counts as 40 dB.
  - 7 dB is the largest gap observed on speech-like data at Phase 9's close (6.2 dB, the pitch vectors), rounded up. The typical difference there is one code.
- **Checked by** `tools/variance.py`. Its `summary.json` gives the result per set ("tolerance").

## 2. What can be observed on the chip

| Signal | Pin | What it gives us | Currently wired? |
|---|---|---|---|
| `SPEAKER` | 8 | Analogue DAC output (current sink, 0–1.5 mA, about 5.9 µA per step, new value every 125 µs) | Yes, into the R2 1K load (top sheet) and the filter chain. Test point TP2 |
| `ROMCLK` | 3 | `OSC` ÷ 4. It sets the exact sample rate (`fosc` / 80) | Yes, to the VSM interface |
| `ADD1/2/4/8`, `M0`, `M1` | — | Every VSM command and every data bit the chip reads | Yes, the Pico sees all of them (GPIO8–13) |
| `T11` | 7 | Sync output, once per sample period | Test point TP3 only |
| `I/O` | 9 | Serial data out | Test point TP4 only |
| `PROM OUT`, `TEST` | 10, 20 | "Testing use only" | No. Leave them alone (see §11) |
| `READY`, `INT`, status register | — | Command handshake, end-of-speech and FIFO events | Yes |

`T11` and `I/O` matter most. The datasheet calls them only "sync" and "serial data out". Community reverse engineering of the TMS52xx family (the notes in MAME's `tms5220.cpp`) suggests that `I/O` carries the **digital sample value serially, framed by `T11`**. This has now been **confirmed on the bench** (see below), so we can capture the exact output word of the lattice filter with no analogue error at all.

**Bench observations (2026-10-06).** Both pins swing from −5 V to +5 V.
- **`T11`:** an 8 kHz pulse train with 5% duty. That is one 6.25 µs bit time (one ROMCLK period) out of the 20 in each 125 µs sample period. It is the per-sample frame marker.
- **`I/O` when idle:** a 50% duty square wave. This fits a 10-bit sample value of −1 (all ones) sent serially in 10 of the 20 bit times, with the line low for the other 10. Confirm that this square wave is at 8 kHz.
- **`I/O` during speech:** the pattern changes on every sample.

**First automated PicoScope captures (2026-10-06).** Settings: probe A ×1 on ±1 V with a −0.5 V offset; probes B–D ×10 on ±1 V (±10 V at the tip). Block mode, at 10.4 MS/s and at 2 MS/s.

- **`T11` period:** 124.9996 µs, with 0.03 µs jitter.
- **`I/O` format, confirmed:** a 10-bit two's-complement sample, **LSB first, in bit slots 2–11**. Slot *k* runs from 6.25·*k* to 6.25·(*k*+1) µs after the `T11` rising edge. Slots 0, 1 and 12–19 are always low, and the line high is a 1. Edges fall on slot boundaries. The sample is the one the DAC outputs in the same period. The idle value is −1.
- **DAC timing:** the DAC changes value 6.25 µs after the `T11` rising edge, that is, on its falling edge.
- **Averaging:** the settled part of each DAC step (`T11` + 12 µs to + 128 µs) averages down from 11 mV of noise to 0.33 mV.
- **DAC level against code:** about −3.9 mV per code across the speech band. There is an extra jump of about 15 mV (about 4 codes) between codes −1 and 0, which needs further study. (Superseded by the Phase 2 measurements below.)
- **The audio stage loads TP2.** It is not a pure 1 K load: C1 and R3 4K7 make the TP2 voltage depend on recent samples. A first-order model with a pole at 294 Hz (the C1 corner is 279 Hz) cuts the residual from 8.6 mV to 1.3 mV rms. For DAC characterisation, remove C1 so TP2 sees only the 1 K. Alternatively, model the network in post-processing.
- **Repeats are not bit-identical.** Repeats of "B" match exactly for the first ~1170 samples, then differ by ±1–2. Words with unvoiced sounds ("C", "ACORN") differ by up to ±26 from the first unvoiced frame onwards. This is consistent with a free-running noise generator that Speak does not reset. The delay from Speak to the first sample also varies: 6 and 25 samples were seen, and 25 samples is one interpolation period. Both are emulator-relevant findings, covered in §11.

**Clock-edge alignment (2026-10-06, Phase 1 complete).** Every `T11` and `I/O` transition follows a ROMCLK **rising** edge. `I/O` rises 215 ns after it and `T11` rises 490 ns after it, each with a spread of 8 ns or less. ROMCLK measures 160.0005 kHz at 50.6 % duty. The DAC steps at the start of slot 1. `I/O` carries the 10-bit code before the DAC clips it (codes up to +134 were seen). The reference datasheet (`tms5220_reference_datasheet/`, §2, §8.2, §8.4, §9.5, §11.2 and Appendix A) gives the full results, with the captures cited.

**DAC transfer function (2026-10-06, Phase 2 complete).** These were measured with the audio stage disconnected, into 996 Ω and 482 Ω.
- **Clipping and history.** The DAC takes the code clipped to −128…+127. Each sample's output depends only on its own code: the earlier samples explain less than 0.2 % of it.
- **Current.** The current is about 20 % below the original's Table 4: 4.49 µA per step and 604 µA at idle, into 996 Ω.
- **Carry errors.** The DAC is binary-weighted with mismatched bits. The step from −1 to 0 is about 6 LSB (26 µA), not 1. The bit-5 carries are 1.4 LSB and the bit-6 carries 0.0–0.4 LSB.
- **Load dependence.** The current depends on the load: it is 3–5 % more into 482 Ω than into 996 Ω, which is about 10–16 kΩ of output resistance. It also falls short at large currents.
- **Step timing.** The DAC step timing is set by the chip: 50 % at 0.85 µs after the slot-1 ROMCLK edge.

The reference datasheet's §8.2 and §8.3 give the details.

## 3. Capture strategy: three layers per utterance

Each capture records three things, so that any difference from an emulator can be traced to its cause:

1. **Input trace (digital, exact).** The Pico's VSM emulation logs every Load-Address, Read and Read-and-Branch, and the bits it returned. This is the exact bit stream the chip parsed. The firmware records this on core 1 as a compact, timestamped event log, together with every host interface cycle. The host downloads it after each capture as `raw/vsm_trace.bin` (`firmware/PROTOCOL.md`).
2. **Output samples (digital, exact).** Capture `T11` and `I/O` with the PicoScope and decode them in software into one integer per 125 µs sample. This is the gold standard for emulator comparison.
3. **Output waveform (analogue, measured).** Use the PicoScope to measure the voltage across R2 (TP2), which is the raw DAC output **before** the filter chain. This checks the DAC transfer function and the hold behaviour, and it is the fallback if layer 2 does not work out.

**All reference captures are taken directly at the TMS5220, before the audio stage.** Layer 3 measures pin 8 across R2 at TP2, DC-coupled and upstream of the audio stage's AC coupling capacitor, so the only thing between the DAC and the probe is the 1 K load. If R2's tolerance or the probe loading turns out to matter, replace R2 with a 0.1% resistor of the same value. Alternatively, feed pin 8 into a dedicated transimpedance stage used only for measurement.

The filtered `Audio_out` signal is useful for listening tests, but it is not reference data. The 280 Hz high-pass and the 4th-order low-pass (see `audio_output_stage.md`) change the waveform. Any filtered recording can be reproduced from layer 2 or layer 3 in software.

## 4. Using the PicoScope 3405D

**Yes, it can be fully automated.** The 3000D series uses Pico Technology's `ps3000a` driver. The official `picosdk-python-wrappers` package covers it, so a Python host script can arm, trigger, stream and download captures with no GUI. The driver libraries (`libps3000a`, `libpicoipp`) are not in nixpkgs as far as I know. They may need a small derivation in `flake.nix`, or a `LD_LIBRARY_PATH` to Pico's `.deb` installation.

### How to wire it

The top sheet has a marked "PicoScope 3405D probe points" block, with one test point per channel and four ground test points (TP6–TP9) for the probe ground clips:

| Channel | Test point | Signal |
|---|---|---|
| A | TP2 `PROBE_A` | `SPEAKER` (pin 8) across R2 1K. The raw DAC staircase, 0–1.5 V and idling at about 0.75 V. **DC-coupled**, ±1 V or ±2 V range |
| B | TP3 `PROBE_B` | `T11` (pin 7), sample sync |
| C | TP4 `PROBE_C` | `I/O` (pin 9), serial data out |
| D | TP5 `PROBE_D` | `SCOPE_TRIG` (Pico GPIO26). The firmware raises it just before it issues Speak and lowers it when Talk Status clears. Use it as the trigger source, so every capture has a deterministic start and length |

`ROMCLK` (`OSC` ÷ 4) stays on TP1, for checking the clock with a frequency counter or by temporarily swapping it onto a channel.

### Resolution

The 3405D has an 8-bit ADC. On the ±1 V range, one ADC count is about 7.8 mV. One DAC step across 1 K is about 5.9 mV, so a single scope sample cannot resolve individual DAC codes. However, each DAC value is held flat for 125 µs. At 1–2 MS/s that gives 125–250 scope samples per DAC sample. If we average the settled middle of each step (aligned to `T11` on Ch B), the noise acts as dither and the average resolves well below one ADC count. That is enough to recover the DAC code and to measure DAC linearity. PicoScope's hardware "resolution enhancement" does a similar job. We do it in post-processing instead, so that it stays explicit and repeatable.

### Mode

Use **streaming mode** at about 1–2 MS/s per channel. That is far inside the USB 3 bandwidth, and it lets one capture cover an entire PHROM sequence with no memory-depth concerns. Save the raw ADC data together with the range and offset metadata, then derive everything else from it.

Block mode is a simpler option. The 3405D has 64 MS of memory, shared between the enabled channels. With all four channels at 2 MS/s, that is enough for about 8 s, so a single word fits easily. 2 MS/s gives 12 scope samples per `I/O` bit slot, and decoding at that rate is reliable.

**Decision (Phase 4): block mode.** The runner captures one take per block, triggered by `SCOPE_TRIG`, so every capture starts at the same point relative to the Speak. The block length comes from the frame count (frames + 2, × 25 ms, plus 150 ms). The longest Acorn word fits many times over in the 8 s limit, and so does an unvoiced run long enough for the noise generator's period (§9.1). Streaming would only be needed for a single stimulus longer than about 8 s, or to drop the scope's re-arm time between takes; neither is needed yet.

**Driver access works without packaging.** PicoScope 7, installed system-wide, ships `libps3000a.so`. `tools/ps3000a.py` loads it through `ctypes`, and the dev shell provides its libusb and libstdc++ dependencies. `tools/run.py` drives the scope and the Pico together (§7, `tools/README.md`).

### The PicoScope captures all of the output

The PicoScope records all four output-side signals on one timebase: the DAC voltage, `T11`, `I/O` and the trigger. That makes it the single capture instrument for layers 2 and 3. The Pico handles the input side: it drives the chip, serves the VSM data, logs the VSM trace (layer 1) and raises `SCOPE_TRIG`.

- **No level shifting is needed.** `T11` and `I/O` swing from −5 V to +5 V. That is fine for the scope's inputs (use the ±10 V or ±20 V range on Ch B and Ch C), but it would need clamping before it could reach a Pico pin.
- **The rates are easy.** `I/O` changes at most once per 6.25 µs bit time (160 kHz). Streaming all four channels at 1–2 MS/s each gives 6–12 scope samples per bit, which is plenty for software decoding.
- **Digital and analogue line up.** Every decoded sample can be checked directly against the DAC step on Ch A for the same 125 µs period.
- **ROMCLK is not recorded.** The clock is crystal-derived and exact, so the 20 bit times in each sample can be located from the `T11` edge alone. ROMCLK (TP1) was captured once, in Phase 1, to establish the clock-edge alignment (§2). `tools/capture.py --d-signal romclk` repeats that capture if it is ever needed.

## 5. Hardware changes

Done in the schematic:

1. **Probe test points** TP2–TP9, as in §4.
2. **Pico-driven oscillator.** The original BBC-style RC oscillator has been removed. The TMS5220 is now always clocked by the Pico. Pico GPIO22 (`OSC_PICO_3V3`) drives the previously spare 74AHCT125 gate U5D, whose ~OE is tied to GND. U5D's 0/5 V output drives a PNP level shifter (Q1 BC557: R1 10K to the base, R25 3.3K from base to emitter, emitter to +5 V, R26 1K from collector to −5 V). The collector drives `OSC` (U1 pin 6) between −5 V and +5 V. The clock is inverted, which doesn't matter.

   The datasheet (Figure A-1) shows only the RC option. `OSC` is not an ordinary logic input: it is the threshold node of the internal RC oscillator, whose timing network returns to −5 V. **A 0–5 V square wave does not clock the chip.** This was tested on the board: there was no ROMCLK with 640 kHz at 0–5 V, and none with 320 kHz at 0–5 V and PROM OUT strapped towards −5 V. The working reference is the Atari Star Wars sound PCB (SP-225, sheet 16A). There, an original TMS5220 is clocked at 672 kHz from a 74LS161 through a 2N3906 PNP (collector 1K to −5 V) and two CD4069 inverters running from ±5 V, so OSC swings rail to rail. Our circuit is that transistor stage without the CMOS buffer. **Verified on the board (2026-10-06):** OSC swings between −5 V and +5 V at 640 kHz, ROMCLK on TP1 reads 160 kHz, and the TMS5220 speaks the Acorn PHROM words with no READY timeouts. The TI-99/4A note that PROM OUT at −5 V enables a 320 kHz external clock did not hold for this chip. Pin 10 drives strongly, so it was left unconnected. A PWM or PIO clock at exactly 640 kHz, derived from the Pico's crystal, makes the sample rate exactly 8000 Hz. It also makes every run sample-aligned and repeatable, and lets the clock be swept to test timing edge cases. Record the measured clock (from `ROMCLK` on TP1, which should read 160 kHz) in every capture.

   **The firmware generates this clock** (`tms5220ClockStart()` in `tms5220.c`). Without it the TMS5220 has no clock at all. 640 kHz does not divide evenly into the RP2350's default 150 MHz system clock, so the firmware sets the system clock to 144 MHz: the 12 MHz crystal × 120 gives a 1440 MHz VCO, ÷ 5 ÷ 2. That is within the chip's rating, and a PWM slice divides it by 225 to give exactly 640 kHz. To sweep the clock, pick a system clock that is a multiple of the wanted frequency, or accept a non-integer PWM period.

3. **Measurement configuration.** The test board is built on a breadboard. For DAC measurements, the link from the `SPEAKER` / R2 node to C1 is removed, so TP2 sees R2 alone. R2 measures 996 Ω on a meter. Its tolerance does not matter, because the measured value is used. For the load test, a second 1 K was put in parallel with R2 (482 Ω measured). `tools/capture.py --board measure|measure-half|audio --load-ohms <R>` records the configuration in each `capture.json`. The audio output works again once the C1 link is put back (`--board audio`).
4. **Pull-down on GPIO22.** R27 (10K, `OSC_PICO_3V3` to GND at U5D's input) gives U5D a defined low input while the Pico boots, before the firmware starts the clock. U5D's output is then low, so Q1 is on and `OSC` is held at +5 V instead of floating. R27 is fitted on the board and in the schematic.

Still to do:

5. **Optional, later: bring `T11` and `I/O` into the Pico.** This is not needed, because the PicoScope captures both (§4). It would only matter for long unattended runs without the scope. They swing from −5 V to +5 V, so they need a clamped level shift to 0–3.3 V (a plain divider is not enough, because the Pico pins must never go negative). Options are a series resistor with Schottky clamps to GND and 3.3 V, or a small NPN stage per signal. GPIO27 and GPIO28 are still free.
6. **Optional: a virtual-ground DAC measurement.** A transimpedance stage would hold `SPEAKER` at 0 V and give the DAC current with no load effect (datasheet §8.3). The spare LM324 section U2B could do this.
7. **Optional, for Phase 5: Pico-switched TMS5220 power.** The firmware cannot power-cycle the TMS5220, and stopping its clock mid-speech has left it unable to complete a write until it was power-cycled by hand (`firmware/PROTOCOL.md`). Phase 5 wants power cycles as a test condition. A high-side switch on the ±5 V rails, driven from a spare GPIO (27 or 28), would make them automatic. It would also let the host recover a wedged chip without help. Note the datasheet's condition for a reliable power-up clear: VSS − VDD must rise to +10 V in under 2 ms, on both rails together.

## 6. Data set layout and file formats

### Per PROM

```
captures/<prom>/
  prom.json           # inventory: name, hash, provenance, target chip, word table
  index.md            # browsable page: word list, frame counts, links to audio, sweep summary
  takes.csv           # every take: its checks, the hash of its output and of its raw files
  sweep.csv           # every swept address: its parse, checks and output hash
  sweep/addr_<address>/   # one directory per swept address, laid out like a word
  <word>/             # one directory per word, <number>_<name>
    compare.json      # every take compared with the first (tools/compare.py)
    vsm01/, vsm02/    # one capture directory per take: VSM Speak ...
    ext01/, ...       # ... and Speak External
```

`prom.json` is a copy of the PROM's manifest, `proms/<prom>.json` (`tools/prom.py`). The manifest gives the image's hashes, its sources and the reference dump it matches, its target chip and its word table. `tools/promimport.py` writes the manifests and the inventory page, `proms/README.md` (Phase 6). It also rebuilds the images from their pinned public sources into `proms/images/`, which is git-ignored, so images that may not be redistributed stay out of the repo.

### Per capture

```
captures/<prom>/<word>/<take>/
  capture.json        # metadata (see below)

  # Original capture format: written once at capture time, never modified
  raw/scope_raw.npz   # raw PicoScope ADC streams (Ch A-D) + range/offset/timebase
                      #   (committed as scope_raw.npz.zip, see "Where it is stored")
  raw/vsm_trace.bin   # layer 1: every VSM command and returned bit, with timestamps

  # Readable formats: regenerated from raw/ by the post-processing scripts
  vsm_trace.csv       # layer 1 as text
  frames.csv          # the frames the chip parsed: bit offset, energy, pitch, K1-K10, repeat flag
  samples.wav         # layer 2: I/O decoded from the scope capture, mono PCM, rate = fosc/80,
                      #   from the first T11 period after SCOPE_TRIG rose to the end of the capture
  samples.csv         # layer 2 as text (sample index, value, T11 timestamp), with the DAC voltage
                      #   and the DAC code recovered from it
  dac.wav             # layer 3: recovered per-sample DAC codes as WAV
  listen.wav          # convenience: filtered and resampled to 48 kHz for listening
```

The `raw/` files are the record of what was measured. The `scope_raw.npz` layout (array names, dtypes, scaling) is documented alongside the scripts, so it can be read without the capture tooling. Every readable file can be rebuilt from `raw/` with one command (`tools/process.py`), and `capture.json` records the script version that built it, under `processed`, together with the checks.

### WAV conventions

- **`samples.wav`:** 16-bit signed PCM, one WAV sample per chip sample, no resampling. The header records the nominal rate (8000 Hz). The *measured* rate goes in the JSON. The chip's 10-bit code is left-justified (code × 64), so the chip's full scale is the WAV's full scale and the file plays at a normal level. The code is the sample shifted right by 6, and the bit width and scaling are documented in the JSON. (Storing the code in the low bits would be equally exact, but at about −48 dBFS it plays as near silence.) A WAV like this is bit-exact, plays in any tool, and diffs trivially against an emulator's output.
- **`dac.wav`:** the same convention, built from the layer 3 recovered codes.
- **`listen.wav`:** 48 kHz, optionally with the BBC filter applied. Nothing is computed from it. It is for ears only.
- Avoid MP3, FLAC-with-resampling and any other processing in the reference files. FLAC is fine for archive size, because it is lossless.

### `capture.json` contents

Record at least:

- The source PHROM (name, file hash), the word number, the VSM address and the command sequence sent.
- The chip: part marking, date code and revision (5220 or 5220C).
- The board revision, the measured `fosc`, the supply voltages and the temperature if you can get it.
- The firmware git hash, the host script git hash and the capture timestamp.
- The final status register value, the `INT` events, the count of bits consumed and the total samples.
- The capture procedure (Phase 5): whether the chip was reset first, and, for the standard procedure, the frame the Speak was timed for and when the chip accepted it.

`tools/run.py` writes all of these. The chip, supply and temperature details come from `tools/bench.json`, which must be updated whenever the chip or the board changes. The results of the checks go under `processed` (`tools/process.py`).

### Where it is stored

The raw scope data is large. Four channels at 2 MS/s is about 8 MB per second of speech uncompressed, so a full PROM with repeats runs to gigabytes. Zip compression brings that down by about 10× (the Phase 1 captures shrank from 305 MB to 26 MB), because the digital channels compress well. There are two kinds of capture:

- **Evidence captures** live in `captures/phase<N>/<id>/`, are cited by the datasheet, and are **committed to the repo**. The raw file is committed as `raw/scope_raw.npz.zip`: a zip that holds the original `scope_raw.npz` unchanged, so its SHA-256 still matches the evidence index. The tools read the zip directly, and `.gitignore` excludes the unzipped `.npz`. Keep each phase's evidence to a few tens of MB.
- **PROM corpus captures** (Phases 7 and 10) live in `captures/corpus/<prom>/` and are **kept local for now**. Only the raw data's zip is kept (`run.py --zip-only`), which makes a take about 2 MB. Apart from each PROM's `prom.json`, `index.md`, `takes.csv` and `sweep.csv`, the directory is git-ignored. How to publish the corpus (release archives, Git LFS or elsewhere) is decided in Phase 12. Until then, the repo holds the scripts, manifests and those index files. `takes.csv` gives the SHA-256 of every take's raw files, so the local copies can be checked, and of its output, so an emulator can be checked against the output without the raw data. Back up the local corpus separately, because it is the only copy.
- **Vector captures** (Phase 8) live in `captures/vectors/vectors_<set>/` and are kept local in the same way. Only each set's `index.md` and `takes.csv` are committed; the set's definition and image are in `captures/phase8/vectors/`, so the vectors can be captured again on any board.

## 7. Automation architecture

```
Host (Python, tools/pico.py and tools/capture.py)
 ├── Pico 2 over USB CDC: a line-based command protocol (firmware/PROTOCOL.md)
 │     UPLOAD <bank> …  load a PHROM/VSM image into an SRAM bank (16 KB each, 4 at once)
 │     SPEAK <addr>     speak from the VSM, with SCOPE_TRIG marking start and end
 │     SPEAKEXT …       Speak External (see §9), the FIFO topped up on INT
 │     CMD / READ       raw command and FIFO writes, status and data reads
 │     RESET, CLOCK     the reset sequence; set or stop the OSC clock
 │     TIME, SPEAK … at_us   the Pico's timer, and Speaks timed to the chip's frame clock
 │     TRACE            download the VSM and host interface trace (layer 1)
 └── PicoScope 3405D over ps3000a: arm, wait for the trigger, save
```

A run loops over a manifest of the PHROM images and their word tables. For each word it uploads the image (once per PHROM), arms the scope, sends `SPEAK`, waits for Talk Status to clear, pulls the traces, writes the capture directory, and runs sanity checks: the layer 2 and layer 3 data agree, and the bit count matches the parsed frame count. Speak each word several times (say 5) to confirm the chip is deterministic. Any difference between repeats is itself a finding, for example an uninitialised state or the noise generator's state carried over between utterances.

`tools/run.py` is this runner (Phase 4). The manifest is `proms/<prom>.json`. After each take it runs `tools/process.py`, which writes the readable files and the checks; after each word, `tools/compare.py`; and at the end, `tools/promindex.py`, which rewrites `index.md`. `capture.py --speak` remains for one-off captures outside a PROM.

Every take uses the standard procedure from Phase 5 (`tools/frameclock.py`). At the start of a run the runner locates the chip's frame clock on the Pico's timer and calibrates the start of each path. Then, for every take, it sends the reset sequence, and times the Speak so that the chip accepts it 12256.1 µs before a frame boundary. That fixes the start delay and the noise generator's state, so repeats should match (datasheet §7).

## 8. PHROM and speech data corpus

The aim is to cover **every original speech PROM that can be found**. This needs an inventory step first. Collect the images, record their hashes and provenance, and check the word tables. Candidates to look for:

- **Acorn PHROM A** (Kenneth Kendall voice). Already in `romdata_acorn.h`, from TMS6100-Emulator.
- **Acorn PHROM US** (American voice), if a dump exists. It is TI's VM61002, which was found (Phase 6).
- **Other TMS6100 / VSM dumps** that are archived in MAME (for example for TI products), checked for whether they target the TMS5220 or the TMS5200/5100. Older chips use different coefficient tables and frame formats. Those dumps will still play on a 5220, but they will sound wrong. That is still a useful test, as long as it is labelled clearly.
- **Speak External data from other TMS5220 hosts.** Arcade boards (for example several Atari games) and Apple II and similar speech cards stored LPC data in CPU ROM rather than in a VSM. We can replay it through Speak External (§9).

### Known sources

PROM images:

- **TMS6100 Multi-PHROM Emulator for the BBC Micro** (eRadiofan): <https://github.com/eRadiofan/TMS6100-Multi-PHROM-Emulator-for-the-BBC-Micro/tree/main/Firmware/Common/Src>. Several PHROM images built into its firmware sources.
- **TMS6100-Emulator** (simoninns): <https://github.com/simoninns/TMS6100-Emulator/tree/master/Firmware/tms6100>. The source of the Acorn PHROM A data in `romdata_acorn.h`, and of a full VM61002 dump.
- **Talkie** (Peter Knight): <https://github.com/going-digital/Talkie>. An Arduino LPC library whose vocabularies are words copied from TI and Acorn PROMs, including 232 words of the VM61003 and VM61005, which are not otherwise available.
- **Star Wars sound ROM disassembly** (nmikstas): <https://github.com/nmikstas/star-wars-arcade-audio>. The game's 23 Speak External speech blocks.
- **MAME** (<https://github.com/mamedev/mame>): the reference hashes of the dumps (`hash/bbc_vsm.xml`, and the drivers' ROM lists), and the list of systems that used a VSM.

Information:

- **TI Portable Speech Lab** (Stuart Conner): <https://www.stuartconner.me.uk/ti_portable_speech_lab/ti_portable_speech_lab.htm>. TI's speech development system, which is background for the authoring guide.

When the images are imported, record which source each came from, along with its hash, in `prom.json`. Where the same PROM appears in more than one source, compare the copies. Phase 6 did this (`proms/README.md`).

For each image, the word table gives the entry points. Also do a **sweep of every byte address**, not just the word starts. Starting mid-word produces garbage, which exercises the frame parser with data no encoder would write. It turned out not to be deterministic: such data can drive the filter far outside the range of speech, where the smallest upset grows to the full output range (Phase 7). The sweep leaves out addresses whose data reaches the end of the image without a stop code, because those read on into an empty bank (`prom.sweepPoints`). Only the Acorn PHROM A is swept at every address (Phase 7). The other dumps are swept at a sample of addresses, and the assembled images are not swept (Phase 10).

The corpus also feeds the authoring guide. Analysing how the original PROMs were encoded (frame rates, use of repeat frames, energy and pitch ranges, word table and header layout) shows what real speech data looks like and what the original tools produced.

## 9. Beyond PHROMs: controlled stimulus

PHROMs only exercise the parameter combinations that someone happened to encode. An emulator needs every path covered. There are two mechanisms, and we already have both:

### 9.1 Speak External (command `0x6x`)

The Pico writes LPC frames straight into the 16-byte FIFO. This gives complete control over the bit stream with no VSM involved. Use it for **synthetic test vectors** generated by a host script:

- **Single-parameter sweeps.** Hold every parameter at a neutral value and step one index through its whole range: every energy index (0–15), every pitch index (0–63), and every K1–K10 index. These tests read out the chip's internal parameter lookup tables one entry at a time.
- **Voiced and unvoiced.** The same parameters with pitch = 0 (noise) against pitch ≠ 0 (the chirp excitation). Long unvoiced runs characterise the noise generator (its LFSR sequence and period).
- **Interpolation.** Pairs of frames with large parameter jumps, to capture the per-interpolation-period stepping, and the documented rule that interpolation is inhibited across voiced/unvoiced and silence transitions.
- **Special frames.** Repeat frames, energy = 0 (silence), and the stop code (energy = 15), including the "interpolate to zero" tail.
- **FIFO and protocol behaviour.** Start-up with exactly 8, then 9, bytes (TS should go high at 9). Starve the FIFO deliberately mid-utterance (BE, abnormal termination, `INT`). Check the BL threshold timing. Confirm that Reset is ignored during Speak External.
- **Pathological input.** Random bit streams, for robustness and determinism.

### 9.2 Synthetic VSM images

The Pico serves arbitrary VSM data. The same vectors can be packed into a fake PHROM to test the VSM path: Read-and-Branch, Load-Address nibble sequencing, Read Byte (`0x1x`), and address wrap-around at bank boundaries. Comparing the Speak External and VSM versions of the same vector confirms that the two input paths are equivalent.

### 9.3 Timing and handshake tests

The VSM trace and the trigger GPIO also give timestamps for: READY assertion times, the delay from Speak to first fetch, how the fetches line up with frame boundaries, how many bits per frame the chip reads ahead, and the reset/power-up sequence. This is the information an emulator needs to be *cycle*-accurate on the host interface, not just sample-accurate on audio.

### 9.4 Confirmation with new speech data

These tests confirm that the documented mechanism is correct and complete, using speech data the chip has never seen. Everything is produced from the datasheet and the project's own tools, with no reference to the original PROMs or to MAME.

1. **Generate the data.**
   - Phrases encoded by the project's encoder from new recordings, for example a set of words and sentences not present in any original PROM.
   - Hand-designed frame sequences that use every feature: voiced, unvoiced, repeat and silence frames, the stop code, large interpolation jumps, and extreme parameter values.
2. **Package it two ways:**
   - as a new VSM/PHROM image with its own word table, laid out as the authoring guide describes, and
   - as Speak External byte streams.
3. **Predict.** Run the reference emulator on each item. Commit the predicted samples and the predicted VSM fetch sequence *before* the chip is run, so the prediction cannot be adjusted to fit the result.
4. **Run on the chip.** Play every item through the Pico (VSM emulation and Speak External), using the repeatable procedure from Phase 5, and capture it like any other PROM.
5. **Compare.** The captured `I/O` samples must match the prediction sample for sample. The VSM trace must match the predicted fetches. The VSM and Speak External versions of each item must produce the same output.
6. **Listen.** Each phrase must be intelligible and must sound as intended. This catches a datasheet and an emulator that agree with each other but are both wrong in the same way.

Any mismatch is a gap in the datasheet. Fix the documentation first, then regenerate the data and rerun the whole set. The new speech data, its predictions and its captures are published as part of the data set, as a worked example of authoring.

If the authored image is meant for a physical PHROM replacement, also check that a BBC Micro's speech system reads its word table and speaks the words.

## 10. From captures to the datasheet

Each datasheet section (§1.1) is drafted as soon as its experiments give results, not at the end:

1. **Hypothesis.** Start from the original datasheet, then MAME's `tms5220.cpp` and other published notes.
2. **Test.** Design a vector (§9) that would show whether the hypothesis is wrong, and capture it.
3. **Write.** Document the confirmed behaviour, citing the capture directories that prove it. Where it differs from the original datasheet, say so explicitly.
4. **Check.** Update the reference emulator to match the text, and rerun it against the whole data set.

Keep a change log in the datasheet listing every correction and addition to the original, with its evidence.

## 11. Risks and open questions

- **What does `I/O` carry?** Resolved (§2): the 10-bit sample, LSB first, which matches the DAC.
- **Run-to-run differences.** Mostly resolved in Phase 5 (datasheet §7). The start delay comes from a free-running frame clock, and the noise from a free-running generator that Reset holds until the next Speak. The standard procedure fixes both, so repeats match to within ±3 codes. What is left are "upsets": small departures that start at a few recurring points in a word, and that no controlled state explains. They are the main open question for Phase 8, because an emulator cannot match the chip bit for bit while the chip does not match itself. In the louder TI-99 words (Phase 6) they reach 15 codes. Phase 7 found a third kind of carried state: the previous utterance changes the first samples of the next one, through the reset sequence. And across the Acorn PHROM A, only 25 % of takes match their word's majority exactly. Phase 8 explained the previous utterance: the interpolator's registers are never cleared, and an utterance that starts with a repeat or silence frame uses the previous one's K values. The upsets remain unexplained; they occur in synthetic data too.
- **Internal state that is not visible on `I/O`.** If `I/O` carries only the final sample, the inner filter state has to be inferred. Carefully designed vectors (for example, one K parameter active at a time) keep that inference tractable.
- **`TEST` and `PROM OUT`.** These may expose internal ROM contents or test modes. They are undocumented and might put the chip into an unexpected state. Treat any exploration of them as a separate, later, deliberate experiment, preferably on a spare chip.
- **Chip variation.** Capture on more than one TMS5220 (and a 5220C, if one is available) to separate design behaviour from part-to-part analogue variation. The digital outputs should match exactly. The analogue outputs will not. Document any 5220/5220C differences in the datasheet.
- **Stopping the clock.** The TMS5220 has no clock of its own, so `OSC` stops whenever the Pico resets or is reflashed. Once, mid-speech, this left the chip unable to complete any write until it was power-cycled (§5 item 7, `firmware/PROTOCOL.md`). Phase 5 found that stops while the chip is idle are safe: up to 1 s they freeze its state, and after 10 s it still works, though its frame clock has moved. Stops during speech were not repeated.
- **Clock fidelity.** The board no longer has the original RC oscillator, so captures are made with a crystal-derived clock. That is ideal for reproducibility, but real BBC Micro hardware ran slightly off 640 kHz and drifted. To study that behaviour, have the Pico sweep the clock frequency rather than refitting an RC network.
- **Licensing of PHROM images.** Keep the original ROM images out of the repo unless their redistribution status is clear. Commit the manifests and hashes, plus the captured output if appropriate. Since Phase 6, `tools/promimport.py` rebuilds every image except the Acorn PHROM A from public sources pinned to a commit, so the repo holds only the manifests. Whether the captured output of each PROM may be published is decided in Phase 12.
- **Copyright of the original datasheet.** See §1.1: the new datasheet is written fresh and cites the original.

## 12. Phases

Each phase lists its goal, tasks, deliverables, completion criteria and status (as of 2026-10-07). Phases 1–5 build the capture system. Phases 6–12 build the data set and the datasheet. Datasheet sections are drafted in whichever phase produces their evidence (§10), not saved up for the end.

| Phase | Depends on | Status |
|---|---|---|
| 1. Bench bring-up and signal identification | — | Done |
| 2. Measurement hardware | 1 | Done |
| 3. Firmware instrumentation | 1 | Done |
| 4. Host tooling | 1, 3 | Done |
| 5. Repeatability | 3, 4 | Done |
| 6. PROM inventory | — | Done |
| 7. Acorn PHROM A reference set | 2, 4, 5, 6 | Done |
| 8. Synthetic vectors and the core datasheet | 4, 5 | Done |
| 9. Reference emulator | 8 (runs alongside it) | In progress: emulator and harness done; not yet bit-exact |
| 10. Remaining PROM corpora | 6, 7 | Done (within the tolerance of §1.5 on every PROM) |
| 11. Authoring, and confirmation with new speech data | 8, 9 | Not started |
| 12. Publish | All | Not started |

### Phase 1: Bench bring-up and signal identification

- **Goal:** the chip runs from the Pico, and every output signal is understood well enough to capture it.
- **Done:**
  - The 640 kHz rail-to-rail clock (§5).
  - Probes A–D wired, and automated PicoScope capture working.
  - The `I/O` format decoded (10 bits, LSB first, slots 2–11).
  - The DAC update timing measured.
  - The audio stage's loading of TP2 identified (§2).
  - The ROMCLK edge alignment: all `T11` and `I/O` edges follow ROMCLK rising edges (§2).
  - The first datasheet sections, drafted in `tms5220_reference_datasheet/`: the system clock (§2, Appendix A), the DAC timing (§8.2), the serial sample output (§8.4), the measured output signals (§9.5) and the pin functions (§11.2). An evidence index (`evidence/README.md`) lists captures p1-01 to p1-03 with their hashes. The captures themselves are committed in `captures/phase1/`, with the raw data zipped (§6).
  - `tools/edge_timing.py`, which measures every edge relative to `T11` and ROMCLK.
- **Delivers:** the findings in §2, and the first datasheet sections.
- **Done when:** those datasheet sections are drafted, with their captures cited.
- **Status:** done (2026-10-06). Handed on:
  - The DAC transfer function, including the jump between codes −1 and 0 → Phase 2.
  - The code resting at 0 rather than −1 between back-to-back words → Phase 5.
  - The mapping from slots to TI's bit times T1–T20, which so far is inferred from the pin name only → Phase 8.

### Phase 2: Measurement hardware

- **Goal:** TP2 measures the DAC and nothing else.
- **Tasks:**
  - Remove C1, or add a measurement-only DAC path (§3), so the audio stage no longer loads the DAC.
  - Fit a 0.1% 1 K load if its tolerance matters.
  - Add the GPIO22 pull-down (§5).
  - Record the board configuration, so that every capture can state which configuration it was taken with.
- **Delivers:** an updated schematic and board, and §5 brought up to date.
- **Done when:**
  - A static code-to-voltage table explains TP2 to within the measurement noise (about 0.3–0.5 mV rms), with no dependence on earlier samples.
  - The jump at zero (§2) has been measured cleanly.
- **Done:**
  - C1 is disconnected for measurements, and R27, the GPIO22 pull-down, is fitted on the board and in the schematic (§5).
  - R2 was measured (996 Ω) rather than replaced with a 0.1 % part.
  - `capture.py` records the board configuration and the load.
  - `tools/dac_transfer.py` gives the per-code table, bit weights, linearity and history check. `tools/dac_compare.py` compares two loads.
  - Captures p2-01 to p2-06 are in the evidence index, and the datasheet's §8.2 and §8.3 are drafted from them.
- **Status:** done (2026-10-06).
  - **No dependence on earlier samples.** Regressing on the four previous samples removes less than 0.02 mV.
  - **The table explains TP2 to within the noise, but the noise is larger than the criterion assumed.** The criterion's 0.3–0.5 mV was too optimistic for this breadboard: idle noise is 0.5–0.8 mV per sample, and during speech the scatter is 1–2 mV. That scatter depends on neither the code nor the history.
  - **The jump at zero is clean.** It measures 5.6–5.9 LSB at both loads.
  - **New finding:** the output current depends on the load (datasheet §8.3).
- **Handed on:**
  - Codes below −111 and the negative clipping were not reached by the demo phrase. They need controlled vectors → Phase 8.
  - Why one word's DAC step is 0.3 µs earlier → Phase 8.
  - The extra noise during speech, which may come from VSM bus activity on the breadboard → Phase 5, when repeat captures are compared.
  - The absolute current scale rests on the scope's DC gain, which is accurate to a few per cent. A meter check of the idle TP2 voltage with the chip silent would tighten it. A virtual-ground measurement (§5 item 6) is optional.

### Phase 3: Firmware instrumentation

- **Goal:** the host can drive every input path without supervision.
- **Tasks:**
  - The USB command protocol of §7: `UPLOAD`, `SPEAK`, `SPEAKEXT`, `CMD` and `TRACE`, plus `RESET` and power-up sequencing for Phase 5.
  - A compact, timestamped VSM trace.
  - VSM image upload into SRAM.
  - Retiring the demo loop.
- **Delivers:** command-driven firmware, with its protocol documented in `firmware/`.
- **Done when:** a host script can upload an image, speak any address or any Speak External stream, and fetch the VSM trace, all repeatedly, with no manual steps.
- **Done:**
  - The `SCOPE_TRIG` output.
  - **The firmware is driven by commands over USB.** It has `INFO`, `STATS`, `CLOCK`, `RESET`, `UPLOAD`/`UNLOAD`, `SPEAK`, `SPEAKEXT`, `WAIT`, `CMD`, `READ`, `TRIG` and `TRACE`, documented in `firmware/PROTOCOL.md`.
  - **The demo loop is retired.** The firmware identifies itself with its git description.
  - **End of speech comes from `INT`.** The status register is read only when `INT` asserts, plus a few reads at the start, so there is little bus activity during speech. Speak External tops up the FIFO on each Buffer Low.
  - **Up to four 16 KB banks of SRAM**, any of the 16 bank numbers. The Acorn PHROM is built in, in bank `F`, and the host can replace it. Uploads are checked with CRC-32.
  - **The VSM trace.** Core 1 logs every Load Address, Dummy Read, bit Read and Read and Branch, with the bit returned. It also logs every change on `WS`, `RS`, `READY`, `INT` and `SCOPE_TRIG`, with the data bus. Each event is 8 bytes and timestamped to about 1.5 µs. The trace holds 32,768 events, about 27 words.
  - **Power-up sequencing so far.** `RESET` (the datasheet's nine `0xFF` and Reset) and `CLOCK` (set or stop `OSC`). Real power cycles need hardware (§5 item 7).
  - **`tools/pico.py`**: the host class, the trace decoder, a command line and `selftest`. `tools/lpc.py` finds where a word ends.
  - **`tools/capture.py --speak` and `--speakext`** arm the scope, speak and save `raw/vsm_trace.bin` and `vsm_trace.csv`, with no manual steps.
  - Captures p3-01 to p3-03 are in the evidence index. (Phase 4 found that they were taken with the C1 link connected, although they were recorded as `measure`. Their `board` entries are corrected; their traces and `I/O` samples are unaffected.)
- **Status:** done (2026-10-06). `pico.py selftest --repeat 3` passes: 54 utterances with no manual steps.
  - **Images.** The Acorn image was uploaded to banks `F` and 3.
  - **Words.** Six words were each spoken from both banks and through Speak External, three times over.
  - **Checks.** In every VSM run, the bits the chip read matched the image. In every Speak External run, the bytes on the bus matched the stream, and the duration matched the VSM's.
- **Findings, for the datasheet (Phase 8):**
  - **No VSM read-ahead.** For every word, the chip read exactly the bits up to the end of its stop code. For example, ACORN reads 1152 bits, 27 frames.
  - **Read Byte (`0x1x`) returns the VSM byte bit-reversed:** 0x23 reads back as 0xC4. The first bit read lands on D0, which is TI's MSB. The FIFO, by contrast, takes a written byte LSB first: the same bytes speak correctly through Speak External.
  - **Each `0xFF` of the reset sequence is itself a Reset command** (`X111XXXX`). Each one makes the chip issue a Load Address, then a Dummy Read, to the VSM, about 250 µs apart. The trace reads the dummy nibble as 7 for each `0xFF`, and as 0 for the final `0x70`.
  - **Talk Status takes time to set.** A status read 22 µs after a Speak (preceded by Load Address) still shows Talk Status clear. The original datasheet asks for up to 287 µs after such a Speak before the next command.
  - **BE is not cleared by a VSM Speak.** It is left from the last Reset or Speak External. A Speak External stream whose stop code ends on a byte boundary finishes with BE set, yet its speech is complete.
- **Handed on:**
  - **Stopping the clock can wedge the chip.** Reflashing the Pico while the old demo loop was speaking left the chip unable to complete any write until it was power-cycled. A reflash while it was idle did no harm. Which clock stops are safe, and whether `CLOCK 0` can be used as a reset → Phase 5, with §5 item 7 if power cycles are to be automated.
  - **Repeats differ.** Repeats of the same word differ by about 3 codes rms, whether they come through the same path or not (p3-01 to p3-03). Between the two VSM runs, the delay from `SCOPE_TRIG` to the first sample of speech also differed, by 4.3 ms. Whether VSM and Speak External are equivalent can only be judged once the start state is understood → Phase 5, then §9.2 in Phase 8.
  - The trace's view of host interface timing (READY, `INT`, Speak to first fetch) is ready for the §9.3 timing tests → Phase 8.

### Phase 4: Host tooling

- **Goal:** one command turns a request ("speak word N of PROM X") into a complete capture directory.
- **Tasks:**
  - The runner that drives the Pico and the scope together.
  - The full §6 directory layout and `capture.json`.
  - A VSM trace parser and frame decoder (`frames.csv`).
  - Checks: layer 2 against layer 3, and the bit count against the frame count.
  - `listen.wav`, and the per-PROM `index.md` generator.
  - A repeat comparator.
  - Choose between block mode and streaming mode (§4).
- **Delivers:** the `tools/` package and its README.
- **Done when:** a single command produces a §6-compliant directory for one word, with all checks passing.
- **Done:**
  - Earlier: `tools/capture.py` and `tools/decode.py` (triggered block capture, `I/O` decode, DAC averaging), and from Phase 3 the Pico client and trace decoder (`tools/pico.py`).
  - **PROM manifests.** `proms/acorn_phrom_a.json` holds the Acorn PHROM A's hash, provenance and 165-entry word table, built from `romdata_acorn.h`. Phase 6 added the other PROMs, and moved the building to `tools/promimport.py`.
  - **The runner, `tools/run.py`.** It takes a PROM and words (numbers, names, `all`, or `--address` for sweep points), a path (VSM, Speak External or both) and a number of takes. It uploads the image if the Pico's bank does not hold it, captures each take, and writes the §6 layout.
  - **`tools/process.py`** rebuilds every readable file from `raw/`: `vsm_trace.csv`, `frames.csv`, `samples.wav` and `.csv`, `dac.wav` and `listen.wav` (48 kHz, through a model of the BBC audio stage). It runs twelve checks and records them in `capture.json` (`tools/README.md` lists them).
  - **The frame decoder** (`tools/lpc.py`): every frame's bit offset, length, kind, energy, repeat flag, pitch and K1–K10 indices. For the VSM path, `frames.csv` also gives when each frame's first bit was read, and in which output sample period.
  - **Layer 2 against layer 3.** The Phase 2 bit-weight model of each `I/O` code is fitted to the DAC voltage with a gain and an offset. The check needs a residual of 3 mV rms or less, a gain within 15 % of the table's, and 99 % of the DAC codes recovered within ±2 of `I/O`.
  - **`tools/compare.py`** aligns each take with the first and compares them sample for sample, and frame for frame. **`tools/promindex.py`** writes `index.md`.
  - **`tools/bench.json`** records the chip (TMS5220NL, `DEP 8423`, and from 2026-10-08 its number) and the measured supplies. Four chips are labelled 1–4 (datasheet Conventions). Chip 1 made every capture before 2026-10-08.
  - **Block mode** is chosen (§4).
- **Status:** done (2026-10-06). One command, `run.py acorn_phrom_a ACORN --takes 2 --path both --out captures/phase4 --zip`, wrote `captures/phase4/acorn_phrom_a/` with four takes. Every check passed on every take, and the runner exited 0. The set is p4-01 to p4-04 in the evidence index.
- **Findings:**
  - **The Phase 3 captures were taken with the C1 link connected.** The layer 3 check failed on them, and on a first Phase 4 run: TP2 carried the audio stage's high-pass history (6.5 mV rms of residual, which a first-order term brings down to 1.8 mV). With the link removed, the residual is 2.2–2.3 mV. The Phase 3 `board` entries are corrected, and the evidence index says so.
  - **The DAC scale has drifted.** It is 7–8.5 % above the Phase 2 table (code −1 at 0.635 V, against 0.592 V), with the supplies within 6 mV of nominal. The table's shape still fits. (Datasheet §8.2.)
  - **Frames are read exactly 25 ms apart, but the start varies by most of a frame.** The first frame read came 12.5–25.5 ms after `SCOPE_TRIG` in three VSM Speaks, while the rest of the word took exactly 674.87 ms each time. (Datasheet §4.)
  - Repeats differ by 3.1–3.5 codes rms after alignment, as in Phase 3. The frames parsed are identical in every take.
- **Handed on:**
  - **The start delay** looks like a wait for the next boundary of a free-running frame timer. Phase 5 should test that, together with the noise generator's state.
  - **The DAC scale drift.** Its cause (chip, load or scope), and whether captures need a per-session scale → Phase 5. A meter reading of TP2 with the chip idle would separate the scope from the chip.
  - **Phase 7 needs only the procedure from Phase 5.** The runner already takes `--reset` and `--idle`, and records them in `capture.json`.

### Phase 5: Repeatability

- **Goal:** a capture procedure whose results can be reproduced, and a documented account of any state the chip carries between utterances.
- **Tasks:**
  - Repeat the same word under different conditions: plain Speak, a Reset command first, a full power cycle, and different idle times.
  - Characterise the noise generator's state and the start delay. So far a 6- or 25-sample start variation has been seen, and unvoiced output differs between repeats (§2). In Phase 4 the first VSM frame read moved by up to 13 ms between takes, while the rest of the word kept exactly the same timing.
- **Delivers:**
  - The standard procedure used by Phase 7 onwards.
  - A datasheet section on state at power-up and at the start of speech.
  - A description of the starting state that the emulator needs.
- **Done when:** repeats under the standard procedure are either bit-identical, or differ only in the way the documented state model predicts.
- **Done:**
  - **Firmware.** Timed starts (`TIME`, and an optional start time on `SPEAK` and `SPEAKEXT`), and `trig_us` and `late_us` in their replies. `POLL` controls status reads during speech; they are now off by default, so the end of speech is found from `INT` alone. `VSMDELAY` delays the VSM data, for timing tests. `CLOCK` reports when it acted (`firmware/PROTOCOL.md`).
  - **`tools/frameclock.py`.** It locates the chip's frame clock on the Pico's timer from one Speak's trace. For each path, it calibrates the start so that the chip accepts the command in the target slot.
  - **The standard procedure, now the runner's default (`run.py --start standard`).** Before every take, the reset sequence; then a Speak timed to the frame clock, accepted 12256.1 µs before a frame boundary. `capture.json` records the frame and the acceptance time. `process.py` checks them (`start_frame`, `start_slot`), and `compare.py` compares the standard takes of a word with their per-sample majority. `--start free` gives the Phase 4 behaviour.
  - **Datasheet §7 drafted.** It covers power-up, the frame clock, the noise generator, clock stops and upsets, and says what an emulator needs. §4, §6, §8.2 and §8.4 have additions.
  - **Evidence:** p5-01 to p5-18 in the evidence index. p5-01 to p5-08 are the standard procedure, through VSM and Speak External; the rest are the experiments.
- **Status:** done (2026-10-06), with one part of the state model still open. One command, `run.py acorn_phrom_a ACORN --takes 4 --path both --out captures/phase5 --zip`, gave eight takes that all passed every check. Within each path, every take started on the same sample, with the same noise, and differed from the others by at most 3 codes. They were not bit-identical: each carries small upsets (below). Elsewhere in the experiments, groups of standard takes were bit-identical, including across clock sessions [p5-14, p5-15]. So the procedure fixes all the state that was found; upsets are the documented remainder.
- **Findings:**
  - **The start delay is a free-running frame clock.** A Speak waits for the first frame boundary at least about 320 µs after it is written. Neither Speak, Reset nor idle time moves the clock; a power cycle does.
  - **The noise generator runs freely with a period of 8191 frames.** It keeps running between words, so takes whose start frames differed by a multiple of 8191 matched. Reset holds it until the next Speak, which restarts it from a fixed state. That is why a Reset before a timed Speak gives the same noise on any frame.
  - **The restart is counted in slots.** The firmware's write sequence sometimes finished one slot (6.25 µs) later, depending on how its timing fell against the chip's READY. That alone changed the noise. The runner therefore calibrates the start time each run, and checks every take.
  - **Upsets.** Even with all of this fixed, most runs of takes include ±1–3 code departures. They start at a few recurring points in each word and then persist for the rest of it. They did not change with the clock rate (600, 640, 800 kHz), the status polls, the VSM data timing, a preceding word, a power cycle or the spacing between takes.
  - **Clock stops while idle are safe.** Up to 1 s they freeze the chip's state. After 10 s the chip still works, but its frame clock has moved. `CLOCK 0` is not a reset.
  - **The VSM data timing has a wide margin.** The chip samples `ADD8` about 10 µs after `M0` rises.
  - **Speak External, accepted at the same time as a VSM Speak, starts 2 samples earlier, with different noise.** Each path repeats as well as the other.
  - **The DAC scale step is real.** A meter read 0.642 V on TP2 at idle (Phase 2: 0.592 V), and the step has not changed since Phase 4.
  - The Phase 1 question of the code resting at 0 between words is resolved (datasheet §8.4).
- **Handed on:**
  - **Upsets → Phase 8.** Find their cause before the emulator is held to bit-for-bit matching. Candidates: an internal state not cleared by Reset, such as the filter's registers; marginal internal logic, sensitive to supply or temperature; or interference on the breadboard. Tests: temperature; supply voltage; decoupling; a second chip; and how the chip's state is left at the end of a word.
  - **Phase 7 uses the standard procedure.** Take at least 5 takes per word and path, and publish all of them. Until upsets are explained, the reference output for a word is the per-sample majority of its takes, and `compare.json` says where there is no majority.
  - **For Phase 8:**
    - the noise generator's polynomial and steps per slot, and its state after Reset;
    - why Speak External starts 2 samples earlier than a VSM Speak;
    - what the original's "T State Counter" is.
  - **For a later hardware revision:**
    - whether the DAC scale step is in the chip or in the load (§5);
    - the Pico-switched power of §5 item 7, if upset tests need many power cycles (the one here was done by hand).
  - **Not looked at:** the extra DAC noise during speech, handed on from Phase 2. It affects only layer 3 → Phase 8.

### Phase 6: PROM inventory

This phase can run alongside Phases 1–5.

- **Goal:** know exactly which original speech data exists.
- **Tasks:**
  - Collect the images from the known sources (§8) and from MAME.
  - Hash them, record their provenance, and compare duplicate copies.
  - Identify each image's target chip (5220, or 5100/5200).
  - Extract and check the word tables.
  - Write `prom.json` for each image.
- **Delivers:** the inventory and the manifests. Images that cannot be redistributed stay out of the repo (§11).
- **Done when:** every image found has been inventoried, and the list is frozen for version 1. Images found later go into version 1.x.
- **Done:**
  - **`tools/promimport.py`.** It fetches each source file at a pinned commit and rebuilds the image from it. It checks each image against MAME's hash, and reads the word table from the image's own index, cross-checked against the source's published word list. It identifies the frame format from the data, and writes the manifests and `proms/README.md`. The images go in `proms/images/`, which is git-ignored.
  - **The inventory, version 1.0** (`proms/README.md`), eight manifests:
    - **Dumps, matching MAME byte for byte:**
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
  - **Multi-bank images.** An image may span several 16 KB banks. `run.py` uploads every bank, and the tools find words by their offset from the image's first bank (`prom.base`). The Pico holds 4 banks at once, so before uploading, `run.py` now unloads banks that belong to other PROMs, and the self-test starts from empty banks.
  - **The DAC check works at the rails** (`process.py` version 3). Loud words failed `layer2_vs_layer3`, because the Phase 2 model is wrong at codes +127 and −128. The fit, the residual and the code recovery are now judged on codes inside ±119, and the rail samples are reported separately.
  - **Evidence:** p6-01 to p6-08, TI-99's MORE and LINE, two takes per path (datasheet §8.2).
- **Status:** done (2026-10-06). The list is frozen as version 1.0. At least one word from every PROM was captured with the standard procedure, through both paths for TI-99, Star Wars and the VM61005 words, and all passed every check. That includes TI-99's LINE, which runs from the first VSM into the second. Of the 22 takes, one TI-99 take in the first run was accepted one slot early and failed `start_slot`. Its run was repeated, and every take in the repeat passed.
- **Findings:**
  - **Every copy found is a good dump.** The two Acorn copies are identical to the image in this repo. The eRadiofan copies stop after their last word, and padded with zeros they match MAME's dumps exactly.
  - **Talkie's words are verbatim.** All 766 of its words that come from a held image match that image byte for byte. On that basis its VM61003 and VM61005 words are taken as original.
  - **Every held image is in the TMS5200/TMS5220 frame format.** Every word in all eight parses to a stop code, and none runs into the next word. Read with the TMS5100's 5-bit pitch field instead, 2–123 words per PROM run into the next one. The data cannot separate the TMS5200 from the TMS5220, which share a frame format. TI-99's target, the TMS5200, comes from the product.
  - **Four word names in `romdata_acorn.h` were wrong.** The Acorn PHROM stores each word's name next to its data. Those names correct words 135 (-TEEN), 137 (-TY), 138 (-Z) and 230 (O). The list's numbers and addresses all match the PHROM's pointer table, including word 272 (TRY), which the list marked as unverified.
  - **The DAC model fails at the rails.** TI-99 speech is loud on the TMS5220, and LINE uses the whole 10-bit `I/O` range. Codes beyond −128…+127 clip to the rail values. Codes −128 and +127 sit about 10 mV and 6 mV from the Phase 2 bit-weight model (datasheet §8.2) [p6-01 to p6-08].
  - **Larger upsets in TI-99 words.** Within a path, takes differed by up to 15 codes, from as early as sample 3 of speech, against 3 codes for the Acorn word in Phase 5. The two LINE takes through Speak External were bit-identical.
  - **The image formats are documented** (`proms/README.md`): Acorn's header, pointer table and stored names; TI's indexed and unindexed layouts; and the TI-99's binary search tree. In TI's own images, every word is followed by one spare byte after the byte holding its stop code. This is material for the authoring guide (Phase 11).
- **Handed on:**
  - **Phase 10** captures the seven PROMs other than the Acorn PHROM A. For `starwars_speech`, the manifest records how many bytes the game sends for each block (`stream_bytes`), including the bytes after the stop code. That matters to Speak External replays (Phase 8). The assembled images are original only word by word, so their address sweeps (§8) are not original data.
  - **Upsets → Phase 8.** The TI-99 words show larger and earlier upsets than the Acorn word. Loud words may be the quickest way to study them.
  - **The DAC at the rails → Phase 8** (the DAC transfer function).
  - **The 4 KB VM71003A** is served padded with zeros. How a real 4 KB VSM answers addresses above 4 KB is not known.
  - **Version 1.x:** the complete VM61003 and VM61005, the CM62312, and Speak External data from other hosts, if they can be found.

### Phase 7: Acorn PHROM A reference set

- **Goal:** the first complete reference data set.
- **Tasks:**
  - Capture every word, several times each, with the Phase 5 procedure.
  - Run the byte-address sweep (§8).
  - Generate the index page and the readable formats.
- **Delivers:** `captures/corpus/acorn_phrom_a/`, with raw data and readable formats (§1.3) and an index page.
- **Done when:** every word and swept address has been captured, all checks pass, and the repeats are consistent with Phase 5.
- **Done:**
  - **The reference set**, `captures/corpus/acorn_phrom_a/`, kept local (§6), about 40 GB. It holds 18,936 takes:
    - **Words:** all 165, with at least five takes through each path that passed every check. That is 1660 passing takes, plus 10 kept that missed their start slot.
    - **Sweep:** one take from each of the 16,144 sweep addresses, and five from every 64th: 17,266 takes. The sweep covers every byte of the image that is not a word start, except 75 addresses whose data runs off the end of the image without a stop code.
    - **Index:** `index.md`, `takes.csv` and `sweep.csv` are committed. `takes.csv` gives every take's checks and the SHA-256 of its raw files and of its output (`talk_sha256`, the codes from sample 0 to the end of talk). So the local copies can be checked, and an emulator can be checked against the outputs without the raw data.
  - **A faster runner** (`run.py`):
    - **Speed:** the scope is held open for the whole run, so a take costs its talk time plus about 1.5 s, against 8–11 s before. The whole set took about 12 hours.
    - **Long runs:** `--sweep`, `--sweep-step`, `--resume` (restart an interrupted run with the same command), and `--zip-only` (keep only the raw data's zip, about 2 MB a take, compressed in the background).
    - **Retakes:** a take that fails only a check of the capture procedure is kept and retaken. After a narrow start-slot miss, the start is recalibrated first.
    - **Banks:** only the PROM's own banks are kept loaded.
  - **Tool fixes:**
    - **`pico.hostCycles` lost READY edges.** The firmware ends a write as soon as it sees READY go low, and when the two shared one trace event, the edge was lost. That caused most apparent start-slot misses.
    - **`frameclock.commandLatch` mistook data for the command.** It could time the latch from a Speak External stream byte that looked like the command. It now looks only at the command write, and `start_slot` fails if READY is never seen going low.
    - **`process.py` (version 4):**
      - It records `talk_sha256`.
      - `duration` allows for a stop code as the first frame.
      - The DAC check judges the gain only over a span of 32 codes or more, so near-silent words no longer fail it, and handles takes with one code only.
    - **`compare.py`** leaves takes that failed a check out of the majority, and lists them.
  - **The previous-utterance test**, `captures/phase7/previous_utterance/` (p7-01 to p7-06). Five more takes from the set are copied into the repo as evidence (p7-07 to p7-11), with the scripts in `captures/phase7/scripts/`.
  - **Datasheet:** §4 (reads and Talk Status), §7 (the previous utterance, upset statistics) and §8.2 (the DAC on loud data).
- **Status:** done (2026-10-07). Every word and sweep address has been captured. Every word, through each path, has at least five takes that passed every check, and every sweep address has a passing take, with two exceptions. Sweep starts 0x3F40D and 0x3F416 fail the DAC check (`layer2_vs_layer3`) on every take, at 3.06–3.16 mV rms against a 3 mV limit, because their data is louder than the DAC model holds for (§8.2). That is a limit of the model, handed to Phase 8, not a fault in the capture. Of all 18,936 takes, 106 failed a check and are kept. 92 missed the start slot; two of those also failed the DAC check. 14 failed only the DAC check, most of them deliberate retakes of the loud sweep starts. The repeats of words are consistent with Phase 5, with upsets up to 7 codes. Those of the sweep are not: see below.
- **Findings:**
  - **The frame format holds for any data.** From all 16,144 sweep starts, the chip read exactly the bits of its frames up to the end of the stop code, and nothing more: 395,398 frames, 12.2 million bits. Silence and stop frames take 4 bits, repeat frames 11, unvoiced 29, voiced 50. Frames were read 25 ms apart in every one.
  - **When Talk Status clears.** It clears *frames* × 25 ms − 0.13 ms after the first frame read, 0.133–0.136 ms in every sweep start. When the first frame is a stop code, it clears 0.09 ms after the read, and the output never leaves idle (569 starts).
  - **The previous utterance changes the start of the next.** The reset sequence does not clear it. Takes of a loud sweep start spoken after the same predecessor matched from the start of speech; after different predecessors, they differed from sample 0–2 [p7-01 to p7-06]. Across the sweep's repeat sample, 46–51 % of takes that followed a different address departed within 20 samples, against 1–2 % of those that followed the same address. No word showed it. A candidate is that the chip interpolates from parameters left by the previous utterance.
  - **Upsets across a whole PROM.** Only 27 of the 330 word groups (word and path) were bit-identical. 25 % of takes matched their group's per-sample majority exactly and 93 % were within 2 codes, with 7 codes at most. Onsets recur at the same sample.
  - **Upsets grow without limit in loud garbage data.** Where a sweep start drives the filter far outside the range of speech, a one-code upset grows to the full 10-bit range. Only 8 of 251 sweep groups were identical, so the sweep is not a deterministic reference, contrary to §8's first assumption.
  - **Across days.** ACORN's majority output is within 3 codes of its Phase 5 majority.
  - **The start slot matters, again.** The 10 word takes accepted one slot (about 6 µs) off target differed from the others by 12–63 codes [p7-09, p7-10]. That is the noise generator's slot-counted restart (Phase 5).
  - **The DAC model fails for very loud data inside ±119 too.** It is 3–6 mV high for codes 64–119 in size [p7-11].
  - **Reads past the end of the image.** They continue into the next bank (address 0x40000), which the Pico serves as empty and the chip reads as zeros, that is, silence frames for ever. That is the emulation, not the chip; the sweep leaves those addresses out.
- **Handed on:**
  - **The previous utterance → Phase 8.** What is carried over (the interpolation's start values, the filter state or something else), and whether the reset sequence or a silent frame clears it. Until then, an emulator is compared with takes spoken after the same predecessor (datasheet §7).
  - **Upsets → Phase 8,** as before. The sweep's loud data amplifies them, which may help to find where they start.
  - **The DAC model → Phase 8** (§8.2), for very loud data.
  - **Phase 10** uses the same commands for the other PROMs. The runner's `--sweep` needs each PROM's image to end in a stop code or to be followed by a defined bank.
  - **Phase 12:** how to publish the local corpus, about 40 GB for this PROM.

### Phase 8: Synthetic vectors and the core datasheet

- **Goal:** the chip's internal behaviour, measured and written up.
- **Tasks:**
  - The Speak External sweeps (§9.1), the synthetic VSM tests (§9.2) and the timing tests (§9.3).
  - Write the datasheet sections in §1.1: the lookup tables, chirp, noise generator, interpolation, lattice filter arithmetic, DAC transfer function, frame format, and VSM and host interface timing.
- **Delivers:** the vectors (published as test material, §1.4), their captures, and the drafted datasheet sections.
- **Done when:** every section in the §1.1 table is drafted, with captured evidence cited for each claim.
- **Done:**
  - **The vectors** (`tools/vectors.py`): 480 vectors in eight sets (energy, pitch, K, noise, interpolation, special frames, random, loud). Each set is written as a synthetic PROM in `captures/phase8/vectors/`, so `run.py` captures it like any other through both paths. All were captured twice per path with the standard procedure: 1993 takes, about 2 GB, kept local in `captures/vectors/` with their index files committed.
  - **The model** (`tools/model.py`): the synthesis path as the datasheet describes it (§8.9 there), with options for every alternative tested. `vecfit.py` compares every take with it and measures the tables (`captures/phase8/analysis/`).
  - **The host interface and VSM tests** (`tools/protocol.py`), from the Pico's trace, with no scope: FIFO filling and emptying, the start of Speak External, commands written during speech, Reset bytes during Speak External, and the VSM commands (`captures/phase8/protocol/`, p8-19 to p8-25).
  - **Two bench experiments** (`captures/phase8/scripts/experiments.py`): the Speak timed 1–3 slots off the standard time, and a loud vector after different predecessors.
  - **Evidence:** p8-01 to p8-18 copied with their raw data into `captures/phase8/evidence/`.
  - **Datasheet:** §3, §4, §5, §6, §7 (noise generator, previous utterance, upsets), §8.1 and the new §8.5–§8.9; §2 and §11.2 completed.
  - **Tool changes:**
    - `lpc.encodeFrames` writes frames back to bits; it reproduces every Acorn word to its stop code.
    - `run.py` records where a set's manifest is, picks a calibration word with frames, and resets before locating the frame clock.
    - `process.py` finds the image of a synthetic set from its manifest.
- **Status:** done (2026-10-07), with the gaps below. Every section of §1.1 is drafted except the authoring guide, which belongs to Phase 11. The lattice filter's section states two things not yet known: its lowest-order arithmetic and its overflow. The model is not yet bit-exact for voiced frames: it is one code off at about one sample per pitch period. Matching every capture bit for bit is Phase 9's test.
- **Findings:**
  - **The tables are MAME's, apart from pitch.** All 14 energy entries and all 63 pitch entries were confirmed as unique fits, and 142 of 168 K entries; no entry was contradicted. The pitch period is the pitch value plus one sample, at every index: one more than MAME.
  - **The multiplier is the patent's.** Every product, the energy's included, goes through a Booth array whose stage 0 inverts without the +1 and whose result has its LSB forced to 1 (US 4,335,277). That cut the error on voiced vectors 4–5 times and explains why the idle code is −1.
  - **The noise register is fully determined.** 13 bits, feedback from bits 12, 3, 2 and 0, one step per slot, sign from bit 0. It matched 32,699 samples bit for bit. Its state at the first sample is a fixed number of steps after 0x1FFF, counted from the command: shifting the Speak by 1–3 slots shifts the state by 1–3 steps, through both paths.
  - **Timing inside a frame.** The frame read at sample 0 becomes the target 25 samples later. Each parameter reaches the filter at a fixed sample of its interpolation period: energy at 2, pitch at 5, K*j* at 2*j*+3. The voiced/unvoiced switch acts 2 samples after the latch.
  - **The previous utterance is explained.** The interpolator's registers are never cleared. A Speak only hides them from the filter until the first frame takes effect. An utterance that starts with a repeat frame, as the Phase 7 bench target does, uses the previous utterance's K values. Words start with full frames and so do not show it.
  - **Rules MAME lacks.** Interpolation from unvoiced to a stop frame is inhibited. A voiced repeat frame keeps K5–K10 zeroed. K5–K10 zeroing waits, like other inhibited changes, for IP 0. After speech ends, the filter keeps running while its parameters are zeroed one per PC.
  - **The FIFO.** Talk Status sets at the 9th byte; shorter streams never start. Buffer Low sets at 8 bytes or fewer. A frame that cannot be read in full sets Buffer Empty, and ends the speech like a stop frame, a frame later. That corrects the original, which says at once. Buffer Empty raises no INT, which also corrects the original.
  - **Commands during speech.** Load Address, Read Byte and Read and Branch are held until speech ends; Speak is ignored; Reset acts at once. A second command while one is held blocks READY.
  - **The reset sequence cannot end a Speak External.** Its bytes become FIFO data. It ends the speech within about two frames instead.
  - **Upsets occur in synthetic data too,** at recurring sample positions, and not according to the predecessor.
- **Handed on:**
  - **The model's remaining one-code differences → Phase 9.** In voiced frames, at a fixed point of each pitch period; and the start rule for a leading silence frame. Finding them is the first task of the emulator work. Overflow is now modelled as a 14-bit wrap (US 4,335,277). The voiced error is a different low-order limit cycle in the zero-excitation part of each pitch period, and many arithmetic forms are already ruled out (datasheet §8.8). A vector built to isolate it would help: voiced frames with one K non-zero at a time, with the longest pitch and a constant energy, so that each period ends in a long free ringing of one lattice stage.
  - **Upsets → still open.** Their cause needs the bench changes listed in Phase 5 (temperature, supply, decoupling, a second chip). They cannot be made from the host. The patent's dynamic logic suggests that temperature and `VDD` are the variables to try first (datasheet §7).
  - **The DAC model for very loud data, and the extra DAC noise during speech → Phase 12** final pass, or a later measurement phase. Neither affects `I/O`, the exact output.
  - **Slots and TI's bit times (Phase 1)** remain an inference from the pin name.
  - **Host interface tests that US 4,335,277 suggests** (bench, no hardware changes):
    - **A second command written while one is held.** p8-23 gave up after 50 ms. The patent holds the host only until the current instruction finishes, so READY may return once speech ends and the held command has run. Retest with a timeout longer than the word.
    - **A write to a full FIFO.** The patent accepts a byte only while the FIFO's top level is empty, so a 17th byte should hold READY until a frame has been taken from the FIFO. No stream so far wrote more than 16 bytes.
  - **Phase 9** starts from the datasheet text, as planned. `tools/model.py` is the measuring tool behind the text, not the emulator; Phase 9 must not copy it.

### Phase 9: Reference emulator

This phase starts as soon as the Phase 8 drafts exist, and runs alongside it.

- **Goal:** an emulator written **only from the datasheet text**.
- **Tasks:**
  - Implement it.
  - Run it against every capture so far, sample for sample, with the VSM trace timing checked too.
  - Fix any mismatch in the datasheet first, then in the emulator. MAME's `tms5220.cpp` may be used as a source of hypotheses, never as a reference.
- **Delivers:** the emulator, and a regression harness that reruns the whole data set.
- **Done when:** it matches all Phase 7 and Phase 8 captures bit for bit. This is acceptance test 1 (§1.5), for the data captured so far.
- **Done (2026-10-07):**
  - **The emulator** (`emulator/`, C99). It was written by a separate agent that read only the reference datasheet and, where that defers, the original manual; it did not see the tools, the captures or MAME. It is stepped one slot at a time and covers the host interface, the FIFO, the VSM interface and the synthesis path. It has a scenario language, a bulk mode and a self-test. `emulator/GAPS.md` lists every choice the text left open.
  - **The regression harness** (`tools/regress.py`). It rebuilds every standard take from its `capture.json`, with its predecessors, runs the emulator, and compares:
    - the `I/O` codes (layer 2);
    - the VSM timing (layer 1);
    - optionally, the model.

    Results are in `captures/phase9/regress/`.
  - **Datasheet fixes found by the emulator.** Writing the emulator exposed 36 gaps. All that affect the output are now closed in the text:
    - the parameter tables had been left out (a `{tables}` placeholder);
    - the Booth multiplier was underspecified;
    - the noise state was off by one sample;
    - energy 0 and the idle value were wrong;
    - what carries between utterances was wrong: it is the coded parameters, not the interpolator's registers;
    - the inhibit and zeroing rules were incomplete;
    - Speak External's timing was not stated;
    - §8.9 now gives an exact reference algorithm.

    The host-interface details that no capture has measured are listed as such (datasheet §3).
  - **A new vector set,** `ring` (48 vectors, 192 takes): the longest pitch, for 140-sample free ringing, including a strong resonance at two energies. Evidence p9-01 and p9-02.
- **Status:** closed (2026-10-08) against a measured tolerance, not bit for bit. The lowest-order arithmetic could not be found from the data (`docs-tech/lowest_order_arithmetic_ideas.md`), and part of it turned out to be marginal (below). So bit-exactness is not reachable now, and Phase 9 ends by measuring how far the emulator is from the chip, beside how far the chip is from itself and from a second chip (datasheet §8.8, "Expected agreement"):
  - **Layer 1 (VSM timing):** exact on every VSM take.
  - **Layer 2 (I/O codes), chip 1 as the reference:** the emulator's typical difference is one code. On speech-like vectors 95–99.8 % of samples are equal (median per set), and on the Acorn words 78.6 %, with a signal-to-difference ratio of 32.5 dB. Chip 1 agrees with its own takes of the Acorn words at 93.4 % and 37.5 dB, and chip 4 agrees with chip 1 on the pitch vectors at 98.8 % and 37.1 dB. So the emulator is about 5–6 dB further from a chip than a chip is from itself or from another chip of the same date code. That margin is the unknown arithmetic. On strongly resonant data it is no further than the chips are from each other.
  - **Acceptance test 1 is now stated as a tolerance (§1.5), observed and unproven.** It holds on the data captured so far: the VSM timing exactly, and layer 2 within the tolerance on the Acorn words and the energy, pitch, K, interpolation, noise and ring vectors.
  - **Outside the tolerance, and carried forward as known gaps:**
    - the loud and random vectors and the decays of `loc`, where a one-code difference grows without limit in an overloaded or ringing filter (the median gap is 12–22 dB);
    - the special frames (15 dB). The failing vectors are built around silence frames, where the first silence frame is approximate in the model (§7). Those vectors carry little signal, so a few codes weigh heavily in the SDR.

    No second chip has been captured on these sets, so how far two chips differ there is not known.
  - **The datasheet is complete for what the model knows.** The emulator gives the model's codes at every sample of every take: the 2,104 vector takes and the 1,660 corpus word takes and the 17,184 address-sweep takes. The VSM timing matched in every VSM take.
  - **It is not yet bit-exact against the chip,** because the model is not:
    - **voiced data** is one code off at a few samples per pitch period (95–97 % of samples equal on speech-like vectors, 79.6 % on the Acorn words);
    - **the chip does not repeat itself exactly** (upsets);
    - **data far outside the range of speech** turns one-code differences into large ones.
- **Findings:**
  - **The voiced error starts earlier than thought.** With a strong resonance it appears 6 samples into the first chirp (p9-01). In steady state the chip's output is then not periodic with the pitch period, deterministically, while the model's is. Large coefficients hide it, and positive K8–K10 bring it out.
  - **Newly ruled out** (datasheet §8.8):
    - a once-per-interpolation-period disturbance of each K;
    - a half-written K on arrival;
    - *b*₁ through a 10-bit Y latch;
    - toward-zero rounding in one chain;
    - per-stage inversion masks, rescored on the ring set;
    - free corrections to the energy product;
    - the product's lowest bit forced to 0 (the Mega99 core's reading of the patent), in any role;
    - a lowest-bit change on alternate samples;
    - the multiplier of US 4,700,323.
  - **A survey of outside sources** found nothing that settles the error:
    - no public netlist of the filter;
    - MAME says its own multiplier is not the chip's;
    - the other decoders and VHDL cores derive from MAME;
    - nobody has published a bit-exact comparison with a real chip.

    The one independent gate-level reading of US 4,335,277, the Mega99 Verilog core, has exactly the model's lattice, apart from the lowest bit, where the chip agrees with the model. So the missing detail is probably one the patents do not describe. The remaining route is a trace of the adder and the delay stack's low-order bits on the TMS5220NL die images (siliconpr0n, through the Wayback Machine).
  - **A second chip** (2026-10-08; ring, pitch and energy vectors, `captures/vectors_chip4/`, kept local). Chips 2 and 3 could not be used: chip 2 gave no `M0` pulses, and chip 3 loaded a wrong address and then stopped responding to writes, perhaps the wedge that a power cycle clears. Chip 4 was compared with chip 1, both against the model (datasheet §8.8):
    - **The voiced error is in the design.** On 121 of 126 pitch-vector pairs, both chips leave the model at the same sample with the same ±1.
    - **There is also a chip-specific ±1.** In about a third of the ring vectors, one chip departs where the other matches the model. It repeats on each chip, and it does not change with the clock (480, 560 and 720 kHz on chip 4, `captures/vectors_chip4/clk*/`). So it is not a timing margin. Chips 1 and 4 are both marked TMS5220NL DEP 8423, so it is probably not a die revision either. A marginal level, such as charge sharing on a precharged node, is the likely cause.
    - **Chip 4's upsets depend on the clock:** about 30 % of pairs of takes differ at 480–640 kHz, and 2 % at 720 kHz.
    - **Chip 4 has more upsets than chip 1,** and its DAC gain differs, so it fails `layer2_vs_layer3` against the Phase 2 table.
  - **The host tests replayed** (2026-10-08, `tools/replay.py`, `captures/phase9/replay/`). All 88 steps of p8-19 to p8-25 were run through the emulator from their recorded host cycles, with each step's frame position fitted.
    - **Datasheet corrections:**
      - a frame's parameters are read over 446 slots, not in one burst (§4);
      - Read Byte reads its bits from slot 13 of a sample period, at least 22 slots after a Dummy Read (§4);
      - reads during a held Read Byte return 0xFF (§3);
      - READY after Read and Branch returns at slot 15, 60–63 slots after the Branch (§3).
    - **The emulator was updated from the revised text alone,** by a separate agent. It now matches every VSM function and every bit read of every VSM step to within 3 µs, every READY latency to within a slot, and every status and data value, apart from two kinds of difference:
      - one read from an unloaded bank, where the Pico's VSM returns 1s and the emulator's returns 0s;
      - Speak External streams that run out of data, where BE and the final Talk Status change by up to 0.5 ms (64 of 15,262 reads).
- **Findings at close (2026-10-08):**
  - **The localisation set** `loc` (31 vectors: unvoiced frames with two large coefficients, energies around K1 = −488, and free decays; `tools/vectors.py`, `captures/vectors/vectors_loc/`).
  - **A marginal part of the error.** The unvoiced departures follow the coefficient's second Booth digit (*d*₁ < 0). Chip 1, refitted and captured again two days later, gave a different ±1 only in those vectors (datasheet §8.8). The first-departure search (`docs-tech/lowest_order_arithmetic_ideas.md` §2.3) can place each departure only to "*u*₁ off by ±2 in one of a few samples", and no rule in the product's operands fits.
  - **The variance measurement,** `tools/variance.py`, results in `captures/phase9/variance/`.
- **Open (carried forward; what bit-exactness would still need):**
  - **The lowest-order arithmetic.** No form tried, on data or patent grounds, reproduces it. Chip 4 shows that the voiced error is in the design, so it can be modelled. A die photograph of the multiplier and adder (as MAME's tables came from a decap) would settle it. Bit-exactness on strongly resonant vectors cannot be expected from any one model, because of the chip-specific departures. The acceptance test needs to say which chip is the reference (chip 1, which made every earlier capture, is the natural choice), or to compare only up to each vector's first chip-specific departure. The ring vectors are the sharpest test of any new idea: `python tools/regress.py captures/vectors/vectors_ring --model` together with a change to `model.py`.
  - **Upsets.** These need the bench changes of Phase 5 (temperature, `VDD`). Chip 4 has more of them, so they are likely marginal internal logic. On chip 4 they become rare at 720 kHz, which fits leakage from dynamic nodes. The chip-specific departures from the model did not change with the clock. Until their cause is known, bit-exactness on PROM words can be judged only against takes that repeat.
  - **A first frame of silence** is approximate in the model (§7), so in the emulator too.
  - **Speak External's flags within a frame.** When BL, BE and TS change at the end of a stream that runs out is not settled (datasheet §5). It needs a test with status reads much closer together than 1 ms.

### Phase 10: Remaining PROM corpora

- **Goal:** cover every PROM in the frozen Phase 6 inventory.
- **Tasks:**
  - Capture every word of the seven PROMs as in Phase 7: five takes through each path with the standard procedure. `starwars_speech` is captured through Speak External only, because the game has no VSM.
  - Sweep a sample of addresses in the four dumped images, one take each, VSM only:
    - every 8th sweep address of `vm61002`, `vm61004` and `vm71003a`;
    - every 32nd of `ti99_speech`.
  - Do not sweep the three assembled images (`vm61003_words`, `vm61005_words`, `starwars_speech`). Their bytes are original only word by word, so data between the words is not original (Phase 6).
  - Generate each PROM's index page, and run the emulator against each PROM.
- **Why a sample, not every address:**
  - A full sweep of the seven PROMs would be about 110,000 takes, about 54 hours on the bench.
  - The full Acorn sweep (Phase 7) has already shown what a sweep shows about the chip: the frame format holds for any data, and Talk Status clears at a fixed time.
  - Mid-word starts often drive the filter outside the range of speech. There one-code upsets grow without limit, takes don't repeat, and the tolerance of §1.5 does not hold. So such takes can't serve as acceptance data, and a full sweep would mostly add unrepeatable output.
  - The sample still checks the frame format and Talk Status on data from four more PROMs.
  - `ti99_speech` is sampled most sparsely: its target chip is the TMS5200, not the TMS5220.
- **Commands,** per PROM:

  ```
  python tools/run.py <prom> all --takes 5 --path both --resume --zip-only
  python tools/run.py <prom> --sweep --sweep-step 8 --resume --zip-only      # vm61002, vm61004, vm71003a
  python tools/run.py ti99_speech --sweep --sweep-step 32 --resume --zip-only
  ```

  For `starwars_speech`, use `--path external`.
- **Budget:** about 7.2 hours of capture, without retakes or restarts. It is estimated from each take's frames, plus 0.94 s a take of fixed cost measured in Phase 7. That estimate gives 0.75 h for the Acorn words, which took 0.78 h.

  | PROM | Words | Word takes (h) | Sweep takes | Sweep (h) |
  |---|---|---|---|---|
  | `ti99_speech` | 339 | 1.6 | 1,002 (every 32nd) | 0.6 |
  | `vm61002` | 206 | 1.0 | 1,983 (every 8th) | 0.9 |
  | `vm61004` | 148 | 0.7 | 1,944 (every 8th) | 0.9 |
  | `vm71003a` | 35 | 0.2 | 489 (every 8th) | 0.2 |
  | `vm61003_words` | 116 | 0.6 | none | 0 |
  | `vm61005_words` | 116 | 0.6 | none | 0 |
  | `starwars_speech` | 23 | 0.1 | none | 0 |
  | **Total** | **983** | **4.8** | **5,418** | **2.6** |

- **Delivers:** a reference data set for each PROM.
- **Done when:**
  - every word of every inventoried PROM has been captured;
  - the sampled sweep addresses have been captured;
  - the emulator matches all of them within the tolerance of §1.5.

  Acceptance test 1 then holds for the whole data set, on speech-range data, as in Phase 9.
- **Version 1.x:** the full sweeps, if a faster runner or a second bench makes them cheap.
- **Done:**
  - **The reference sets**, `captures/corpus/<prom>/` for the seven PROMs, kept local (§6), about 30 GB. They hold 15,178 takes, all from chip 1:

    | PROM | Words | Word takes | Sweep addresses | Bench time |
    |---|---|---|---|---|
    | `ti99_speech` | 339 | 3,407 | 1,002 (every 32nd) | 2.9 h |
    | `vm61002` | 206 | 2,070 | 1,983 (every 8th) | 2.0 h |
    | `vm61004` | 148 | 1,479 | 1,944 (every 8th) | 1.7 h |
    | `vm71003a` | 35 | 351 | 489 (every 8th) | 0.5 h |
    | `vm61003_words` | 116 | 1,167 | none | 0.7 h |
    | `vm61005_words` | 116 | 1,164 | none | 0.7 h |
    | `starwars_speech` | 23 | 116 | none | 0.2 h |

    - **Words:** every word, with at least five takes through each path that passed every check, except as below. Every retake was for a start-slot miss (48 word takes).
    - **Sweep:** one take from each sampled address. All but nine have a take that passed every check. Those nine fail only `layer2_vs_layer3`, on quiet output (code spans of ±20 or less), which the DAC model's check does not judge well.
    - **Index:** `index.md`, `takes.csv` and `sweep.csv` per PROM are committed, as in Phase 7.
    - **Bench time:** 8.7 hours in one unattended run, against the 7.2 budgeted. The fixed cost per take was 1.5–2.0 s, more than in Phase 7, mostly on the longer TI-99 and VM61002 words.
  - **Words too short for Speak External.** Speak External does not start speech until the FIFO holds 9 bytes (datasheet §6). Two words are shorter: `vm71003a` 35 "(0.25)" (5 bytes) and `vm61004` 35 "(Pause)" (8 bytes), both silence followed by a stop code. Through Speak External, "(0.25)" never spoke, and its five takes are kept with `talk_status` and `duration` failing. The bytes such a word leaves in the FIFO also turn the next reset sequence into data, so its Reset is never executed and the next take does not start from the standard state. `run.py` now speaks words shorter than 9 bytes through the VSM only, so "(Pause)" was captured that way.
  - **The tools cover every PROM.** `regress.py corpus` and `variance.py` now take every PROM in `captures/corpus/`.
  - **The follow-up tests (2026-10-09),** run after the first regression left `starwars_speech` outside the tolerance:
    - **Where the emulator's error comes from.** Over every PROM word, the takes 2–5 of each word and path (which follow the same word) were compared with each other and with the model. About 87 % of the model's departures from the chip start at samples where all four takes agree. So most of the emulator's error is deterministic: the lowest-order arithmetic (datasheet §8.8). The rest starts where the chip's takes disagree (upsets), and a small share, large in a few words, came from the onset after silence (below).
    - **The onset vectors** (`tools/vectors.py` set `onset`, 52 vectors, 2 takes per path, chips 1 and 4; `captures/vectors/vectors_onset/`, `captures/vectors_chip4/vectors_onset/`): voiced, then 1 to 6 silence frames, then voiced, at seven pitches before and four after; and the same after unvoiced speech, into a repeat frame, at a loud resonant setting, and with no silence.
    - **The repeat test** (49 takes each of six words, chip 1; `captures/phase10/repeat/`): `acorn_phrom_a` 213 L, `vm61002` 115 NOR, `vm61004` 71 ERROR and `ti99_speech` 345 WELL through the VSM, and `starwars_speech` 12 and 13 through Speak External.
    - **Chip 4 on PROM words** (`captures/corpus_chip4/`, index files committed): every `starwars_speech` word through Speak External, and every `vm61003_words` word through both paths, five takes each. Chip 4 fails `layer2_vs_layer3` on every take, as in Phase 9 (its DAC gain differs); no other check failed apart from start-slot retakes.
    - **Corpus mining** (`tools/departures.py`, `captures/phase10/departures/`). The model's internal state at every clean deterministic departure (15,851), every clean upset start (387) and a 3 % sample of agreeing samples (56,999 controls), over every PROM word; and the same over the repeat test.
    - **Bench time:** about 1.4 hours (chip 1 0.6 h, chip 4 0.9 h).
  - **The emulator against the chip** (`captures/phase10/regress/`, `captures/phase10/variance/`), after the correction of datasheet §8.6 below:
    - **It equals `tools/model.py`** on every take of every PROM, words and sweeps, the Acorn PHROM A included: no sample different.
    - **Layer 1 (VSM path):** the bits read and the time Talk Status cleared match on every take (within 4.4 µs). The frame-read samples match on every take but 18 (below).
    - **Layer 2, against the tolerance of §1.5,** with chip 4 as the reference where it was captured, as §1.5 prescribes:

      | PROM | Reference | Reference (dB) | Emulator (dB) | Gap (dB) | |
      |---|---|---|---|---|---|
      | `ti99_speech` | chip 1 takes | 39.6 | 34.8 | 4.9 | within |
      | `vm61002` | chip 1 takes | 34.8 | 31.3 | 3.6 | within |
      | `vm61004` | chip 1 takes | 35.3 | 31.3 | 4.1 | within |
      | `vm71003a` | chip 1 takes | 38.7 | 32.8 | 5.9 | within |
      | `vm61003_words` | chip 4 | 32.0 | 31.7 | 0.3 | within |
      | `vm61005_words` | chip 1 takes | 36.1 | 31.6 | 4.5 | within |
      | `starwars_speech` | chip 4 | 35.6 | 32.5 | 3.1 | within |

      *Median SDR over the speech window. The Acorn PHROM A gives 37.5, 32.6 and 4.9. Against chip 1's own takes, `starwars_speech` was outside (40.7 dB capped at 40, against 32.5: a gap of 7.5 dB).*
- **Status:** done (2026-10-09). Every word of every inventoried PROM is captured, the sampled sweep addresses are captured, and the emulator matches all of them within the tolerance of §1.5: the VSM timing exactly, and layer 2 on every PROM. The tolerance rule was not changed. `starwars_speech` came within it through the reference that §1.5 already names, a second chip.
- **Findings:**
  - **After silence that followed voiced speech, the chirp starts one sample later than the datasheet said.** The pitch counter's hold at an inhibited change lasts three samples of IP 0 (PC 0 A and B, and PC 1 A), not two. The onset vectors show it on both chips, through both paths, every take: after 1 to 6 silence frames, at every pitch before and after, into a voiced or a repeat frame. After unvoiced speech the hold's length makes no difference, because the counter is at 0 until the new pitch arrives. With the correction the model matches the onset of every onset vector on both chips. Across the data set its error power fell by up to 13 times (`vm61003_words`; 6.6 times `vm61004`, 2.2 times `starwars_speech`), and no PROM or vector set got worse; 28 of about 40,000 takes lost a few matching samples in chaotic stretches. The median SDRs barely moved, because the error was concentrated in a few words. Datasheet §8.6 and §8.9 were corrected first, then `tools/model.py` (option `pitch_hold`), then the emulator, by a separate agent from the revised text alone (`emulator/tms5220.c`, step 2.3). In the PROM words the corrected model now matches every such onset in the Acorn, `vm61003_words`, `vm61004` and Star Wars words (166, whatever the number of silence frames). So the first report's "on time after three silence frames" did not hold; the lag measure used then (best shift over 40 samples) is coarse. In TI-99 the measure gives mixed lags on its loud onsets, and its words that start with a silence frame are 3 samples off: the known approximation of a first silence frame (datasheet §7).
  - **What is different about the Star Wars data.** Not the chip's behaviour: its upsets start at the same rate as on every other PROM (about 5 a second) and carry the same energy each. Two things in the data make chip 1 repeat it unusually well:
    - **It is loud,** an RMS of 30.7 codes against 19–20 for the Acorn and VM PROMs. So the same upsets weigh less in the SDR (about 3.7 dB).
    - **It has many short pitch periods.** Of its 23 words, 9 have a mean pitch index below 25, against 13 of the 2,201 other word groups. There upsets die away sooner (a median of 77 samples at indices 1–15, against about 500 at 32–47), so chip 1's takes agree better (43.6 dB below index 25). The emulator's deterministic error does not shrink with them. Star Wars' words with a pitch index of 32 or more were within the old reference (a gap of 4.9 dB), and the short-pitch words of `vm61002` and `vm61004` show the same large gap.

    The emulator's deterministic departures start twice as often on Star Wars as on most PROMs, but a model of them fitted without Star Wars predicts its rate (474 against 564 predicted; 0.44–1.08 of the prediction elsewhere). So that too is its data: more unvoiced samples, and more coefficients of the kind below.
  - **What an upset is.** Each starts as a single ±1 at one of a few fixed samples per word, and at each such sample a take in step leaves with a fixed chance, 2 % to about 50 % (the repeat test). It goes towards the nearer output step. There, the model's 15-bit value is one or two internal steps from the boundary, the same place the deterministic departures start. Upsets and deterministic departures are both enriched for coefficients whose second Booth digit is negative (*d*₁ < 0, Phase 9), independently for each K. K6–K10 matter most, and K7 more for upsets than for deterministic departures. In the repeat test the emulator's deterministic departures never varied: at 99.6 % of them no take out of 49 gave the emulator's value. So the lowest-order arithmetic is deterministic at most samples and marginal at a few, which are the upsets. Datasheet §7 and §8.8 now say this.
  - **Two chips agree on PROM words no better than the emulator agrees with either.** On `vm61003_words` chip 4 is 32.0 dB from chip 1 and the emulator 31.7 dB; on Star Wars 35.6 and 32.5 dB. A chip agrees with itself much better, because its upsets are few and its departures from the model repeat.
  - **TI-99 is loud, as in Phase 6.** Only 61 % of samples are equal, against 73–79 % for the other PROMs, with differences of up to 16 codes at the 90th percentile. Its take-to-take agreement is high, though, and the emulator is within the tolerance.
  - **Long takes: one frame read off by a sample.** In 18 TI-99 sweep takes of 5.5 s or more, all in the index area at 0x0000–0x0D20, one frame read falls one sample later than the 200-sample grid gives, always between samples 44,100 and 45,500. The 25 ms frame period check passes on all of them. The Pico's timer, which times the reads, drifts against the chip's sample clock, and by then a read late in its sample moves into the next. This is a limit of the measurement, not of the chip or the emulator.
- **Handed on:**
  - **The lowest-order arithmetic → `docs-tech/lowest_order_arithmetic_ideas.md`.** The corpus departures and upset starts above are a far larger set of labelled cases than Phase 9's vectors, and they point at the same coefficients (*d*₁ < 0). The repeat test gives each marginal sample a probability, which a candidate rule should place nearest the boundary. `tools/departures.py` writes them with the model's state at each (`events.csv`, kept local), per word (`words.csv`) and summarised (`summary.json`).
  - **Upsets and the bench (plan Phase 5):** the repeat test at a second `VDD` or temperature would show whether each marginal sample's chance moves, as a marginal node predicts.
  - **Chip 1 refitted:** `tools/bench.json` names chip 4 while it is fitted. Change it back when chip 1 returns.

### Phase 11: Authoring, and confirmation with new speech data

- **Goal:** prove that the documented mechanism is complete, by generating new speech data from the documentation and running it through the real TMS5220.
- **Tasks:**
  - Write the authoring guide and the encoder, using only the datasheet.
  - Check the round trip: decode every original PROM word to frames, re-encode it, and confirm the bits are identical (acceptance test 3).
  - Generate the project's own speech data: new phrases from new recordings, and hand-designed frame sequences that use every feature. Package it as a new PHROM image with a word table, and as Speak External streams.
  - Commit the emulator's predictions before running anything on the chip.
  - Play everything on the TMS5220, capture it, compare it with the predictions sample for sample, and listen to it (§9.4).
  - Optionally, check the new PHROM image on a BBC Micro.
- **Delivers:** the authoring guide, the encoder, and the new speech data with its predictions and captures, published as a worked example.
- **Done when:** all the new speech data plays on the chip as predicted, within the tolerance of §1.5, and the round trip is bit-identical. These are acceptance tests 2 and 3. Speech-range data only: the tolerance holds on no set that overloads the filter.

### Phase 12: Publish

- **Goal:** release version 1.0.
- **Tasks:** do a final pass through the datasheet, including the change log against the original. Decide how the local PROM corpus captures are published (§6), and publish them. Tag the datasheet, data set, emulator, tools and test vectors together.
- **Done when:** all three acceptance tests in §1.5 pass on the frozen data set, and the release is published.
