# traces/

Cycle-by-cycle execution evidence produced by the simulator.

## What goes here

- `cdb_broadcast_log.csv` — per-cycle CDB broadcast record (tag, value,
  source reservation station).
- `reservation_station_occupancy.csv` — per-cycle occupancy for every
  reservation station, used for the required occupancy trace and for the
  IPC-vs-reservation-station-count analysis.

## Required evidence

At least one representative window of cycle-by-cycle reservation-station
occupancy must be included, per the Experimental Design requirements (§L).
