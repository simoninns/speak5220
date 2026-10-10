# Host interface tests

Tests p8-19 to p8-25, made on 2026-10-07, measure READY, INT, the status register, the FIFO and the VSM functions of each command. The datasheet uses them in [§3](../cpu-interface.md#3-cpu-interface), [§4](../vsm-interface.md#4-voice-synthesis-memory-vsm-interface), [§5](../io-structure.md#5-io-structure) and [§6](../commands.md#6-description-of-commands).

## How they are recorded

The tests are in `investigation/captures/phase8/protocol/<test>/`, made by `investigation/tools/protocol.py` ([controlled stimulus](../../capture/stimulus.md)). No scope: each step's raw VSM trace (`raw/<step>.bin`, the firmware's events, unchanged) records every host cycle with READY and `INT`, and every VSM function. `steps.json` gives each step's notes, `result.json` the decoded cycles, INT edges and VSM functions (`python investigation/tools/protocol.py <test> --analyse`). `investigation/captures/phase8/protocol/SHA256SUMS` lists the raw files' hashes. Trace times are late by up to about 1.5 µs ([firmware protocol](../../hardware/firmware-protocol.md)).

## The tests

| ID | Test | What it does |
|---|---|---|
| `p8-19` | `fifo_start` | Speak External with 8, 9, 12 and 16 bytes, one at a time with a status read after each, then left to run dry |
| `p8-20` | `starve` | Speak External with 9, 10, 13, 14, 15 and 16 bytes at once, then status reads every 1 ms |
| `p8-21` | `external_start` | 60 Speak Externals with the 9th byte written at a random time |
| `p8-22` | `commands_during_speech` | Load Address, Read Byte, Read and Branch, Speak, Reset and NOP written 200 ms into ACORN |
| `p8-23` | `held_command` | Two Load Address writes during speech, traced to the end of the word |
| `p8-24` | `reset_in_external` | `0x70` and `0xFF` written during Speak External |
| `p8-25` | `vsm_commands` | Load Address with 1 to 6 nibbles, repeated Read Bytes, Read and Branch, and Speak after Read and Branch |

The `load_address` step of `p8-22` left a command held when the test host gave up on READY; the next step needed retried resets (recorded in its notes).

The same tests are replayed through the [reference emulator](../../emulator/index.md) by `investigation/tools/replay.py`, which compares its host-visible behaviour with the traces.
