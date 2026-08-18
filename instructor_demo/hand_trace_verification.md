# Hand-Trace Verification — `demo_tomasulo.c`

**Course:** CPEN 315 / CPEN 733 — Advanced Computer Architecture Systems and Design  
**Project:** Project 5: Tomasulo on the Trotro Network  
**Role:** Architecture Lead (and Project Manager)  
**Date:** Week 1 Checkpoint Verification

---

## 1. Program Instruction Sequence & Latency Assumptions

| ID | Assembly Instruction | Functional Unit | Latency | Source Operands | Destination |
|:--:|:---------------------|:---------------:|:-------:|:---------------:|:-----------:|
| 1  | `ADD.D F2, F4, F6`   | Adder / Sub     | 2       | F4=4.0, F6=2.0  | F2          |
| 2  | `ADD.D F8, F10, F12` | Adder / Sub     | 2       | F10=10.0, F12=5.0 | F8        |
| 3  | `MUL.D F14, F2, F6`  | Multiplier      | 6       | F2 (Pending Inst 1), F6=2.0 | F14 |
| 4  | `SUB.D F16, F8, F2`  | Adder / Sub     | 2       | F8 (Pending Inst 2), F2 (Inst 1) | F16 |

- **Reservation Stations:** 3 Add/Sub (`ADD1`, `ADD2`, `ADD3`), 2 Multiplier (`MUL1`, `MUL2`).
- **CDB Width:** Single 64-bit CDB (1 broadcast per cycle).
- **CDB Arbitration Rule:** Oldest-Issued-Instruction-First.

---

## 2. Cycle-by-Cycle Verification Table

| Cycle | RS Occupancy (By Hand) | CDB Broadcast (By Hand) | RS Occupancy (Simulator) | CDB Broadcast (Simulator) | Match? |
|:-----:|:-----------------------|:------------------------|:-------------------------|:--------------------------|:------:|
| **1** | `ADD1`: Busy, ADD.D, Vj=4.0, Vk=2.0 (WAIT) | None | `ADD1`: Busy, ADD.D, Vj=4.0, Vk=2.0, Rem=2 (WAIT) | None | **YES** |
| **2** | `ADD1`: EXEC (Rem=1)<br>`ADD2`: Busy, ADD.D, Vj=10.0, Vk=5.0 (WAIT) | None | `ADD1`: EXEC (Rem=1)<br>`ADD2`: WAIT (Rem=2) | None | **YES** |
| **3** | `ADD1`: DONE (Rem=0)<br>`ADD2`: EXEC (Rem=1)<br>`MUL1`: WAIT (Qj=ADD1, Vk=2.0) | None | `ADD1`: DONE (Rem=0)<br>`ADD2`: EXEC (Rem=1)<br>`MUL1`: WAIT (Qj=ADD1, Vk=2.0) | None | **YES** |
| **4** | `ADD1`: Busy, SUB.D, Qj=ADD2, Vk=6.0 (WAIT)<br>`ADD2`: DONE (Rem=0)<br>`MUL1`: EXEC (Vj=6.0, Vk=2.0, Rem=5) | **ADD1 -> 6.00** (Inst #1) | `ADD1`: WAIT (Qj=ADD2, Vk=6.0)<br>`ADD2`: DONE (Rem=0)<br>`MUL1`: EXEC (Vj=6.0, Vk=2.0, Rem=5) | **ADD1 -> 6.00** (Inst #1) | **YES** |
| **5** | `ADD1`: EXEC (Vj=15.0, Vk=6.0, Rem=1)<br>`MUL1`: EXEC (Rem=4) | **ADD2 -> 15.00** (Inst #2) | `ADD1`: EXEC (Vj=15.0, Vk=6.0, Rem=1)<br>`MUL1`: EXEC (Rem=4) | **ADD2 -> 15.00** (Inst #2) | **YES** |
| **6** | `ADD1`: DONE (Rem=0)<br>`MUL1`: EXEC (Rem=3) | None | `ADD1`: DONE (Rem=0)<br>`MUL1`: EXEC (Rem=3) | None | **YES** |
| **7** | `MUL1`: EXEC (Rem=2) | **ADD1 -> 9.00** (Inst #4) | `MUL1`: EXEC (Rem=2) | **ADD1 -> 9.00** (Inst #4) | **YES** |
| **8** | `MUL1`: EXEC (Rem=1) | None | `MUL1`: EXEC (Rem=1) | None | **YES** |
| **9** | `MUL1`: DONE (Rem=0) | None | `MUL1`: DONE (Rem=0) | None | **YES** |
| **10**| All RS IDLE | **MUL1 -> 12.00** (Inst #3) | All RS IDLE | **MUL1 -> 12.00** (Inst #3) | **YES** |

---

## 3. Key Observations & Architectural Verification

1. **Parallel Issue & Execution (ILP Exploitation):**
   - Instructions 1 and 2 are independent arithmetic operations (`ADD.D`). They issue in consecutive cycles (Cycle 1 and Cycle 2) and execute concurrently across separate reservation stations (`ADD1` and `ADD2`).
2. **Dynamic RAW Hazard Resolution (CDB Snooping):**
   - Instruction 3 (`MUL.D`) depends on `F2`, produced by Instruction 1 (`ADD1`).
   - When Instruction 3 issues at Cycle 3, it tags `Qj = ADD1`.
   - At Cycle 4, `ADD1` broadcasts `6.00` on the CDB. `MUL1` snoops the CDB, captures `Vj = 6.00`, clears `Qj`, and immediately transitions from `WAIT` to `EXEC` on Cycle 4.
3. **Out-of-Order Completion:**
   - Instruction 4 (`SUB.D`) issues at Cycle 4, executes in Cycles 5..6, and completes result writeback at **Cycle 7**.
   - Instruction 3 (`MUL.D`), which was issued *earlier* (Cycle 3), completes at **Cycle 10** due to the 6-cycle multiplier latency.
   - **Conclusion:** Instruction 4 completed before Instruction 3 without causing WAR or WAW hazards because tag renaming insulated `F16` and intermediate operands.

---

## 4. Discrepancies Found and Resolved

- **Discrepancy 1 (Timing of CDB Snooping & Execution Start):**
  - *Question:* Can an instruction that captures its missing operand from the CDB during cycle $T$ begin executing in cycle $T$ or cycle $T+1$?
  - *Resolution:* In standard Tomasulo timing (and Model 91), operand forwarding from the CDB completes during the write-result phase of cycle $T$. The reservation station is now ready and execution begins in the execution phase of cycle $T$. This was verified and implemented consistently across our simulator and hand trace.
- **Discrepancy 2 (Station Recycling):**
  - *Question:* Can a reservation station that writes its result to the CDB on cycle $T$ be allocated to a newly issuing instruction in the same cycle?
  - *Resolution:* Because the 3-phase pipeline executes in the order (1) Write Result -> (2) Execute -> (3) Issue, station `ADD1` is freed in Phase 1 of Cycle 4 and successfully re-allocated to Instruction 4 in Phase 3 of Cycle 4 without requiring an extra bubble cycle.

---

## 5. Checkpoint Sign-off

- [x] Hand trace derived independently and validated line-by-line against simulator log.
- [x] All 4 instructions verified for correctness of operands, latencies, and final register state (`F2=6.00, F8=15.00, F14=12.00, F16=9.00`).
- [x] Architecture Lead ready for whiteboard defence.
