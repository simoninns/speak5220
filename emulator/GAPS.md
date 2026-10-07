# Datasheet gaps found while writing the emulator

The emulator in this directory was written only from
`docs-tech/tms5220_reference_datasheet/tms5220_reference_datasheet.md` (and the
original manual where the reference defers to it). This file lists what that
text still leaves open, revised against the datasheet of 2026-10-07 with the
exact reference algorithm of §8.9. In the code, open points are marked
`DATASHEET GAP:` and the unmeasured host-interface items `UNMEASURED (§3)`.
Timing choices are fields of `tms5220_config` (script operation `set`).

## 1. Remaining ambiguities in the text

Most important first.

1. **§4 vs §8.9, a stop code as the first frame.** §4: "If the first frame
   read is a stop code, Talk Status clears 0.09 ms after it is read, and the
   output never leaves its idle value." In §8.9 that frame is still loaded at
   sample 25, and "if not (energy index is 0 and loads is 1): clear Z and U" clears
   the zeroing. The chip then stays *talking* until sample 200, with the old K
   indices as targets and energy 0. Whether the output stays idle then depends
   on the inhibit rule and the old K values. It did stay idle in every case
   tried here, but the text does not say it always will. Nor does it say whether
   talking really runs on for 200 samples after TS has cleared.
   **Chosen:** §8.9 literally. TS clears 14 slots after the first bit (0.09 ms
   is 14.4 slots; the rounding is not stated).
2. **§5 vs §6, Buffer Empty when the FIFO empties exactly.** §5: "Buffer Empty
   sets when a frame read needs more bits than the FIFO holds". §6: "A Speak
   External stream whose stop code ends exactly on a byte boundary finishes
   with BE set". **Chosen:** BE also sets when a frame read leaves the FIFO
   empty.
3. **§5, the FIFO after a frame runs out.** Whether the bits a truncated frame
   used, or any left over, stay in the FIFO is not said. (§8.9 now settles that
   the frame counts as a stop code.) **Chosen:** the FIFO is emptied.
4. **§5, when FIFO bits are taken.** "at the frame's read time". **Chosen:**
   slot 17 of the frame-read sample (sample 198 of the frame clock for Speak
   External).
5. **§8.9 "Not known for this path": Reset during speech.** "the model treats it
   as the end of speech (step 3 from then on)". It is not said what happens to
   requested, Z, U, a frame half read, or writes already queued for `seen`.
   **Chosen:** talking and requested are cleared, the frame being read is
   dropped, and Z, U, idx, cur and the write queue are left as they are.
6. **Power-up noise register.** §8.9 gives the power-up state of the synthesis
    state, but the noise register is outside that loop and §7 does not cover
    power-up. **Chosen:** held at 0x1FFF, as after Reset.

## 2. Not measured yet (§3 list): current choices

The datasheet lists these as unmeasured. None of them changes the output codes
of the captures. The emulator keeps these choices:

| Item (§3 "Not measured yet") | Choice |
|---|---|
| READY latencies as whole slots | Command write 4 slots, FIFO write 3, status read 3, from the strobe slot |
| When TS sets after Speak | 2 slots after the latch |
| When the Speak's Dummy Read and each command's VSM functions occur | Latch + 2 slots (Load Address then lands 37.5 µs after the strobe, inside §4's 35–45 µs); further functions 2 slots apart |
| What releases READY after a second command while one is held | The held command running when TS clears, then the normal 4-slot latency |
| A held command when a Reset arrives | Discarded |
| Which reads return Read Byte data | Only the first read after a Read Byte, which waits for the 8th bit |
| INT for the FIFO purges of Speak External and Reset; Reset releasing INT | A purge that sets BL raises INT; Reset releases INT after its own changes |
| A 17th byte to a full FIFO | Discarded (logged `INFO fifo_overflow`) |
| When writes stop going to the FIFO after Speak External | When TS clears |
| Speak External during a VSM Speak; Speak while another waits for its boundary | Both ignored |
| Read and Branch timer start, and what the next command waits for | 48 slots from the Branch function, then the 4-slot latency. This gives about 331 µs after the next strobe, short of the measured 386–486 µs |
| Power-up VSM functions and INT | None issued; INT inactive; status 0x7F |
| Speak External counters after the utterance (§8.5 "for that utterance") | The 2-sample offset is kept until the next Speak starts. At a Speak External start, positions 198–199 (IP 7, PC 11) are skipped; at the next Speak start, positions 0–1 are repeated. Neither changes `cur`, because the IP 0 that follows sets every register |
| Speak External start lead (§5 "as for Speak") | 52 slots from both the command's latch and the 9th byte |

Two other choices of the same kind: the Reset Load Address nibble is 8 for
every Reset (§4 observed it only during speech), and only Speak and Speak
External release the noise hold (§7).

## 3. Resolved by the revised datasheet

Earlier entries now settled by the text and implemented as written:
- the Booth recoding, the exact stages 1–4 and no wrap inside the multiplier (§8.8; the worked examples are in `make test`);
- energy 0 through the multiplier, u₁₁ = +1, idle u₁ = −9;
- the noise held state (0x1FFF), the latch as reference, the release delays and the slot-0 sampling (§7). This gives 0x00EB / 0x00AF at slot 0 of sample 1, and is checked;
- the start-of-speech zeroing driving the registers to 0 in IP 0;
- indices (not values) persisting;
- stop counted as silence in all three inhibit rules, with silence and stop keeping the previous voicing;
- the pitch-counter hold through IP 0's PC 0 A and B, and the counter advancing only while talking;
- the excitation voicing from oldU of the previous sample;
- the idle zero writes one sample earlier;
- K5–K10 zeroing, with silence frames and voiced repeats;
- the end of speech (TS at slot 16 of sample 199; talking ends after sample 200);
- the Speak External 2-sample lead;
- a truncated Speak External frame acting as a stop code;
- the Speak start threshold (52 slots from the latch);
- Read and Branch byte order (§4);
- every frame load clears both Z and U ("clear Z and U", §8.9), so a silence frame ends the K5–K10 zeroing (§8.7);
- the energy target of a stop frame (Table 8-4 now gives 0 at index 15);
- where a Speak takes effect: Z, U, loads and requested at the start of the frame-read sample, talking at its end (confirmed).
