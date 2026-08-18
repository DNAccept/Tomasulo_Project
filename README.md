# Tomasulo on the Trotro Network

Dynamic Scheduling for Route-Optimisation Kernels
CPEN 315 / CPEN 733 — Advanced Computer Architecture Systems and Design
University of Ghana, Legon — Project 5

## Overview

Cycle-accurate implementation of Tomasulo's algorithm (reservation stations,
tag-based register renaming, common data bus) applied to an out-of-order
floating-point kernel modelling trotro (shared-taxi) route-cost optimisation.
Quantifies the IPC improvement dynamic scheduling delivers over in-order issue
on the identical kernel.

## Team

| Role | Name |
|---|---|
| Project Manager & Architecture Lead | *(you)* |
| C/C++ Implementation & Performance Lead | |
| Python/MATLAB Experimentation & Data Analysis Lead | |
| Testing, Validation, Documentation & Reproducibility Lead | |

## Repository Structure

See the README in each subdirectory for what belongs there and why.

- `docs/` — architecture diagrams, project proposal, paper notes
- `starter_code/` — instructor-provided demo and starter files (unmodified)
- `instructor_demo/` — team's verified run of the demo, hand-trace evidence
- `student_implementation/` — the team's actual simulator, kernel generator, analysis scripts
- `configs/` — seed, reservation-station counts, functional-unit latencies
- `datasets/` — the team's seeded route-optimisation kernel
- `traces/` — CDB broadcast logs, reservation-station occupancy traces
- `tests/` — hand-derived validation test vectors
- `results/` — raw, processed, and figure outputs
- `presentation/` — paper-review deck, final defence deck
- `report/` — IEEE-style technical report
- `ai_use_declaration/` — AI-use log per Part I §7
- `matlab/` — analytical IPC-sensitivity model

## Requirements

- C/C++ compiler (gcc/g++)
- Python 3.x
- MATLAB (or GNU Octave)
- Graphviz (optional, for reservation-station/CDB visualisation)

## Build & Run

```
# TODO: fill in once starter_tomasulo.c is extended
```

## Reproducibility

Team seed and all configuration parameters are recorded in `configs/`.
See `configs/team_seed.json` for the assigned seed used to generate this
team's unique kernel instance.

## AI-Use Declaration

See `ai_use_declaration/ai_use_log.md`, updated weekly per Part I §7.
