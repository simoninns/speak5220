# Emulator API

The emulator is in the `speak5220::core::emulator` namespace, in
`app/src/core/emulator/`, and is part of the `s5core` library. The chip is
advanced one slot at a time; see the [timing model](index.md#timing-model).
None of the classes is thread-safe: one chip is used from one thread, and
parallel work gives each job its own chip.

## The chip

`Tms5220` (`tms5220.h`) is the chip in its power-up state. It borrows a VSM
bus, a sample sink and an observer, any of which may be null, and which
must outlive it:

```cpp
using namespace speak5220::core::emulator;

Tms6100 vsm;
vsm.LoadMemory(phrom_bytes, 0x00000);
Tms5220 chip(Tms5220Config{}, &vsm, &sink, &observer);  // default gap choices
chip.SetPhase(0);                          // before the first step

chip.Step();                               // advance one slot

chip.Write(0x50);                          // start a write at the current slot
while (chip.HostBusy()) chip.Step();       // wait for READY
chip.Read();                               // status or Read Byte data
while (chip.HostBusy()) chip.Step();
int value = chip.HostValue();
```

`Write()` and `Read()` return false, and do nothing, while another host
cycle is in progress. Its other calls:

- `StatusByte()`, `IsTalking()`, `InterruptActive()`, `Code()` (the current
  sample period's `I/O` code), `Slot()` (the next slot to run),
  `NextBoundary()` (the slot of the next VSM frame-read point) and
  `LatticeU1()` (the lattice output u₁, −9 when idle);
- `AbandonHostCycle()`, for a host that gives up waiting for READY.

## Output and events

- **`ISampleSink`** (`sample_sink.h`): `OnSample(index, slot, code)`, once
  per sample period at its slot 0, with the signed 10-bit `I/O` code.
- **`ITms5220Observer`** (`tms5220_event.h`): `OnEvent(event)`, in the order
  events happen. A `Tms5220Event` is a VSM function (with its nibble or the
  bit read), a completed host cycle (strobe and READY slots, value), a
  change of TS, BL, BE or INT, or an informative milestone (`Milestone`: a
  Speak taking effect, a frame read or loaded, the end of speech, a held or
  ignored command, a Reset halting speech, a FIFO overflow). They are the
  lines of the [event log](command-line.md#events).

## The VSM

`IVsmBus` (`vsm_bus.h`) is the four VSM functions the chip issues on M0/M1
([§4](../datasheet/vsm-interface.md)): `Load(nibble)`, `Read()` (returns the
next data bit), `Dummy()` and `Branch()`. Any VSM model can be plugged in;
`Tms6100` (`tms6100.h`) is a minimal one with a 1 MB memory, and the unit
tests use a mock.

## The arithmetic

`chip_arithmetic.h` holds the chip's arithmetic as `constexpr` functions,
for the chip and its tests: `Booth()` (the [§8.8](../datasheet/lattice-filter.md)
multiplier) with `BoothDigits()` and `BoothClosedForm()`, `Asr()`,
`Wrap14()` and `NoiseStep()`. `DacCurrentMicroamps(code, load)` gives the
approximate `SPEAKER` current from the [§8.2](../datasheet/dac-and-audio.md)
bit weights (`DacLoad::k996Ohm` or `k482Ohm`).

## Configuration

All timing choices the datasheet leaves open are fields of `Tms5220Config`
(`tms5220_config.h`). Its defaults are those described in
[Implementation choices](choices.md). `SetConfigParameter(config, name,
value)` sets a field by name; in a scenario script, that is the `set`
operation ([Command line](command-line.md#scenario-scripts)).

## A host

`ChipDriver` (`chip_driver.h`) runs a chip as a host would. `Write()` and
`Read()` run a host cycle until READY goes active, `StepTo()`,
`UntilIdle(margin)` and `Boundary(lead)` advance the chip, and `Mark(name)`
reports a mark. The speak procedures are those of the command line's
[bulk mode](command-line.md#bulk-mode):

- `SpeakFromVsm(address, options)`: Load Address for `address`, then Speak,
  latched `SpeakOptions::speak_at` slots (default 1961, the standard
  procedure of [§7](../datasheet/power-up.md)) before a VSM frame-read slot;
- `SpeakExternal(stream, options)`: Speak External the same way, then the
  stream's bytes, 16 at once and 8 more whenever a status read every 1 ms
  shows Buffer Low ([§12.9](../datasheet/authoring.md));

each preceded by the reset sequence if `SpeakOptions::reset` is set, and
followed by `UntilIdle(SpeakOptions::margin)`.

## How the application speaks a phrase

`speak5220::core::synth::Synth` (`app/src/core/synth/synth.h`) speaks packed
frames from power-up on a new chip, by either path (`SynthPath::kVsm`: the
phrase at VSM address 0, as a PHROM word is spoken; or
`SynthPath::kSpeakExternal`, as the BBC BASIC export speaks it). It returns
the codes from the sample period of the first frame read until the output
has been idle again, and the same as 16-bit samples (codes × 64). Playback,
the quality rating, the waveforms and the audio exports all use it; results
are cached per phrase. `speak5220-cli synth` speaks a frames file through it
([command-line options](../../application/reference/cli-options.md)).

The encoder has its own synthesiser (`ReferenceSynthesizer`, in the same
directory), which runs §8.9 without the slot-level timing for its closed
loop. Its codes match the emulator's VSM path sample for sample from the
first frame read (`ReferenceSynthesizerTest.MatchesEmulatorVsmPath`).
