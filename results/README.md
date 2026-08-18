# results/

## Structure

- `raw/` — unprocessed simulator output per run (one subfolder or file set
  per configuration/seed combination).
- `processed/` — summarised/aggregated data (e.g. IPC per configuration,
  ready for plotting).
- `figures/` — final plots for the report and presentation, e.g.
  `ipc_comparison.png` (Tomasulo vs. in-order) and
  `rs_occupancy_trace.png` (cycle-by-cycle occupancy window).

## Rule

Nothing in `figures/` should exist without a corresponding file in
`processed/` (and ultimately `raw/`) that a reviewer could use to
regenerate it — see the Reproducibility statement required in the report.
