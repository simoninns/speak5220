# 4. Voice synthesis memory (VSM) interface

From the Pico's trace of every VSM function. The VSM here is the Pico's emulation of a TMS6100.

**The VSM holds the address, not the chip (New).** The TMS5220 has no address counter of its own. Each Load Address command is passed to the VSM at once as one Load Address function with the command's low nibble, about 35–45 µs after the write [p8-25]. The VSM shifts the nibble into its address register, least significant nibble first, as the original describes; five are needed for a full 20-bit address. With fewer, the VSM's register holds the earlier address shifted by the nibbles given, and a sixth nibble shifts the address on [p8-25]. Every read then advances the VSM's own register. So how an address carries from one bank to the next, or wraps, is the VSM's behaviour: with this emulation, reads run on from 0x3FFFF into 0x40000 [corpus `acorn_phrom_a`].

**The Dummy Read (New).** The first VSM read after a Load Address is preceded by one Dummy Read, whether that read is for Speak or for Read Byte. Further Read Bytes do not repeat it [p8-25].

**When Read Byte reads (New).** The eight bits are read two slots apart, starting at **slot 13** of a sample period (slots 13, 15, 17, 19, 1, 3, 5 and 7). Without a Dummy Read, reading starts in the sample period after the one in which the command was latched. After a Load Address, the Dummy Read comes first, 2 slots after the latch. Reading then starts at the **first slot 13 that is at least 22 slots after the Dummy Read**. In seven cases, a slot 13 that was 22 to 33 slots after the Dummy Read was used. One 21 slots after and one 14 slots after were not, and the next slot 13 was used instead [p8-25]. Four Read Bytes on one trace started in the same slot of the sample period, 149 to 241 µs after their writes. A Read Byte held during speech read its byte from slot 13 of the sample period after Talk Status cleared [p8-22]. Read and Branch does not need one: a Read Byte straight after it read the branch target's first byte with no Dummy Read [p8-25].

**Read and Branch (New).** Command `0x30` makes the VSM replace its address with the two bytes at its current address. Loading the address of word 160's entry in the Acorn PHROM A's pointer table (0x3C140) and then Read and Branch left the VSM at 0x3CBB6, the word ACORN; a Speak then spoke it [p8-25]. The bytes there are 0xB6, 0x0B: the first byte read is the low byte, and the 14-bit value 0x0BB6 replaces the low 14 bits of the address, as the original's §4 says. This is how a BBC Micro speaks a word by number.

**Reset (New).** Each Reset command issues a Load Address function and a Dummy Read, as the original's §6.6 says [p3-01]. The nibble loaded was 8 (0b1000) for a Reset written during speech [p8-22].

**Reading the speech data.**

- **No read-ahead.** A Speak reads the VSM exactly up to the last bit of the stop code and no further. For ACORN that is 1152 bits, starting with a Dummy Read at the loaded address [p3-01, p3-03].
- **From any address.** The address sweep of the Acorn PHROM A spoke from 16,144 start addresses: every byte of the image that is not a word start, except the 75 whose data runs off the end of the image without a stop code. From every one, the chip read exactly the bits of the frames up to the end of the stop code, and nothing more. In all, that is 395,398 frames and 12.2 million bits [p7-07, p7-08, corpus `acorn_phrom_a`]. Most of these starts give data no encoder would write, so the frame lengths hold for any bits:
    - energy 0 (silence) and energy 15 (stop): 4 bits;
    - repeat: 11 bits;
    - unvoiced: 29 bits;
    - voiced: 50 bits.

    The frames were 204,144 voiced, 142,816 repeat, 22,841 silence and 9,453 unvoiced.
- **Reset uses the VSM.** Every Reset command issues a Load Address and then a Dummy Read to the VSM, as the original's §6.6 says. This includes each `0xFF` byte of the initialisation sequence, which decodes as Reset (`X111XXXX`) [p3-01].
- **One frame every 25 ms.** During a Speak, the chip reads each frame's bits in a burst, and the bursts start exactly 25.000 ms apart (within 0.12 µs, at the nominal clock) [p4-01, p4-02]. This held for all 15,575 sweep starts with more than one frame, silence frames included [corpus `acorn_phrom_a`].
- **When Talk Status clears.** It clears 0.13 ms before the end of the frame that follows the stop frame: 24.87 ms after the stop frame is read. Measured from the first frame read, that is *frames* × 25 ms − 0.13 ms, counting the stop frame. Across 15,575 sweep starts it was 0.133–0.136 ms short of *frames* × 25 ms, so ACORN's 674.87 ms is 27 × 25 ms − 0.13 ms [p7-08, corpus `acorn_phrom_a`]. **If the first frame read is a stop code**, Talk Status clears 0.09 ms after it is read, and the output never leaves its idle value. This held at all 569 such starts [p7-07, corpus `acorn_phrom_a`].
- **The start waits for the frame clock.** A Speak starts on the next boundary of a frame clock that runs all the time, so the first read comes 0.3–25.3 ms after the Speak ([§7](power-up.md#7-power-up-clear-and-the-state-between-utterances)) [p5-09, p5-10]. The time from the first read to Talk Status clearing is the same for every Speak of a word: 674.87 ms, 27 frames, for ACORN [p3-01, p4-01, p4-02].
- **When a frame is read.** Each frame's first bit is read about 106 µs after `T11` rises, at the start of slot 17 of a sample period, in every frame [p5-01]. That sample period is the last of the frame's interpolation cycle (IP 7, PC 12; [§8.7](excitation-interpolation.md#87-interpolation)), and the frame starts to take effect 25 samples later.
- **The rest of the frame (New).** The bits are read two slots (12.5 µs) apart within each parameter, but each parameter is read in a later sample period, so a voiced frame takes 446 slots, about 22 sample periods. Times of each parameter's first bit, in slots from the frame's first bit, in every voiced frame of ACORN [p8-25]:

    | Parameter | Energy | Repeat and pitch | K1 | K2 | K3 | K4 | K5 | K6 | K7 | K8 | K9 | K10 |
    |---|---|---|---|---|---|---|---|---|---|---|---|---|
    | Bits | 4 | 1 + 6 | 5 | 5 | 4 | 4 | 4 | 4 | 4 | 3 | 3 | 3 |
    | First bit, slots | 0 | 34 | 78 | 118 | 160 | 200 | 240 | 280 | 320 | 362 | 402 | 442 |

    Counted from the energy's slot 17, each parameter starts about two sample periods after the one before. The parameters start in slots 17, 11, 15, 15, 17, 17, 17, 17, 17, 19, 19 and 19 of their sample periods, the last in the 23rd sample period of the frame. Shorter frames stop after the parameters they have. The bits follow one every 12.5 µs only within a parameter [p5-18, p8-25].
- **When the data must be valid.** The VSM emulation puts each data bit on `ADD8` after it sees `M0` rise. The chip still read every bit correctly with that delayed by up to 9.5 µs, but not by 10 µs [p5-18]. So the chip samples `ADD8` about 10 µs after `M0` rises, shortly before the next read. The emulation, which normally answers within about 1.5 µs, has a wide margin.
