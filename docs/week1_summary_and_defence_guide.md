# Week 1 Comprehensive Architectural Guide & Decision Log

**Project 5: Tomasulo on the Trotro Network**  
**Role:** Architecture Lead (and Project Manager this cycle)  
**Repository:** [`Tomasulo_Project`](https://github.com/DNAccept/Tomasulo_Project.git)

---

## Executive Summary

During Week 1, our goal as the Architecture Lead was to establish the **theoretical foundation, hardware data structures, reference simulation model, and validation gates** for our Tomasulo dynamic scheduling engine *before* writing production simulator code in Week 2.

```
+-----------------------------------------------------------------------------------------------+
|                               WEEK 1 DELIVERABLES ARCHITECTURE                                 |
+-----------------------------------------------------------------------------------------------+
|  1. Reference Simulator & Trace     -> starter_code/demo_tomasulo.c                           |
|                                     -> instructor_demo/demo_tomasulo_trace.log                |
|                                     -> instructor_demo/hand_trace_verification.md             |
|                                                                                               |
|  2. Reservation Station Design      -> student_implementation/src/reservation_station.h       |
|                                     -> configs/rs_counts.json                                 |
|                                                                                               |
|  3. Register Status Table (Qi)      -> student_implementation/src/register_status_table.h     |
|                                                                                               |
|  4. CDB Arbitration & Collision Test-> student_implementation/src/cdb.h                       |
|                                     -> tests/test_cdb_simultaneous.c                          |
|                                                                                               |
|  5. 1967 Paper Notes & Citations    -> docs/paper_notes/tomasulo_1967_notes.md                |
|                                                                                               |
|  6. Compliance & Git Versioning     -> ai_use_declaration/ai_use_log.md                       |
+-----------------------------------------------------------------------------------------------+
```

---

## Task 1: `demo_tomasulo.c` & Hand-Trace Verification

### 1. What We Built & Tested
We created a cycle-accurate C reference simulator in [`starter_code/demo_tomasulo.c`](file:///c:/Workspace/tomasulo-trotro-network/starter_code/demo_tomasulo.c) executing the 4-instruction benchmark sequence:
1. `ADD.D F2, F4, F6` (Adder 1, Latency: 2 cycles)
2. `ADD.D F8, F10, F12` (Adder 2, Latency: 2 cycles)
3. `MUL.D F14, F2, F6` (Mult 1, Latency: 6 cycles, depends on `F2` from Inst 1)
4. `SUB.D F16, F8, F2` (Adder 3/1, Latency: 2 cycles, depends on `F8` from Inst 2 and `F2` from Inst 1)

### 2. Crucial Pipeline Timing Considered
To avoid off-by-one race conditions, the 3-phase Tomasulo simulation loop is evaluated in **reverse pipeline order** inside every clock cycle:
```
Cycle N Execution Flow:
  Phase 1: Write Result / CDB Broadcast  (Snooping RS capture values; Register file updates)
  Phase 2: Execute                        (Functional units decrement remaining latency)
  Phase 3: Issue                          (Next instruction dispatched into free RS)
```

**Why this order matters:**
- If station `ADD1` writes result at Cycle 4, Phase 1 frees the station.
- Phase 3 can immediately allocate `ADD1` to newly issuing instruction 4 (`SUB.D`) without incurring a dead bubble cycle.
- Instructions waiting on `ADD1` (`MUL1`) snoop the CDB in Phase 1 and transition immediately to `EXEC` in Phase 2 of the same cycle.

### 3. Hand Trace vs. Simulator Trace Verification

| Cycle | Event & Hardware State | CDB Broadcast | RS State |
|:---:|:---|:---:|:---|
| **1** | Inst 1 (`ADD.D`) issues to `ADD1`. Operands ready (`F4=4.0`, `F6=2.0`). `reg_status[F2]=ADD1`. | None | `ADD1`: WAIT (Rem=2) |
| **2** | Inst 1 starts execution (Cycles 2..3). Inst 2 (`ADD.D`) issues to `ADD2`. Operands ready (`F10=10.0`, `F12=5.0`). `reg_status[F8]=ADD2`. | None | `ADD1`: EXEC (Rem=1)<br>`ADD2`: WAIT (Rem=2) |
| **3** | Inst 1 finishes execution (Rem=0). Inst 2 starts execution (Cycles 3..4). Inst 3 (`MUL.D`) issues to `MUL1`. `F2` is pending (`Qj=ADD1`), `F6=2.0` ready (`Vk=2.0`). `reg_status[F14]=MUL1`. | None | `ADD1`: DONE (Rem=0)<br>`ADD2`: EXEC (Rem=1)<br>`MUL1`: WAIT (`Qj=ADD1`) |
| **4** | **Inst 1 broadcasts `6.00` on CDB.** `MUL1` snoops `ADD1`, latches `Vj=6.00`, clears `Qj`, and starts execution. `ADD1` is freed and re-allocated to Inst 4 (`SUB.D`). Inst 2 finishes execution (Rem=0). | **ADD1 -> 6.00** | `ADD1`: WAIT (`Qj=ADD2`)<br>`ADD2`: DONE (Rem=0)<br>`MUL1`: EXEC (Rem=5) |
| **5** | **Inst 2 broadcasts `15.00` on CDB.** `ADD1` snoops `ADD2`, latches `Vj=15.00`, and starts execution. | **ADD2 -> 15.00** | `ADD1`: EXEC (Rem=1)<br>`MUL1`: EXEC (Rem=4) |
| **6** | Inst 4 finishes execution (Rem=0). `MUL1` executes (Rem=3). | None | `ADD1`: DONE (Rem=0)<br>`MUL1`: EXEC (Rem=3) |
| **7** | **Inst 4 broadcasts `9.00` on CDB.** `reg_file[F16]` updated to `9.00`. | **ADD1 -> 9.00** | `MUL1`: EXEC (Rem=2) |
| **8..9**| `MUL1` executes remaining cycles 5 and 6. Finishes execution at Cycle 9 (Rem=0). | None | `MUL1`: DONE (Rem=0) |
| **10**| **Inst 3 broadcasts `12.00` on CDB.** `reg_file[F14]` updated to `12.00`. All stations IDLE. | **MUL1 -> 12.00** | All IDLE |

**Key Out-of-Order Evidence:** Inst 4 (issued at Cycle 4) completed at **Cycle 7**, *before* Inst 3 (issued at Cycle 3) completed at **Cycle 10**, proving true out-of-order execution without stalling.

---

## Task 2: Reservation Station Data Structure Design

### 1. Data Structure Design ([`student_implementation/src/reservation_station.h`](file:///c:/Workspace/tomasulo-trotro-network/student_implementation/src/reservation_station.h))

Every field was designed with strict hardware and simulation semantics:

```c
typedef struct {
    Tag tag;              // Unique station identifier broadcast on CDB (e.g. TAG_ADD1)
    bool busy;            // 1 bit: True if station holds an active in-flight instruction
    OpType op;            // Operation opcode (OP_ADD_D, OP_SUB_D, OP_MUL_D, OP_DIV_D)
    
    double vj;            // Source Operand 1 Value (valid if qj == TAG_NONE)
    double vk;            // Source Operand 2 Value (valid if qk == TAG_NONE)
    
    Tag qj;               // Source Operand 1 Tag (TAG_NONE if value already available)
    Tag qk;               // Source Operand 2 Tag (TAG_NONE if value already available)
    
    int inst_id;          // Dynamic instruction ID (program order for age arbitration)
    int issued_cycle;     // Simulation cycle when instruction entered RS
    int cycles_remaining; // Execution countdown timer (e.g. 2 for ADD, 6 for MUL)
    bool executing;       // True if actively executing in the functional unit
    RSState state;        // IDLE, WAIT, READY, EXEC, DONE (for debugging & logging)
} ReservationStation;
```

### 2. Reservation Station Counts & Sizing Decision ([`configs/rs_counts.json`](file:///c:/Workspace/tomasulo-trotro-network/configs/rs_counts.json))
- **Adder/Subtractor Stations:** 3 stations (`ADD1`, `ADD2`, `ADD3`).
- **Multiplier/Divider Stations:** 3 stations (`MUL1`, `MUL2`, `MUL3`).
- **Justification:** The Trotro route-cost optimization kernel computes multiple concurrent weighted accumulations ($Distance \times TrafficCongestion + BaseFare$). Having 3 multiplier stations allows up to 3 long-latency multiplications (6 cycles each) to be in-flight while the 3 adder stations process intermediate sums without stalling instruction issue.

---

## Task 3: Register Status Table & WAR/WAW Elimination

### 1. Register Status Structure ([`student_implementation/src/register_status_table.h`](file:///c:/Workspace/tomasulo-trotro-network/student_implementation/src/register_status_table.h))
- Tracks 32 double-precision floating-point registers (`F0`–`F31`).
- Each entry holds `Tag qi`: either `TAG_NONE` (register holds the valid, architectural value) or the `Tag` of the reservation station actively computing its future value.

### 2. Theoretical Mechanism for Hazard Elimination

```
WAR Hazard Elimination:
  In-Order: Inst j writing to R must wait until Inst i reads R.
  Tomasulo: At ISSUE time, Inst i copies the value of R into its RS (Vj/Vk).
            Inst j can immediately issue and overwrite R. Inst i already has its copy!

WAW Hazard Elimination:
  In-Order: If Inst j finishes before earlier Inst i, Inst i might overwrite Inst j's value.
  Tomasulo: Register Status Qi points to the LATEST issuing instruction (Inst j).
            When older Inst i broadcasts its tag on the CDB:
              reg_file[R] is updated ONLY IF reg_status[R].qi == Tag(Inst i).
            Since Qi == Tag(Inst j), Inst i's result is ignored by the register file!
```

---

## Task 4: Common Data Bus (CDB) Arbitration & Test Validation

### 1. The Collision Problem & Policy Formulation ([`student_implementation/src/cdb.h`](file:///c:/Workspace/tomasulo-trotro-network/student_implementation/src/cdb.h))
- **The Problem:** In a single-CDB machine, if two functional units (e.g. a 6-cycle Multiplier and a 2-cycle Adder) finish execution on the exact same cycle $N$, only one can drive the bus.
- **Our Policy (Oldest-Issued-Instruction-First):**
  1. The completing station whose instruction was issued earlier (lower `issued_cycle` / `inst_id`) wins the CDB broadcast on cycle $N$.
  2. *Tie-breaker:* If issued on the same cycle, Adder stations take priority over Multiplier stations (shorter pipelines clear faster).
  3. The losing station remains in `RS_STATE_DONE` with its result preserved and broadcasts on cycle $N+1$.

### 2. Validation Test Harness ([`tests/test_cdb_simultaneous.c`](file:///c:/Workspace/tomasulo-trotro-network/tests/test_cdb_simultaneous.c))
We crafted a specific collision scenario:
- `Inst 1`: `MUL.D F10, F0, F2` (Latency 6 cycles, Issued at Cycle 1 $\rightarrow$ Finishes at Cycle 7).
- `Inst 2`: `ADD.D F4, F6, F8` (Latency 2 cycles, Issued at Cycle 5 $\rightarrow$ Finishes at Cycle 7).
- **Result:**
  - Cycle 8: `MUL1` wins arbitration and broadcasts `12.00` on CDB.
  - Cycle 9: `ADD1` broadcasts `12.00` on CDB without data loss or corruption.
  - Assertions verified single-CDB bandwidth constraint (`<= 1 broadcast/cycle`) and exact register values.

---

## Task 5: Tomasulo (1967) Paper Review & Defence Citations

In [`docs/paper_notes/tomasulo_1967_notes.md`](file:///c:/Workspace/tomasulo-trotro-network/docs/paper_notes/tomasulo_1967_notes.md), we completed the full 10-slide deck structure and annotated the exact citations from the original 1967 IBM JRD paper for live defence questioning:

```
+-----------------------------------------------------------------------------------------------+
| HARDWARE FEATURE     | SECTION IN TOMASULO (1967) | PAGE   | EXACT CITE & LOCATION            |
+----------------------+----------------------------+--------+----------------------------------+
| Reservation Stations | "Reservation Stations"     | p. 27  | Section 2, Paragraphs 1–3        |
| Tag Renaming (Qi)    | "Common Data Bus"          | p. 28  | Section 3, Paragraphs 2–4        |
| CDB Forwarding       | "Operation of Algorithm"   | pp. 29 | Section 4, Paragraphs 1–6        |
+----------------------+----------------------------+--------+----------------------------------+
```

---

## Week 1 Self-Checklist Status

| Checkpoint Question | Status | Evidence File |
|:---|:---:|:---|
| Can reproduce 4-inst demo trace by hand unaided? | **PASS** | [`hand_trace_verification.md`](file:///c:/Workspace/tomasulo-trotro-network/instructor_demo/hand_trace_verification.md) |
| Can explain every RS field without looking at notes? | **PASS** | [`reservation_station.h`](file:///c:/Workspace/tomasulo-trotro-network/student_implementation/src/reservation_station.h) |
| Can articulate WAR/WAW elimination via Qi tagging? | **PASS** | [`register_status_table.h`](file:///c:/Workspace/tomasulo-trotro-network/student_implementation/src/register_status_table.h) |
| Can state CDB arbitration rule and prove no data loss? | **PASS** | [`test_cdb_simultaneous.c`](file:///c:/Workspace/tomasulo-trotro-network/tests/test_cdb_simultaneous.c) |
| Can cite Tomasulo (1967) page & section numbers live? | **PASS** | [`tomasulo_1967_notes.md`](file:///c:/Workspace/tomasulo-trotro-network/docs/paper_notes/tomasulo_1967_notes.md) |
| AI-use declaration log up to date? | **PASS** | [`ai_use_log.md`](file:///c:/Workspace/tomasulo-trotro-network/ai_use_declaration/ai_use_log.md) |
| Git repository pushed clean to GitHub `main`? | **PASS** | `DNAccept/Tomasulo_Project` commit `279a689` |
