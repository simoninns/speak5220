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

1. **An emulator written from the datasheet alone** reproduces every capture in the data set bit for bit (layer 2), and matches the VSM trace timing (layer 1). Where the chip carries state between utterances (§2, §11), the emulator is given the documented starting state.
2. **New speech data generated by this project** plays on the real TMS5220 exactly as documented. These are new phrases that are in no original PROM, built with the project's own encoder from the datasheet alone. They are delivered both as a new VSM/PHROM image and as Speak External streams. The emulator's output is predicted and recorded *before* the data is played on the chip. The chip's captured output must then match that prediction sample for sample (§9.4).
3. **Re-encoding original data gives back the original bits.** Decoding each original PROM word to frames with the documented frame format, then re-encoding it, reproduces the original bit stream.

Anything that fails any of these tests is a gap in the datasheet.

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
- **DAC level against code:** about −3.9 mV per code across the speech band. There is an extra jump of about 15 mV (about 4 codes) between codes −1 and 0, which needs further study.
- **The audio stage loads TP2.** It is not a pure 1 K load: C1 and R2 4K7 make the TP2 voltage depend on recent samples. A first-order model with a pole at 294 Hz (the C1 corner is 279 Hz) cuts the residual from 8.6 mV to 1.3 mV rms. For DAC characterisation, remove C1 so TP2 sees only the 1 K. Alternatively, model the network in post-processing.
- **Repeats are not bit-identical.** Repeats of "B" match exactly for the first ~1170 samples, then differ by ±1–2. Words with unvoiced sounds ("C", "ACORN") differ by up to ±26 from the first unvoiced frame onwards. This is consistent with a free-running noise generator that Speak does not reset. The delay from Speak to the first sample also varies: 6 and 25 samples were seen, and 25 samples is one interpolation period. Both are emulator-relevant findings, covered in §11.

**Clock-edge alignment (2026-10-06, Phase 1 complete).** Every `T11` and `I/O` transition follows a ROMCLK **rising** edge. `I/O` rises 215 ns after it and `T11` rises 490 ns after it, each with a spread of 8 ns or less. ROMCLK measures 160.0005 kHz at 50.6 % duty. The DAC steps at the start of slot 1. `I/O` carries the 10-bit code before the DAC clips it (codes up to +134 were seen). The reference datasheet (`tms5220_reference_datasheet/`, §2, §8.2, §8.4, §9.5, §11.2 and Appendix A) gives the full results, with the captures cited.

## 3. Capture strategy: three layers per utterance

Each capture records three things, so that any difference from an emulator can be traced to its cause:

1. **Input trace (digital, exact).** The Pico's VSM emulation logs every Load-Address, Read and Read-and-Branch, and the bits it returned. This is the exact bit stream the chip parsed. The Pico can already do this (`TMS6100_TRACE`). It needs to be turned into a compact log that can be uploaded after each capture.
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

Block mode is a simpler option, and it is what has been tested so far. The 3405D has 64 MS of memory, shared between the enabled channels. With all four channels at 2 MS/s, that is enough for about 8 s, so a single word fits easily. 2 MS/s gives 12 scope samples per `I/O` bit slot, and decoding at that rate is reliable.

**Driver access works without packaging.** PicoScope 7, installed system-wide, ships `libps3000a.so`. `tools/ps3000a.py` loads it through `ctypes`, and the dev shell provides its libusb and libstdc++ dependencies. `tools/capture.py` and `tools/decode.py` are the first parts of the Phase 4 tooling (see `tools/README.md`).

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

Still to do:

3. **Optional, later: bring `T11` and `I/O` into the Pico.** This is not needed, because the PicoScope captures both (§4). It would only matter for long unattended runs without the scope. They swing from −5 V to +5 V, so they need a clamped level shift to 0–3.3 V (a plain divider is not enough, because the Pico pins must never go negative). Options are a series resistor with Schottky clamps to GND and 3.3 V, or a small NPN stage per signal. GPIO27 and GPIO28 are still free.
4. **Pull-down on GPIO22.** U5D's input relies on the RP2350's default pad pull-down while the Pico boots, so Q1 stays off and OSC sits at −5 V until the firmware starts the clock. Consider an explicit 10K to GND.

## 6. Data set layout and file formats

### Per PROM

```
captures/<prom>/
  prom.json           # inventory: name, hash, provenance, target chip, word table
  index.md            # browsable page: word list, frame counts, links to audio
  <word>/             # one capture directory per word (or per swept address)
```

### Per capture

```
captures/<prom>/<word>/
  capture.json        # metadata (see below)

  # Original capture format: written once at capture time, never modified
  raw/scope_raw.npz   # raw PicoScope ADC streams (Ch A-D) + range/offset/timebase
                      #   (committed as scope_raw.npz.zip, see "Where it is stored")
  raw/vsm_trace.bin   # layer 1: every VSM command and returned bit, with timestamps

  # Readable formats: regenerated from raw/ by the post-processing scripts
  vsm_trace.csv       # layer 1 as text
  frames.csv          # the frames the chip parsed: bit offset, energy, pitch, K1-K10, repeat flag
  samples.wav         # layer 2: I/O decoded from the scope capture, mono PCM, rate = fosc/80
  samples.csv         # layer 2 as text (sample index, value, T11 timestamp)
  dac.wav             # layer 3: recovered per-sample DAC codes as WAV
  listen.wav          # convenience: filtered and resampled to 48 kHz for listening
```

The `raw/` files are the record of what was measured. The `scope_raw.npz` layout (array names, dtypes, scaling) is documented alongside the scripts, so it can be read without the capture tooling. Every readable file can be rebuilt from `raw/` with one command, and `capture.json` records the script version that built it.

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

### Where it is stored

The raw scope data is large. Four channels at 2 MS/s is about 8 MB per second of speech uncompressed, so a full PROM with repeats runs to gigabytes. Zip compression brings that down by about 10× (the Phase 1 captures shrank from 305 MB to 26 MB), because the digital channels compress well. There are two kinds of capture:

- **Evidence captures** live in `captures/phase<N>/<id>/`, are cited by the datasheet, and are **committed to the repo**. The raw file is committed as `raw/scope_raw.npz.zip`: a zip that holds the original `scope_raw.npz` unchanged, so its SHA-256 still matches the evidence index. The tools read the zip directly, and `.gitignore` excludes the unzipped `.npz`. Keep each phase's evidence to a few tens of MB.
- **PROM corpus captures** (Phases 7 and 10) live in `captures/corpus/<prom>/` and are **kept local for now**. The whole directory is git-ignored. How to publish them (release archives, Git LFS or elsewhere) is decided in Phase 12. Until then, the repo holds the scripts, manifests, `prom.json`, `index.md` and the hashes of the local raw files, so the local copies can be checked. Back up the local corpus separately, because it is the only copy.

## 7. Automation architecture

```
Host (Python)
 ├── Pico 2 over USB CDC: a simple command protocol
 │     UPLOAD <image>   load a PHROM/VSM image into SRAM (16 KB per PHROM)
 │     SPEAK <addr>     speak from the VSM, with the trigger GPIO marking start/end
 │     SPEAKEXT <bytes> speak external (see §9)
 │     CMD <byte>       raw command write/status read, for protocol tests
 │     TRACE            dump the VSM trace (layer 1)
 └── PicoScope 3405D over ps3000a: arm streaming, wait for the trigger, save
```

A run loops over a manifest of the PHROM images and their word tables. For each word it uploads the image (once per PHROM), arms the scope, sends `SPEAK`, waits for Talk Status to clear, pulls the traces, writes the capture directory, and runs sanity checks: the layer 2 and layer 3 data agree, and the bit count matches the parsed frame count. Speak each word several times (say 5) to confirm the chip is deterministic. Any difference between repeats is itself a finding, for example an uninitialised state or the noise generator's state carried over between utterances.

The current firmware is a fixed demo loop. It will need to become a command-driven tool, with the VSM emulation on core 1 unchanged.

## 8. PHROM and speech data corpus

The aim is to cover **every original speech PROM that can be found**. This needs an inventory step first. Collect the images, record their hashes and provenance, and check the word tables. Candidates to look for:

- **Acorn PHROM A** (Kenneth Kendall voice). Already in `romdata_acorn.h`, from TMS6100-Emulator.
- **Acorn PHROM US** (American voice), if a dump exists.
- **Other TMS6100 / VSM dumps** that are archived in MAME (for example for TI products), checked for whether they target the TMS5220 or the TMS5200/5100. Older chips use different coefficient tables and frame formats. Those dumps will still play on a 5220, but they will sound wrong. That is still a useful test, as long as it is labelled clearly.
- **Speak External data from other TMS5220 hosts.** Arcade boards (for example several Atari games) and Apple II and similar speech cards stored LPC data in CPU ROM rather than in a VSM. We can replay it through Speak External (§9).

### Known sources

PROM images:

- **TMS6100 Multi-PHROM Emulator for the BBC Micro** (eRadiofan): <https://github.com/eRadiofan/TMS6100-Multi-PHROM-Emulator-for-the-BBC-Micro/tree/main/Firmware/Common/Src>. Several PHROM images built into its firmware sources.
- **TMS6100-Emulator** (simoninns): <https://github.com/simoninns/TMS6100-Emulator/tree/master/Firmware/tms6100>. The source of the Acorn PHROM A data in `romdata_acorn.h`.

Information:

- **TI Portable Speech Lab** (Stuart Conner): <https://www.stuartconner.me.uk/ti_portable_speech_lab/ti_portable_speech_lab.htm>. TI's speech development system, which is background for the authoring guide.

When the images are imported, record which source each came from, along with its hash, in `prom.json`. Where the same PROM appears in more than one source, compare the copies.

For each image, the word table gives the entry points. Also do a **sweep of every byte address**, not just the word starts. Starting mid-word produces garbage, but it is deterministic garbage, which makes it excellent emulator test material.

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
- **Run-to-run differences.** The noise generator and the internal sample and interpolation counters are not reset by Speak, so repeated words are not bit-identical (§2). The data set must record this rather than hide it. Options:
  - Reset the chip (Reset command, or a power cycle) before each word, and test whether that makes repeats identical.
  - Infer the noise LFSR state from unvoiced frames.
  - Treat the starting state as an input that the emulator must reproduce.
- **Internal state that is not visible on `I/O`.** If `I/O` carries only the final sample, the inner filter state has to be inferred. Carefully designed vectors (for example, one K parameter active at a time) keep that inference tractable.
- **`TEST` and `PROM OUT`.** These may expose internal ROM contents or test modes. They are undocumented and might put the chip into an unexpected state. Treat any exploration of them as a separate, later, deliberate experiment, preferably on a spare chip.
- **Chip variation.** Capture on more than one TMS5220 (and a 5220C, if one is available) to separate design behaviour from part-to-part analogue variation. The digital outputs should match exactly. The analogue outputs will not. Document any 5220/5220C differences in the datasheet.
- **Clock fidelity.** The board no longer has the original RC oscillator, so captures are made with a crystal-derived clock. That is ideal for reproducibility, but real BBC Micro hardware ran slightly off 640 kHz and drifted. To study that behaviour, have the Pico sweep the clock frequency rather than refitting an RC network.
- **Licensing of PHROM images.** Keep the original ROM images out of the repo unless their redistribution status is clear. Commit the manifests and hashes, plus the captured output if appropriate.
- **Copyright of the original datasheet.** See §1.1: the new datasheet is written fresh and cites the original.

## 12. Phases

Each phase lists its goal, tasks, deliverables, completion criteria and status (as of 2026-10-06). Phases 1–5 build the capture system. Phases 6–12 build the data set and the datasheet. Datasheet sections are drafted in whichever phase produces their evidence (§10), not saved up for the end.

| Phase | Depends on | Status |
|---|---|---|
| 1. Bench bring-up and signal identification | — | Done |
| 2. Measurement hardware | 1 | Not started |
| 3. Firmware instrumentation | 1 | Started |
| 4. Host tooling | 1, 3 | Started |
| 5. Repeatability | 3, 4 | Not started |
| 6. PROM inventory | — | Sources listed (§8) |
| 7. Acorn PHROM A reference set | 2, 4, 5, 6 | Not started |
| 8. Synthetic vectors and the core datasheet | 4, 5 | Not started |
| 9. Reference emulator | 8 (runs alongside it) | Not started |
| 10. Remaining PROM corpora | 6, 7 | Not started |
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

### Phase 3: Firmware instrumentation

- **Goal:** the host can drive every input path without supervision.
- **Done:** the `SCOPE_TRIG` output.
- **Tasks:**
  - The USB command protocol of §7: `UPLOAD`, `SPEAK`, `SPEAKEXT`, `CMD` and `TRACE`, plus `RESET` and power-up sequencing for Phase 5.
  - A compact, timestamped VSM trace.
  - VSM image upload into SRAM.
  - Retiring the demo loop.
- **Delivers:** command-driven firmware, with its protocol documented in `firmware/`.
- **Done when:** a host script can upload an image, speak any address or any Speak External stream, and fetch the VSM trace, all repeatedly, with no manual steps.

### Phase 4: Host tooling

- **Goal:** one command turns a request ("speak word N of PROM X") into a complete capture directory.
- **Done:** `tools/capture.py` and `tools/decode.py` (triggered block capture, `I/O` decode, DAC averaging, a WAV and CSV per word).
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

### Phase 5: Repeatability

- **Goal:** a capture procedure whose results can be reproduced, and a documented account of any state the chip carries between utterances.
- **Tasks:**
  - Repeat the same word under different conditions: plain Speak, a Reset command first, a full power cycle, and different idle times.
  - Characterise the noise generator's state and the start delay. So far a 6- or 25-sample start variation has been seen, and unvoiced output differs between repeats (§2).
- **Delivers:**
  - The standard procedure used by Phase 7 onwards.
  - A datasheet section on state at power-up and at the start of speech.
  - A description of the starting state that the emulator needs.
- **Done when:** repeats under the standard procedure are either bit-identical, or differ only in the way the documented state model predicts.

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

### Phase 7: Acorn PHROM A reference set

- **Goal:** the first complete reference data set.
- **Tasks:**
  - Capture every word, several times each, with the Phase 5 procedure.
  - Run the byte-address sweep (§8).
  - Generate the index page and the readable formats.
- **Delivers:** `captures/acorn_phrom_a/`, with raw data and readable formats (§1.3) and an index page.
- **Done when:** every word and swept address has been captured, all checks pass, and the repeats are consistent with Phase 5.

### Phase 8: Synthetic vectors and the core datasheet

- **Goal:** the chip's internal behaviour, measured and written up.
- **Tasks:**
  - The Speak External sweeps (§9.1), the synthetic VSM tests (§9.2) and the timing tests (§9.3).
  - Write the datasheet sections in §1.1: the lookup tables, chirp, noise generator, interpolation, lattice filter arithmetic, DAC transfer function, frame format, and VSM and host interface timing.
- **Delivers:** the vectors (published as test material, §1.4), their captures, and the drafted datasheet sections.
- **Done when:** every section in the §1.1 table is drafted, with captured evidence cited for each claim.

### Phase 9: Reference emulator

This phase starts as soon as the Phase 8 drafts exist, and runs alongside it.

- **Goal:** an emulator written **only from the datasheet text**.
- **Tasks:**
  - Implement it.
  - Run it against every capture so far, sample for sample, with the VSM trace timing checked too.
  - Fix any mismatch in the datasheet first, then in the emulator. MAME's `tms5220.cpp` may be used as a source of hypotheses, never as a reference.
- **Delivers:** the emulator, and a regression harness that reruns the whole data set.
- **Done when:** it matches all Phase 7 and Phase 8 captures bit for bit. This is acceptance test 1 (§1.5), for the data captured so far.

### Phase 10: Remaining PROM corpora

- **Goal:** cover every PROM in the frozen Phase 6 inventory.
- **Tasks:** capture each one, as in Phase 7, and run the emulator against each.
- **Delivers:** a reference data set for each PROM.
- **Done when:** every inventoried PROM has been captured and the emulator matches all of them. Acceptance test 1 then holds for the whole data set.

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
- **Done when:** all the new speech data plays on the chip exactly as predicted, and the round trip is bit-identical. These are acceptance tests 2 and 3.

### Phase 12: Publish

- **Goal:** release version 1.0.
- **Tasks:** do a final pass through the datasheet, including the change log against the original. Decide how the local PROM corpus captures are published (§6), and publish them. Tag the datasheet, data set, emulator, tools and test vectors together.
- **Done when:** all three acceptance tests in §1.5 pass on the frozen data set, and the release is published.
