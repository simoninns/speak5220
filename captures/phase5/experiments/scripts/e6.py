# Clock stops while idle: does the chip survive, and does its frame grid freeze?
import os
import sys, time, json
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "..", "tools"))
import pico as P, frameclock as fc
F = fc.FRAME_US
stops = [float(x) for x in sys.argv[1].split(",")]
rows = []
with P.Pico() as p:
    for s in stops:
        g = fc.FrameClock.locate(p).gridUs
        f0 = p.command("CLOCK 0")[0]
        time.sleep(s)
        f1 = p.command("CLOCK 640000")[0]
        t0, t1 = int(f0["at_us"]), int(f1["at_us"])
        time.sleep(0.05)
        row = {"stop_s": s, "stopped_us": t1 - t0}
        try:
            r = p.speak(0x3D065, 3000)
            row["speak_after"] = r
            g2 = fc.FrameClock.locate(p).gridUs
            row["grid_shift_us"] = (g2 - g) % F
            row["expected_shift_us"] = (t1 - t0) % F
            row["shift_error_us"] = ((g2 - g) - (t1 - t0) + F / 2) % F - F / 2
        except P.PicoError as e:
            row["speak_error"] = str(e)
            try:
                row["reset_after"] = p.reset()
                p.speak(0x3D065, 3000)
                row["recovered_by_reset"] = True
            except P.PicoError as e2:
                row["recovered_by_reset"] = False
                row["reset_error"] = str(e2)
                print(row); rows.append(row); break
        print(row, flush=True); rows.append(row)
json.dump(rows, open("e6.json", "w"), indent=1)
