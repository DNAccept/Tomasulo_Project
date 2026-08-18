# configs/

All parameters needed to reproduce any reported result, per the
reproducibility requirement (System Requirements, §G).

## What goes here

- `team_seed.json` — the team's assigned seed (Part I §7 integrity
  framework). Every generated kernel and every reported result must trace
  back to this file.
- `rs_counts.json` — number of reservation stations per functional-unit
  type (must support at least 3 per type; report a structural-hazard stall
  if exceeded).
- `fu_latencies.json` — functional-unit latencies (e.g. ADD=2, MUL=6,
  per H&P's illustrative figures) as assigned to your team.

## Rule

Never hard-code seeds, RS counts, or latencies directly in source files —
everything team-specific lives here so a reviewer (or the instructor at
the defence) can swap in an unseen configuration and rerun.
