# Encoder

The encoder turns 8 kHz speech into TMS5220 frames, and the packagers pack the
frames into the chip's bit stream, a Speak External stream, or an Acorn PHROM
or TI indexed VSM image. It is the application's encoder, in
`app/src/core/encoder/` (and `app/src/core/packaging/`); the
[user manual](../../application/user-manual/encoder.md) covers using it, and
the [option reference](../../application/reference/encoder-options.md) lists
its options. These pages describe its method, its quality and the choices
behind it.

## Written from the datasheet only

The encoder was written from the [reference datasheet](../datasheet/index.md)
alone, mainly §8 (the [reference algorithm](../datasheet/reference-algorithm.md)
and the sections before it) and §12 ([authoring speech data](../datasheet/authoring.md)):
no other encoder, emulator source or original PROM data was used to write
it. Whatever the datasheet leaves open is listed in
[Implementation choices](choices.md). Section numbers (§N) on these pages
refer to the reference datasheet.

Some defaults were then tuned against Acorn's own PHROM phrases, so that the
encoder's voicing, repeats and loudness come near theirs, and the rest for
the best quality over a corpus of 243 test inputs
([Encoder options](../../application/reference/encoder-options.md)).

TI's own encoder was studied afterwards for ideas to test. Its methods are
hypotheses for the encoder, measured objectively, not a specification. What
was adopted is described in [How it works](how-it-works.md).

## Pages

- [How it works](how-it-works.md): the method, step by step.
- [Quality](quality.md): what the encoder achieves on the test inputs, and
  where the quality goes.
- [Implementation choices](choices.md): what the datasheet leaves open for an encoder
  author, and the choices made.
- [How TI encoded speech](ti-encoder.md): TI's 1981 minicomputer program and
  the Portable Speech Lab, compared with the encoder and measured.
