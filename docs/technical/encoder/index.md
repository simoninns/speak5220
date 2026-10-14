# Encoder

An encoder that turns 8 kHz speech into TMS5220 frames, and packs the frames
into the chip's bit stream, a Speak External stream, or an Acorn PHROM or TI
indexed VSM image. These pages describe the project's research encoder,
written in Python from the datasheet alone.

The application's encoder re-implements it in C++ and is the one that is
maintained ([The encoder and Optimise](../../application/user-manual/encoder.md)). It
follows the research encoder step by step up to the level setting. It adds
preprocessing and two options (`resonance_hold_db`, `voicing_min_run`),
measures `target_rms` as loudness, gives pauses a quiet frame, lets the
loudest peaks clip a little, and has five different defaults
([Encoder options](../../application/reference/encoder-options.md)).

The research encoder's Python source (`tms5220enc.py`, its self-test
`test_tms5220enc.py`, and `evaluate.py`, which made the measurements in
[Quality](quality.md)) is in the repository's history at commit `6abe9ed`.

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

- [How it works](how-it-works.md): the method, step by step, and the
  encoder's options.
- [Quality](quality.md): what the encoder achieves on the test inputs.
- [Implementation choices](choices.md): what the datasheet leaves open for an encoder
  author, and the choices made.
- [How TI encoded speech](ti-encoder.md): TI's 1981 minicomputer program and
  the Portable Speech Lab, compared with the encoder and measured.
