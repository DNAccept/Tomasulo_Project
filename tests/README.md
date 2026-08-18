# tests/

Hand-derived validation tests. Write these BEFORE the simulator can pass
them — they define correctness, not confirm it after the fact.

## What goes here

- `test_war_waw_hazard.c` — the WAR/WAW hand-traced example from the
  project brief: `ADD.D F2,F4,F6` followed by `MUL.D F2,F2,F8`. Must
  reproduce Tomasulo's tag-based renaming eliminating the WAR stall.
  This must match the simulator's own trace exactly before the full-kernel
  run is trusted (validation gate, §L).
- `test_cdb_simultaneous.c` — forces two functional units to complete in
  the same cycle and verifies correct CDB arbitration (no silently
  overwritten result).
- `test_vectors/` — additional hand-traced input/expected-output pairs.

## Rule

Every reservation station's tag-matching and every CDB arbitration
decision that could plausibly go wrong needs an explicit test here, not
just an anecdotal "it worked on the demo."
