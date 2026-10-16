# Optimise

**Optimise** searches a phrase's encoder settings for the encoding with
the best [quality rating](encode.md#the-quality-rating) that fits the
project's PHROMs. Open it with **Optimise...** in the
[encoder panel](encode.md#the-encoder-panel), for the phrase shown, or
with **Phrase > Optimise...** (Ctrl+Shift+O), for every selected phrase.
It is offered when a selected phrase has audio and is not locked; locked
phrases are skipped.

Nothing changes until you accept the results. While the window is open,
the main window cannot be edited.

## The options

Set these before pressing **Start**:

| Option | Default | Range | What it does |
|---|---|---|---|
| **Limit each phrase to ... bytes** | Off; when ticked, the largest size of the phrases now | 1 to 16 128 bytes | Only settings whose frames fit in this many bytes are chosen. Ticked alone, it finds better settings that are no bigger |
| **Trade quality for size** | Off; when ticked, 0.03 score per kbit/s | 0 to 3, steps of 0.01 | Chooses the settings with the best overall score less this much for each kbit/s of the encoding, so a smaller encoding can win over a slightly better one. At 0.03, saving 1 kbit/s is worth 0.03 of the score |

Whatever the options, settings whose encoding would take the project past
its [budget](phrases.md#phroms-and-the-project-budget) of 64 PHROMs are
never chosen. Each phrase is checked with the earlier phrases' results in
place, so the results fit together.

Optimise changes only the encoder settings that most affect the
encoding: **Target RMS**, **Repeat trade-off**, **Voicing threshold**,
**Silence gate**, **Analysis window**, **Lag window**, **Voicing
evidence** and **Slow gain control**. It leaves **Resonance hold** and
**Shortest voicing run** as they are, and never changes the
[preprocessing](preprocess.md): each phrase keeps its own, and its
encodings are scored against its preprocessed recording. The settings it
chooses become the phrase's own settings, as if set on the
[Encode view](encode.md#the-settings) with **This phrase**.

## The buttons

| Button | When | What it does |
|---|---|---|
| **Start** | Before the search | Begins the search |
| **Cancel** | While searching | Stops the search. Nothing changes |
| **Accept** | After the search | Keeps the results: each phrase's new settings and frames, with their quality, as one step to undo (**Undo Optimise HELLO**, or **Undo Optimise 3 Phrases**) |
| **Revert** | After the search | Closes the window, keeping the phrases as they were |
| **Close** | Before the search, or when no better settings were found | Closes the window. Escape, or closing the window, does the same, and stops a search that is running |

## While it searches

The window shows:

- a progress bar, the phrase being optimised and the candidates tried so
  far;
- the best so far on the quality rating's bar, with a dashed mark at the
  score it started from;
- the best so far's rating, score and size, against those it started
  from, and the settings it changes.

**Show every candidate** lists every set of settings tried, with the
phrase, its number, the stage of the search (*start*, *grid* or
*descent*), the settings it changes, its size, bit rate, overall score
and what became of it: *scored*, *over budget*, *over limit*, *failed* or
*cancelled*.

## Listening to the results

When the search finds better settings, **Listen** compares them by ear:

| Button | Plays |
|---|---|
| **▶ Original** | The phrase's frames before Optimise, which Revert keeps |
| **▶ Optimised** | The frames Optimise found, which Accept keeps |

Each plays the whole phrase at the recording's level, as the Encode
view's waveform plays it, through the [listening
options](encode.md#listening-options) chosen in the Audio menu, so the two
compare fairly. Pressing a button while it plays pauses it; pressing the
other stops it and plays from the beginning. When several phrases
changed, choose which to hear in the list beside **Listen**. A phrase
with no frames before Optimise has only **Optimised**.

If nothing better is found, the window says *No better settings were found
within the limits.* and nothing changes.

## How the search works

Each phrase's search starts from its current settings, then tries:

1. **A coarse grid** around them over the settings with the most effect:
   the voicing threshold ±0.05, the voicing evidence ±2, the repeat
   trade-off halved and doubled, and the target RMS ±4: 81 combinations.
2. **Steps along each setting** from the best so far: one step down and
   one up along the target RMS, the repeat trade-off, the silence gate,
   the analysis window, the lag window, the voicing threshold and
   evidence, and the slow gain control, all tried together. It moves to
   the best of them if that is better, and otherwise halves the steps,
   until they are as small as they go.

It tries at most 400 settings per phrase, never the same settings twice.
Candidates run in parallel on every core, and the result is the same
whatever the number of cores: the best has the highest score, and a tie
goes to the settings tried first.

Optimise takes about 12 seconds per second of speech on one core, and
about 1.4 seconds on 16 cores: a phrase of a few seconds takes a few
seconds on a computer with many cores, and under a minute on one.
