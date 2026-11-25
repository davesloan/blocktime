# BlockTime
# Copyright (C) 2026 Dave Sloan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Makes tests/data/flight-klas-ksan/events-2026-11-24.jsonl, the debrief's real-flight fixture (milestone 10), from
# Dave's event log of 2026-11-24 (recorder 0.10.0, run with --probe). The flight is turned into 0.11.0's format:
# - `cockpit` events are derived from the probe's PMDG 737 lines (seat belts, landing lights, gear, VREF, landing
#   flaps, CMD A/B), on change, as the recorder writes them since D-159;
# - the touchdown gains the true heading (from the telemetry) and the wind (KSAN ATIS Q, 330 at 9; see README.md);
#   pitch and bank weren't logged, so they stay out;
# - a `bounces` event with 0 (the telemetry shows no bounce);
# - probe lines, cabin and PA messages (INTERCOM channels) and audio links are dropped, and the telemetry keeps only
#   the fields the debrief reads (a telemetry line equal to the one before it is dropped).
#
# Run once: python -I tools/make_debrief_fixture.py <log dir> <output file>

import json
import sys
from datetime import datetime, timedelta

FLIGHT = "2026-11-24T23:53:16.837Z"
FILES = ["events-2026-11-24.jsonl", "events-2026-11-25.jsonl"]
TELEMETRY_KEEP = ["on_ground", "parking_brake", "alt_agl_ft", "alt_ind_ft", "vs_fpm", "gs_kt", "ias_kt",
                  "heading_true_deg", "engines", "eng_running", "beacon", "strobe", "flaps_te_pct",
                  "flaps_handle"]


def ts(line):
    return datetime.fromisoformat(line["ts"].replace("Z", "+00:00"))


def main(log_dir, out_path):
    lines = []
    for name in FILES:
        with open(f"{log_dir}/{name}", encoding="utf-8") as f:
            lines += [json.loads(l) for l in f if l.strip()]
    out_idx = next(i for i, l in enumerate(lines)
                   if l["type"] == "block_out" and l["data"].get("flight_id") == FLIGHT)
    in_idx = next(i for i, l in enumerate(lines)
                  if l["type"] == "block_in" and l["data"].get("flight_id") == FLIGHT)
    start = ts(lines[out_idx]) - timedelta(minutes=31)
    window = [l for l in lines[:in_idx + 1] if ts(l) >= start]

    out = []
    last_cockpit = None
    last_telemetry = None
    heading = 0
    for l in window:
        t, d = l["type"], l["data"]
        if t == "telemetry":
            heading = d.get("heading_true_deg", heading)
            kept = {k: d[k] for k in TELEMETRY_KEEP if k in d}
            for k, v in kept.items():
                if isinstance(v, float):
                    kept[k] = round(v, 6 if k in ("lat", "lon") else 2)
            if kept != last_telemetry:  # the log is change-based: a line equal to the last one adds nothing
                last_telemetry = kept
                out.append({"ts": l["ts"], "type": t, "data": kept})
        elif t == "probe_pmdg_737":
            c = {
                "landing_lights": any(v == 1 for v in d["LTS_LandingLtFixedSw"]) or
                any(v == 2 for v in d["LTS_LandingLtRetractableSw"]),
                "seat_belts": d["COMM_FastenBeltsSelector"],
                "gear_down": all(v == 1 for v in d["MAIN_annunGEAR_locked"]),
                "pmdg": True,
                "vref_kt": d["FMC_LandingVREF"],
                "landing_flaps": d["FMC_LandingFlaps"],
                "autopilot": bool(d["MCP_annunCMD_A"] or d["MCP_annunCMD_B"]),
            }
            if c != last_cockpit:
                last_cockpit = c
                e = dict(c)
                if l["ts"] >= lines[out_idx]["ts"]:
                    e["flight_id"] = FLIGHT
                out.append({"ts": l["ts"], "type": "cockpit", "data": e})
        elif t.startswith("probe_"):
            continue
        elif t == "atc_exchange":
            entry = dict(d["entry"])
            if entry.get("channel", "").startswith("INTERCOM"):
                continue
            entry.pop("atc_url", None)
            entry.pop("pilot_url", None)
            out.append({"ts": l["ts"], "type": t, "data": {**d, "entry": entry}})
        elif t == "touchdown":
            td = dict(d)
            td.update({"heading_true_deg": round(heading) % 360, "wind_dir_deg": 330, "wind_kt": 9})
            out.append({"ts": l["ts"], "type": t, "data": td})
            at = datetime.fromisoformat(d["time"].replace("Z", "+00:00")) + timedelta(seconds=15)
            out.append({"ts": l["ts"], "type": "bounces", "data": {
                "flight_id": FLIGHT, "time": at.strftime("%Y-%m-%dT%H:%M:%S.") + f"{at.microsecond // 1000:03d}Z",
                "bounces": 0}})
        else:
            out.append(l)

    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        for l in out:
            f.write(json.dumps(l, separators=(",", ":"), ensure_ascii=False) + "\n")
    print(f"{len(out)} lines")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
