# 6. Description of commands

Commands written with Talk Status clear behave as the original describes. A command written while a VSM Speak is talking was tested with each command type, 200 ms into the word ACORN [p8-22, p8-23]:

| Command written during speech | What the chip did |
|---|---|
| Load Address (`0x4x`) | Accepted at once, but held. It reached the VSM when speech ended, at the moment Talk Status cleared [p8-23] |
| Read Byte (`0x1x`) | Held. When speech ended the chip read the byte from the VSM; a data read then returned it [p8-22] |
| Read and Branch (`0x3x`) | Held, and executed when speech ended [p8-22] |
| Speak (`0x5x`) | Ignored. The word finished normally and no second Speak followed [p8-22] |
| Reset (`0x7x`) | Executed at once. The VSM reads stopped mid-word, Talk Status was clear at the next status read, and the Reset's Load Address and Dummy Read followed [p8-22] |
| NOP (`0x0x`, `0x2x`) | No effect [p8-22] |

A second command written while one is held keeps READY inactive ([§3](cpu-interface.md#3-cpu-interface)).

**Read Byte (`X001XXXX`).** Returns the VSM byte bit-reversed: the chip reads it LSB first, and the first bit read appears on D0, TI's most significant bit [p3-04]. Four Read Bytes in a row returned the bit-reversed image bytes 0x23, 0x9B, 0x35, 0x85 as 0xC4, 0xD9, 0xAC, 0xA1 [p8-25]. The FIFO works the other way: Speak External shifts each written byte out LSB first (original §5). So a VSM byte read with Read Byte must be bit-reversed before it can be passed to Speak External.

**Speak (`X101XXXX`) and Speak External (`X110XXXX`).**

- Talk Status is not set at once: a status read 22 µs after Speak (preceded by Load Address) still shows it clear [p3-01].
- A Speak (or Speak External) starts on the first frame boundary at least about 320 µs after it is written ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) [p5-09, p5-10, p5-12].
- Speak External starts when the 9th byte arrives ([§5](io-structure.md#5-io-structure)), on the next frame boundary [p8-21].
- Accepted at the same time before a frame boundary, Speak External's output starts 2 samples before Speak's [p5-01, p8-02].

**Buffer flags outside Speak External.** After a VSM Speak, BL and BE hold whatever the last Reset or Speak External left [p3-01, p3-02, p8-22]. A Speak External stream whose stop code ends exactly on a byte boundary finishes with BE set; its speech is nevertheless complete [p3-02].

**Reset (`X111XXXX`).** It halts speech at once and resets the noise generator's state ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)), but not the frame clock ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) or the coded parameters ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) [p5-09, p8-09]. During Speak External it is not a command ([§5](io-structure.md#5-io-structure)).
