# Capturing PROM words

`run.py` is the capture runner ([Automation](../capture/automation.md)). One command takes a PROM, words and a number of takes, and writes complete capture directories ([Data layout](../capture/data-layout.md)):

```
python investigation/tools/run.py acorn_phrom_a ACORN                    # one take, VSM Speak
python investigation/tools/run.py acorn_phrom_a 160 179 --takes 3 --path both
python investigation/tools/run.py acorn_phrom_a all --takes 5            # the whole PROM
python investigation/tools/run.py acorn_phrom_a --address 0x3CBB7        # a sweep point outside the word table
python investigation/tools/prom.py list acorn_phrom_a                    # the word table

# A reference set: every word, then the address sweep
python investigation/tools/run.py acorn_phrom_a all --takes 5 --path both --sweep --resume --zip-only
python investigation/tools/run.py acorn_phrom_a --sweep --sweep-step 64 --sweep-takes 5 --resume --zip-only
```

Words are given by number or by name. `--path` is `vsm` (Speak from the emulated VSM, the default), `external` (Speak External of the same bytes) or `both`. Words shorter than 9 bytes are spoken through the VSM only: Speak External does not start speech until the FIFO holds 9 bytes ([datasheet §6](../datasheet/commands.md)). The output goes under `investigation/captures/corpus/` (kept local) unless `--out` says otherwise. `--zip` also archives each `raw/scope_raw.npz` as `.npz.zip`, for evidence captures. `--zip-only` keeps only the zip, compressed in the background, which makes corpus takes about 7× smaller. `--board` is as in the [board configuration table](index.md#probes); `--load-ohms` defaults to 996.

## Long runs

- **`--sweep`** adds the address sweep ([PROM corpus](../capture/prom-corpus.md)), one take through the VSM from each sweep address (`--sweep-takes`). The sweep covers every byte address of the image that is not a word start, except addresses whose data reaches the end of the image without a stop code. Those run into the next bank, which the Pico serves as empty, so what the chip reads there depends on the board, not on the PROM. `--sweep START:END` limits the sweep to a range. `--sweep-step N` takes every Nth address, to give a sample of the sweep extra takes.
- **`--resume`** counts the takes already made that passed every check, and makes only the missing ones, so an interrupted run can be restarted with the same command.
- **Retakes.** A take that fails only checks of the capture procedure (`start_slot`, `start_frame`, `trace_complete`, `scope_overflow`, `io_format`, `fosc`, `external_stream`, `vsm_image`) is kept and retaken, up to `--retries` times (2). If `start_slot` was missed by less than a frame, the path's start may have drifted, so it is recalibrated first. Failures that come from the speech data, such as a sweep point without a stop code, are not retaken.
- **Speed.** The scope is opened once per run, so a take costs its talk time plus about 1.5 s. The runner prints the time left after each entry. If a take raises an error, the runner keeps its directory, which has no `capture.json`, so the tools skip it. Five errors in a row stop the run.

## The standard procedure

The standard procedure is the default ([datasheet §7](../datasheet/power-up.md)). The TMS5220 carries two things from one utterance to the next:
- **A frame clock.** It runs all the time, and a Speak waits for its next boundary.
- **A noise generator.** It runs freely until a Reset holds it, and the next Speak restarts it.

So that every take starts from the same state, the runner works in two stages.

At the start of a run, it:
1. Uploads the PROM image to its bank, if the Pico does not hold it already, and empties every other bank. Reads that run past the end of the image then find an empty bank in every run.
2. Locates the frame clock on the Pico's timer from one Speak's trace (`frameclock.py`).
3. For each path, calibrates the start, so that the chip accepts the Speak command 12256.1 µs before a frame boundary, in the same 6.25 µs slot every time. This takes about 40 s per path.

For each take, it:
1. Sends the reset sequence.
2. Arms the scope for the word's length, worked out from its frames.
3. Clears the trace and has the Pico speak, timed to start half a frame before a frame boundary.
4. Saves `raw/scope_raw.npz`, `raw/vsm_trace.bin` and `capture.json`.
5. Runs `process.py` on the take ([Readable files and checks](checks.md)).

`--start free` speaks as soon as the scope is armed, without timing the Speak to the frame clock; `--reset` then adds the reset sequence before each take, and `--idle` sets the wait between takes.

The timed start itself is described in [Firmware protocol](../hardware/firmware-protocol.md#timed-starts). To run at another clock frequency, see [Other clocks](pico.md#other-clocks).

## The PROM index

After each word the runner runs `compare.py` ([Comparing repeats](checks.md#comparing-repeats)). At the end, and every 500 entries, it rewrites the PROM's index with `promindex.py`:
- **`index.md`:** the word table and a summary of the sweep.
- **`takes.csv`:** one row per take, with its checks, the SHA-256 of its output and of its raw files. The output hash, `talk_sha256`, covers the `I/O` codes from sample 0 to the end of talk, as 16-bit little-endian integers.
- **`sweep.csv`:** one row per sweep address, with its parse, its checks, its output hash and the number of distinct outputs among its takes.

The corpus is kept local ([Data layout](../capture/data-layout.md)). These index files and `prom.json` are the only parts of it in the repo. The runner exits non-zero if any check failed.

## Capture layout

```
<out>/<prom>/
  prom.json, index.md, takes.csv, sweep.csv
  sweep/addr_<address>/     one per address sweep point, laid out like a word
  <number>_<name>/          one per word
    compare.json
    vsm01/, vsm02/, ...     one per take: VSM Speak
    ext01/, ...             Speak External
      capture.json          metadata, the raw files' SHA-256 (raw_sha256),
                            plus "processed": summary and checks
      raw/scope_raw.npz     raw scope data (never modified; with --zip-only, only
                            raw/scope_raw.npz.zip, which holds it unchanged)
      raw/vsm_trace.bin     raw VSM trace (never modified)
      vsm_trace.csv, frames.csv, samples.wav, samples.csv, dac.wav, listen.wav
```

## PROM manifests

PROMs are described by manifests, `investigation/proms/<name>.json`: the image's source and hashes, its bank, provenance, target chip and word table. The inventory is in [PROM corpus](../capture/prom-corpus.md). `promimport.py` writes both, and rebuilds the images from their pinned public sources into `investigation/proms/images/`, which is git-ignored. Run it once before capturing any PROM except the Acorn PHROM A, whose image is in `investigation/firmware/src/romdata_acorn.h`:

```bash
python investigation/tools/promimport.py              # fetch, rebuild and check every image; write the manifests
python investigation/tools/prom.py list ti99_speech   # a PROM's word table
```

An image may span several banks (TI-99's is 32 KB, in banks 0 and 1); `run.py` uploads all of them. `bench.json` records the chip's marking, the measured supplies and the temperature; every `capture.json` copies it, so update it whenever the chip or the board changes.
