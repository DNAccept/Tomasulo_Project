/**
 * @file register_status_table.h
 * @brief Register Status Table (Qi) and Register File definitions.
 * 
 * Course: CPEN 315 / CPEN 733 — Advanced Computer Architecture Systems and Design
 * Project 5: Tomasulo on the Trotro Network
 * 
 * ============================================================================
 * ARCHITECTURAL PRINCIPLE: WAR & WAW HAZARD ELIMINATION VIA TAG RENAMING
 * ============================================================================
 * In a standard in-order pipeline or scoreboarding architecture:
 * 1. Write-After-Read (WAR / Anti-dependency):
 *    Occurs when instruction j writes to register R after instruction i reads R.
 *    If j finishes and writes R before i reads it, i reads incorrect updated data.
 *    In Tomasulo's algorithm, operands are copied directly into the reservation
 *    station (Vj/Vk) or tag-bound at ISSUE time. Once instruction i has issued,
 *    it no longer reads from register R; it holds the value in its RS. Therefore,
 *    instruction j can complete and overwrite R immediately without corrupting i.
 * 
 * 2. Write-After-Write (WAW / Output dependency):
 *    Occurs when instruction i and instruction j both write to destination R.
 *    If j finishes before i, the register file might mistakenly retain i's older value.
 *    In Tomasulo's algorithm, the Register Status table tracks the tag of the
 *    *latest* issuing producer for R. When instruction i broadcasts its tag on the
 *    CDB, the register file updates R *only if* the Register Status table's Qi still
 *    points to i's tag. Because j was issued after i, Qi points to j's tag. Hence,
 *    i's result is ignored by the register file, completely eliminating WAW hazards!
 * ============================================================================
 */

#ifndef REGISTER_STATUS_TABLE_H
#define REGISTER_STATUS_TABLE_H

#include "reservation_station.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NUM_ARCH_REGISTERS 32

/**
 * @brief Entry in the Register Status Table.
 */
typedef struct {
    Tag qi;                     /**< Tag of RS currently computing value for this reg (TAG_NONE if ready) */
} RegisterStatusEntry;

/**
 * @brief Complete Register State: Architectural Register File + Status Table.
 */
typedef struct {
    double reg_file[NUM_ARCH_REGISTERS];                /**< 64-bit Floating-point Register File (F0..F31) */
    RegisterStatusEntry status[NUM_ARCH_REGISTERS];     /**< Register Status Table (Qi tags) */
} RegisterStatusTable;

/**
 * @brief Initialize the Register Status Table and Register File.
 * @param rst Pointer to RegisterStatusTable instance
 */
static inline void rst_init(RegisterStatusTable* rst) {
    if (!rst) return;
    for (int i = 0; i < NUM_ARCH_REGISTERS; i++) {
        rst->reg_file[i] = 0.0;
        rst->status[i].qi = TAG_NONE;
    }
}

/**
 * @brief Check if an architectural register is ready in the register file.
 * @param rst Pointer to RegisterStatusTable
 * @param reg_num Architectural register index (0..31)
 * @return true if register is ready (no pending producer), false otherwise
 */
static inline bool rst_is_ready(const RegisterStatusTable* rst, int reg_num) {
    if (!rst || reg_num < 0 || reg_num >= NUM_ARCH_REGISTERS) return false;
    return (rst->status[reg_num].qi == TAG_NONE);
}

/**
 * @brief Get the pending producer tag for a register.
 * @param rst Pointer to RegisterStatusTable
 * @param reg_num Architectural register index (0..31)
 * @return Producing RS tag, or TAG_NONE if operand is already valid in register file
 */
static inline Tag rst_get_tag(const RegisterStatusTable* rst, int reg_num) {
    if (!rst || reg_num < 0 || reg_num >= NUM_ARCH_REGISTERS) return TAG_NONE;
    return rst->status[reg_num].qi;
}

/**
 * @brief Assign a new producer tag to a destination register upon instruction issue.
 * @param rst Pointer to RegisterStatusTable
 * @param dest_reg Architectural register index (0..31)
 * @param producer_tag Tag of the allocated reservation station
 */
static inline void rst_set_producer(RegisterStatusTable* rst, int dest_reg, Tag producer_tag) {
    if (!rst || dest_reg < 0 || dest_reg >= NUM_ARCH_REGISTERS) return;
    rst->status[dest_reg].qi = producer_tag;
}

/**
 * @brief Update register file upon CDB broadcast if Qi matches broadcast tag.
 * @param rst Pointer to RegisterStatusTable
 * @param broadcast_tag Tag broadcast on the Common Data Bus
 * @param broadcast_val Value broadcast on the Common Data Bus
 */
static inline void rst_on_cdb_broadcast(RegisterStatusTable* rst, Tag broadcast_tag, double broadcast_val) {
    if (!rst || broadcast_tag == TAG_NONE) return;
    for (int r = 0; r < NUM_ARCH_REGISTERS; r++) {
        if (rst->status[r].qi == broadcast_tag) {
            rst->reg_file[r] = broadcast_val;
            rst->status[r].qi = TAG_NONE; // Register file now holds latest valid value
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* REGISTER_STATUS_TABLE_H */
