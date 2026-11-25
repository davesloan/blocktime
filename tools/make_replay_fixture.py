# BlockTime
# Copyright (C) 2026 Dave Sloan
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Cuts one flight from the event log for the replay's tests (milestone 11, tests/data/README.md): the lines
# loadFlightEvents reads (30 minutes before block out to block in, other flights' lines dropped), every field kept.
# Dropped: probe and pmdg_data lines, cabin and PA messages (`INTERCOM` channels), audio links (`*_url`), and four
# telemetry lines in five, except within 2 minutes of block out, the touchdown and block in.
#
#   python tools/make_replay_fixture.py <events dir> <flight id> <out .jsonl>

import json
import pathlib
import sys
from datetime import datetime, timedelta


def parse_ts(text):
    return datetime.strptime(text.replace("Z", "+0000"), "%Y-%m-%dT%H:%M:%S.%f%z")


def main():
    events_dir, flight_id, out_path = pathlib.Path(sys.argv[1]), sys.argv[2], pathlib.Path(sys.argv[3])
    lead_in = timedelta(minutes=30)
    lines = []
    for f in sorted(events_dir.glob("events-*.jsonl")):
        for raw in f.read_text(encoding="utf-8").splitlines():
            try:
                j = json.loads(raw)
            except json.JSONDecodeError:
                continue
            lines.append((parse_ts(j["ts"]), j, raw))

    out = next(i for i, (_, j, _) in enumerate(lines)
               if j["type"] == "block_out" and j["data"].get("flight_id") == flight_id)
    end = next(i for i, (_, j, _) in enumerate(lines)
               if i > out and j["type"] == "block_in" and j["data"].get("flight_id") == flight_id)
    start = out
    while start > 0 and lines[start - 1][0] >= lines[out][0] - lead_in:
        start -= 1
    flight = lines[start:end + 1]

    keep_near = [lines[out][0], lines[end][0]]
    keep_near += [t for t, j, _ in flight if j["type"] == "touchdown"]
    near = lambda t: any(abs(t - k) <= timedelta(minutes=2) for k in keep_near)

    kept, telemetry = [], 0
    for t, j, raw in flight:
        other = j.get("data", {}).get("flight_id")
        if other and other != flight_id:
            continue
        if j["type"] == "pmdg_data" or j["type"].startswith("probe_"):
            continue
        if j["type"] == "atc_exchange":
            entry = j["data"].get("entry", {})
            if "INTERCOM" in str(entry.get("channel", "")).upper():
                continue
            for k in [k for k in entry if k.endswith("_url")]:
                del entry[k]
            raw = json.dumps(j, separators=(",", ":"), ensure_ascii=False)
        if j["type"] == "telemetry":
            telemetry += 1
            if telemetry % 5 != 1 and not near(t):
                continue
        kept.append(raw)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text("\n".join(kept) + "\n", encoding="utf-8", newline="\n")
    print(f"{len(kept)} lines of {len(flight)} written to {out_path}")


if __name__ == "__main__":
    main()
