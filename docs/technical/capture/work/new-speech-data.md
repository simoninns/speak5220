# New speech data

The aim is to show that the documented mechanism is complete, by generating new speech data from the documentation and running it through the real TMS5220 ([confirmation with new speech data](../stimulus.md#confirmation-with-new-speech-data)). The criteria are acceptance tests 2 and 3 ([acceptance tests](../index.md#acceptance-tests)): all the new speech data plays on the chip as predicted, within the tolerance, and the round trip of the original data is bit-identical. They apply to speech-range data only: the tolerance holds on no set that overloads the filter.

## The test speech

The encoder's test speech was fixed before the encoder was written.

- **Synthetic speech:** `investigation/tools/ttsgen.py` renders the prompts in `investigation/tools/prompts.csv` with Piper. There are 122 prompts in nine voices, 210 files and about 9.5 minutes. They are written to `investigation/captures/phase11/inputs/tts/` as 8 kHz FLAC, 6 MB in all, with an index of hashes of the samples.
- **Real recordings:** the originals are kept local in `investigation/local/recordings/`, each listed in its `sources.csv`. `investigation/tools/recimport.py` splits each original into its prompts and writes them to `investigation/captures/phase11/inputs/recordings/` in the same way as the TTS files. Only originals whose licence allows publication are imported. They are 33 CMU ARCTIC prompts (a0001 to a0033), read by the project's author: 1.7 minutes, 1.2 MB.
- **Hand-designed frame sequences** need no audio. Rendered through the emulator, they give inputs whose frames are known, so the encoder's parameters can be checked directly, not only by ear.

## The authoring guide and the encoder

- **The authoring guide** is [datasheet §12](../../datasheet/authoring.md). It covers the bit budget; when a frame is heard; levels, the excitation's spectrum and the sign convention of the K; pitch, and the choice of frame kinds; an encoding method; VSM images, word tables and Speak External streams; and how the original PROMs were encoded (`investigation/tools/promstats.py`, `investigation/captures/phase11/promstats/`).
- **The encoder** ([encoder](../../encoder/index.md), `investigation/encoder/`). It was written by a separate agent that read only the datasheet, the [PROM inventory](../prom-inventory.md) for the word-table formats, the emulator's command line, and the test inputs. Its [list of choices](../../encoder/choices.md) gives 31 points the datasheet left open. Those that concern the chip are fixed in the text: §8.9's wording in four places, and in §12 the frame-to-output mapping, the level targets, two numerical errors and the Acorn header. It covers:
    - LPC analysis, quantisation and level setting by closed-loop synthesis;
    - its own copy of the [§8.9 algorithm](../../datasheet/reference-algorithm.md), which matches the emulator sample for sample;
    - the packers for Acorn PHROMs, TI indexed images and Speak External streams.

  It produces about 1300 bit/s, against 1372 for the original PROMs. On the 243 inputs: a median level error of 1.2 dB per frame, output peaks of at most 120 codes, and nothing clipped or wrapped.

The data, predictions and captures on this page were made with the encoder at commit 01b7fff and stay as they are. The current encoder adds a repeat search, pre-emphasis and voicing evidence ([encoder](../../encoder/index.md)).

## The round trip

**Acceptance test 3 passes.** Every word of the eight PROMs, 1148 words, parsed with the documented frame format and packed again by the encoder, is bit-identical to the original up to the end of its stop code (`investigation/tools/phase11.py roundtrip`, `investigation/captures/phase11/roundtrip/`). The rest of the stop code's byte is padding the chip never reads. In the TI and Acorn images it is always all 1s, where the encoder writes 0s.

## The data, the predictions and the captures

The new speech data (`investigation/captures/phase11/data/`) is 347 utterances and 10.4 minutes, in five synthetic PROMs:

| Set | Contents | Utterances | Speech |
|---|---|---|---|
| `phrom` | The 58 single words, numbers and computer phrases, one voice each, as an Acorn-format PHROM with word table and names (16 KB) | 58 | 56 s |
| `tts1`, `tts2` | Every TTS input; inputs over 8 s are cut at a pause, because the Pico's trace holds about 16 s of VSM speech | 226 | 523 s |
| `rec` | The 33 recordings | 33 | 96 s |
| `hand` | Hand-designed frame sequences: every frame kind and transition, every energy and pitch, each K from end to end, a word padded to 9 bytes for Speak External, and a word that starts with a repeat frame | 30 | 9 s |

- **The predictions** (`investigation/captures/phase11/predictions/`) were committed before any of the data was played (commit 01b7fff). They give the emulator's output for every utterance and path, and for each VSM utterance the bits read, every frame read and when Talk Status clears. Each utterance was predicted after the word `run.py` speaks before it, and also after itself; only the two hand items built to depend on their predecessor differ.
- **The captures** (`investigation/captures/phase11/takes/`, kept local, index files committed): three takes per path of every utterance on chip 1, with the standard procedure, 2082 takes in 2.5 hours (`investigation/captures/phase11/bench.sh`). No take failed a check of the capture or of the data. 28 missed their start slot and were retaken. Every take failed `layer2_vs_layer3`: the DAC gain fitted on chip 1 was 1.17 (median), against 1.09 on the [PROM corpora](prom-corpora.md), outside the check's 15 % limit. The residual stayed at 2.3 mV RMS or less (95th percentile), and every DAC code was recovered within ±2. The gain does not affect `I/O`.

## Results

Against the committed predictions (`investigation/tools/phase11.py compare`, `investigation/captures/phase11/results/`):

| Set | Takes | Emulator SDR (dB, median) | Chip 1's own takes (dB) | Gap (dB) | Within | Layer 1 (VSM takes) |
|---|---|---|---|---|---|---|
| `phrom` | 322 | 29.6 | 35.7 | 6.1 | yes | 150 of 150 exact |
| `rec` | 198 | 29.7 | 36.5 | 6.9 | yes | 99 of 99 exact |
| `tts1` | 1223 | 29.1 | 35.1 | 6.0 | yes | 594 of 612 exact |
| `tts2` | 131 | 28.4 | 34.0 | 5.6 | yes | 44 of 65 exact |
| `hand` | 180 | 24.6 | identical (capped at 40) | 15.4 | no | 90 of 90 exact |

- **Acceptance test 2 holds on all the new speech:** the PHROM, the TTS and the recordings are within the tolerance through both paths, and the VSM timing matches the prediction.
- **Layer 1, the exceptions.** In 39 takes one frame read was logged one sample late. It always came after sample 45,400, in takes over 5.7 s long. It is the limit of measurement found on the long TI-99 takes ([the remaining PROM corpora](prom-corpora.md#findings)): the Pico's timer drifts against the chip's clock. Every take read exactly the predicted bits, and Talk Status cleared within 3.6 µs of the prediction.
- **The hand-designed set is outside the tolerance,** as the synthetic vectors were against chip 1 alone ([reference emulator validation](reference-emulator.md#result)). Its takes repeat exactly, so the reference is capped at 40 dB. Its items are quiet: most have an RMS of 5–25 codes, against about 19 for speech. So the emulator's known one-code voiced error ([datasheet §8.8](../../datasheet/lattice-filter.md)) gives a median of 24.6 dB, although the difference is at most 2 codes in 14 of its 30 items. The larger differences are in strongly resonant filters (`k_all_max` at energy 1, `v_u_alternate`, `u_then_vrepeat`), where one code grows. Every frame read and every bit matched the prediction. This is explained by the emulator's known error on very quiet items, not by a gap in the authoring guide. A second chip would give the fair reference, as the tolerance provides ([known gaps](../../datasheet/gaps.md#other-chips-and-the-tms5220c)).
- **The two paths on the chip.** Voiced data does not give exactly identical output through both paths, shifted by 2 samples. On the hand set's voiced items the VSM and Speak External outputs differ by ±1 at scattered samples from about the second frame on. Each path repeats exactly from take to take. So this is the deterministic lowest-order error coming out differently in the two paths, not noise. 9 of the 21 voiced pairs were identical.

### Intelligibility

`investigation/tools/asrcheck.py` (whisper.cpp `small.en`, `investigation/captures/phase11/results/asr/`) transcribed each item's input, the prediction and the chip's take vsm02, and took the word error rate against the prompt's text:

| Items | Input | Prediction | Chip |
|---|---|---|---|
| Sentences and passages (5 words or more, 205 items) | 18.8 % | 29.5 % | 29.2 % |
| The recordings | 5.8 % | 15.3 % | 16.6 % |
| The passages (`tts2`) | 2.5 % | 8.2 % | 8.4 % |
| Single words (83) | 81 % | 120 % | 117 % |

- **The chip says what the prediction says.** Their error rates agree in every group, and half the transcripts (152 of 301) are word for word the same.
- **The encoding costs words.** On sentences it adds about 10 points of error, as LPC-10 at 8 kHz is expected to.
- **Single words are not a fair test.** Without context the recogniser fails even on the clean input. Numbers written as digits count as errors too.

This is an objective check that words survive, not a substitute for listening. `investigation/captures/phase11/takes/listen.html` (kept local) sets each item's input beside the chip's output through both paths; listening is left to the project's author. The new PHROM image has not been tried on a BBC Micro ([open questions](../open-questions.md#checks-not-made)).

## Findings for the datasheet

- **An utterance can depend on the previous one through K5–K10.** An unvoiced frame leaves the stored K5–K10 indices alone, and a silence frame ends their zeroing. So an utterance that opens unvoiced, then has silence before its first voiced frame, uses the previous utterance's K5–K10. The emulator's predictions showed it in about 20 of the first encoding's utterances. [Datasheet §12.2](../../datasheet/authoring.md) warns of it, and the encoder avoids it.
- **§8.9 clarifications** found by the encoder's own copy of the algorithm: which indices an unvoiced frame writes, that the pitch counter steps every sample, and the order of two writes within a sample ([datasheet §8.9](../../datasheet/reference-algorithm.md)).
