# Synthetic vectors

The synthetic vector sets are PROM images built to exercise one part of the chip at a time. Their takes measure the parameter tables, the excitation, interpolation and the lattice filter, and test the model of [§8.9](../reference-algorithm.md#89-reference-algorithm-and-the-model). A citation of the form [vectors `<set>`] refers to a whole set; the evidence takes p8-01 to p8-18, p9-01 and p9-02 are single takes copied with their raw data. All were captured on 2026-10-07. The datasheet uses them throughout [§7](../power-up.md#7-power-up-clear-and-the-state-between-utterances) and [§8](../coded-parameters.md).

## The vector sets

`investigation/tools/vectors.py` builds eight sets of synthetic test vectors ([controlled stimulus](../../capture/stimulus.md)), each written as a synthetic PROM in `investigation/captures/phase8/vectors/` (`<set>.json`, the image `<set>.bin`, and every vector's frames in `<set>.csv`):

| Set | Vectors | What they exercise |
|---|---|---|
| `energy` | 28 | every energy index, voiced and unvoiced, K near zero |
| `pitch` | 63 | every non-zero pitch index |
| `k` | 264 | every index of every K, voiced; K1–K4 also unvoiced |
| `noise` | 3 | 1 s and 4 s of unvoiced frames |
| `interp` | 25 | jumps of each parameter; transitions between voiced, unvoiced and silent frames |
| `special` | 13 | repeat, silence and stop frames in unusual places; the shortest utterances |
| `random` | 60 | random frames |
| `loud` | 24 | loud voiced frames with random K |

Board, chip, firmware and procedure: as for the [Acorn PHROM A reference set](acorn-reference-set.md). Each vector was captured twice through each path with the standard procedure, by `run.py` (1936 takes: 480 vectors × 2 takes × 2 paths, plus 16 retakes; about 2 GB, kept local in `investigation/captures/vectors/`):

    python investigation/tools/vectors.py build all
    python investigation/tools/run.py investigation/captures/phase8/vectors/<set>.json all --takes 2 --path both --out investigation/captures/vectors --zip-only --resume

`investigation/captures/vectors/vectors_<set>/index.md` and `takes.csv` are committed, with every take's checks and hashes. `investigation/tools/vecfit.py` compares every take with the model (`investigation/captures/phase8/analysis/vectors.csv`) and measures the tables (`tables.csv`, `pitch_periods.csv`, `summary.json`).

## Evidence takes: p8-01 to p8-18

Copied with their raw data (zipped, unchanged) into `investigation/captures/phase8/evidence/<id>/`. `investigation/captures/phase8/evidence/SHA256SUMS` lists the hashes of the copied files; `investigation/captures/phase8/scripts/evidence_table.py` rebuilds this table, including the model comparison, which uses each take's predecessor. p8-13 to p8-18 come from `investigation/captures/phase8/scripts/experiments.py` (`latch_offset` and `previous`).

| ID | Take (under `investigation/captures/vectors/`) | Vector | Path | Model | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` | What it shows |
|---|---|---|---|---|---|---|---|
| p8-01 | `vectors_energy/24_u_e10/vsm01` | u_e10 | vsm | exact | `70583d98c67b17e1b5dacdccbd57127f76ed24d9d0656e796cfcbabc74465bf8` | `fb3c4b817ac23e659cfc2ca7f057e6a7cd753fe1c3d26165b9f24f1fc5a6b862` | Unvoiced, neutral K, energy index 10: the model matches bit for bit, including the noise from the state 1958 steps after 0x1FFF |
| p8-02 | `vectors_energy/24_u_e10/ext01` | u_e10 | external | exact | `1e7cc4a113d956ad76280086c5c11c23d9b5727b4be084733f541535e8c68d55` | `966a7516723bd85f2b9c4679dc6ce87d3bb1fce7217467eac356fcb809f659de` | The same through Speak External: bit for bit, from the state 1923 steps after 0x1FFF |
| p8-03 | `vectors_noise/2_noise_e10/vsm01` | noise_e10 | vsm | exact | `be01448cb7293bf8e7a43237d5765af8952887178e5167eef35b0bb6aafe25d9` | `892937541fddd91dec45796727f404bd8be8cb74d0a6be242823c5cb111c4e6e` | 1.1 s of unvoiced frames: the noise register matches over 8699 samples |
| p8-04 | `vectors_energy/10_v_e10/vsm01` | v_e10 | vsm | 41 of 1899 differ (max 1) | `2e56ef959bef2a677b5a28b114bf88b06d728a86c6aa44aea005ff5f8f535082` | `789d0c09fa90a8bef68946e09761a89636e068d4bfa7e4b7869963a13294f163` | Voiced, neutral K: within one code; one slip per pitch period |
| p8-05 | `vectors_pitch/63_p63/vsm01` | p63 | vsm | 45 of 1899 differ (max 1) | `c74cb43b7b00496da3c1206b583673b1d76569f5f1b72a388224b318de3c2305` | `e9847fafd37f6346200085373c687a99eb4eda353841ee7c4a9b20296c83699f` | Pitch index 63: period 160 samples (159 + 1); the chirp and its zero tail |
| p8-06 | `vectors_k/16_k1_15/vsm01` | k1_15 | vsm | 26 of 1899 differ (max 2) | `c3de637b300afac8e1478fec3a3c695058823e52c8321e2fd06d702fa5c39195` | `e141feacfafabf6017eec79622e46c111ec010549ee911723688f96cf2befb33` | A K1 sweep point |
| p8-07 | `vectors_interp/13_v_u_v/vsm01` | v_u_v | vsm | 49 of 2499 differ (max 10) | `5b978f570db5d45f831cfdd718110b82975e138c370d797cc6467e25654d6a12` | `1d23386c70cbc6d3081c432d88528f5dddacfcb0b1d416a4f812b6694a053895` | Voiced, unvoiced, voiced: inhibited interpolation and the excitation switch |
| p8-08 | `vectors_interp/19_s_v/vsm01` | s_v | vsm | 431 of 1899 differ (max 34) | `60cf2607031ac3007b51b4aa906ff5a7b775fd6aa034d074a81f80ea813a5d58` | `ce53a347162de0f7850203bb5ae49b83a0e0b070bdf474b2b2d85967911a6cb7` | Silence first: the zeroing lasts until the second frame, then the previous utterance's K values appear |
| p8-09 | `vectors_special/7_repeat_first/vsm01` | repeat_first | vsm | 189 of 1699 differ (max 1) | `828c05565444d3b240571af972904844f30a6b34dc157f123de805e2d64ca73c` | `b65427329fbce24cd2ce00fa42af6cb1fc4d53bb814b0847a19cf35226008d8a` | A repeat frame first: K values from the previous utterance (within 1 code with the predecessor, 520 without) |
| p8-10 | `vectors_special/8_repeat_after_unvoiced/vsm01` | repeat_after_unvoiced | vsm | 315 of 1699 differ (max 2) | `e2f42050ca9f8bea1b8bc84fa52744de87e7a0e88bcd4ca68aa20e575be1873c` | `c08190c86eac86c5abd3848e4d5e2a4efbacca6dc8b97f6117a693181b93395c` | A voiced repeat frame after unvoiced frames keeps K5–K10 at zero |
| p8-11 | `vectors_special/3_one_voiced/ext01` | one_voiced | external | chip never spoke (stream under 9 bytes) | `54f7bbfba09051e621c70c14e4888242475297f0123916d6fe5efa989e2ee59b` | `5499227dccb02146d3b341192accfb0a145b31aeec04fcca10d42869391e597b` | A 7-byte Speak External stream: the chip never starts |
| p8-12 | `vectors_random/1_r00/vsm01` | r00 | vsm | 1725 of 2699 differ (max 814) | `dfac39e53a34c062d79eaafe74c129566b3ab54a9f9c75bb5644c20902689f13` | `7f181d4c24021d8284c9f6631de4e7f6e69dcc0c606642d90238f9dddc5c681d` | Random frames: loud data, where one-code differences grow |
| p8-13 | `experiments/latch_offset/vsm01` | u_e14, latch_offset 0 | vsm | 1 of 1899 differ (max 1) | `f53449c5a940b959735e89e4e11884add99c16a1df14834a1b759eb7b6cc51d7` | `3577a1f29d116e29432225251dc621ff8204accc34150277442c79f1a10e8b8e` | Latch at the standard time: noise state 1958 steps (one end-of-speech sample off) |
| p8-14 | `experiments/latch_offset/vsm02` | u_e14, latch_offset 1 | vsm | exact | `1e4ae2ef8364a5df7dbc30a17f9cbc254a4aaa08b58c83634064e04e129210c6` | `feffe6324ce50ba691da22d4bb0b8fea5a70a3fa65659f2f01fd2a3125db4030` | Latch one slot later: noise state one step earlier, bit for bit |
| p8-15 | `experiments/latch_offset/vsm03` | u_e14, latch_offset -1 | vsm | 1 of 1899 differ (max 1) | `c286f037437b8870db9b989f71efbdd77089458778d5fce48a4c3e54635c53a0` | `b7a70b926df08027f194a7e8abae697364f97e204ec7908d538dfcecdd862d4e` | Latch one slot earlier: noise state one step later (one end-of-speech sample off) |
| p8-16 | `experiments/latch_offset/ext02` | u_e14, latch_offset 1 | external | exact | `9527fe2177291814edf94db193ded0eeb7749320d9c3ce046e0a9e0dd2d0165c` | `876ceaa25395ef842fba68e3395f2393889c6280670cc648cf945a522443ca13` | Speak External, latch one slot later: one step earlier, bit for bit |
| p8-17 | `experiments/previous_utterance/vsm02` | loud03, previous_utterance - | vsm | 472 of 1499 differ (max 16) | `de94051caaf1e178e5df8b2d83b38a7d5cafbabef123a39037083264ff74b488` | `8ff1dddc00789305cf5eebb25cb5959fb114f3d9348deb5dbfe896475d237b52` | A loud vector after a take of itself: one of five upset variants |
| p8-18 | `experiments/previous_utterance/vsm05` | loud03, previous_utterance loud10 | vsm | 562 of 1499 differ (max 17) | `514fb4c369da15e6409e9c8fc56fd1c9328869b171dd1051f0f5c24559b8a0ba` | `33c5f66cfed920d92d68ea4b6aaa1d9da0a65a83503e4f8db7d9345ef8ef8440` | The same after another vector: the start is the same; it is another upset variant |

## The ring vectors

One more synthetic vector set, `ring` (`investigation/tools/vectors.py`), is built to expose the filter's lowest-order arithmetic: 48 vectors, each held for six voiced frames at the longest pitch (160 samples), so that every pitch period ends in about 140 samples of free ringing. Board, chip, firmware and procedure: as for the other vector sets.

| Vectors | Count | What they hold |
|---|---|---|
| `n_e01` … `n_e14` | 14 | Neutral K at every energy |
| `k<j>_<i>` | 20 | One K at each end of its table, the others neutral, energy index 10 |
| `res_e06_k1_<i>`, `res_e14_k1_<i>` | 14 | K2 = 506 (a resonance of radius about 0.99) with K1 at 7 values, at energy indices 6 and 14 |

Captured twice through each path with the standard procedure (192 takes, kept local in `investigation/captures/vectors/vectors_ring/`; `index.md` and `takes.csv` committed):

    python investigation/tools/vectors.py build ring
    python investigation/tools/run.py investigation/captures/phase8/vectors/ring.json all --takes 2 --path both --out investigation/captures/vectors --zip-only --resume

The four `res_e14_k1_16` takes failed only the `layer2_vs_layer3` check (the DAC model at very loud codes, [§8.2](../dac-and-audio.md#82-da-conversion)); their `I/O` codes are exact.

**Evidence takes**, copied with their raw data (zipped, unchanged) into `investigation/captures/phase9/evidence/<id>/`; `investigation/captures/phase9/evidence/SHA256SUMS` lists the hashes of the copied files.

| ID | Take (under `investigation/captures/vectors/`) | Vector | Path | Model | SHA-256 of `scope_raw.npz` | SHA-256 of `vsm_trace.bin` | What it shows |
|---|---|---|---|---|---|---|---|
| p9-01 | `vectors_ring/38_res_e06_k1_20/vsm01` | res_e06_k1_20 | vsm | 630 of 1899 differ (max 3), first at 188 | `5026c999a61e1e3167c9f92df14804f62fc16f1a70198da8c7de6aab6e60da4d` | `e42b191b7775bab4fc7164f30df9f113d58e0a2a5ce04d29eebfc4988bdd6a98` | The earliest divergence seen: 6 samples into the first chirp, while the coefficients are still arriving one per PC ([§8.8](../lattice-filter.md#88-lattice-filter)) |
| p9-02 | `vectors_ring/14_n_e14/vsm01` | n_e14 | vsm | 271 of 1899 differ (max 1), first at 220 | `2b5f073a2ec965ccd84e7581556afd0fa35d95a736ad81f9de2d69c990202f37` | `93abf637ad43540a396091bb7ea70c042c363d02d806f09f5974f09b0901cdc5` | Neutral K, energy index 14: in the 140-sample tails the chip and the model settle into different low-order limit cycles |
