#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def parse_demo_log(path: Path):
    text = path.read_text(errors="replace")
    rows, broadcasts, issues = [], [], {}
    cycle = None
    in_rs = False
    for line in text.splitlines():
        m = re.match(r"\s*CYCLE\s+(\d+)\s*$", line)
        if m:
            cycle = int(m.group(1)); in_rs = False; continue
        if line.strip() == "[Reservation Stations]":
            in_rs = True; continue
        if line.startswith("[Common Data Bus"):
            in_rs = False; continue
        if line.startswith("[Instruction Status]"):
            continue
        if in_rs and cycle is not None and "|" in line:
            parts = [p.strip() for p in line.split("|")]
            if len(parts) >= 8 and parts[0] in {"ADD1","ADD2","ADD3","MUL1","MUL2","MUL3"}:
                tag, busy, op, vj, vk, qj, qk, rem = parts[:8]
                state = parts[8] if len(parts) > 8 else ""
                inst = ""
                rows.append({"cycle": cycle, "tag": tag, "busy": busy,
                             "op": op, "qj": qj, "qk": qk,
                             "remaining": rem, "state": state, "inst_id": inst})
        if cycle is not None and "BROADCAST -> Producer:" in line:
            m = re.search(r"Producer:\s*(\w+),\s*Value:\s*([-+0-9.eE]+)\s*\(Inst #(\d+)\)", line)
            if m:
                broadcasts.append({"cycle": cycle, "tag": m.group(1),
                                   "value": float(m.group(2)), "inst_id": int(m.group(3))})
        # Parse final summary issue/write columns if present.
        m = re.match(r"\s*(\d+)\s*\|\s*[^|]+\|\s*(\d+)\s*\|.*\|\s*(\d+)\s*$", line)
        if m and "Final Instruction Summary Table" in text[:text.find(line)] if False else False:
            pass
    return rows, broadcasts


def summarize(rows, broadcasts):
    cycles = sorted({r["cycle"] for r in rows})
    occ = []
    for c in cycles:
        busy = [r for r in rows if r["cycle"] == c and r["busy"] == "YES"]
        occ.append({"cycle": c, "busy_rs": len(busy),
                    "busy_add_rs": sum(r["busy"] == "YES" and r["tag"].startswith("ADD") for r in rows if r["cycle"] == c),
                    "busy_mul_rs": sum(r["busy"] == "YES" and r["tag"].startswith("MUL") for r in rows if r["cycle"] == c)})
    if broadcasts:
        first_issue = min(b["cycle"] for b in broadcasts)
        last_write = max(b["cycle"] for b in broadcasts)
        completed = len({b["inst_id"] for b in broadcasts})
        ipc = completed / last_write if last_write else 0.0
    else:
        ipc = 0.0
    return occ, [{"metric": "broadcast_count", "value": len(broadcasts)},
                 {"metric": "completed_instructions_seen", "value": len({b["inst_id"] for b in broadcasts})},
                 {"metric": "ipc_proxy_completed_over_last_write_cycle", "value": ipc}]


def write_csv(path, rows, fields):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("trace", type=Path)
    ap.add_argument("--out-dir", type=Path, default=ROOT / "results" / "processed")
    args = ap.parse_args()
    rows, broadcasts = parse_demo_log(args.trace)
    occ, summary = summarize(rows, broadcasts)
    write_csv(args.out_dir / "reservation_station_occupancy.csv", occ,
              ["cycle", "busy_rs", "busy_add_rs", "busy_mul_rs"])
    write_csv(args.out_dir / "cdb_broadcasts.csv", broadcasts,
              ["cycle", "tag", "value", "inst_id"])
    write_csv(args.out_dir / "ipc_summary.csv", summary, ["metric", "value"])
    print(f"Parsed {len(rows)} RS rows and {len(broadcasts)} CDB broadcasts.")
    print(f"Outputs written to {args.out_dir}")

if __name__ == "__main__": main()
