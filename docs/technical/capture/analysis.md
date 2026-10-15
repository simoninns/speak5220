# From captures to the datasheet

Each section of the [reference datasheet](../datasheet/index.md) is drafted as soon as its experiments give results, not saved up for the end. Every statement goes through the same four steps:

1. **Hypothesis.** Start from the original datasheet, then published emulations and notes, such as the patent [US 4,335,277](../sources/patent-us4335277/index.md).
2. **Test.** Design a vector ([controlled stimulus](stimulus.md)) that would show whether the hypothesis is wrong, and capture it.
3. **Write.** Document the confirmed behaviour, citing the capture directories that prove it ([evidence index](../datasheet/evidence.md)). Where it differs from the original datasheet, say so explicitly.
4. **Check.** Update the reference model and emulator to match the text, and rerun them against the whole data set (`investigation/tools/model.py`, `investigation/tools/regress.py`; [reference emulator validation](work/reference-emulator.md)).

The datasheet marks every correction to the original (**Correction**) and every addition (**New**), with its evidence.

## The model and the emulator

Two implementations of the synthesis path check the text, and they are kept apart:

- **`investigation/tools/model.py`** is the measuring tool behind the text. It implements the synthesis path as the datasheet describes it ([§8.9](../datasheet/reference-algorithm.md)), with options for every alternative tested, and `investigation/tools/vecfit.py` compares every take with it.
- **The [reference emulator](../emulator/index.md)**, the application's emulator, is written only from the datasheet text. It was first written in C, by a separate agent that did not see the tools, the captures or any other emulator, and the C++ emulator re-implements that code with identical output. A gap in the emulator's behaviour is therefore a gap in the text. Each gap is fixed in the datasheet first, then in the emulator, from the revised text alone. Other emulators may be used as a source of hypotheses, never as a reference.

The [encoder](../encoder/index.md) is written the same way, from the datasheet alone, and its own copy of the [§8.9 algorithm](../datasheet/reference-algorithm.md) is a third check of the text ([new speech data](work/new-speech-data.md)).
