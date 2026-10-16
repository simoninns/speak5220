# The Preprocess view

The **Preprocess** tab (**View > Preprocess**, Ctrl+2) prepares the
selected phrase's recording for the chip before it is encoded. It shows the
recording as preprocessed, and below it the preprocessing settings.

Preprocessing is the second of the three stages of making a phrase:

1. [**Original**](original.md): the recording as imported, never changed.
2. **Preprocess**: the original with a noise gate, a bass cut, a treble
   tilt and a de-esser applied. All are off by default, and the
   preprocessed recording is then the original itself.
3. [**Encode**](encode.md): the frames the encoder makes from the
   preprocessed recording.

The preprocessed recording is what the encoder aims at, and what the
[quality rating](encode.md#the-quality-rating) measures the encoding
against. So a preprocessing chosen for the speaker the phrases will be
played on is not counted against the encoding, and scores made with
different preprocessing do not compare. [Optimise](optimise.md) never
changes the preprocessing.

## The waveform

The waveform, titled **Preprocessed (8 kHz)**, shows and plays the
recording with the settings below it. It follows the settings as they are
moved, before the change is encoded, and keeps its zoom; a change stops it
if it is playing. Its controls (play, play start, zoom) are those of
every view, described in [Waveforms](waveforms.md).

The note beside the title names the preprocessing, for example *bass cut
200 Hz, treble tilt 0.4, de-esser 10 dB*, or says *no preprocessing: the
same as the original*.

A treble tilt can take loud sounds past full scale. The recording is then
shown and played scaled down to fit, and the note says by how much
(*scaled by −2.0 dB to fit*). Only the display and playback are scaled:
the encoder sets its level itself (**Target RMS** on the
[Encode view](encode.md#target-rms)), and a
[WAV export](export.md) of the preprocessed audio clips such samples
instead.

**Gated stretches.** With the [noise gate](#noise-gate) on, the stretches
it turns down by more than 6 dB (to half their size or less) are shaded:
dimmed and hatched. The note gives the gate's level and speed, for example
*noise gate 30 dB, 100 ms: shaded stretches are turned down*, or says
*nothing is quiet enough to gate*. Hovering over a shaded stretch says it
is gated, and the status bar adds **gated**. The shading follows the
gate's sliders as they move, so the gate can be set by eye.

## The settings

| Control | What it does |
|---|---|
| **Settings for** | **This phrase** edits the phrase's own settings; **Project defaults** edits the settings every phrase starts from. It always matches the chooser on the Encode view. See [phrase settings and project defaults](phrases.md#phrase-settings-and-project-defaults) |
| **Preset** | Sets all five preprocessing settings at once: [None, BBC Micro or Custom](#preset) |
| **Noise gate** | Silences the quiet stretches between words, such as breaths and room noise ([below](#noise-gate)) |
| **Gate speed** | How fast the noise gate closes and opens ([below](#gate-speed)) |
| **Bass cut** | Cuts the bass below a frequency ([below](#bass-cut)) |
| **Treble tilt** | Lifts the treble ([below](#treble-tilt)) |
| **De-esser** | Softens the hiss of *s* and *sh* ([below](#de-esser)) |

Each setting is a row with a slider, its value, and a **↺** button that
puts it back to its default; the noise gate, the bass cut and the
de-esser also have a check box that turns them on and off. How the rows
work, and when a change is applied, is described under
[the settings rows](phrases.md#the-settings-rows).

The preprocessing is applied in a fixed order, the usual one in a studio:
the noise gate, then the bass cut, then the treble tilt, then the
de-esser. The encoder finds the pitch and the voicing (buzz or hiss) in
the recording after the noise gate but before the other three, since a
brighter recording makes voiced sounds look like hiss; it takes
everything else, the spectra and the loudness, from the fully
preprocessed recording.

There is no normaliser or compressor. The encoder sets the loudness itself
from **Target RMS** and brings quiet words up with **Slow gain control**
(on the [Encode view](encode.md)), so a recording's own level makes no
difference, and compressing it first changes the chip's coarse energy
steps very little while making the pitch harder to follow.

### At a glance

The key is the setting's name in the [project file](../reference/project-file.md)
and in `speak5220-cli new --option` ([command-line tool](cli.md)). The
slider covers the range where useful values lie; any value in the full
range can be set in the project file, and the slider widens to show it.
Levels in dBFS are relative to a full-scale RMS (a full-scale sine is
−3 dBFS).

| Key | Setting | Type | Default | Range | Slider |
|---|---|---|---|---|---|
| `noise_gate_db` | Noise gate | Number, or `null` (off) | Unset | 6 to 80 | 12 to 60 dB, steps of 1 |
| `noise_gate_ms` | Gate speed | Number | 100 | 5 to 2000 | 20 to 500 ms, steps of 10 |
| `bass_cut_hz` | Bass cut | Number, or `null` (off) | Unset | 50 to 600 | 100 to 400 Hz, steps of 10 |
| `treble_tilt` | Treble tilt | Number | 0 (off) | 0 to 0.95 | 0 to 0.9, steps of 0.05 |
| `de_ess_db` | De-esser | Number, or `null` (off) | Unset | 1 to 30 | 3 to 18 dB, steps of 1 |

### Preset

**Preset** sets all the preprocessing settings at once:

| Preset | Noise gate | Gate speed | Bass cut | Treble tilt | De-esser | For |
|---|---|---|---|---|---|---|
| **None** | off | 100 ms | off | 0 (off) | off | The recording as it is |
| **BBC Micro** | off | 100 ms | 200 Hz | 0.4 | off | Phrases played on a BBC Micro's small speaker, balanced like Acorn's own |
| **Custom** | | | | | | Shown when the settings match no preset, for example after moving a slider. Choosing it changes nothing |

Choosing a preset replaces the five preprocessing settings; the encoder's
settings are left as they are. The gate speed does not count towards a
match while the gate is off. The preset is not stored: it is worked out
from the settings.

**Why BBC Micro.** Recordings, unlike Acorn's phrases, carry much of their
energy in the bass, which the BBC Micro's small speaker cannot play.
Acorn's own phrases, a voice that TI recorded and encoded, have about
11 dB less at 125 Hz than a natural recording of a similar voice and a
few dB more treble; the preset comes close to that balance. The phrase
then sounds louder and clearer for the same peaks: on 33 recorded
sentences, about 1.3 dB louder to the ear (A-weighted) at the default
Target RMS. The quality rating, which compares the encoding with the
preprocessed recording, is usually a little lower with it.

### Noise gate

**Noise gate** (`noise_gate_db`), off by default, silences the quiet
stretches of the recording, such as breaths, lip noise and room noise,
while leaving the speech alone. Tick its check box to turn it on; it
starts at the level its default has, or at `30 dB` when the default is
off.

The setting is a level in dB below the recording's loudest sound. The
gate measures the level at each sample as the RMS of the 20 ms around it.
Where that is more than the setting below the loudest level, the gate
closes and the sound is made silent; elsewhere it is open and the sound
passes unchanged. A lower number gates more: at `20 dB` anything under a
tenth as loud as the loudest sound is silenced; at `40 dB` only sound
under a hundredth as loud.

The gate fades between open and closed rather than switching (see
[gate speed](#gate-speed)), so it makes no clicks and does not chop words.
A breath the gate closes on is silent in the encoding too, and the
encoder gives the pause it leaves [quiet frames](../reference/encoder-behaviour.md#quiet-pauses).

Loudness alone cannot tell a breath from a soft *h*, though: if a word
loses its ending, lower the number or make the gate slower. Breaths are
often 25 to 35 dB below the loudest sound, so a breath that `30 dB`
leaves unshaded may need `25 dB` or less. Set it by eye with the
[shading](#the-waveform): low enough to cover the breaths, not so low that
it eats into the words.

The noise gate is not the [**Silence gate**](encode.md#silence-gate) of
the Encode view, which only decides which frames of the encoding are
silent and does not change the preprocessed recording.

### Gate speed

**Gate speed** (`noise_gate_ms`, `100 ms` by default) is how long the
noise gate takes to close, from open to silent, after the sound falls
under its level. It opens in a quarter of this time, finishing as the
sound rises, so an onset is never cut. A shorter time mutes more of the
gaps between words; a longer one lets soft word endings, such as a
trailing *s*, fade out naturally.

It is greyed out, and has no effect, while the noise gate is off.

### Bass cut

**Bass cut** (`bass_cut_hz`), off by default, cuts the bass below the
frequency set. Tick its check box to turn it on; it starts at the
frequency its default has, or at `200 Hz` when the default is off.

It is an order-2 Butterworth high-pass run forwards and then backwards
over the recording, so it shifts nothing in time (zero phase): the
recording is −6 dB at the frequency set and falls by 24 dB per octave
below it.

The speech then has less of its energy in the bass, where a small speaker
such as a BBC Micro's gives little back and where the chip's loudest peaks
come from, and more in the band where speech is understood. The encoded
phrase can be played louder for the same peaks.

### Treble tilt

**Treble tilt** (`treble_tilt`, `0`, shown as *off*, by default) lifts the
treble: each sample has this fraction of the sample before it taken away
(the filter 1 − μz⁻¹). At `0.4` it raises the top of the band, 4 kHz,
by about 3 dB, leaves 2 kHz nearly as it was and lowers the lowest bass by
about 4 dB; at `0.9`, it raises 4 kHz by about 6 dB and lowers the bass by
about 20 dB. With the [bass cut](#bass-cut) it makes the recording brighter.

### De-esser

**De-esser** (`de_ess_db`), off by default, softens the hiss of *s* and
*sh* sounds, which the chip can make harsh. Tick its check box to turn it
on; it starts at the level its default has, or at `10 dB` when the
default is off.

It is a split-band de-esser, as used in broadcast and recording studios:

1. The treble band, above 2.5 kHz (where an *s*'s hiss is in an 8 kHz
   recording), is split off with a zero-phase order-2 Butterworth
   high-pass.
2. Every 10 ms, the band's share of the sound is measured: its RMS over
   the RMS of the whole, in dB.
3. The band is turned down by that share plus the setting, between none
   and the whole setting. So where the sound is all hiss (a share of
   0 dB), the band is turned down by the whole setting; where the band is
   the setting's dB or more below the whole, as in a vowel, it is not
   turned down at all.
4. The band is added back, turned down, to the rest of the sound, which
   is left as it is.

So a larger number both catches more and turns down further. Around
`6 dB` takes the edge off; `15 dB` and more makes an *s* noticeably
duller. The gain changes by at most 1 dB per millisecond, and starts
before the hiss, so it makes no clicks. It comes after the treble tilt,
so it also tames the hiss the tilt brightens.

## Exporting the preprocessed recording

The **WAV audio** export can write each phrase's preprocessed recording
(**Audio: Preprocessed**), as a 24-bit, 8 kHz WAV file. See
[Export](export.md).
