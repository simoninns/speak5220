# 5. I/O structure

The FIFO and the status register, measured with Speak External streams of 40 identical 50-bit voiced frames, written one byte at a time with a status read after each, or N bytes at once and then left to run dry [p8-19, p8-20].

**Status register.** Bit 7 (TI's D0) is Talk Status (TS), bit 6 Buffer Low (BL) and bit 5 Buffer Empty (BE). The low five bits read as 1. After a Reset the status reads 0x7F: TS clear, BL and BE set (the FIFO is empty).

**Filling the FIFO (confirms the original).**

- After a Speak External command and one data byte, the status is 0x5F: BE clears with the first byte.
- It stays 0x5F up to the 8th byte, and becomes 0x9F with the **9th**: BL clears and TS sets at once [p8-19].
- With 8 bytes or fewer, speech never starts: TS stays clear, and the output stays idle. A Speak External stream shorter than 9 bytes therefore does nothing until more bytes arrive [p8-11, p8-19].
- Speech then starts on a frame boundary, as for Speak ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)): the first frame was taken 2.6 to 24.2 ms after the 9th byte, in 60 writes at random times [p8-21].

**Emptying it.** Bits leave the FIFO as the chip reads each frame. A byte leaves when its last bit has been read. The VSM path reads a frame's parameters over 446 slots ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)), and the Speak External flags fit the same schedule better than reading all the bits at once. With all the bits taken at the frame's read time, the flags come up to 0.5 ms early or late, when the host tests are replayed through the [reference emulator](../emulator/index.md) (`investigation/tools/replay.py`). The exact rule is not known (below).

- **Buffer Low** sets, with INT, at the frame read that leaves **8 bytes or fewer** in the FIFO. From 14 bytes, the first frame (50 bits, 6 whole bytes) left 8 and set BL; from 15 it left 9 and did not [p8-20]. This confirms the original's half-full boundary.
- **Buffer Empty** sets when a frame read needs more bits than the FIFO holds. In every stream of 9, 10, 12, 13, 14, 15 and 16 bytes it set at the first frame that could not be read in full [p8-19, p8-20].

**The times of BL, BE and TS within a frame** are not known ([Appendix B](gaps.md#speak-external-flags-within-a-frame)). Taking all of a frame's bits at its read time, and clearing TS a fixed time after the frame that runs out, puts BE and the final INT within one frame of the chip. It does not put them in the right slot. Against the chip, the final INT came 52 to 458 slots earlier than the start INT plus whole frames would give, depending on how many bytes were written [p8-19, p8-20, p8-24]. The examples below are exact to the status reads' 1 ms spacing.

**Running out (Correction).** The original says that Buffer Empty clears Talk Status, "causing speech to terminate immediately". On this chip TS stays set after BE, and clears 23–24.6 ms later, about one frame. That matches the end of a word, where TS clears 24.87 ms after the stop frame is read ([§4](vsm-interface.md#4-voice-synthesis-memory-vsm-interface)). So a frame that runs out of data ends the speech as a stop frame does. INT goes active when TS clears, not when BE sets ([§3](cpu-interface.md#3-cpu-interface)). Examples [p8-20]:

| Bytes written | BL set (INT) | BE set (status read) | TS clear (INT) |
|---|---|---|---|
| 9 | 18.4 ms | 44.3 ms | 67.9 ms |
| 13 | 20.5 ms | 68.7 ms | 93.3 ms |
| 16 | 49.8 ms | 75.6 ms | 99.0 ms |

*Times from the Speak External command. BE is the first status read that showed it; reads were about 1 ms apart.*

**Writes during Speak External are data (confirms the original).** Every byte written while a Speak External is in progress goes into the FIFO, including bytes that decode as Reset (`0x70`, `0xFF`). Neither issued the Load Address and Dummy Read that a Reset does, and the speech carried on [p8-24].

- **The reset sequence cannot end a Speak External.** Nine `0xFF` bytes and a Reset (the original's §7 initialisation sequence) written to a chip with a Speak External pending become 10 bytes of FIFO data. If the FIFO then holds 9 bytes or more, speech starts and stops at the first frame of `0xFF`, which is a stop code. So the sequence brings the chip to idle within about two frames, but not at once, and the final Reset is never executed [p8-19].
