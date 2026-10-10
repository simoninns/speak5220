# Synthetic vectors and the model

These tools build synthetic frame sequences, capture them like a PROM, compare the chip with the candidate model of the synthesis path, and test the host interface and VSM commands with raw command sequences.

```
python investigation/tools/vectors.py build all                  # write the vector sets to investigation/captures/phase8/vectors/
python investigation/tools/run.py investigation/captures/phase8/vectors/energy.json all --takes 2 --path both \
    --out investigation/captures/vectors --zip-only --resume     # capture a set like a PROM
python investigation/tools/model.py investigation/captures/vectors/vectors_energy/*/vsm01   # compare takes with the model
python investigation/tools/vecfit.py investigation/captures/vectors            # every take vs the model; table measurements
python investigation/tools/protocol.py all                       # host interface and VSM command tests (no scope)
python investigation/tools/protocol.py fifo_start --analyse      # rebuild a test's result.json from its traces
```

## vectors.py

`vectors.py` builds sets of frame sequences: energy, pitch, K, noise, interpolation, special frames, random, loud, the ring and loc sets for the filter arithmetic and the lowest-order error, and the onset set for the first pitch pulse after silence. Each set is a synthetic PROM: a manifest (`<set>.json`) whose word table lists the vectors, the image (`<set>.bin`), and every vector's frames (`<set>.csv`). `run.py` accepts a manifest path in place of a PROM name ([Capturing PROM words](capture.md)). Speak External streams are the vectors' bytes up to their stop codes. `lpc.encodeFrames` does the encoding: each field most significant bit first, the bytes read least significant bit first.

## model.py

`model.py` is the candidate model of the synthesis path that the datasheet's [§8.5](../datasheet/frame-timing.md)–[§8.9](../datasheet/reference-algorithm.md) describe. Its defaults are the measured behaviour; its options select the alternatives that were tested.
- **`compareTake`** models a standard-procedure take and compares it with the chip's `I/O` codes. The noise state and alignment for each path come from `STANDARD_START`. Its `before` argument speaks earlier utterances first: the coded parameters carry over ([datasheet §7](../datasheet/power-up.md)).
- **It is not the reference emulator.** That is written from the datasheet alone ([Emulator](../emulator/index.md)).

## vecfit.py

`vecfit.py` writes `investigation/captures/phase8/analysis/`:
- `vectors.csv`: every take compared with the model;
- `tables.csv`: each table entry perturbed by 1, 2 and 4 either way, confirmed when its value fits best;
- `pitch_periods.csv`: each pitch period, measured from the output alone;
- `summary.json`.

## protocol.py

`protocol.py` drives the chip with raw commands and bytes (`CMD`, `READ`, `WAIT`; see [Firmware protocol](../hardware/firmware-protocol.md)) and keeps each step's raw trace. The trace times every host cycle with READY and `INT`, and every VSM function, on the chip's crystal. Results go to `investigation/captures/phase8/protocol/<test>/`.
- **Between steps** it ends any Speak External first. The reset sequence cannot: during Speak External its bytes are FIFO data.
- **The tests:** FIFO start and starvation, the start of Speak External against the frame clock, commands written during speech, a held command, Reset bytes during Speak External, and the VSM commands.

The vector captures, like the corpus, are kept local; each set's `index.md` and `takes.csv` are committed.
