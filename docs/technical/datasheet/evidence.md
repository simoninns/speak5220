# Evidence index

Every measured statement in the reference datasheet cites a capture by its ID, for example [p1-03]. This page says how the captures are stored and lists every ID. The pages below give each capture's settings and hashes, grouped by what was measured.

## How the captures are stored

Each capture is in the repository under `investigation/captures/phase<N>/<ID>/`. The raw scope data is in `raw/scope_raw.npz.zip`, which holds the original `scope_raw.npz` unchanged. That file is never modified, and the SHA-256 hashes on these pages are of the `.npz` inside the zip. `capture.json` records the settings and the host commit, and the derived files sit next to it. Captures driven through the Pico also keep the Pico's trace of every VSM function and host cycle, `raw/vsm_trace.bin`, and its hash is given too.

To regenerate the analysis from a raw capture:

```
python investigation/tools/decode.py investigation/captures/phase1/<ID>
python investigation/tools/edge_timing.py investigation/captures/phase1/<ID> --plot
```

The DAC analysis pools several captures ([DAC transfer and the rails](evidence/dac.md)).

Two kinds of citation refer to whole sets rather than single captures:

- **[corpus `<prom>`]** is a PROM's whole reference set, kept outside the repository until it is published. Its index is committed in `investigation/captures/corpus/<prom>/` (`index.md`, `takes.csv`, `sweep.csv`), with every take's checks and hashes. The Acorn PHROM A set is described in [The Acorn PHROM A reference set](evidence/acorn-reference-set.md).
- **[vectors `<set>`]** is a synthetic vector set. Its captures are kept local, and its index is committed in `investigation/captures/vectors/vectors_<set>/` (`index.md`, `takes.csv`). The sets are described in [Synthetic vectors](evidence/vectors.md).

## Captures by ID

| IDs | Captured | What was measured | Page |
|---|---|---|---|
| p1-01 to p1-03 | 2026-10-06 | Output signals: `T11`, `I/O`, ROMCLK and the DAC step, edge by edge | [Output signals and clock timing](evidence/output-signals.md) |
| p2-01 to p2-06 | 2026-10-06 | The DAC transfer function at two loads, and the DAC step timing | [DAC transfer and the rails](evidence/dac.md) |
| p3-01 to p3-04 | 2026-10-06 | First VSM traces: Speak, Speak External and Read Byte | [VSM reads and the two input paths](evidence/speech-paths.md) |
| p4-01 to p4-04 | 2026-10-06 | One word through both paths with the capture runner: frame timing and the start delay | [VSM reads and the two input paths](evidence/speech-paths.md) |
| p5-01 to p5-18 | 2026-10-06 | The standard procedure; the frame clock, the noise generator, clock stops and the `ADD8` margin | [Start timing, noise and repeats](evidence/start-and-noise.md) |
| p6-01 to p6-08 | 2026-10-06 | Loud TI-99/4A speech: the DAC rails | [DAC transfer and the rails](evidence/dac.md#loud-speech-and-the-dac-rails) |
| p7-01 to p7-11 | 2026-10-06 to 2026-10-07 | The Acorn PHROM A reference set and the previous utterance | [The Acorn PHROM A reference set](evidence/acorn-reference-set.md) |
| p8-01 to p8-18 | 2026-10-07 | Synthetic vectors: the model, the noise state, the latch and the previous utterance | [Synthetic vectors](evidence/vectors.md) |
| p8-19 to p8-25 | 2026-10-07 | Host interface tests: READY, INT, the FIFO and the VSM functions | [Host interface tests](evidence/host-interface.md) |
| p9-01, p9-02 | 2026-10-07 | The `ring` vectors: the filter's lowest-order arithmetic | [Synthetic vectors](evidence/vectors.md#the-ring-vectors) |
| p12-01 to p12-08 | 2026-10-11 | Silence frames inside an utterance: the filter's ringing, and quietening it with a neutral frame | [Silence and pauses](evidence/pauses.md) |

Other measurements are cited by their directory rather than an ID, for example [`investigation/captures/promout/chip1/`] for the `PROM OUT` reads ([§8.10](prom-out.md#810-parameter-rom-output-prom-out)), [`investigation/captures/phase9/variance/`] for the agreement figures of [§8.8](lattice-filter.md#88-lattice-filter), and [`investigation/captures/phase11/`] for the new speech data of [§12](authoring.md#12-authoring-speech-data).
