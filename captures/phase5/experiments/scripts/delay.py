# VSM data timing margin: delay each ADD8 data bit (VSMDELAY) and check the chip still reads the word
import os
import sys, json
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools"))
import numpy as np, pico as P
rows = []
with P.Pico() as p:
    p.reset()
    for us in [float(x) for x in sys.argv[1].split(",")]:
        p.command(f"VSMDELAY {int(us * 144)}")
        p.traceClear()
        row = {"delay_us": us}
        try:
            r = p.speak(0x3CBB6, 3000)
            t = P.decodeTrace(p.trace()[0], P.TRACE_CLOCK_HZ)
            reads = t[t["type"] == P.EV_READ]
            row.update({"bits_read": int(len(reads)), "talk_us": r["us"],
                        "read_period_us": float(np.median(np.diff(reads["time_s"])) * 1e6)})
        except P.PicoError as e:
            row["error"] = str(e)
            p.command("VSMDELAY 0"); p.reset()
        print(row, flush=True); rows.append(row)
    p.command("VSMDELAY 0"); p.reset()
json.dump(rows, open("vsm_delay.json", "w"), indent=1)
