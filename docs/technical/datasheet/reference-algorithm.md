# 8.9 Reference algorithm and the model

**New.** `investigation/tools/model.py` implements [§8.5](frame-timing.md#85-timing-of-a-frame)–[§8.8](lattice-filter.md#88-lattice-filter) and [§7](power-up.md#7-power-up-clear-and-the-state-between-utterances). Its defaults are the behaviour described here. Its options select the alternatives that were tested and rejected, so that each claim above can be checked against the captures:

    python investigation/tools/model.py investigation/captures/vectors/vectors_energy/*/vsm01   # compare takes with the model
    python investigation/tools/vecfit.py investigation/captures/vectors                          # table confirmation, every take

**How well it matches** (`investigation/captures/phase9/regress/`, `investigation/tools/regress.py`; standard takes that passed their checks, the DAC model's apart, each spoken after the takes captured before it back to the last with a voiced frame):

| Data | Takes | Takes bit-exact | Samples equal to the chip | Emulator = model |
|---|---|---|---|---|
| Unvoiced energy vectors and noise | 68 | 64 (the other 4: one sample each) | 99.998 % | all |
| Voiced energy vectors | 56 | 0 | 95.2 % | all |
| Pitch | 252 | 0 | 96.7 % | all |
| K sweeps | 1056 | 92 | 96.8 % | all |
| Interpolation | 100 | 4 | 94.0 % | all |
| Special frames | 44 | 3 | 86.0 % | all |
| Ring | 192 | 4 | 76.2 % | all |
| Random and loud | 336 | 0 | 49–54 % | all |
| Acorn PHROM A words | 1660 | 0 | 79.6 % | all |
| Acorn PHROM A address sweep | 17,184 | 60 | 45.7 % | all |

Where the model differs from the chip in voiced speech-like data, it is by one code (two in the K sweeps). Larger differences come from data that drives the filter beyond the range of speech, where a one-code difference grows ([§8.8](lattice-filter.md#88-lattice-filter)), and from the chip's own upsets ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). "Emulator = model" counts takes where the [reference emulator](../emulator/index.md) (written from this datasheet alone) gives the model's codes at every sample. The VSM path's timing matched in every VSM take: the bits read, the sample period of every frame read, and Talk Status clearing within 4.4 µs of the trace, whose times are late by up to about 1.5 µs and polled.

**Reference algorithm (exact).** This is the synthesis path, sample period by sample period, as the model runs it. Everything in it is integer arithmetic; `>>` is an arithmetic shift rounding down, and wrap14() takes a value to 14-bit two's complement.

*State.* It all persists from one utterance to the next; only the noise register is touched by Reset.

| Name | Meaning |
|---|---|
| IP, PC, A/B | The counters of [§8.5](frame-timing.md#85-timing-of-a-frame). They run all the time |
| idx[0..11] | Coded indices of the current target frame: energy, pitch, K1–K10 (the parameter RAM, [§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) |
| cur[0..11] | The interpolator's registers: energy, pitch and K1–K10 values |
| seen[0..11] | The values the filter (energy, K1–K10) and the pitch counter (pitch) use |
| queue | Pending writes to seen: (sample period due, parameter, value) |
| talking, requested | Whether the chip is speaking; whether it should go on (cleared by a stop frame) |
| Z, U | The start-of-speech zeroing (all parameters); the K5–K10 zeroing |
| loads, lastU | Frames loaded since the Speak; whether the last non-repeat frame was unvoiced |
| inhibit | Whether the change to the current target is inhibited |
| oldS, oldU | Silence and unvoiced flags, latched at each frame-read sample |
| hold, count | Pitch counter hold; pitch counter |
| noise | The 13-bit noise register ([§8.6](excitation-interpolation.md#86-excitation), [§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) |
| u[1..11], b[1..10] | The lattice ([§8.8](lattice-filter.md#88-lattice-filter)) |

*Definitions.* silence(idx) is true when idx[0] is 0 or 15; unvoiced(idx) when idx[1] is 0. target(i) is 0 when Z is set (any i), or when U is set and i ≥ 6 (K5–K10); otherwise it is the table value at idx[i] (Tables 8-4 to 8-6). The filter delay d(i) is 2 for the energy, 3 for the pitch and 1 for each K. shift(IP) is 0, 3, 3, 3, 2, 2, 1, 1 for IP 0 to 7.

*A Speak* takes effect in the first frame-read sample (PC 12 of IP 7) whose slot 17 is at least 52 slots after the latch ([§6](commands.md#6-description-of-commands), [§8.5](frame-timing.md#85-timing-of-a-frame)). At the start of that sample it sets Z and U, sets loads to 0 and sets requested. The first frame is read in that sample, and talking is set at its end (step 6). While talking and requested, a frame is read in every frame-read sample; once a stop frame has been loaded, none is.

*Loading a frame* (in PC 12 of IP 0, while talking; the frame read 25 samples earlier, or a stop code if a Speak External ran out of data):

    loads = loads + 1
    if not (energy index is 0 and loads is 1): clear Z and U
    idx[0] = energy index
    if energy index is 15: clear requested
    if the frame is not silence or stop:
        idx[1] = pitch index
        if repeat:  U = (pitch index is 0) or lastU
        else:       U = (pitch index is 0);  lastU = U
                    idx[2..5] = the frame's K1–K4;  if voiced: idx[6..11] = its K5–K10
                    (an unvoiced frame leaves idx[6..11] as they were; U zeroes them in use)
    inhibit = (oldU ≠ unvoiced(idx)) or (oldS and not silence(idx)) or (oldU and silence(idx))

*Each sample period n:*

1. Apply every queued write whose due period is n or earlier.
2. **If talking:**
    1. In PC 12 of IP 0, load a frame (above).
    2. In the A sample of PC *p* (0–11), update parameter *i* = *p*:

            now  = inhibit and IP ≠ 0
            step = 0 if now, else (target(i) − cur[i]) >> shift(IP)
            zero = Z if i ≤ 5, else U;   if i ≥ 6 and now: zero = false
            if zero and Z:  cur[i] = cur[i] + step;  value = 0      (start: the filter sees 0)
            else if zero:   cur[i] = 0;              value = 0      (K5–K10 zeroing)
            else:           cur[i] = cur[i] + step;  value = cur[i]
            queue (n + d(i), i, value)

    3. In the A sample of PC 1 of IP 0, clear hold.
    4. The excitation's voicing is oldU *as it was during sample n − 1*, before that sample's step 6. Unvoiced: −64 if bit 0 of noise is 1, else +64. Voiced: the chirp at address min(count, 51) (Table 8-7).
3. **If not talking:** in the A sample of PC *p* (0–11), write 0 to seen[*p*] at n + d(*p*) − 1, that is in this sample for a K (applied before step 4, although step 1 has already run), one sample later for the energy and two for the pitch; cur and idx are kept. The excitation's voicing is oldU as it is now.
4. **Filter.** With E = seen[0], K*j* = seen[*j* + 1] and x the excitation:

        u11 = wrap14(booth(E, 64·x))
        for i = 10 down to 1:  u_i = wrap14(u_(i+1) − booth(K_i, b_i))
        for i = 10 down to 2:  b_i = wrap14(b_(i−1) + booth(K_(i−1), u_(i−1)))
        b_1 = u_1

    booth() is the multiplier of [§8.8](lattice-filter.md#88-lattice-filter), with its first argument the coefficient.
5. **Output.** The `I/O` code of sample n is u_1 >> 4 ([§8.4](serial-output.md#84-serial-sample-output-t11-and-io)).
6. **End of the period.** If this is PC 12 of IP 7 (a frame-read sample):

        if inhibit and talking: set hold
        oldS = silence(idx);  oldU = unvoiced(idx)
        talking = requested;  a Speak latched in time starts here (above)

    Then, in every sample period (not only frame-read samples), if talking: count = count + 1, and count = 0 if count ≥ seen[1] + 1 or hold is set.

The noise register is not part of this loop: it steps once per slot, 20 times per sample period, talking or not, from its release after the command until the next Reset ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)). Step 2.4 reads it at slot 0 of the sample period.

**Power-up, and Reset during speech.** Neither has been measured ([Appendix B](gaps.md#the-state-at-power-up)). An emulator can start with every index, register and flag at 0 and the filter at its idle state. The model treats a Reset written during speech as the end of speech (step 3 from then on).
