# Acorn PHROM A reference set

The Acorn PHROM A is the first complete reference data set: every word captured several times through both paths with the standard procedure ([repeatability](repeatability.md)), a sweep of every byte address ([PROM corpus](../prom-corpus.md#words-and-the-address-sweep)), and the index page and readable formats for all of it.

## The set

`investigation/captures/corpus/acorn_phrom_a/`, kept local ([data layout](../data-layout.md#where-it-is-stored)), about 40 GB. It holds 18,936 takes:

- **Words:** all 165, with at least five takes through each path that passed every check. That is 1660 passing takes, plus 10 kept that missed their start slot.
- **Sweep:** one take from each of the 16,144 sweep addresses, and five from every 64th: 17,266 takes, of which 17,184 passed the checks the regression uses ([reference emulator validation](reference-emulator.md)). The sweep covers every byte of the image that is not a word start, except 75 addresses whose data runs off the end of the image without a stop code.
- **Index:** `index.md`, `takes.csv` and `sweep.csv` are committed. `takes.csv` gives every take's checks and the SHA-256 of its raw files and of its output (`talk_sha256`, the codes from sample 0 to the end of talk). So the local copies can be checked, and an emulator can be checked against the outputs without the raw data.

Every word and sweep address has been captured. Every word, through each path, has at least five takes that passed every check, and every sweep address has a passing take, with two exceptions. Sweep starts 0x3F40D and 0x3F416 fail the DAC check (`layer2_vs_layer3`) on every take, at 3.06–3.16 mV rms against a 3 mV limit, because their data is louder than the DAC model holds for ([datasheet §8.2](../../datasheet/dac-and-audio.md)). That is a limit of the model, not a fault in the capture. Of all 18,936 takes, 106 failed a check and are kept. 92 missed the start slot; two of those also failed the DAC check. 14 failed only the DAC check, most of them deliberate retakes of the loud sweep starts. The repeats of words are consistent with the [repeatability](repeatability.md) results, with upsets up to 7 codes. Those of the sweep are not: see below.

## The runner for long runs

- **Speed:** the scope is held open for the whole run, so a take costs its talk time plus about 1.5 s, against 8–11 s when the scope is reopened for each take. The whole set took about 12 hours.
- **Long runs:** `--sweep`, `--sweep-step`, `--resume` (restart an interrupted run with the same command), and `--zip-only` (keep only the raw data's zip, about 2 MB a take, compressed in the background).
- **Retakes:** a take that fails only a check of the capture procedure is kept and retaken. After a narrow start-slot miss, the start is recalibrated first.
- **Banks:** only the PROM's own banks are kept loaded.

Tool details that matter to the data:

- **`pico.hostCycles` must not lose READY edges.** The firmware ends a write as soon as it sees READY go low, and the two can share one trace event. Losing that edge causes apparent start-slot misses.
- **`frameclock.commandLatch` looks only at the command write.** A Speak External stream byte can look like the command, so timing the latch from it would be wrong. `start_slot` fails if READY is never seen going low.
- **`process.py` version 4** records `talk_sha256`; its `duration` check allows for a stop code as the first frame; and the DAC check judges the gain only over a span of 32 codes or more, so near-silent words do not fail it, and handles takes with one code only.
- **`compare.py`** leaves takes that failed a check out of the majority, and lists them.

## Evidence

- **The previous-utterance test,** `investigation/captures/phase7/previous_utterance/` (p7-01 to p7-06).
- Five more takes from the set are copied into the repo as evidence (p7-07 to p7-11), with the scripts in `investigation/captures/phase7/scripts/`.

The results are written up in the datasheet's [§4](../../datasheet/vsm-interface.md) (reads and Talk Status), [§7](../../datasheet/power-up.md) (the previous utterance, upset statistics) and [§8.2](../../datasheet/dac-and-audio.md) (the DAC on loud data).

## Findings

- **The frame format holds for any data.** From all 16,144 sweep starts, the chip read exactly the bits of its frames up to the end of the stop code, and nothing more: 395,398 frames, 12.2 million bits. Silence and stop frames take 4 bits, repeat frames 11, unvoiced 29, voiced 50. Frames were read 25 ms apart in every one.
- **When Talk Status clears.** It clears *frames* × 25 ms − 0.13 ms after the first frame read, 0.133–0.136 ms in every sweep start. When the first frame is a stop code, it clears 0.09 ms after the read, and the output never leaves idle (569 starts).
- **The previous utterance changes the start of the next.** The reset sequence does not clear it. Takes of a loud sweep start spoken after the same predecessor matched from the start of speech; after different predecessors, they differed from sample 0–2 [p7-01 to p7-06]. Across the sweep's repeat sample, 46–51 % of takes that followed a different address departed within 20 samples, against 1–2 % of those that followed the same address. No word showed it. The cause is the chip's stored parameters, which a Speak does not clear ([synthetic vectors](synthetic-vectors.md#findings), [reference emulator validation](reference-emulator.md)). An emulator is compared with takes spoken after the same predecessor ([datasheet §7](../../datasheet/power-up.md)).
- **Upsets across a whole PROM.** Only 27 of the 330 word groups (word and path) were bit-identical. 25 % of takes matched their group's per-sample majority exactly and 93 % were within 2 codes, with 7 codes at most. Onsets recur at the same sample.
- **Upsets grow without limit in loud garbage data.** Where a sweep start drives the filter far outside the range of speech, a one-code upset grows to the full 10-bit range. Only 8 of 251 sweep groups were identical, so the sweep is not a deterministic reference. The sweep's loud data amplifies upsets, which may help to find where they start.
- **Across days.** ACORN's majority output is within 3 codes of its majority in the [repeatability](repeatability.md) captures.
- **The start slot matters.** The 10 word takes accepted one slot (about 6 µs) off target differed from the others by 12–63 codes [p7-09, p7-10]. That is the noise generator's slot-counted restart ([repeatability](repeatability.md#findings)).
- **The DAC model fails for very loud data inside ±119 too.** It is 3–6 mV high for codes 64–119 in size [p7-11].
- **Reads past the end of the image.** They continue into the next bank (address 0x40000), which the Pico serves as empty and the chip reads as zeros, that is, silence frames for ever. That is the emulation, not the chip; the sweep leaves those addresses out. For the other PROMs, the runner's `--sweep` needs each image to end in a stop code or to be followed by a defined bank.
