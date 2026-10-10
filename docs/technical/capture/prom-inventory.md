# Speech PROM inventory

The PROMs of the reference data set ([PROM corpus](prom-corpus.md)). This page is written by `investigation/tools/promimport.py`, which also rebuilds every image from its pinned public source and checks it.

Each PROM has a manifest, `investigation/proms/<name>.json`: the image's hashes and source, the reference dump it matches, the other copies compared with it, the target chip and the evidence for it, and the word table read from the image itself. The images are not committed ([open questions](open-questions.md#licensing)); `python investigation/tools/promimport.py` rebuilds them in `investigation/proms/images/`, and `python investigation/tools/run.py <name> ...` captures from them.

## Held

| Manifest | Contents | Part | Target | Words | Image | Reference dump | Words parsed in each format |
|---|---|---|---|---|---|---|---|
| `investigation/proms/acorn_phrom_a.json` | Acorn PHROM A (Kenneth Kendall voice) | CM62024 | TMS5220 | 165 (+95 aliases) | 16384 bytes, dump, bank F | cm62024: matches | 5200/5220: 0 overruns, 0 without a stop code; 5100: 53 overruns, 0 without a stop code |
| `investigation/proms/vm61002.json` | TI VM61002 (US male voice, industrial vocabulary) | VM61002 | TMS5220 | 206 | 16384 bytes, dump, bank 0 | vm61002: matches | 5200/5220: 0 overruns, 0 without a stop code; 5100: 48 overruns, 0 without a stop code |
| `investigation/proms/vm61004.json` | TI VM61004 (US male voice, military vocabulary) | VM61004 | TMS5220 | 148 | 16384 bytes, dump, bank 2 | vm61004: matches | 5200/5220: 0 overruns, 0 without a stop code; 5100: 38 overruns, 0 without a stop code |
| `investigation/proms/vm71003a.json` | TI VM71003A (US female voice, talking clock) | VM71003A | TMS5220 | 35 | 4096 bytes, dump, bank 4 | vm71003a: matches | 5200/5220: 0 overruns, 0 without a stop code; 5100: 6 overruns, 0 without a stop code |
| `investigation/proms/ti99_speech.json` | TI-99/4A Solid State Speech Synthesizer VSMs (US male voice) | CD2325A (0x0000-0x3FFF) and CD2326A (0x4000-0x7FFF) | TMS5200 | 339 (+34 aliases) | 32768 bytes, dump, bank 0 | cd2325a: matches; cd2326a: matches | 5200/5220: 0 overruns, 0 without a stop code; 5100: 123 overruns, 2 without a stop code |
| `investigation/proms/starwars_speech.json` | Atari Star Wars (1983) arcade speech | speech data in sound ROM 0, 136021-107 | TMS5220 | 23 | 6323 bytes, assembled, bank A | none: assembled, see the manifest | 5200/5220: 0 overruns, 0 without a stop code; 5100: 2 overruns, 1 without a stop code |
| `investigation/proms/vm61003_words.json` | TI VM61003 words (US male voice), from Talkie | VM61003 | TMS5220 | 116 | 14492 bytes, assembled, bank 1 | none: assembled, see the manifest | 5200/5220: 0 overruns, 0 without a stop code; 5100: 20 overruns, 0 without a stop code |
| `investigation/proms/vm61005_words.json` | TI VM61005 words (US male voice), from Talkie | VM61005 | TMS5220 | 116 | 16376 bytes, assembled, bank 3 | none: assembled, see the manifest | 5200/5220: 0 overruns, 0 without a stop code; 5100: 16 overruns, 0 without a stop code |

- **Words parsed in each format:** every word read with the TMS5200/TMS5220's 6-bit pitch field and with the TMS5100's 5-bit one: how many run past the start of the next word, and how many never reach a stop code. Data written for one format misparses in the other, and a word cannot overlap the next, so one overrun rules a format out. This identifies the format from the data alone. It cannot tell the TMS5200 from the TMS5220, which share a frame format but not their parameter tables; that comes from the product the PROM was made for.
- **Image kind:** a *dump* is a copy of a real VSM, checked against its published reference hash. An *assembled* image holds original words in a layout made for this project or by its source, so only the words, and not the image's layout or read-ahead, are original.
- **Copies compared:** every source copy of each image is hashed in its manifest (`copies`). The Acorn PHROM A's three copies are identical. The eRadiofan copies of VM61002, VM61004, VM71003A and the TI-99 VSMs stop after the last word; padded with zeros they are identical to the full dumps.

## Image formats

- **`acorn`:** Acorn's PHROM format (CM62024). Bytes 0-63 are a header, read through Read Byte so bit reversed: a format byte (0) and a data flag, then null-terminated strings (`(C)1982 Acorn`, `Word PHROM A`, `1.00`), then 16-bit counts at bytes 56-59: 95 'ASCII associated' words and 260 pointers. From byte 64 comes the pointer table, little-endian, ended by 0xFF; word number N's pointer is at byte 2N, so numbers 32-126 are the ASCII aliases and 127-291 the words. Each word's name sits in the bytes between the previous word's stop code and its own data, stored backwards, one character per byte, bit reversed and shifted left by one.
- **`ti_indexed`:** TI's indexed format (VM61002, VM71003A). Byte 0 is 0, byte 1 the word count, then a little-endian 16-bit pointer per word, word n's at byte 2n. Every word ends with one spare byte after the byte holding its stop code. The assembled images (Star Wars, the Talkie words) use the same index, with the words as their sources give them.
- **`unindexed`:** No index (VM61004). The first word starts at byte 0, and each later word starts one spare byte after the byte holding the previous word's stop code.
- **`ti99`:** The TI-99/4A speech VSMs. Byte 0 is 0x55, then 373 index records, read bit reversed: a length byte, the phrase's name in ASCII, the left and right pointers of a binary search tree, a zero byte, the phrase's address (big-endian, so the byte order is the reverse of Read and Branch's) and its length in bytes. The first record is the tree's root. 34 records share their data with another name.

## Known, not held

| Data | Target | Notes |
|---|---|---|
| VM61003 (complete image) | TMS5220 | Listed as a BBC Micro speech PHROM, vm61003 (Weather), 16 KB, SHA-1 `aa3bbb28fbdb848d609ec47ef8c250d4ff0d8e53`. No public copy of the image was found; 116 of its words are held as `vm61003_words`. |
| VM61005 (complete image) | TMS5220 | Listed as a BBC Micro speech PHROM, vm61005 (Avionic), 16 KB, SHA-1 `5cf785153dcce406e9a8f5d166af50e288b33126`. No public copy of the image was found; 116 of its words are held as `vm61005_words`. |
| CM62312 (Exelvision EXL 100 system speech) | TMS5220C | Exelvision EXL 100, 16 KB, SHA-1 `03863087a071b8f22d36a52d18243f1c33e17ff7`. No public copy found. Written for the TMS5220C. |
| Speak External data in other hosts' program ROMs | TMS5220 | Atari arcade games other than Star Wars (for example The Empire Strikes Back, Gauntlet, Indiana Jones and the Temple of Doom, Paperboy, 720°), and speech cards such as the Apple II Echo II. Their LPC data is embedded in program code, so each needs its own disassembly to find the phrase boundaries. Not collected. |

## Left out

| Data | Where | Why |
|---|---|---|
| VSMs for the TMS5100/TMS5110 family (TMC0281, CD2801/CD2802, TMS5110A) | Speak & Spell, Speak & Math, Speak & Read, Language Translator, Spelling B, Tiger K28 and Bingo Bear, Eva  | Their frames have a 5-bit pitch field, so the TMS5220 misparses them from the first voiced frame. They test nothing the synthetic vectors do not test better. |
| M58817/M58819 data (Nintendo Radar Scope) | Nintendo Radar Scope | Mitsubishi's TMS5110 clone and its own speech ROM: the TMS5100 frame format. |
| Talkie `Vocab_Soundbites` | going-digital/Talkie | From a custom BBC Micro phrase ROM made by a hobbyist, not original data. |

## Sources

- `eradiofan`: <https://github.com/eRadiofan/TMS6100-Multi-PHROM-Emulator-for-the-BBC-Micro> at `b1a0bd835707` (licence: GPL-3.0)
- `tms6100emu`: <https://github.com/simoninns/TMS6100-Emulator> at `0d78f97b20be` (licence: GPL-3.0)
- `talkie`: <https://github.com/going-digital/Talkie> at `7f57628bcfa7` (licence: GPL-2.0)
- `swaudio`: <https://github.com/nmikstas/star-wars-arcade-audio> at `2e738970d94d` (licence: none stated)
- Reference hashes: published ROM hash lists, at the commit pinned in `investigation/tools/promimport.py` (hashes only)
- The Acorn PHROM A image is in this repo, `investigation/firmware/src/romdata_acorn.h`.
