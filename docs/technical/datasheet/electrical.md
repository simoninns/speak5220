# 9. Electrical characteristics

The original's ratings and DC characteristics (§9.1–§9.4) are not repeated here.

## 9.5 Measured output signals

**New.** Measured on one chip, at room temperature, with `VSS` = +5 V and `VDD` = −5 V. Each output drives only a 10 MΩ ×10 probe, apart from ROMCLK, which also drives the VSM interface on the test board [p1-01, p1-03].

| Pin | Low level | High level | Rise (10–90 %) | Fall (90–10 %) |
|---|---|---|---|---|
| ROMCLK (3) | −2.7 V | +4.6 V | 36 ns | 173 ns |
| `T11` (7) | −3.0 V | +4.5 V | 73 ns | 480–510 ns |
| `I/O` (9) | −2.3 V | +4.5 V | 114 ns | 670–700 ns |

All three outputs swing from near `VSS` down to well below 0 V. Their falls are 5–7 times slower than their rises, which fits a strong pull-up to `VSS` and a weaker pull-down. Any circuit that takes `T11` or `I/O` into 0–3.3 V logic must clamp the negative swing.
