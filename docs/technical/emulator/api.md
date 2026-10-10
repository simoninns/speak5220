# Emulator API

The C interface is declared in `investigation/emulator/tms5220.h` (the chip) and
`investigation/emulator/tms6100.h` (the VSM). The chip is advanced one slot at a time; see
the [timing model](index.md#timing-model).

## Usage

    tms5220 chip;  tms6100 vsm;
    tms6100_init(&vsm);  tms6100_load_file(&vsm, "phrom.bin", 0x00000);
    tms5220_init(&chip, NULL);                 /* NULL: default gap choices */
    tms5220_set_vsm(&chip, tms6100_func, &vsm);
    tms5220_set_sample_cb(&chip, my_sample_fn, ctx);  /* (ctx, index, slot, code) */
    tms5220_set_event_cb(&chip, my_event_fn, ctx);    /* VSM, host, flag events */
    tms5220_set_phase(&chip, 0);               /* before the first step */

    tms5220_step(&chip);                       /* advance one slot */

    tms5220_write(&chip, 0x50);                /* start a write at the current slot */
    while (tms5220_host_busy(&chip)) tms5220_step(&chip);   /* wait for READY */
    tms5220_read(&chip);                       /* status or Read Byte data */
    while (tms5220_host_busy(&chip)) tms5220_step(&chip);
    int v = tms5220_host_value(&chip);

## Other calls

- `tms5220_status()`, `tms5220_talking()`, `tms5220_int_active()`,
  `tms5220_code()`, `tms5220_next_boundary()` (slot of the next VSM
  frame-read point).
- The arithmetic is exposed for testing: `tms5220_booth()`, `tms5220_asr()`,
  `tms5220_wrap14()`, `tms5220_noise_step()`.
- `tms5220_dac_current_ua(code, load)` gives the approximate `SPEAKER` current
  from the [§8.2](../datasheet/dac-and-audio.md) bit weights (load 0 = 996 Ω,
  1 = 482 Ω).

## The VSM

The VSM is a callback `int fn(void *ctx, int func, int nibble)` with `func`
one of `TMS5220_VSM_LOAD` (nibble), `TMS5220_VSM_READ` (returns the bit),
`TMS5220_VSM_DUMMY`, `TMS5220_VSM_BRANCH`; any VSM model can be plugged in.

## Configuration

All timing choices the datasheet leaves open are fields of `tms5220_config`.
`tms5220_default_config()` gives the defaults described in
[Implementation choices](choices.md). In a scenario script they are changed with the
`set` operation ([Command line](command-line.md#scenario-scripts)).
