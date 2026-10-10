# Encoder

An encoder that turns 8 kHz speech into TMS5220 frames, and packs the frames
into the chip's bit stream, a Speak External stream, or an Acorn PHROM or TI
indexed VSM image. The code is in `investigation/encoder/`.

## Written from the datasheet only

The encoder was written from the [reference datasheet](../datasheet/index.md)
alone, mainly §8 (the [reference algorithm](../datasheet/reference-algorithm.md)
and the sections before it) and §12 ([authoring speech data](../datasheet/authoring.md)):
no other encoder, emulator source or original PROM data was used. The
project's [reference emulator](../emulator/index.md) binary was run only as a
black box, to check `synthesize()`. Whatever the datasheet leaves open is
listed in [Implementation choices](choices.md). Section numbers (§N) on these pages refer
to the reference datasheet.

TI's own encoder was studied afterwards for ideas to test. Its methods are
hypotheses for the encoder, measured objectively, not a specification. What was adopted is described
in [How it works](how-it-works.md).

## Pages

- [Usage](usage.md): the command line, the Python API, the `encode()` options
  and the self-test.
- [How it works](how-it-works.md): the method, step by step.
- [Quality](quality.md): what the encoder achieves on the test inputs.
- [Implementation choices](choices.md): what the datasheet leaves open for an encoder
  author, and the choices made.
- [How TI encoded speech](ti-encoder.md): TI's 1981 minicomputer program and
  the Portable Speech Lab, compared with the encoder and measured.

## Files

| File | Contents |
|---|---|
| `tms5220enc.py` | The encoder, packer, synthesis and packaging (importable as `tms5220enc`) |
| `test_tms5220enc.py` | Self-test: `nix develop -c python3 investigation/encoder/test_tms5220enc.py` |
| `evaluate.py` | Encodes the test inputs (`investigation/captures/phase11/inputs/`) and measures the result (the numbers in [Quality](quality.md)) |
