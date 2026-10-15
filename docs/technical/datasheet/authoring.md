# 12. Authoring speech data

**New.** The original has no authoring guide: TI encoded speech for its customers. This section gives what someone needs to write new speech data that the chip plays as intended, using only this datasheet. [§12.1](#121-the-bit-budget) to [§12.5](#125-choosing-the-kind-of-each-frame) say what the chip does with the data and what follows from it for an author. [§12.6](#126-an-encoding-method) gives an encoding method. [§12.7](#127-vsm-images) to [§12.9](#129-speak-external-streams) cover packaging: VSM images, word tables and Speak External streams. [§12.10](#1210-how-the-original-data-was-encoded) describes how the original data was encoded, from every word of the eight PROMs in the data set (`investigation/tools/promstats.py`, `investigation/captures/phase11/promstats/`).

## 12.1 The bit budget

A frame lasts 25 ms (200 samples at 8 kHz; [§2](clock.md#2-system-clock)), whatever its kind. Its length in bits depends on its kind ([§8.1](coded-parameters.md#81-coded-speech-parameters)):

| Frame | Bits | Bit rate if every frame were this kind | Carries |
|---|---|---|---|
| Voiced | 50 | 2000 bit/s | energy, pitch, K1–K10 |
| Unvoiced | 29 | 1160 bit/s | energy, pitch 0, K1–K4 (K5–K10 are set to zero, [§8.7](excitation-interpolation.md#87-interpolation)) |
| Repeat | 11 | 440 bit/s | energy and pitch; the K indices stay as they were |
| Silence | 4 | 160 bit/s | energy 0; pitch and K indices stay as they were |
| Stop | 4 | — | the end of the utterance |

The original PROMs average 1372 bit/s (1054–1695 per PROM; [§12.10](#1210-how-the-original-data-was-encoded)). A 16 KB VSM bank holds about 95 seconds at that rate, less its word table.

A frame's bits are packed with no gaps: the next frame starts at the bit after the previous one ends. Each field is written most significant bit first, and the bit stream is packed into bytes least significant bit first ([§8.1](coded-parameters.md#81-coded-speech-parameters), [§6](commands.md#6-description-of-commands)). After the stop code, the rest of its byte is padding, which the chip never reads ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)). In the Acorn and TI images every word's padding is all 1s, as if more stop codes followed (`investigation/captures/phase11/roundtrip/`); 0s work as well. An utterance therefore starts on a byte boundary, so that a VSM address or a Speak External stream can point at it.

## 12.2 When a frame is heard

The output is not a sequence of fixed frames: each parameter moves from one frame's value to the next during the frame ([§8.7](excitation-interpolation.md#87-interpolation)). Counting from the sample period in which a frame is read (sample 0, [§8.5](frame-timing.md#85-timing-of-a-frame)):

- The frame becomes the interpolation target at sample 25.
- Its parameters move towards it over the next seven interpolation periods. By the end of each one they have moved 0.125, 0.234, 0.33, 0.498, 0.623, 0.812 and 0.906 of the way. These follow from the shifts of [§8.7](excitation-interpolation.md#87-interpolation): 3, 3, 3, 2, 2, 1, 1.
- They reach it in IP 0, samples 201–225 (each parameter in its own PC).

So each frame is the *end point* of a 25 ms glide, reached about 200 samples after it is read. The first frame of an utterance is the exception: after the start of speech, interpolation is inhibited, so its values arrive at once in IP 0 ([§8.7](excitation-interpolation.md#87-interpolation)). An encoder should measure each frame at the time its values are reached. Counted from the sample period of the first frame read ([§8.5](frame-timing.md#85-timing-of-a-frame)), frame *j* (from 0) reaches its values at about sample 225 + 200·*j*, and the sound starts at about sample 206. So frame *j* describes the sound about 25·*j* ms after the start of the sound, where the sound starts with frame 0's values. A level or spectrum measured over a window always mixes in the glide from the frame before.

These cases are inhibited: no glide, and the change happens at once in IP 0 of the next frame ([§8.7](excitation-interpolation.md#87-interpolation)):

- voiced to unvoiced, and back;
- silence to sound;
- unvoiced to silence or stop.

A voicing change therefore takes effect in one step, where the frame's values are reached. At an inhibited change to voiced, the chirp starts from its beginning ([§8.6](excitation-interpolation.md#86-excitation)), so a voiced frame after silence always starts with a fresh pitch pulse.

**The start and end of an utterance.**

- **Start with a voiced or unvoiced frame.** Repeat and silence frames load only some indices. The rest come from whatever the chip spoke last, even another program's utterance and even after a Reset ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). A first frame that is silence also has an approximate start rule ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). All but 39 of the 1148 original words start with a voiced or unvoiced frame. Those 39 start with silence, and their start depends on what was spoken before them.
- **Ending on a voiced or silence frame fades out.** The stop frame sets the energy target to 0, and the energy glides down over the following frame.
- **Ending on an unvoiced frame does not fade.** An unvoiced frame followed by a stop frame (or by silence) is inhibited: the noise keeps its full level to the end, then stops in one step ([§8.5](frame-timing.md#85-timing-of-a-frame), [§8.7](excitation-interpolation.md#87-interpolation)). To end a hiss quietly, step its energy down over a few unvoiced repeat frames first.
- **After the stop frame, the chip is talking for one more frame.** Talk Status clears 24.87 ms after the stop frame is read ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)), so an utterance of *n* frames, the stop frame included, takes *n* × 25 ms from its first read.

**K5–K10 after an unvoiced frame.** An unvoiced frame zeroes K5–K10, and a voiced *repeat* frame that follows keeps them at zero ([§8.1](coded-parameters.md#81-coded-speech-parameters), [§8.7](excitation-interpolation.md#87-interpolation)). The first voiced frame after an unvoiced one must therefore be a full voiced frame, not a repeat.

**K5–K10 before the first voiced frame.** An unvoiced frame writes only K1–K4; it zeroes K5–K10 in use but leaves their stored indices alone ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)). Until the utterance's first full voiced frame, those indices are the previous utterance's. A silence frame ends the zeroing ([§8.7](excitation-interpolation.md#87-interpolation)), so an utterance that starts unvoiced and then has a silence frame before any voiced frame filters its decay, and whatever follows, with the previous utterance's K5–K10. Its output then depends on what was spoken before it. To avoid this, put no silence frame between an opening unvoiced sound and the first voiced frame (use an unvoiced repeat frame at a low energy instead, which keeps the zeroing), or drop a faint opening sound.

**Silence inside an utterance rings.** A silence frame sets only the energy ([§8.1](coded-parameters.md#81-coded-speech-parameters)). The filter keeps the K of the frame before, and with no excitation the lattice does not come to rest at zero: it settles into a low-order limit cycle ([§8.8](lattice-filter.md#the-lowest-order-arithmetic)) a code or two either side of it. In a 12-frame pause after unvoiced speech, chip 1's output stays between codes −2 and +1 in every frame to the end of the pause, repeating at about the pitch period of the speech before it. The DAC's largest step is the one from code −1 to 0 ([§8.2](dac-and-audio.md#82-da-conversion)), so this is louder than its codes suggest: 12 to 13 mV rms at TP2, 23 dB below the loudest frame of the utterance, a buzz through the whole pause [p12-01, p12-02]. Not every pause rings. What the filter settles into depends on the K and its state, and a two-frame pause in the same utterance settled at codes 0 and +1 [p12-01, p12-02]. The reference emulator does not reproduce this reliably, since it is the lowest-order arithmetic that [§8.8](lattice-filter.md#the-lowest-order-arithmetic) does not know: in one pause it is silent where the chip rings, and after a neutral frame it rings where the chip is silent [p12-07, p12-08]. Judge the quiet of a pause on the chip, not on the model.

**Quietening a pause.** A voiced frame at energy index 1 with every K at its table entry nearest zero (Table 8-6: K1 index 23, K2 9, K3 8, K4 6, K5 7, K6 6, K7 6, K8–K10 3) leaves the filter neutral K to hold. The silence frames after it are then exactly code 0, at the bench's noise floor: 0.7 to 0.8 mV rms at TP2, 47 dB below the loudest frame, in all three pauses tested, two of two utterances [p12-03 to p12-06, p12-08]. Put it in the second frame of the pause, not the first:

- In the first frame of the pause, the change from the sound before is a glide ([§8.7](excitation-interpolation.md#87-interpolation)). The K then move to neutral while that sound fades, which dulls its end. In the model, over the 243 recordings of the phase 11 corpus, this made the output slightly worse in the median.
- After a silence frame, the change is inhibited: the K jump while the energy is already 0, and the frame's only output is the chirp at energy index 1, within a code of zero [p12-05, p12-06].
- With no silence frame after it, the next sound would glide in from it, voiced to voiced, instead of starting at once as it does after silence.

So the pause needs at least three frames, and the neutral frame costs 46 bits: a 50-bit voiced frame in place of a 4-bit silence frame. Around the quietened pauses, takes with and without the neutral frame differ no more than two takes of the same stream do [p12-01, p12-02, p12-05, p12-06]. A repeat frame straight after such a pause would take the neutral K, so it must be sent in full; a voiced repeat after an unvoiced frame (see above) cannot be, and such a pause is better left alone.

## 12.3 Levels: energy, excitation and the filter

**The output code.** The excitation *x* ([§8.6](excitation-interpolation.md#86-excitation)) is multiplied by the energy value *E* (Table 8-4) and filtered by the lattice ([§8.8](lattice-filter.md#88-lattice-filter)). The output code is *u*₁ shifted right by 4. Over the speech range the multiplier is very nearly exact: *product* ≈ *K* × *M* / 512. So:

    code ≈ E × (x × 64 / 512) × h / 16  =  E × x × h / 128

Here *x* is in units of 1/128 and *h* is the filter's response. The excitation is one of two kinds:

- **Unvoiced:** *x* = ±64 at random. Its power is the same at every frequency, and its RMS is 64.
- **Voiced:** the chirp of Table 8-7, once per pitch period of *P* samples, with zero for the rest of the period. The chirp's energy is Σ*c*² = 65,546 per period, so its RMS is √(65,546 / *P*): 64.0 at *P* = 16, 25.6 at *P* = 100 and 20.2 at *P* = 160. At the same energy index, a low voice is therefore quieter than a high one.

The chirp's spectrum is not flat. It is a single pulse whose samples are all positive, so most of its energy is at low frequencies: relative to 250 Hz it is +3.9 dB at 100 Hz, −8.8 dB at 500 Hz, −11.7 dB at 1 kHz, −17.6 dB at 2 kHz and about −20 dB at 3–4 kHz. Voiced output has this tilt as well as the filter's spectrum. A voiced frame's filter should describe the speech spectrum *divided by* this tilt. An encoder can approximate that by analysing voiced speech after first-order pre-emphasis (about *y*(*n*) = *s*(*n*) − 0.9·*s*(*n*−1)). Unvoiced excitation is flat, so unvoiced frames are analysed without pre-emphasis. The chip applies no de-emphasis of its own.

**The filter's gain.** The lattice of [§8.8](lattice-filter.md#88-lattice-filter) realises the all-pole filter 1 / *A*(*z*), with *k*ᵢ = *K*ᵢ / 512. *A*(*z*) = 1 + *a*₁*z*⁻¹ + … + *a*₁₀*z*⁻¹⁰ is built from the *k*ᵢ by the step-up recursion:

    A_0(z) = 1
    A_i(z) = A_(i−1)(z) + k_i · z^(−i) · A_(i−1)(1/z)        i = 1 … 10

That is, *a*ᵢ⁽ⁱ⁾ = *k*ᵢ and *a*ⱼ⁽ⁱ⁾ = *a*ⱼ⁽ⁱ⁻¹⁾ + *k*ᵢ·*a*ᵢ₋ⱼ⁽ⁱ⁻¹⁾. Check the sign with one stage, *u*₁ = *u*₂ − *K*₁·*b*₁, where *b*₁ is the previous output: it gives *y*(*n*) = *x*(*n*) − *k*₁·*y*(*n*−1), which is 1 / (1 + *k*₁*z*⁻¹). A strong low-frequency resonance therefore needs a large *negative* K1, which is why half of K1's table (indices 0–15) lies between −501 and −437. These are the reflection coefficients that the Levinson–Durbin recursion gives from the autocorrelation of the signal, in the convention where the prediction error is *e*(*n*) = *s*(*n*) + Σ *a*ⱼ·*s*(*n*−*j*). The filter is stable for any |*k*ᵢ| < 1, and every table entry is inside that range.

The filter's power gain can be large: with K2 = 506 (*k* = 0.988) a resonance gains about 40 dB. The level a frame produces is best found by running the frame through the reference algorithm ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)) and measuring it. The formula above is only an estimate for a first choice of energy index.

**How loud.** The DAC takes the code clipped to −128…+127 ([§8.2](dac-and-audio.md#82-da-conversion)), and the filter's 14-bit values wrap at ±8192, which is ±512 codes ([§8.8](lattice-filter.md#88-lattice-filter)). A wrap turns a loud sound into a burst of full-scale noise, and must never happen. For clean output, keep the peaks within ±127. Where that conflicts with an RMS target, the peak limit wins: speech scaled to peaks of ±120 has an RMS of about 15–20 codes. A resonant filter can make even energy index 1 too loud, so check the level of each frame by synthesis. The original data has an RMS of about 19–20 codes over its speech (the Acorn and VM PROMs) and 31 codes (Star Wars); the TI-99 data runs louder, into the DAC's clipping ([§8.2](dac-and-audio.md#82-da-conversion)).

**The energy steps.** The energy table (Table 8-4) is roughly logarithmic: about 3 dB per index from index 7 to 14. The steps are coarser at the bottom: index 1 to 2 is 6 dB, and 5 to 6 is 2.5 dB. Index 1 is 41 dB below index 14. A frame quieter than that is silence (energy 0).

## 12.4 Pitch

The pitch period is the pitch value plus one sample (Table 8-5): from 16 samples (500 Hz) at index 1 to 160 samples (50 Hz) at index 63. Indices 1–28 step one sample at a time (6 % to 2.4 %, at 16–42 samples). Above that the steps grow to about 3–4 %. Choose the index whose period is nearest the measured period in samples.

The pitch counter keeps running across voiced frames, so a change of pitch shortens or lengthens the current period only; it does not restart the chirp. Pitch glides between frames like every other parameter, except at an inhibited change ([§12.2](#122-when-a-frame-is-heard)).

## 12.5 Choosing the kind of each frame

- **Voiced** for periodic sound, **unvoiced** for noise (fricatives, aspiration, the bursts of plosives). The chip has no mixed excitation. Voiced fricatives (/v/, /z/) are coded as voiced, with the noise left out, or as unvoiced, with the voicing left out. The original data chose voiced for most.
- **Repeat** when the K indices would be the same as, or close to, the previous frame's. A repeat frame keeps the stored K indices and changes only energy and pitch, and it saves 39 bits (voiced) or 18 bits (unvoiced). The original data uses repeats for 21 % of its frames, in runs of mostly 1–3, and as much as 45 % in VM61002. A repeat frame can change voicing: an unvoiced repeat frame zeroes K5–K10, but a voiced repeat frame after an unvoiced frame does not restore them ([§12.2](#122-when-a-frame-is-heard)).
- **Silence** (energy 0) for pauses inside an utterance. Its pitch and K indices stay stored, so the frame after it may be a repeat frame. Silence to sound is an inhibited change: the sound arrives at full level in one step, and a voiced sound with a fresh pitch pulse.
- **Stop** (energy 15) once, at the end.

## 12.6 An encoding method

This is one method that follows from [§12.1](#121-the-bit-budget) to [§12.5](#125-choosing-the-kind-of-each-frame). It is not TI's, which is not published. The project's encoder implements it ([§12.11](#1211-the-projects-encoder)).

1. **Resample** the speech to the chip's sample rate: `fosc` / 80, 8000 Hz at 640 kHz. Band-limit it to below 4 kHz.
2. **Choose the frame times.** Frame *n* describes the sound at 25·*n* ms ([§12.2](#122-when-a-frame-is-heard)). Drop leading and trailing quiet; the first frame should be the first frame with sound.
3. **Analyse each frame** over a window of 25–40 ms centred on its time (a Hamming window, for example):
    - **Voicing and pitch.** Decide whether the frame is periodic, and if so its period, from 16 to 160 samples.
    - **The spectrum.** Compute the autocorrelation of the windowed signal: of the pre-emphasised signal for a voiced frame, of the plain signal for an unvoiced one ([§12.3](#123-levels-energy-excitation-and-the-filter)). Find *k*₁–*k*₁₀ by the Levinson–Durbin recursion (order 4 is enough for an unvoiced frame).
    - **The level:** the RMS of the frame's unemphasised signal over its window.
4. **Quantise.** Map each *k*ᵢ to the nearest table entry (Table 8-6). Near ±1, where the filter is sensitive, compare the entries in a warped measure such as the log-area ratio, log((1 − *k*)/(1 + *k*)), rather than in *k* itself. Map the period to the nearest pitch index.
5. **Set the energy.** Pick the energy index that gives the frame's target RMS in codes, with the quantised K and pitch. Either run the frame through the reference algorithm ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)) and measure, or use the estimate of [§12.3](#123-levels-energy-excitation-and-the-filter). Scale the whole utterance once, so that its loudest frame peaks within the DAC's ±127 ([§12.3](#123-levels-energy-excitation-and-the-filter)).
6. **Choose the frame kinds** ([§12.5](#125-choosing-the-kind-of-each-frame)). Below a level threshold, a frame is silence. Make a frame a repeat where its K indices hardly differ from those stored. Make the first frame a full frame, and put a full voiced frame after every unvoiced one. End with a stop frame, after fading the energy where the last sound is unvoiced ([§12.2](#122-when-a-frame-is-heard)).
7. **Pack** the frames into bits and bytes ([§12.1](#121-the-bit-budget)).
8. **Check by synthesis.** Run the bytes through the reference algorithm ([§8.9](reference-algorithm.md#89-reference-algorithm-and-the-model)), or through an emulator built from it, and compare with the input: the level frame by frame, and by listening. The chip's output differs from the reference algorithm by about one code at a few samples ([§8.8](lattice-filter.md#88-lattice-filter)), which is inaudible.

## 12.7 VSM images

A TMS6100 VSM, or an emulation of one, holds the data the chip reads by address ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)).

- **Byte order.** The chip reads each byte least significant bit first, so a VSM byte holds the bits of the stream in that order: the same bytes as the Speak External stream ([§12.9](#129-speak-external-streams)).
- **Words start on byte boundaries,** at any address. The chip reads exactly to the end of the stop code and no further ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)), so no padding is needed after a word. TI's own images leave one spare byte after the byte holding each stop code ([PROM corpus](../capture/prom-corpus.md)), and a VSM image may do the same. The chip does not need it.
- **Speaking a word by its address.** Load Address with five nibbles, least significant first, then Speak ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface), [§6](commands.md#6-description-of-commands)).
- **Speaking a word by its number, through a pointer table.** Load the address of the word's pointer, then Read and Branch ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)). The VSM replaces the low 14 bits of its address with the two bytes there, low byte first, and keeps the bits above. Then Speak. So a pointer can reach only its own 16 KB bank, and the bytes of a pointer are stored as the value itself, not bit-reversed. A word's data may run on past the end of the bank; how a VSM continues there is its own behaviour ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)).
- **Reading data bytes from the host.** Read Byte returns each VSM byte bit-reversed ([§6](commands.md#6-description-of-commands)). Any header or text meant for the host, such as names and counts, is therefore stored bit-reversed, so that the host reads it the right way round.

## 12.8 Word tables

Each of the following formats is used by original PROMs in the data set ([PROM corpus](../capture/prom-corpus.md)). The chip itself knows none of them: only the host does.

**Acorn PHROM** (the BBC Micro's speech system; the Acorn PHROM A, CM62024). One 16 KB bank:

| Bytes | Contents (as the host reads them through Read Byte, so stored bit-reversed) |
|---|---|
| 0, 1 | 0x00, 0xFF |
| 2–55 | Null-terminated ASCII strings, then zeros. The Acorn PHROM A has `(C)1982 Acorn`, `Word PHROM A` and `1.00` |
| 56, 57 | The number of "ASCII associated" entries (little-endian; 95 in the Acorn PHROM A) |
| 58, 59 | The number of pointers, the ASCII entries included (little-endian; 260 = 95 + 165 words) |
| 60–63 | 0x00, 0x02, 0x00, 0x02 in the Acorn PHROM A. Their use is not known ([Appendix B](gaps.md#the-acorn-phrom-format-and-the-bbc-micro)); a new image should copy them |

- **The pointer table** starts at byte 64. Word number *N* (from 32) has its pointer at byte 2*N*, as 14-bit offsets in the bank, low byte first and not bit-reversed (they are read by Read and Branch). One byte, 0xFF, follows the last pointer.
- **"ASCII associated" entries.** The first entries, numbers 32 to 126, give a word for each printable ASCII character: 'A' (65) points to the word "A", and so on. In the Acorn PHROM A, the words proper are numbered from 127, and every one of the 95 ASCII entries points to one of them (space to the first word). Every header byte is stored bit-reversed, the counts included.
- **Each word's name** is stored in the bytes just before its data, between the end of the previous word (or of the pointer table) and the word. It is stored backwards, its first character next to the data, one byte per character: the character code shifted left by one, then bit-reversed. These bytes are never spoken.
- **Speaking word *N*** from bank *B*: Load Address *B*·0x4000 + 2*N*, Read and Branch, Speak.

Whether a BBC Micro reads a new image in this format has not been tested ([Appendix B](gaps.md#the-acorn-phrom-format-and-the-bbc-micro)).

**TI indexed** (VM61002, VM71003A). Byte 0 is 0, byte 1 is the number of words *n* (stored as is, not bit-reversed), and word *i* (1 to *n*) has its pointer at byte 2*i*, low byte first. Every word ends with one spare byte after the byte that holds its stop code.

**Unindexed** (VM61004). The first word at byte 0, and each word one spare byte after the byte holding the previous word's stop code. The host must know where each word starts.

**TI-99/4A** (the CD2325A and CD2326A VSMs). A binary search tree of names, read by the host through Read Byte; see [PROM corpus](../capture/prom-corpus.md).

## 12.9 Speak External streams

The same bytes as the VSM image, from the utterance's first byte to the byte that holds its stop code, are written to the FIFO after a Speak External command ([§5](io-structure.md#5-io-structure), [§6](commands.md#6-description-of-commands)). They are not bit-reversed.

- **At least 9 bytes.** Speech starts only when the FIFO holds 9 bytes ([§5](io-structure.md#5-io-structure)). A shorter utterance never starts, and its bytes stay in the FIFO, where they turn the next reset sequence into data. Lengthen it with silence frames before the stop code, 4 bits each.
- **Keep the FIFO fed.** Write 16 bytes, then 8 more each time Buffer Low sets (INT). A frame that cannot be read in full ends the speech, like a stop frame ([§5](io-structure.md#5-io-structure)).
- **Send nothing after the stop code's byte.** Every byte written while Speak External is in progress is data, even 0xFF and Reset ([§5](io-structure.md#5-io-structure)).
- **The start.** Speech starts on the frame boundary after the 9th byte, and runs 2 samples ahead of a VSM Speak ([§8.5](frame-timing.md#85-timing-of-a-frame)).

## 12.10 How the original data was encoded

Every word of the eight PROMs in the data set, 1148 words, parsed with the frame format of [§8.1](coded-parameters.md#81-coded-speech-parameters) [`investigation/captures/phase11/promstats/`]:

| PROM | Words | Voiced | Unvoiced | Repeat | Silence | Stop | Bit rate (mean) |
|---|---|---|---|---|---|---|---|
| Acorn PHROM A | 165 | 62.9 % | 14.2 % | 12.1 % | 5.7 % | 5.1 % | 1493 bit/s |
| VM61002 | 206 | 36.5 % | 9.7 % | 44.7 % | 4.7 % | 4.4 % | 1054 bit/s |
| VM61004 | 148 | 67.0 % | 16.5 % | 8.0 % | 3.7 % | 4.7 % | 1581 bit/s |
| VM71003A | 35 | 68.7 % | 17.6 % | 2.6 % | 6.6 % | 4.6 % | 1607 bit/s |
| VM61003 (words) | 116 | 67.4 % | 19.9 % | 4.9 % | 3.6 % | 4.2 % | 1613 bit/s |
| VM61005 (words) | 116 | 72.0 % | 16.7 % | 3.4 % | 4.0 % | 3.8 % | 1662 bit/s |
| TI-99/4A | 339 | 41.7 % | 12.5 % | 35.0 % | 6.3 % | 4.5 % | 1149 bit/s |
| Star Wars | 23 | 68.1 % | 28.0 % | 0.7 % | 1.3 % | 2.0 % | 1695 bit/s |
| **All** | **1148** | **54.6 %** | **14.8 %** | **21.4 %** | **4.9 %** | **4.4 %** | **1372 bit/s** |

*Shares of frames.*

- **Length.** A word is 16 to 31 frames (10th to 90th percentile), 0.4 to 0.8 s; the median is 22 frames.
- **Start and end.**
    - 1109 words start with no silence frame. The other 39 start with one or more, and depend on what was spoken before them ([§12.2](#122-when-a-frame-is-heard)).
    - Before the stop frame, words end on a voiced frame (501), a repeat frame (290), an unvoiced frame (276) or silence (81).
    - 80 words have trailing silence frames, and 4.1 % of all frames are silence inside words.
- **Repeat frames** come in runs of 1 (1727 runs), 2 (711), 3 (318) and 4 or more (305).
- **Energy.**
    - Voiced frames cluster at indices 9–14, with a peak at 13.
    - Unvoiced frames sit lower, at indices 1–8 with a peak at 5–6; only 26 unvoiced frames are above index 9.
    - No original frame uses index 15 except as the stop code.
- **Pitch.** Indices 29 to 59 (5th to 95th percentile), a median of 44: periods of 45 to 138 samples (178 to 58 Hz), with a median of 79 samples (101 Hz). These are male voices apart from VM71003A.
- **K indices.** Voiced frames use the full range of every K except K1, which never uses index 0.

## 12.11 The project's encoder

The project's encoder, in the Speak5220 application, is an encoder and packager written from this section and the rest of this datasheet alone, without the project's model or the original PROMs. Its [documentation](../encoder/index.md) gives the method, and [its choices](../encoder/choices.md) list where it had to choose. The results below were made with its first version.

- **The round trip.** Every word of the eight PROMs (1148), parsed with the frame format of [§8.1](coded-parameters.md#81-coded-speech-parameters) and packed again, is bit-identical to the original up to the end of its stop code [`investigation/captures/phase11/roundtrip/`].
- **New speech on the chip.** 347 utterances (10.4 minutes) were encoded from new speech and hand-designed frames, and packaged as an Acorn-format PHROM and as other VSM images. The [reference emulator](../emulator/index.md)'s predictions were committed before the chip was run. Each utterance was then captured three times through each path on chip 1. The speech is within the tolerance of the capture work's acceptance tests ([capture overview](../capture/index.md), and [§8.8](lattice-filter.md#88-lattice-filter)) in every set: the emulator's median SDR is 28.4–29.7 dB, against 34.0–36.5 dB for chip 1's own takes. Every VSM take read exactly the predicted bits, and every frame read on time, apart from a known limit of the measurement in takes over 5.7 s ([new speech data](../capture/work/new-speech-data.md)) [`investigation/captures/phase11/results/`]. The hand-designed frames are quiet synthetic data, on which the one-code error of [§8.8](lattice-filter.md#88-lattice-filter) weighs as it does on the synthetic vectors.
