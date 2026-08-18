# starter_code/

Instructor-provided files. **Do not modify.**

## What goes here

- `demo_tomasulo.c` — complete, working 4-instruction demo (two independent
  ADD.D operations followed by a dependent MUL.D), with full cycle-by-cycle
  console trace of reservation-station state and CDB broadcasts.
- `starter_tomasulo.c` — working simulation loop, instruction fetch/issue
  dispatch, and reservation-station/register-status-table data structures,
  with TODO-marked `issue()`, `execute()`, and `write_result()`/CDB-broadcast
  functions.

## Why this stays separate

Keeping this directory untouched means the diff between what was provided
and what your team built stays visible in Git history — useful evidence
for the defence and for individual-contribution verification.

Your actual implementation (extending these files) goes in
`student_implementation/src/`.
