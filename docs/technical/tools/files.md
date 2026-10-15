# File list

The files in `investigation/tools/`, with the page that describes each one.

- `pico.py`: the host side of the firmware protocol, with the trace decoder, the self-test and a command line ([Pico control](pico.md)).
- `run.py`: the capture runner ([Capturing PROM words](capture.md)).
- `process.py`: builds a capture's readable files from `raw/` and runs the checks ([Readable files and checks](checks.md)).
- `compare.py`: the repeat comparator ([Comparing repeats](checks.md#comparing-repeats)). `promindex.py`: writes a PROM's `index.md`, `takes.csv` and `sweep.csv`.
- `prom.py`: PROM manifests (`investigation/proms/`). `promimport.py`: the PROM inventory; it rebuilds the images and writes the manifests. `bench.json`: the bench details recorded in every capture ([PROM manifests](capture.md#prom-manifests)).
- `frameclock.py`: the chip's frame clock on the Pico's timer, and the calibration of timed starts. `python investigation/tools/frameclock.py` locates the clock and calibrates both paths.
- `lpc.py`: the LPC frame parser and encoder: frame boundaries, the coded parameters of each frame, and frames back to bytes.
- `vectors.py`, `model.py`, `vecfit.py`, `protocol.py`: the synthetic vectors, the synthesis model, the table measurements and the host interface tests ([Synthetic vectors and the model](vectors-and-model.md)).
- `regress.py`, `variance.py`: the regression harness for the emulator (run as `tms5220emu`, [Command line](../emulator/command-line.md)), and the measured agreement between chips and the model ([Emulator regression](emulator-regression.md)).
- `ttsgen.py`, `prompts.csv`, `recimport.py`: the encoder test speech and recordings ([Encoder test speech and quality](encoder-speech.md)).
- `promstats.py`, `phase11.py`, `asrcheck.py`: the PROM statistics, new speech data, predictions, comparison and intelligibility check ([Encoder test speech and quality](encoder-speech.md#new-speech-data-and-its-predictions)).
- `speechqual.py`: the objective encoding-quality measure ([Encoding quality](encoder-speech.md#encoding-quality)).
- `promout.py`: reads the parameter ROM through `PROM OUT` ([PROM OUT](prom-out.md)).
- `ps3000a.py`: ctypes wrapper for the ps3000a driver, including the MSO's digital ports.
- `tms5220_decode.py`: decodes `I/O` (a 10-bit two's-complement sample, LSB first, in bit slots 2–11 after the `T11` rising edge) and averages the DAC voltage over each settled step.
- `capture.py`, `decode.py`: single captures outside a PROM, and decoding of multi-word captures. `run.py` uses `capture.py`'s scope functions ([Single captures and analysis](single-captures.md)).
- `dac_transfer.py`: the static DAC transfer function from one or more captures. It writes a per-code table of TP2 voltage and current (`dac_table.csv`), fits one weight per DAC input bit, and reports linearity, the step from −1 to 0, and whether the output depends on earlier samples (`dac_transfer.json`, `dac_transfer.png`). `--zero` subtracts the scope's zero offset, measured from a capture taken with probe A's tip grounded.
- `dac_compare.py`: compares the tables from two loads, giving the current ratio per code and the equivalent output resistance. The reference datasheet's §8.2 and §8.3 come from these two tools.
- `edge_timing.py`: measures `T11`, `I/O`, DAC and, if probe D is on TP1, ROMCLK edge timing. It writes `edge_timing.json`, and `edge_timing.png` (one annotated sample period). The reference datasheet's §8.4 and §9.5 come from its output.
- `die.py`: the die image tools ([Die image](die-image.md)).
