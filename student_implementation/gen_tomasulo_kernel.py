#!/usr/bin/env python3
from __future__ import annotations
import argparse, csv, json, random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "configs" / "team_seed.json"
DATASETS = ROOT / "datasets"


def load_seed() -> int:
    with CONFIG.open() as f:
        seed = json.load(f).get("assigned_seed")
    if seed is None:
        raise SystemExit("ERROR: configs/team_seed.json has no assigned_seed yet. Ask the PM for the official seed.")
    try:
        return int(seed)
    except (TypeError, ValueError):
        raise SystemExit("ERROR: assigned_seed must be an integer.")


def F(n: int) -> str:
    if not 0 <= n <= 31:
        raise ValueError(f"Register F{n} is outside the 32-register model.")
    return f"F{n}"


def generate_kernel(seed: int, terms: int = 4):
    """Generate independent squared-distance/weight terms plus a dependent reduction."""
    if not 2 <= terms <= 4:
        raise ValueError("Use 2..4 terms with the starter's 32-register budget.")
    rng = random.Random(seed)
    rows = []
    inputs = {}
    weights = {}
    next_id = 1

    # Per term: five input registers + two temporaries.
    # terms=4 uses F0..F19 for inputs, F20..F27 for temporaries, F28 for reduction.
    for t in range(terms):
        b = 5 * t
        x1, y1 = F(b), F(b + 1)
        x2, y2 = F(b + 2), F(b + 3)
        w = F(b + 4)
        dx, dy = F(20 + 2*t), F(20 + 2*t + 1)

        vals = [rng.uniform(1, 50) for _ in range(4)]
        weight = rng.uniform(0.5, 2.0)
        inputs.update(dict(zip([x1, y1, x2, y2], vals)))
        inputs[w] = weight
        weights[t] = weight

        for op, dest, src1, src2, stage in [
            ("SUB.D", dx, x2, x1, "dx"),
            ("SUB.D", dy, y2, y1, "dy"),
            ("MUL.D", dx, dx, dx, "dx_squared"),
            ("MUL.D", dy, dy, dy, "dy_squared"),
            ("ADD.D", dx, dx, dy, "distance_squared"),
            ("MUL.D", dx, dx, w, "weighted_distance"),
        ]:
            rows.append({"id": next_id, "op": op, "dest": dest, "src1": src1,
                         "src2": src2, "route_term": t, "stage": stage, "weight": weight})
            next_id += 1

    # Final reduction is intentionally dependent. First ADD combines term 0 and term 1.
    accum = F(28)
    term0 = F(20)
    for t in range(1, terms):
        term_reg = F(20 + 2*t)
        rows.append({"id": next_id, "op": "ADD.D", "dest": accum,
                     "src1": term0 if t == 1 else accum, "src2": term_reg,
                     "route_term": t, "stage": "weighted_sum", "weight": ""})
        next_id += 1
        term0 = accum

    metadata = {
        "seed": seed,
        "terms": terms,
        "instruction_count": len(rows),
        "description": "Seeded trotro route-cost kernel: independent route-distance terms followed by a dependent weighted-sum reduction.",
        "input_registers": inputs,
        "weights": weights,
        "generator_version": "week1-v1"
    }
    return rows, metadata


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--terms", type=int, default=4)
    ap.add_argument("--out", type=Path, default=DATASETS / "route_kernel.csv")
    ap.add_argument("--metadata", type=Path, default=DATASETS / "route_kernel_metadata.json")
    args = ap.parse_args()
    seed = load_seed()
    rows, meta = generate_kernel(seed, args.terms)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="") as f:
        fields = ["id", "op", "dest", "src1", "src2", "route_term", "stage", "weight"]
        w = csv.DictWriter(f, fieldnames=fields); w.writeheader(); w.writerows(rows)
    with args.metadata.open("w") as f: json.dump(meta, f, indent=2)
    print(f"Generated {len(rows)} instructions using seed {seed}.")
    print(f"Kernel: {args.out}\nMetadata: {args.metadata}")

if __name__ == "__main__":
    main()
