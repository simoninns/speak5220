# 3. CPU interface

Measured with the host interface tests (`investigation/tools/protocol.py`), from the Pico's trace of every host cycle, at 640 kHz. Trace times are late by up to about 1.5 µs. The same tests were replayed through the [reference emulator](../emulator/index.md) with `investigation/tools/replay.py`.

**READY (New).** The original (§3.2) says READY goes inactive 100 ns after `RS` or `WS` goes active, and active again when the transfer can complete. These are the times from the strobe to READY active again [p8-19 to p8-25]:

*Table 3-1. Host cycle times, strobe to READY active, in µs. Medians, with the range seen.*

| Cycle | Median | Range | Cycles |
|---|---|---|---|
| Status read | 14.7 | 11.5–17.4 (95 %) | 12,106 |
| Command write (Load Address, Speak, Speak External, Reset, Read Byte, Read and Branch) | 22.7 | 18.4–23.9 | 129 |
| Data write to the FIFO during Speak External | 16.6 | 14.7–20.7 | 527 |
| Data read after Read Byte | 55–145 | up to 237 | 5 |
| A command written straight after Read and Branch | 386–486 | | 2 |

- **A status read or a write** completes in 12 to 24 µs, two to four slots.
- **After Read Byte**, the data read waits until the chip has read the eight bits from the VSM, and after a Load Address also the Dummy Read: up to 237 µs [p8-25]. READY goes active 4 slots (25 µs) after the eighth bit is read. When the bits are read is given in [§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface).
- **A Read Byte held during speech** ([§6](commands.md#6-description-of-commands)). Reads made while it is held do not wait: they return 0xFF at once, and do not use it up. After Talk Status clears, the chip reads the byte. The next read then waits for the eighth bit (237 µs after Talk Status cleared, in the one case measured) and returns the byte [p8-22].
- **After Read and Branch**, the next write waits 386 to 486 µs [p8-25]. The chip does not see the VSM finish a branch: [US 4,335,277](../sources/patent-us4335277/index.md) times Read and Branch with a fixed timer (240 µs in its 10 kHz design, 48 bit times, which would be 300 µs at 640 kHz). A VSM must therefore finish the branch within that time. **When READY returns (New).** In both cases measured, READY for the next write went active at the start of **slot 15** of a sample period. It was the first slot 15 that came more than 59 slots after the Read and Branch function. The Branch fell in slot 16 in one case and slot 12 in the other, and READY came 79 and 63 slots after it. So the limit is between 60 and 63 slots, and the wait does not depend on when the next write is made [p8-25].
- **A second command while one is held.** A command that cannot run during speech is accepted at once (READY active after about 21 µs) and held until speech ends ([§6](commands.md#6-description-of-commands)). Another write while it is held keeps READY inactive. The test host gave up after 50 ms; the chip then did not drive READY active again until it had been reset [p8-23]. A host should not write a second command while one is held: it should poll Talk Status first.

**INT (Correction).** INT went active for Talk Status going clear and for Buffer Low going set, and a status read released it [p8-19, p8-20]. **It did not go active when Buffer Empty was set**, which the original's §3.3 lists as a cause: in eight Speak External streams that ran out of data, the status read 1 to 2 ms after Buffer Empty set found it set, with no INT between [p8-19, p8-20]. The INT at the end of those streams came when Talk Status cleared, a frame later ([§5](io-structure.md#5-io-structure)). The original's description matches the design in US 4,335,277, where any change of BE, BL or Talk Status raises INT, and BE resets the talk latch at once. This chip behaves differently on both counts.

**Status reads during speech.** During a VSM Speak, status reads return 0xFF or 0xDF: Buffer Low and Buffer Empty keep whatever the last Reset or Speak External left ([§6](commands.md#6-description-of-commands)) [p8-22].

**The latch (definition).** Throughout this datasheet a command is *latched* in the slot in which READY goes active for its write. The start of speech ([§6](commands.md#6-description-of-commands), [§7](power-up.md#7-power-up-clear-and-the-state-between-utterances), [§8.5](frame-timing.md#85-timing-of-a-frame)) and the noise register's release ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) are timed from it.

**Not measured.** Several details of READY, INT, Talk Status and the FIFO have not been measured. None of them changes the output of any capture in the data set. They are listed in [Appendix B](gaps.md#host-interface-timing).
