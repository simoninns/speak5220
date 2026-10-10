# Open questions

What is not known about the chip itself is listed once, in the datasheet's [Appendix B: Known gaps](../datasheet/gaps.md), with what would settle each gap. This page lists the open questions about the project's method and data set.

## Internal state that is not visible on `I/O`

`I/O` carries only the final sample, so the inner filter state has to be inferred. Carefully designed vectors (for example, one K parameter active at a time) keep that inference tractable.

## Clock fidelity

The board has no RC oscillator, so captures are made with a crystal-derived clock. That is ideal for reproducibility, but real BBC Micro hardware ran slightly off 640 kHz and drifted. To study that behaviour, the Pico can sweep the clock frequency rather than an RC network being refitted.

## Licensing

The original ROM images stay out of the repo unless their redistribution status is clear. The repo holds the manifests and hashes, plus the captured output where appropriate. `investigation/tools/promimport.py` rebuilds every image except the Acorn PHROM A from public sources pinned to a commit ([PROM corpus](prom-corpus.md)). Whether the captured output of each PROM may be published is not decided.

## Publishing the corpus

The PROM corpus and vector captures are kept local ([data layout](data-layout.md#where-it-is-stored)): about 40 GB for the Acorn PHROM A and about 30 GB for the other seven PROMs. The repo holds their index files and hashes. How to publish them (release archives, Git LFS or elsewhere) is not decided. A release would tag the datasheet, data set, emulator, tools and test vectors together, once the acceptance tests pass on the frozen data set.

## Copyright of the original datasheet

The new datasheet is written fresh: it follows the original's structure and cites it, but does not copy its text or figures wholesale ([reference capture](index.md#the-reference-datasheet)).

## Checks not made

- **Listening.** `investigation/captures/phase11/takes/listen.html` (kept local) sets each new speech item's input beside the chip's output through both paths. Listening is left to the project's author ([new speech data](work/new-speech-data.md)).
- **A BBC Micro.** The new PHROM image has not been tried in a BBC Micro's speech system ([Appendix B](../datasheet/gaps.md#the-acorn-phrom-format-and-the-bbc-micro)).
- **The full address sweeps** of the PROMs other than the Acorn PHROM A, if a faster runner or a second bench makes them cheap ([the remaining PROM corpora](work/prom-corpora.md)).
