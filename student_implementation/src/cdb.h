/**
 * @file cdb.h
 * @brief Common Data Bus (CDB) and Arbitration Policy definitions.
 * 
 * Course: CPEN 315 / CPEN 733 — Advanced Computer Architecture Systems and Design
 * Project 5: Tomasulo on the Trotro Network
 * 
 * ============================================================================
 * CDB ARBITRATION POLICY SPECIFICATION
 * ============================================================================
 * Problem:
 * In a single-CDB architecture, only ONE reservation station can broadcast its
 * tag and result per clock cycle. When multiple functional units finish execution
 * in the same clock cycle N (e.g., a 6-cycle MUL issued at cycle 1 and a 2-cycle
 * ADD issued at cycle 5 both finish at cycle 7), a structural collision occurs.
 * 
 * Policy:
 * 1. Primary Priority — Oldest Issued Instruction First (Age-based arbitration):
 *    The station holding the instruction with the lowest issue cycle (or earliest
 *    program sequence ID) is granted the CDB broadcast in cycle N.
 *    Rationale: Oldest-first minimizes instruction window drag, unblocks older
 *    dependent instructions sooner, and adheres closely to sequential program semantics.
 * 
 * 2. Secondary Priority / Tie-Breaker:
 *    If two completing instructions have identical issue timestamps, Adder/Subtractor
 *    stations take priority over Multiplier/Divider stations (TAG_ADD1..3 < TAG_MUL1..3).
 *    Rationale: Shorter latency units clear quicker, freeing compact reservation
 *    stations for upcoming loops.
 * 
 * 3. Deferred Stations:
 *    Any station losing arbitration remains in the DONE state with its result preserved,
 *    and competes again for the CDB on cycle N+1. No result is ever dropped or overwritten.
 * ============================================================================
 */

#ifndef CDB_H
#define CDB_H

#include "reservation_station.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Common Data Bus (CDB) hardware bus state.
 */
typedef struct {
    bool valid;                 /**< Bus busy flag: true if a result is being broadcast this cycle */
    Tag tag;                    /**< Producer RS Tag carrying the result */
    double value;               /**< 64-bit floating-point computed data value */
    int inst_id;                /**< Instruction ID of the broadcasting instruction (for logging) */
} CommonDataBus;

/**
 * @brief Reset CDB state at the start of a clock cycle.
 * @param bus Pointer to CommonDataBus
 */
static inline void cdb_clear(CommonDataBus* bus) {
    if (!bus) return;
    bus->valid = false;
    bus->tag = TAG_NONE;
    bus->value = 0.0;
    bus->inst_id = 0;
}

/**
 * @brief Broadcast a computed result onto the CDB.
 * @param bus Pointer to CommonDataBus
 * @param tag Producer RS tag
 * @param value Computed double-precision result
 * @param inst_id Instruction sequence identifier
 */
static inline void cdb_broadcast(CommonDataBus* bus, Tag tag, double value, int inst_id) {
    if (!bus) return;
    bus->valid = true;
    bus->tag = tag;
    bus->value = value;
    bus->inst_id = inst_id;
}

#ifdef __cplusplus
}
#endif

#endif /* CDB_H */
