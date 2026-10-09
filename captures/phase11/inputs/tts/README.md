# Encoder test speech (Phase 11)

Rendered by `tools/ttsgen.py` from the prompts in `tools/prompts.csv` with [Piper](https://github.com/OHF-Voice/piper1-gpl) 1.4.2 and the voices below ([rhasspy/piper-voices](https://huggingface.co/rhasspy/piper-voices)), then resampled to 8000 Hz, 16-bit mono FLAC (lossless), with peaks at -3 dBFS. Piper's render is random, so these files are the frozen inputs; `index.csv` gives the SHA-256 of each file's decoded samples (16-bit little-endian).

Each voice is trained on the dataset shown, under that dataset's licence; the speech in these files is attributed to it.

| Voice | Model | Speaker | Dataset licence | Dataset |
|---|---|---|---|---|
| `alba` | `en_GB-alba-medium` |  | CC BY 4.0 | https://datashare.ed.ac.uk/handle/10283/3270 |
| `cori` | `en_GB-cori-medium` |  | Public domain (LibriVox) | https://librivox.org |
| `nem` | `en_GB-northern_english_male-medium` |  | CC BY-SA 4.0 | http://www.openslr.org/83/ |
| `sef` | `en_GB-southern_english_female-low` |  | CC BY-SA 4.0 | http://www.openslr.org/83/ |
| `vctk_lo` | `en_GB-vctk-medium` | p245 | CC BY 4.0 | https://datashare.ed.ac.uk/handle/10283/3443 |
| `vctk_hi` | `en_GB-vctk-medium` | p307 | CC BY 4.0 | https://datashare.ed.ac.uk/handle/10283/3443 |
| `joe` | `en_US-joe-medium` |  | CC0 | https://github.com/OHF-Voice/voice-datasets |
| `kristin` | `en_US-kristin-medium` |  | Public domain (LibriVox) | https://librivox.org |
| `norman` | `en_US-norman-medium` |  | Public domain (LibriVox) | https://librivox.org |
