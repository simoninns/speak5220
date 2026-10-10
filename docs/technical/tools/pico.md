# Pico control

`pico.py` talks to the firmware ([Firmware protocol](../hardware/firmware-protocol.md)). It finds the Pico by its USB ID; `--port` overrides that.

```
python investigation/tools/pico.py info                            # firmware version, clock, loaded banks
python investigation/tools/pico.py reset                           # nine 0xFF, then Reset
python investigation/tools/pico.py upload 0xF image.bin            # load a VSM image into a bank ('builtin' = Acorn PHROM A)
python investigation/tools/pico.py speak 0x3CBB6 --trace out       # speak a word, save the VSM trace in out/
python investigation/tools/pico.py speakext builtin --word 0x3CBB6 # the same word through Speak External
python investigation/tools/pico.py cmd 0x10 && python investigation/tools/pico.py read  # Read Byte: a raw command, then a read
python investigation/tools/pico.py demo                            # "Acorn BBC Computer"
python investigation/tools/pico.py selftest --repeat 3             # the firmware acceptance test
```

## Self-test

`selftest` needs no scope. It does the following, and exits non-zero if any check fails:

1. Uploads the Acorn image to banks `F` and 3, and checks their CRCs.
2. Resets the chip.
3. Reads a byte with Read Byte (command `0x1x`), and checks it against the image.
4. Speaks six words from each bank and through Speak External, as many times as `--repeat` asks.

For every word it checks:

- the bits the chip read from the VSM against the image;
- that the chip read the whole word, up to its stop code;
- the final status;
- that the Speak External bytes on the bus match the stream;
- that the Speak External duration matches the VSM duration.

## The trace and the library

The trace is saved as `raw/vsm_trace.bin` (the firmware's 8-byte events, unchanged) and `vsm_trace.csv` (decoded: one row per event, times in µs). `pico.py` can also be used as a library: the `Pico` class, `decodeTrace()`, `hostCycles()` and `bitsRead()`. `Pico.time()` reads the Pico's microsecond timer. `speak()` and `speakExternal()` take an optional `atUs`, a time on that timer at which the command starts.

## Other clocks

`python investigation/tools/pico.py clock <hz>` (while the chip is idle) sets `OSC`, and `run.py` then works as at 640 kHz. The frame period is taken from the exact PWM divider, because `INFO` rounds the frequency to whole hertz. The Speak's target latch time scales with the clock, so the noise generator restarts in the same slot and `model.py` lines up as at 640 kHz. Restore 640 kHz afterwards.
