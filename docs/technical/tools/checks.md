# Readable files and checks

`process.py` rebuilds a take's readable files from `raw/` and can be rerun at any time: `python investigation/tools/process.py <take>...`. `run.py` runs it on every take ([Capturing PROM words](capture.md)).

## Readable files

- `frames.csv`: one row per frame the chip parsed, up to the stop code. The bits come from the VSM trace (VSM path) or from the bytes written to the FIFO (Speak External). Columns: bit offset, length, kind, energy, repeat, pitch and the K1–K10 indices. For the VSM path, `read_us` is when the frame's first bit was read, after `SCOPE_TRIG` rose, and `read_sample` is the `samples.wav` sample period it fell in. The parameters are the coded indices, each taken most significant bit first (confirmed by the synthetic vector captures; the values are in [datasheet §8.1](../datasheet/coded-parameters.md)).
- `samples.wav`, `samples.csv`: layer 2, from the first `T11` period after `SCOPE_TRIG` rose to the end of the capture. The WAV convention is as for `decode.py` ([Single captures and analysis](single-captures.md)). The CSV adds the DAC voltage and the DAC code recovered from it.
- `dac.wav`: layer 3, the recovered DAC codes (code × 64). The codes come from the DAC bit-weight model ([datasheet §8.2](../datasheet/dac-and-audio.md)) fitted to this capture. They are measured, not exact: about 2/3 match `I/O` exactly, and almost all are within ±2.
- `listen.wav`: for listening only. It holds the DAC codes for each sample period at 48 kHz and passes them through a model of the BBC Micro's audio filter ([Audio output stage](../hardware/audio-output-stage.md); `--listen-filter none` leaves the filter out).

## Checks

The checks go into `capture.json` under `processed.checks`. Each is true, false, or null when it does not apply:

| Check | Passes when |
|---|---|
| `scope_overflow` | No channel went over range |
| `io_format` | No `I/O` activity outside bit slots 2–11 |
| `fosc` | The `T11` rate gives `OSC` within 50 ppm of the clock set |
| `trace_complete` | No trace events were dropped |
| `talk_status` | Talk Status was seen set, and was clear at the end |
| `stop_frame` | The parsed data ends with a stop frame |
| `vsm_image` | VSM: the bits read are the image's, from the word address |
| `bits_vs_frames` | VSM: the bits read are exactly the frames up to the end of the stop code |
| `frame_period` | VSM: frame reads are 25 ms apart, within 0.1 ms |
| `duration` | VSM: Talk Status cleared within 2 ms of frames × 25 ms after the first frame read. Speak External: the talk time exceeds frames × 200 samples by 0–400 samples |
| `external_stream` | Speak External: the bytes on the bus are the stream sent |
| `start_frame` | VSM: the first frame read is on the frame clock's grid (within 5 µs), and on the frame it was timed for. Speak External: the timed start was not late |
| `start_slot` | Standard procedure: the chip accepted the Speak within 2 µs of the target time before the frame |
| `layer2_vs_layer3` | The DAC bit-weight model of the `I/O` codes, fitted with a gain and an offset, leaves 3 mV rms or less; the gain is within 15 %; and 99 % of the recovered DAC codes are within ±2. All three are judged on codes inside ±119: at the rails (127, and −128, which clipped codes take) the chip sits 6–10 mV below the model, so the rail samples are reported separately (`rail_samples`, `rail_residual_mean_mv`). The gain is judged only when the codes span at least 32 (`gain_judged`): over a few codes around 0 the fit is dominated by the 6-LSB step between −1 and 0. Null for `--board audio` |

The gain limit is wide on purpose. It catches a wrong board configuration (half load gives about 0.5, and the audio stage leaves a large residual), not drift: the DAC scale has been seen 7 % above the bit-weight table. The fitted gain is recorded in every capture.

## Comparing repeats

`compare.py` compares the takes of a word with the first one taken: `python investigation/tools/compare.py <word dir>` writes `compare.json` there. Each take is shifted by up to ±400 samples to match the first one best, because without the standard procedure the start of speech varies. Then the comparison reports the differing samples, the first difference (counted from the start of speech), the largest and rms difference, and whether the frames are identical.

Takes made with the standard procedure should match exactly, apart from upsets ([datasheet §7](../datasheet/power-up.md)). These are small departures, of a few codes, that start at a few points in a word. Only the standard takes that passed every check count. A take that missed its start slot has different noise, so it is listed in `standard_failed_takes` instead. For each path, `compare.json` (`standard_variants`) gives:
- the per-sample majority of those takes, as the reference output;
- for each take, how far it departs from that majority, and the sample where each upset starts;
- the number of distinct outputs;
- the samples where no majority exists.
