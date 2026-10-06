# TMS5220 Reference Capture Plan

This is a high-level plan for building a reference corpus of TMS5220 behaviour. Each capture records the exact input the chip was given and the exact output it produced. The corpus has two uses:

1. To document every known speech PHROM, word by word, as audio that anyone can listen to.
2. To act as a bit-exact test suite for a future TMS5220 emulator.

The second use sets the bar. "Sounds right" is not enough. For each capture we want to know which bits the chip consumed, when it consumed them, and which sample values it produced.

## 1. What can be observed on the chip

| Signal | Pin | What it gives us | Currently wired? |
|---|---|---|---|
| `SPEAKER` | 8 | Analogue DAC output (current sink, 0–1.5 mA, about 5.9 µA per step, new value every 125 µs) | Yes, into the R2 1K load (top sheet) and the filter chain. Test point TP2 |
| `ROMCLK` | 3 | `OSC` ÷ 4. It sets the exact sample rate (`fosc` / 80) | Yes, to the VSM interface |
| `ADD1/2/4/8`, `M0`, `M1` | — | Every VSM command and every data bit the chip reads | Yes, the Pico sees all of them (GPIO8–13) |
| `T11` | 7 | Sync output, once per sample period | Test point TP3 only |
| `I/O` | 9 | Serial data out | Test point TP4 only |
| `PROM OUT`, `TEST` | 10, 20 | "Testing use only" | No. Leave them alone (see §9) |
| `READY`, `INT`, status register | — | Command handshake, end-of-speech and FIFO events | Yes |

`T11` and `I/O` matter most. The datasheet calls them only "sync" and "serial data out". Community reverse engineering of the TMS52xx family (the notes in MAME's `tms5220.cpp`) suggests that `I/O` carries the **digital sample value serially, framed by `T11`**. If that holds for our chip, we can capture the exact output word of the lattice filter with no analogue error at all. **We must confirm this on the bench before we rely on it.** It is the first experiment in the plan (Phase 1).

## 2. Capture strategy: three layers per utterance

Each capture records three things, so that any difference from an emulator can be traced to its cause:

1. **Input trace (digital, exact).** The Pico's VSM emulation logs every Load-Address, Read and Read-and-Branch, and the bits it returned. This is the exact bit stream the chip parsed. The Pico can already do this (`TMS6100_TRACE`). It needs to be turned into a compact log that can be uploaded after each capture.
2. **Output samples (digital, exact).** If `I/O`/`T11` work as expected, capture them on the Pico with PIO and record one integer per 125 µs sample. This is the gold standard for emulator comparison.
3. **Output waveform (analogue, measured).** Use the PicoScope to measure the voltage across R2 (TP2), which is the raw DAC output **before** the filter chain. This checks the DAC transfer function and the hold behaviour, and it is the fallback if layer 2 does not work out.

**All reference captures are taken directly at the TMS5220, before the audio stage.** Layer 3 measures pin 8 across R2 at TP2, DC-coupled and upstream of the audio stage's AC coupling capacitor, so the only thing between the DAC and the probe is the 1 K load. If R2's tolerance or the probe loading turns out to matter, replace R2 with a 0.1% resistor of the same value. Alternatively, feed pin 8 into a dedicated transimpedance stage used only for measurement.

The filtered `Audio_out` signal is useful for listening tests, but it is not reference data. The 280 Hz high-pass and the 4th-order low-pass (see `audio_output_stage.md`) change the waveform. Any filtered recording can be reproduced from layer 2 or layer 3 in software.

## 3. Using the PicoScope 3405D

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

### Why the PicoScope is not the main capture path

The Pico 2 is already on the bus. It can capture `T11`/`I/O` with PIO, sample-exactly and for free, alongside the VSM trace and with a shared timebase. The PicoScope's role is **characterisation and validation**: it confirms what the Pico captures, it measures the analogue DAC, and it measures oscillator frequency and jitter. For bulk corpus runs it is optional.

## 4. Hardware changes

Done in the schematic:

1. **Probe test points** TP2–TP9, as in §3.
2. **Pico-driven oscillator.** The original BBC-style RC oscillator has been removed. The TMS5220 is now always clocked by the Pico. Pico GPIO22 (`OSC_PICO_3V3`) drives the previously spare 74AHCT125 gate U5D, whose ~OE is tied to GND. U5D's 0/5 V output drives a PNP level shifter (Q1 BC557: R1 10K to the base, R25 3.3K from base to emitter, emitter to +5 V, R26 1K from collector to −5 V). The collector drives `OSC` (U1 pin 6) between −5 V and +5 V. The clock is inverted, which doesn't matter.

   The datasheet (Figure A-1) shows only the RC option. `OSC` is not an ordinary logic input: it is the threshold node of the internal RC oscillator, whose timing network returns to −5 V. **A 0–5 V square wave does not clock the chip.** This was tested on the board: there was no ROMCLK with 640 kHz at 0–5 V, and none with 320 kHz at 0–5 V and PROM OUT strapped towards −5 V. The working reference is the Atari Star Wars sound PCB (SP-225, sheet 16A). There, an original TMS5220 is clocked at 672 kHz from a 74LS161 through a 2N3906 PNP (collector 1K to −5 V) and two CD4069 inverters running from ±5 V, so OSC swings rail to rail. Our circuit is that transistor stage without the CMOS buffer. The TI-99/4A note that PROM OUT at −5 V enables a 320 kHz external clock did not hold for this chip. Pin 10 drives strongly, so it was left unconnected. A PWM or PIO clock at exactly 640 kHz, derived from the Pico's crystal, makes the sample rate exactly 8000 Hz. It also makes every run sample-aligned and repeatable, and lets the clock be swept to test timing edge cases. Record the measured clock (from `ROMCLK` on TP1, which should read 160 kHz) in every capture.

   **The firmware generates this clock** (`tms5220ClockStart()` in `tms5220.c`). Without it the TMS5220 has no clock at all. 640 kHz does not divide evenly into the RP2350's default 150 MHz system clock, so the firmware sets the system clock to 144 MHz: the 12 MHz crystal × 120 gives a 1440 MHz VCO, ÷ 5 ÷ 2. That is within the chip's rating, and a PWM slice divides it by 225 to give exactly 640 kHz. To sweep the clock, pick a system clock that is a multiple of the wanted frequency, or accept a non-integer PWM period.

Still to do:

3. **Bring `T11` and `I/O` into the Pico** for PIO capture (layer 2). They are 5 V outputs, so they need level-shifting, for example through two inputs of a spare 3.3 V buffer. GPIO27 and GPIO28 are still free. Bodge wires from TP3 and TP4 with resistor dividers are enough for first experiments. Wait until Phase 1 has confirmed what `I/O` carries before committing this to the board.
4. **Pull-down on GPIO22.** U5D's input relies on the RP2350's default pad pull-down while the Pico boots, so Q1 stays off and OSC sits at −5 V until the firmware starts the clock. Consider an explicit 10K to GND.

## 5. File formats

Each capture is a directory with one JSON sidecar describing it:

```
captures/<source>/<item>/
  capture.json        # metadata (see below)
  vsm_trace.bin/.csv  # layer 1: every VSM command and returned bit, with timestamps
  samples.wav         # layer 2: digital output, mono PCM, rate = measured fosc/80
  samples.csv         # layer 2: same data as text (sample index, value, T11 timestamp)
  dac_raw.npz         # layer 3: raw PicoScope streams + range/offset/timebase
  dac.wav             # layer 3: recovered per-sample DAC codes as WAV
  listen.wav          # convenience: filtered/resampled to 48 kHz for listening
```

### WAV conventions

- **`samples.wav`:** 16-bit signed PCM, one WAV sample per chip sample, no resampling. The header records the nominal rate (8000 Hz). The *measured* rate goes in the JSON. The chip's native value goes in the low bits, and the bit width and scaling are documented in the JSON. A WAV like this is bit-exact, plays in any tool, and diffs trivially against an emulator's output.
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

## 6. Automation architecture

```
Host (Python)
 ├── Pico 2 over USB CDC: a simple command protocol
 │     UPLOAD <image>   load a PHROM/VSM image into SRAM (16 KB per PHROM)
 │     SPEAK <addr>     speak from the VSM, with the trigger GPIO marking start/end
 │     SPEAKEXT <bytes> speak external (see §8)
 │     CMD <byte>       raw command write/status read, for protocol tests
 │     TRACE            dump the VSM trace and the T11/I/O sample capture
 └── PicoScope 3405D over ps3000a: arm streaming, wait for the trigger, save
```

A run loops over a manifest of the PHROM images and their word tables. For each word it uploads the image (once per PHROM), arms the scope, sends `SPEAK`, waits for Talk Status to clear, pulls the traces, writes the capture directory, and runs sanity checks: the layer 2 and layer 3 data agree, and the bit count matches the parsed frame count. Speak each word several times (say 5) to confirm the chip is deterministic. Any difference between repeats is itself a finding, for example an uninitialised state or the noise generator's state carried over between utterances.

The current firmware is a fixed demo loop. It will need to become a command-driven tool, with the VSM emulation on core 1 unchanged.

## 7. PHROM and speech data corpus

This needs an inventory step first. Collect the images, record their hashes and provenance, and check the word tables. Candidates to look for:

- **Acorn PHROM A** (Kenneth Kendall voice). Already in `romdata_acorn.h`, from TMS6100-Emulator.
- **Acorn PHROM US** (American voice), if a dump exists.
- **Other TMS6100 / VSM dumps** that are archived in MAME (for example for TI products), checked for whether they target the TMS5220 or the TMS5200/5100. Older chips use different coefficient tables and frame formats. Those dumps will still play on a 5220, but they will sound wrong. That is still a useful test, as long as it is labelled clearly.
- **Speak External data from other TMS5220 hosts.** Arcade boards (for example several Atari games) and Apple II and similar speech cards stored LPC data in CPU ROM rather than in a VSM. We can replay it through Speak External (§8).

For each image, the word table gives the entry points. Also do a **sweep of every byte address**, not just the word starts. Starting mid-word produces garbage, but it is deterministic garbage, which makes it excellent emulator test material.

## 8. Beyond PHROMs: controlled stimulus

PHROMs only exercise the parameter combinations that someone happened to encode. An emulator needs every path covered. There are two mechanisms, and we already have both:

### 8.1 Speak External (command `0x6x`)

The Pico writes LPC frames straight into the 16-byte FIFO. This gives complete control over the bit stream with no VSM involved. Use it for **synthetic test vectors** generated by a host script:

- **Single-parameter sweeps.** Hold every parameter at a neutral value and step one index through its whole range: every energy index (0–15), every pitch index (0–63), and every K1–K10 index. These tests read out the chip's internal parameter lookup tables one entry at a time.
- **Voiced and unvoiced.** The same parameters with pitch = 0 (noise) against pitch ≠ 0 (the chirp excitation). Long unvoiced runs characterise the noise generator (its LFSR sequence and period).
- **Interpolation.** Pairs of frames with large parameter jumps, to capture the per-interpolation-period stepping, and the documented rule that interpolation is inhibited across voiced/unvoiced and silence transitions.
- **Special frames.** Repeat frames, energy = 0 (silence), and the stop code (energy = 15), including the "interpolate to zero" tail.
- **FIFO and protocol behaviour.** Start-up with exactly 8, then 9, bytes (TS should go high at 9). Starve the FIFO deliberately mid-utterance (BE, abnormal termination, `INT`). Check the BL threshold timing. Confirm that Reset is ignored during Speak External.
- **Pathological input.** Random bit streams, for robustness and determinism.

### 8.2 Synthetic VSM images

The Pico serves arbitrary VSM data. The same vectors can be packed into a fake PHROM to test the VSM path: Read-and-Branch, Load-Address nibble sequencing, Read Byte (`0x1x`), and address wrap-around at bank boundaries. Comparing the Speak External and VSM versions of the same vector confirms that the two input paths are equivalent.

### 8.3 Timing and handshake tests

The VSM trace and the trigger GPIO also give timestamps for: READY assertion times, the delay from Speak to first fetch, how the fetches line up with frame boundaries, how many bits per frame the chip reads ahead, and the reset/power-up sequence. This is the information an emulator needs to be *cycle*-accurate on the host interface, not just sample-accurate on audio.

## 9. Risks and open questions

- **What does `I/O` carry?** If it is not the sample value (or is only active in a test mode), layer 2 falls back to layer 3 (PicoScope plus averaging). That path is slower and noisier, but still workable.
- **`TEST` and `PROM OUT`.** These may expose internal ROM contents or test modes. They are undocumented and might put the chip into an unexpected state. Treat any exploration of them as a separate, later, deliberate experiment, preferably on a spare chip.
- **Chip variation.** Capture on more than one TMS5220 (and a 5220C, if one is available) to separate design behaviour from part-to-part analogue variation. The digital outputs should match exactly. The analogue outputs will not.
- **Clock fidelity.** The board no longer has the original RC oscillator, so captures are made with a crystal-derived clock. That is ideal for reproducibility, but real BBC Micro hardware ran slightly off 640 kHz and drifted. To study that behaviour, have the Pico sweep the clock frequency rather than refitting an RC network.
- **Licensing of PHROM images.** Keep the original ROM images out of the repo unless their redistribution status is clear. Commit the manifests and hashes, plus the captured output if appropriate.

## 10. Phases

1. **Probe.** Use the PicoScope manually on TP2–TP5 to identify the format of `I/O` and its relationship to `T11` and the DAC steps. Measure `fosc`. Write up the findings in `docs-tech/`.
2. **Instrument the firmware.** Build the command protocol, the PIO capture of `T11`/`I/O`, the compact VSM trace dump, the trigger GPIO and image upload.
3. **Host tooling.** Write the Python runner, the PicoScope streaming capture, the capture directory writer, the step-averaging DAC recovery and the cross-check between layers 2 and 3.
4. **Acorn PHROM A.** Capture the full corpus with repeats to prove determinism. Publish it as the first reference set.
5. **Synthetic vectors.** Run the Speak External sweeps (§8.1), then derive and document the lookup tables, the noise LFSR, the chirp table, the interpolation rules and the DAC transfer function.
6. **More corpora.** Capture the other PHROMs and Speak External datasets as they are collected.
7. **Emulator harness.** Feed an emulator the same input trace and diff its output against `samples.wav`, sample for sample. MAME's `tms5220.cpp` is a useful first comparison target and a source of hypotheses.
