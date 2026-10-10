# Emulator: implementation choices

The emulator was written only from the [reference datasheet](../datasheet/index.md)
(and the original manual where the reference defers to it), with the exact
reference algorithm of [§8.9](../datasheet/reference-algorithm.md). This page
lists the points where that text left the emulator a choice, and the choice
made. What is not known about the chip itself is listed in the datasheet's
[Appendix B: Known gaps](../datasheet/gaps.md); this page gives only the
emulator's side. In the code, open points are marked `DATASHEET GAP:` and the unmeasured
host-interface items `UNMEASURED (§3)`. Timing choices are fields of
`tms5220_config` (script operation `set`, see
[Command line](command-line.md#scenario-scripts)).

## Ambiguities in the text

Most important first.

1. **[§4](../datasheet/vsm-interface.md) vs §8.9, a stop code as the first
   frame.** §4: "If the first frame read is a stop code, Talk Status clears
   0.09 ms after it is read, and the output never leaves its idle value." In
   §8.9 that frame is still loaded at sample 25, and "if not (energy index is
   0 and loads is 1): clear Z and U" clears the zeroing. The chip then stays
   *talking* until sample 200, with the old K indices as targets and energy 0.
   Whether the output stays idle then depends on the inhibit rule and the old
   K values. It did stay idle in every case tried here, but the text does not
   say it always will. Nor does it say whether talking really runs on for 200
   samples after TS has cleared.
   **Chosen:** §8.9 literally. TS clears 14 slots after the first bit
   (0.09 ms is 14.4 slots; the rounding is not stated).
2. **[§5](../datasheet/io-structure.md) vs [§6](../datasheet/commands.md),
   Buffer Empty when the FIFO empties exactly.** §5: "Buffer Empty sets when a
   frame read needs more bits than the FIFO holds". §6: "A Speak External
   stream whose stop code ends exactly on a byte boundary finishes with BE
   set". **Chosen:** BE also sets when a frame read leaves the FIFO empty.
3. **§5, the FIFO after a frame runs out.** Whether the bits a truncated
   frame used, or any left over, stay in the FIFO is not said. (§8.9 settles
   that the frame counts as a stop code.) **Chosen:** the FIFO is emptied.
4. **§5, when FIFO bits are taken, and the times of BL, BE and TS within a
   frame.** §5 says the Speak External flags fit the VSM path's 446-slot
   parameter schedule (§4) better than taking all of a frame's bits at once,
   and that with all the bits at once BE and the final INT are within a frame
   of the chip but not in the right slot. The rule is a
   [known gap](../datasheet/gaps.md#speak-external-flags-within-a-frame): not whether bits leave on the §4 schedule, when a byte counts as
   left within it, when BL and BE are judged, nor when TS clears after a
   frame that runs out. **Chosen:** since the text does not require a rule,
   all of a frame's bits leave at once at slot 17 of the frame-read sample
   (sample 198 of the frame clock for Speak External), BL and BE are judged
   then, and TS clears 3979 slots after that slot, as for a stop frame.
5. **§8.9, Reset during speech** (a [known gap](../datasheet/gaps.md#a-reset-during-speech)).
   "The model treats a Reset written during speech as the end of speech (step 3
   from then on)." It is not said what
   happens to requested, Z, U, a frame half read, or writes already queued
   for `seen`. **Chosen:** talking and requested are cleared, the frame being
   read is dropped, and Z, U, idx, cur and the write queue are left as they
   are.
6. **[§3](../datasheet/cpu-interface.md), a read while a Read Byte is held.**
   It returns 0xFF at once and does not use the Read Byte up, but the text
   does not say whether it otherwise acts as a status read (releasing INT).
   **Chosen:** it is a data read (`READ_DATA 0xFF`) with the status-read
   latency (`read_ready`), and INT is left as it is.
7. **§3, READY after Read and Branch.** The first slot 15 more than 59 and
   at most 63 slots after the Branch function (two cases). **Chosen:**
   `branch_ready` = 60, the first slot 15 at least 60 slots after the Branch
   (the text's "more than 59 slots"); the measured cases (Branch in slot 16:
   79 slots; slot 12: 63) hold for any value in 60–63. A write whose normal
   latency ends later than that slot gets the normal latency.
8. **§3, the data read after Read Byte.** READY goes active 4 slots after the
   eighth bit. **Chosen:** for a read whose strobe comes later, the larger of
   that and the status-read latency.
9. **Power-up noise register** (part of a [known gap](../datasheet/gaps.md#the-state-at-power-up)).
   §8.9 gives a power-up state for the
   synthesis, but the noise register is outside that loop and
   [§7](../datasheet/power-up.md) does not cover power-up. **Chosen:** held at
   0x1FFF, as after Reset.

## Host interface timing

The datasheet's [host interface gaps](../datasheet/gaps.md#host-interface-timing)
have not been measured. None of them changes the output codes of the captures.
The emulator makes these choices:

| Gap | Choice |
|---|---|
| READY latencies as whole slots | Command write 4 slots, FIFO write 3, status read 3, from the strobe slot |
| When TS sets after Speak | 2 slots after the latch |
| When the Speak's Dummy Read and each command's VSM functions occur | Latch + 2 slots (Load Address then lands 37.5 µs after the strobe, inside §4's 35–45 µs); further functions 2 slots apart |
| What releases READY after a second command while one is held | The held command running when TS clears, then the normal 4-slot latency |
| A held command when a Reset arrives | Discarded |
| Which reads return Read Byte data when it is not held | Only the first read after a Read Byte, which waits for the 8th bit (§3 settles the held case) |
| What a command written after Read Byte waits for | The 8th bit, then the 4-slot latency |
| INT for the FIFO purges of Speak External and Reset; Reset releasing INT | A purge that sets BL raises INT; Reset releases INT after its own changes |
| A 17th byte to a full FIFO | Discarded (logged `INFO fifo_overflow`) |
| When writes stop going to the FIFO after Speak External | When TS clears |
| Speak External during a VSM Speak; Speak while another waits for its boundary | Both ignored |
| The exact length of the Read and Branch wait | See item 7 above: `branch_ready` = 60 |
| Power-up VSM functions and INT | None issued; INT inactive; status 0x7F |
| Speak External counters after the utterance ([§8.5](../datasheet/frame-timing.md) "for that utterance") | The 2-sample offset is kept until the next Speak starts. At a Speak External start, positions 198–199 (IP 7, PC 11) are skipped; at the next Speak start, positions 0–1 are repeated. Neither changes `cur`, because the IP 0 that follows sets every register |
| Speak External start lead (§5 "as for Speak") | 52 slots from both the command's latch and the 9th byte |

Two other choices of the same kind: the Reset Load Address nibble is 8 for
every Reset (§4 observed it only during speech), and only Speak and Speak
External release the noise hold (§7).

## Settled by the datasheet

These points are stated in the datasheet and implemented as written:

- the Booth recoding, the exact stages 1–4 and no wrap inside the multiplier
  ([§8.8](../datasheet/lattice-filter.md); the worked examples are in
  `make test`);
- energy 0 through the multiplier, u₁₁ = +1, idle u₁ = −9;
- the noise held state (0x1FFF), the latch as reference, the release delays
  and the slot-0 sampling (§7). This gives 0x00EB / 0x00AF at slot 0 of
  sample 1, and is checked;
- the start-of-speech zeroing driving the registers to 0 in IP 0;
- indices (not values) persisting;
- stop counted as silence in all three inhibit rules, with silence and stop
  keeping the previous voicing;
- the pitch-counter hold through IP 0's PC 0 A and B and PC 1 A (three
  samples), and the counter advancing only while talking;
- the excitation voicing from oldU of the previous sample;
- the idle zero writes one sample earlier;
- K5–K10 zeroing, with silence frames and voiced repeats;
- the end of speech (TS at slot 16 of sample 199; talking ends after sample
  200);
- the Speak External 2-sample lead;
- a truncated Speak External frame acting as a stop code;
- the Speak start threshold (52 slots from the latch);
- Read and Branch byte order (§4);
- the VSM frame read: each parameter's first bit at 0, 34, 78, 118, 160, 200,
  240, 280, 320, 362, 402 and 442 slots from the frame's first bit, its bits
  two slots apart, repeat and pitch read as one 7-bit parameter, and shorter
  frames stopping after their last parameter (§4). The frame is loaded 483
  slots after its first bit, after K10's last bit (446);
- Read Byte's bits at slot 13 onwards: of the sample period after the latch;
  after a Load Address, the Dummy Read 2 slots after the latch and the bits
  from the first slot 13 at least 22 slots after it; for a held one, of the
  sample period after Talk Status cleared; the data read's READY 4 slots
  after the 8th bit (§3, §4);
- reads while a Read Byte is held return 0xFF at once and do not use it up
  (§3);
- READY after Read and Branch at slot 15, whenever the next write is made
  (§3), implemented by `branch_ready`;
- every frame load clears both Z and U ("clear Z and U", §8.9), so a silence
  frame ends the K5–K10 zeroing ([§8.7](../datasheet/excitation-interpolation.md));
- the energy target of a stop frame (Table 8-4 gives 0 at index 15);
- where a Speak takes effect: Z, U, loads and requested at the start of the
  frame-read sample, talking at its end.
