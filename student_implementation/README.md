# student_implementation/

The team's actual implementation. This is where real work happens.

## What goes here

- `src/tomasulo_sim.c` — the extended simulator (built out from
  `starter_code/starter_tomasulo.c`).
- `src/reservation_station.c/.h` — reservation-station data structure
  (busy bit, operation, Qj/Qk source tags, unique producer tag).
- `src/register_status_table.c/.h` — maps architectural registers to
  producing reservation-station tags.
- `src/cdb.c/.h` — common data bus broadcast and arbitration logic.
- `src/functional_unit.c/.h` — functional-unit pipelines with configurable
  multi-cycle latency (ADD=2, MUL=6 baseline) and structural-hazard
  detection when reservation stations are full.
- `gen_tomasulo_kernel.py` — seeded route-optimisation kernel generator.
- `analyze_tomasulo_trace.py` — reservation-station occupancy and IPC
  analysis/plotting.

## Conventions

- One reservation station struct/array per functional-unit type.
- Every TODO from the starter code should be replaced with a real
  implementation, not a stub that merely avoids crashing (per the
  code-review checklist, Part V §12).
- Comment WHY, not just WHAT, for any non-obvious architectural decision.
