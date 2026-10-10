# Emulator regression

These tools run the reference emulator against the captured data, measure how far apart outputs of the same utterance are, and search for the corrections behind the chip's departures from the model.

```
nix develop -c make -C investigation/emulator test               # build the emulator and run its self-test
python investigation/tools/vectors.py build ring                 # the ring vector set (filter arithmetic)
python investigation/tools/regress.py vectors corpus             # every vector set and the Acorn PHROM A words
python investigation/tools/regress.py corpus --sweep             # ... and its address sweep
python investigation/tools/regress.py vectors --model            # also compare the emulator with model.py
python investigation/tools/vectors.py build loc                  # localisation and decay vectors (lowest-order error)
python investigation/tools/vectors.py build onset                # the first pitch pulse after silence
python investigation/tools/variance.py                           # chip against itself, chip against chip, model against chip
python investigation/tools/departures.py corpus repeat            # where the chip departs from the model and from itself
python investigation/tools/fitsearch.py vectors corpus           # the single correction behind every departure from the model
python investigation/tools/tablefit.py --fit vectors --hold corpus # fit product-table entries, scoring held-out runs
python investigation/tools/replay.py                             # the host interface tests (p8-19 to p8-25) through the emulator
python investigation/tools/replay.py --detail vsm_commands read_byte_repeat   # one step, chip and emulator side by side
```

`investigation/emulator/` is the reference emulator: C99, written from the reference datasheet alone ([Emulator](../emulator/index.md) and [Emulator choices](../emulator/choices.md)). It is stepped one slot at a time and covers the host interface, the VSM interface and the synthesis path.

## replay.py

`replay.py` replays the host-interface and VSM command tests (`protocol.py`, p8-19 to p8-25; see [Synthetic vectors and the model](vectors-and-model.md#protocolpy)) through the emulator. Each step's recorded host cycles become a scenario, written at the slots of their recorded strobes. The step's position against the frame clock is not recorded, so it is fitted: one value per step, searched over all 4000 slots of a frame. It compares:
- the value of every status and data read;
- READY latencies;
- INT edges;
- the times of the Load Address, Dummy Read and Read and Branch functions;
- every VSM bit read.

A step is compared only up to the end of its recording, or up to a host cycle that the chip never released. Results are in `investigation/captures/phase9/replay/`. `--detail TEST STEP` prints one step's recorded and emulated timelines side by side.

## regress.py

`regress.py` runs the emulator on every standard take that passed its checks, and compares it with the chip.
- **Rebuilding a take.** Each take is rebuilt from its `capture.json`: the image and address, the path, and the time the command was latched before the frame boundary. The takes captured before it are spoken first, back to the latest that has a voiced frame, because the coded parameters carry over ([datasheet §7](../datasheet/power-up.md)).
- **Layer 2:** the `I/O` codes, from 5 samples before the first frame read to the end of the capture.
- **Layer 1, VSM path:** the number of bits read, the sample period of each frame read, and when Talk Status cleared.
- **`--model`:** also runs `model.py` on the same take, and counts the samples where the emulator and the model differ. Zero means the datasheet text reproduces the model exactly.
- **Output:** one CSV per set and `summary.json` in `investigation/captures/phase9/regress/`. `TMS5220EMU` selects another emulator binary.

## variance.py

`variance.py` measures how far apart two outputs of the same utterance are, the same way for five kinds of pair:
- chip 1's own takes, in the same session;
- chip 1 two days apart (`investigation/captures/vectors_chip1_repeat/`);
- chip 4 against chip 1: the vector sets in `investigation/captures/vectors_chip4/`, and the PROM words in `investigation/captures/corpus_chip4/` (takes 2-5, which follow the same word on both chips);
- the model against each chip, rebuilt as `regress.py` does.

For each pair it gives the share of samples equal, the RMS and largest difference, and the signal-to-difference ratio, over the speech window. Corpus takes are paired only when both follow the same word, because the filter's state carries over. Output: `pairs.csv` and `summary.json` in `investigation/captures/phase9/variance/` ([datasheet §8.8](../datasheet/lattice-filter.md), "Expected agreement").

## departures.py

`departures.py` finds where the chip departs from the model, and from itself, in groups of takes of one word and path that all follow a take of the same word. Its sources are `corpus` (every PROM's words) and `repeat` (`investigation/captures/phase10/repeat/`, 49 takes of six words). It labels deterministic departures (every take agrees, the model differs), upset starts (the model and the takes agree, then some takes leave) and a sample of controls, each with the model's state there: u1 and its position in the output step, the excitation, and each K with its Booth digits and the b value it multiplies. Output in `investigation/captures/phase10/departures/<source>/`: `events.csv` (kept local), `words.csv` and `summary.json`, with a logistic fit of each kind against the controls ([Lowest-order arithmetic](../die-analysis/lowest-order-arithmetic.md)).

## fitsearch.py

`fitsearch.py` runs the first-departure search (sections 3.1 and 2.4 of [Lowest-order arithmetic](../die-analysis/lowest-order-arithmetic.md)) on every vector take (`vectors`: chips 1 and 4 and chip 1's repeat) or every PROM word (`corpus`: the per-sample majority of takes 2–5). For each departure from the model it finds the single product correction (±1, ±2, in the departure's sample or the 3 before it) that keeps the model matched longest, applies it, and moves on to the next. It records each take's filter inputs once with `model.py` and then replays only the filter, from a table of the multiplier's products, with all candidates of a departure in one numpy batch. `--check` only confirms that the replay equals `model.py`. Output in `investigation/local/fitsearch/<source>/`: `departures.jsonl` (the correction chosen and every one tied with it) and `runs/*.npz` (inputs, states, the chip's codes on the model's index, and the corrections).

## tablefit.py

`tablefit.py` fits single entries of the product table to the runs `fitsearch.py` saved, from the corrections tied at each run's first departure. It scores each entry by replaying the runs that use it, and scores a held-out source each round without fitting it. Output: `investigation/local/fitsearch/tablefit_<fit>.json`.
