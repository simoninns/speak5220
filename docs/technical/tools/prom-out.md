# PROM OUT

`promout.py` reads the TMS5220's parameter ROM through its `PROM OUT` pin.

```
python investigation/tools/promout.py idle     # the stored indices' table values, from the stream as it is now
python investigation/tools/promout.py dump     # every energy, pitch and K entry, against the datasheet (about 4 min)
```

`PROM OUT` (pin 10) is the parameter ROM's serial output ([datasheet §8.10](../datasheet/prom-out.md)). It goes directly to the PicoScope's logic input D0 at TP10: an open-drain output, read at a +3 V threshold.

`dump` speaks 64 runs through Speak External, each of 25 identical frames that set every parameter to a known index. It reads the fields in mid-speech, plus energy indices 0 and 15. First it records D0's levels against threshold and where its edges fall within the slot. It writes `investigation/captures/promout/chip<N>/promout.json` (every run's 500-slot cycle and decoded fields) and `rom.csv` (each entry read against the datasheet's value). The chip number comes from `bench.json`. It exits non-zero if any entry differs.
