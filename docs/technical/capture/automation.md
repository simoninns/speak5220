# Automation

The host drives the Pico and the PicoScope together, so one command turns a request ("speak word N of PROM X") into a complete capture directory ([data layout](data-layout.md)) with no manual steps.

## Architecture

```
Host (Python, investigation/tools/pico.py and investigation/tools/capture.py)
 ├── Pico 2 over USB CDC: a line-based command protocol (firmware protocol)
 │     UPLOAD <bank> …  load a PHROM/VSM image into an SRAM bank (16 KB each, 4 at once)
 │     SPEAK <addr>     speak from the VSM, with SCOPE_TRIG marking start and end
 │     SPEAKEXT …       Speak External (see stimulus), the FIFO topped up on INT
 │     CMD / READ       raw command and FIFO writes, status and data reads
 │     RESET, CLOCK     the reset sequence; set or stop the OSC clock
 │     TIME, SPEAK … at_us   the Pico's timer, and Speaks timed to the chip's frame clock
 │     TRACE            download the VSM and host interface trace (layer 1)
 └── PicoScope 3405D over ps3000a: arm, wait for the trigger, save
```

The command protocol is described in [firmware protocol](../hardware/firmware-protocol.md), Speak External in [controlled stimulus](stimulus.md), and the scope in [using the PicoScope](oscilloscope.md).

## A run

A run loops over a manifest of the PHROM images and their word tables. For each word it uploads the image (once per PHROM), arms the scope, sends `SPEAK`, waits for Talk Status to clear, pulls the traces, writes the capture directory, and runs sanity checks: the layer 2 and layer 3 data agree, and the bit count matches the parsed frame count. Each word is spoken several times (at least 5 in the reference sets) to confirm the chip is deterministic. Any difference between repeats is itself a finding, for example an uninitialised state or the noise generator's state carried over between utterances.

`investigation/tools/run.py` is this runner ([host tooling](work/host-tooling.md)). The manifest is `investigation/proms/<prom>.json`. After each take it runs `investigation/tools/process.py`, which writes the readable files and the checks; after each word, `investigation/tools/compare.py`; and at the end, `investigation/tools/promindex.py`, which rewrites `index.md`. `capture.py --speak` remains for one-off captures outside a PROM. The [tools](../tools/index.md) pages give the command lines and the checks.

## The standard procedure

Every take uses the standard procedure (`investigation/tools/frameclock.py`, [repeatability](work/repeatability.md)). At the start of a run the runner locates the chip's frame clock on the Pico's timer and calibrates the start of each path. Then, for every take, it sends the reset sequence, and times the Speak so that the chip accepts it 12256.1 µs before a frame boundary. That fixes the start delay and the noise generator's state, so repeats should match ([datasheet §7](../datasheet/power-up.md)).
