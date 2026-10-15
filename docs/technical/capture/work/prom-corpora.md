# The remaining PROM corpora

The aim is to cover every PROM in the frozen inventory ([PROM corpus](../prom-corpus.md#the-inventory)) as the [Acorn PHROM A](acorn-phrom-a.md) was covered, and to compare the emulator with all of it: acceptance test 1 for the whole data set, on speech-range data ([acceptance tests](../index.md#acceptance-tests)).

## Method

- **Words:** every word of the seven PROMs, five takes through each path with the standard procedure. `starwars_speech` is captured through Speak External only, because the game has no VSM.
- **Sweep:** a sample of addresses in the four dumped images, one take each, VSM only: every 8th sweep address of `vm61002`, `vm61004` and `vm71003a`, and every 32nd of `ti99_speech`. The three assembled images (`vm61003_words`, `vm61005_words`, `starwars_speech`) are not swept. Their bytes are original only word by word, so data between the words is not original ([PROM corpus](../prom-corpus.md#words-and-the-address-sweep)).
- **Index pages** for each PROM, and the emulator run against each PROM.

Why a sample, not every address:

- A full sweep of the seven PROMs would be about 110,000 takes, about 54 hours on the bench.
- The full Acorn sweep has already shown what a sweep shows about the chip: the frame format holds for any data, and Talk Status clears at a fixed time ([Acorn PHROM A reference set](acorn-phrom-a.md#findings)).
- Mid-word starts often drive the filter outside the range of speech. There one-code upsets grow without limit, takes don't repeat, and the tolerance does not hold. So such takes can't serve as acceptance data, and a full sweep would mostly add unrepeatable output.
- The sample still checks the frame format and Talk Status on data from four more PROMs.
- `ti99_speech` is sampled most sparsely: its target chip is the TMS5200, not the TMS5220.

The commands, per PROM:

```
python investigation/tools/run.py <prom> all --takes 5 --path both --resume --zip-only
python investigation/tools/run.py <prom> --sweep --sweep-step 8 --resume --zip-only      # vm61002, vm61004, vm71003a
python investigation/tools/run.py ti99_speech --sweep --sweep-step 32 --resume --zip-only
```

For `starwars_speech`, use `--path external`.

## The sets

`investigation/captures/corpus/<prom>/` for the seven PROMs, kept local ([data layout](../data-layout.md#where-it-is-stored)), about 30 GB. They hold 15,178 takes, all from chip 1:

| PROM | Words | Word takes | Sweep addresses | Bench time |
|---|---|---|---|---|
| `ti99_speech` | 339 | 3,407 | 1,002 (every 32nd) | 2.9 h |
| `vm61002` | 206 | 2,070 | 1,983 (every 8th) | 2.0 h |
| `vm61004` | 148 | 1,479 | 1,944 (every 8th) | 1.7 h |
| `vm71003a` | 35 | 351 | 489 (every 8th) | 0.5 h |
| `vm61003_words` | 116 | 1,167 | none | 0.7 h |
| `vm61005_words` | 116 | 1,164 | none | 0.7 h |
| `starwars_speech` | 23 | 116 | none | 0.2 h |

- **Words:** every word, with at least five takes through each path that passed every check, except as below. Every retake was for a start-slot miss (48 word takes).
- **Sweep:** one take from each sampled address. All but nine have a take that passed every check. Those nine fail only `layer2_vs_layer3`, on quiet output (code spans of ±20 or less), which the DAC model's check does not judge well.
- **Index:** `index.md`, `takes.csv` and `sweep.csv` per PROM are committed, as for the Acorn PHROM A.
- **Bench time:** 8.7 hours in one unattended run. The fixed cost per take was 1.5–2.0 s, more than for the Acorn PHROM A, mostly on the longer TI-99 and VM61002 words.
- **Words too short for Speak External.** Speak External does not start speech until the FIFO holds 9 bytes ([datasheet §6](../../datasheet/commands.md)). Two words are shorter: `vm71003a` 35 "(0.25)" (5 bytes) and `vm61004` 35 "(Pause)" (8 bytes), both silence followed by a stop code. Through Speak External, "(0.25)" never spoke, and its five takes are kept with `talk_status` and `duration` failing. The bytes such a word leaves in the FIFO also turn the next reset sequence into data, so its Reset is never executed and the next take does not start from the standard state. `run.py` speaks words shorter than 9 bytes through the VSM only, so "(Pause)" was captured that way.
- **The tools cover every PROM.** `regress.py corpus` and `variance.py` take every PROM in `investigation/captures/corpus/`.

## Follow-up tests

These tests look into where the emulator's error comes from, prompted by `starwars_speech`, which a first comparison against chip 1's own takes left outside the tolerance:

- **Where the emulator's error comes from.** Over every PROM word, the takes 2–5 of each word and path (which follow the same word) were compared with each other and with the model. About 87 % of the model's departures from the chip start at samples where all four takes agree. So most of the emulator's error is deterministic: the lowest-order arithmetic ([datasheet §8.8](../../datasheet/lattice-filter.md)). The rest starts where the chip's takes disagree (upsets), and a small share, large in a few words, came from the onset after silence (below).
- **The onset vectors** (`investigation/tools/vectors.py` set `onset`, 52 vectors, 2 takes per path, chips 1 and 4; `investigation/captures/vectors/vectors_onset/`, `investigation/captures/vectors_chip4/vectors_onset/`): voiced, then 1 to 6 silence frames, then voiced, at seven pitches before and four after; and the same after unvoiced speech, into a repeat frame, at a loud resonant setting, and with no silence.
- **The repeat test** (49 takes each of six words, chip 1; `investigation/captures/phase10/repeat/`): `acorn_phrom_a` 213 L, `vm61002` 115 NOR, `vm61004` 71 ERROR and `ti99_speech` 345 WELL through the VSM, and `starwars_speech` 12 and 13 through Speak External.
- **Chip 4 on PROM words** (`investigation/captures/corpus_chip4/`, index files committed): every `starwars_speech` word through Speak External, and every `vm61003_words` word through both paths, five takes each. Chip 4 fails `layer2_vs_layer3` on every take, as in the [second-chip comparison](reference-emulator.md#a-second-chip) (its DAC gain differs); no other check failed apart from start-slot retakes.
- **Corpus mining** (`investigation/tools/departures.py`, `investigation/captures/phase10/departures/`). The model's internal state at every clean deterministic departure (15,851), every clean upset start (387) and a 3 % sample of agreeing samples (56,999 controls), over every PROM word; and the same over the repeat test.
- **Bench time:** about 1.4 hours (chip 1 0.6 h, chip 4 0.9 h).

## The emulator against the chip

Results in `investigation/captures/phase10/regress/` and `investigation/captures/phase10/variance/`, with the pitch-hold rule of [datasheet §8.6](../../datasheet/excitation-interpolation.md) as described under Findings:

- **The emulator equals `investigation/tools/model.py`** on every take of every PROM, words and sweeps, the Acorn PHROM A included: no sample different.
- **Layer 1 (VSM path):** the bits read and the time Talk Status cleared match on every take (within 4.4 µs). The frame-read samples match on every take but 18 (below).
- **Layer 2, against the tolerance,** with chip 4 as the reference where it was captured, as the tolerance prescribes:

| PROM | Reference | Reference (dB) | Emulator (dB) | Gap (dB) | |
|---|---|---|---|---|---|
| `ti99_speech` | chip 1 takes | 39.6 | 34.8 | 4.9 | within |
| `vm61002` | chip 1 takes | 34.8 | 31.3 | 3.6 | within |
| `vm61004` | chip 1 takes | 35.3 | 31.3 | 4.1 | within |
| `vm71003a` | chip 1 takes | 38.7 | 32.8 | 5.9 | within |
| `vm61003_words` | chip 4 | 32.0 | 31.7 | 0.3 | within |
| `vm61005_words` | chip 1 takes | 36.1 | 31.6 | 4.5 | within |
| `starwars_speech` | chip 4 | 35.6 | 32.5 | 3.1 | within |

*Median SDR over the speech window. The Acorn PHROM A gives 37.5, 32.6 and 4.9. Against chip 1's own takes, `starwars_speech` is outside (40.7 dB capped at 40, against 32.5: a gap of 7.5 dB).*

Every word of every inventoried PROM is captured, the sampled sweep addresses are captured, and the emulator matches all of them within the tolerance: the VSM timing exactly, and layer 2 on every PROM. `starwars_speech` comes within it through the reference that the tolerance already names, a second chip; the tolerance rule itself is unchanged.

## Findings

- **After silence that followed voiced speech, the pitch counter holds for three samples.** Its hold at an inhibited change lasts three samples of IP 0 (PC 0 A and B, and PC 1 A), not two, so the chirp starts one sample later than a two-sample hold gives. The onset vectors show it on both chips, through both paths, every take: after 1 to 6 silence frames, at every pitch before and after, into a voiced or a repeat frame. After unvoiced speech the hold's length makes no difference, because the counter is at 0 until the new pitch arrives. With this rule the model matches the onset of every onset vector on both chips. Across the data set it cut the model's error power by up to 13 times (`vm61003_words`; 6.6 times `vm61004`, 2.2 times `starwars_speech`), against a two-sample hold, and no PROM or vector set got worse; 28 of about 40,000 takes lost a few matching samples in chaotic stretches. The median SDRs barely moved, because the error was concentrated in a few words. The rule is in [datasheet §8.6 and §8.9](../../datasheet/excitation-interpolation.md), in `investigation/tools/model.py` (option `pitch_hold`), and in the emulator (then the C emulator's `tms5220.c`, step 2.3), which a separate agent updated from the revised text alone. In the PROM words the model matches every such onset in the Acorn, `vm61003_words`, `vm61004` and Star Wars words (166, whatever the number of silence frames). A lag measure taken as the best shift over 40 samples is too coarse to show this. In TI-99 that measure gives mixed lags on its loud onsets, and its words that start with a silence frame are 3 samples off: the known approximation of a first silence frame ([datasheet §7](../../datasheet/power-up.md)).
- **What is different about the Star Wars data.** Not the chip's behaviour: its upsets start at the same rate as on every other PROM (about 5 a second) and carry the same energy each. Two things in the data make chip 1 repeat it unusually well:
    - **It is loud,** an RMS of 30.7 codes against 19–20 for the Acorn and VM PROMs. So the same upsets weigh less in the SDR (about 3.7 dB).
    - **It has many short pitch periods.** Of its 23 words, 9 have a mean pitch index below 25, against 13 of the 2,201 other word groups. There upsets die away sooner (a median of 77 samples at indices 1–15, against about 500 at 32–47), so chip 1's takes agree better (43.6 dB below index 25). The emulator's deterministic error does not shrink with them. Star Wars' words with a pitch index of 32 or more were within the chip-1 reference (a gap of 4.9 dB), and the short-pitch words of `vm61002` and `vm61004` show the same large gap.

    The emulator's deterministic departures start twice as often on Star Wars as on most PROMs, but a model of them fitted without Star Wars predicts its rate (474 against 564 predicted; 0.44–1.08 of the prediction elsewhere). So that too is its data: more unvoiced samples, and more coefficients of the kind below.
- **What an upset is.** Each starts as a single ±1 at one of a few fixed samples per word, and at each such sample a take in step leaves with a fixed chance, 2 % to about 50 % (the repeat test). It goes towards the nearer output step. There, the model's 15-bit value is one or two internal steps from the boundary, the same place the deterministic departures start. Upsets and deterministic departures are both enriched for coefficients whose second Booth digit is negative (*d*₁ < 0, [reference emulator validation](reference-emulator.md#findings)), independently for each K. K6–K10 matter most, and K7 more for upsets than for deterministic departures. In the repeat test the emulator's deterministic departures never varied: at 99.6 % of them no take out of 49 gave the emulator's value. So the lowest-order arithmetic is deterministic at most samples and marginal at a few, which are the upsets ([datasheet §7](../../datasheet/power-up.md) and [§8.8](../../datasheet/lattice-filter.md)).
- **Two chips agree on PROM words no better than the emulator agrees with either.** On `vm61003_words` chip 4 is 32.0 dB from chip 1 and the emulator 31.7 dB; on Star Wars 35.6 and 32.5 dB. A chip agrees with itself much better, because its upsets are few and its departures from the model repeat.
- **TI-99 is loud,** as the [inventory captures](../prom-corpus.md#what-the-inventory-showed) showed. Only 61 % of samples are equal, against 73–79 % for the other PROMs, with differences of up to 16 codes at the 90th percentile. Its take-to-take agreement is high, though, and the emulator is within the tolerance.
- **Long takes: one frame read off by a sample.** In 18 TI-99 sweep takes of 5.5 s or more, all in the index area at 0x0000–0x0D20, one frame read falls one sample later than the 200-sample grid gives, always between samples 44,100 and 45,500. The 25 ms frame period check passes on all of them. The Pico's timer, which times the reads, drifts against the chip's sample clock, and by then a read late in its sample moves into the next. This is a limit of the measurement, not of the chip or the emulator.

## Open

The lowest-order arithmetic and the cause of upsets: [known gaps](../../datasheet/gaps.md#synthesis). The full sweeps of these PROMs could be added if a faster runner or a second bench makes them cheap.
