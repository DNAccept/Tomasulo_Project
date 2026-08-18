/**
 * @file reservation_station.h
 * @brief Reservation Station data structures and interface definitions.
 * 
 * Course: CPEN 315 / CPEN 733 — Advanced Computer Architecture Systems and Design
 * Project 5: Tomasulo on the Trotro Network
 * 
 * Architecture Reference:
 * R. M. Tomasulo, "An Efficient Algorithm for Exploiting Multiple Arithmetic Units,"
 * IBM Journal of Research and Development, Vol. 11, No. 1, Jan 1967, pp. 25–33.
 */

#ifndef RESERVATION_STATION_H
#define RESERVATION_STATION_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Supported floating-point operation types in the trotro kernel.
 */
typedef enum {
    OP_NONE = 0,
    OP_ADD_D,       /**< Double-precision floating-point addition */
    OP_SUB_D,       /**< Double-precision floating-point subtraction */
    OP_MUL_D,       /**< Double-precision floating-point multiplication */
    OP_DIV_D        /**< Double-precision floating-point division */
} OpType;

/**
 * @brief Unique Reservation Station Producer Tags.
 * 
 * In Tomasulo's algorithm, tags uniquely identify the functional unit / reservation
 * station that will produce a result. A tag of TAG_NONE (0) indicates that the operand
 * value is already resolved and present in the corresponding Vj/Vk field or register file.
 */
typedef enum {
    TAG_NONE = 0,
    /* Adder / Subtractor Reservation Stations (3 stations) */
    TAG_ADD1 = 1,
    TAG_ADD2 = 2,
    TAG_ADD3 = 3,
    /* Multiplier / Divider Reservation Stations (3 stations) */
    TAG_MUL1 = 4,
    TAG_MUL2 = 5,
    TAG_MUL3 = 6
} Tag;

/**
 * @brief Lifecycle execution state of an in-flight instruction.
 */
typedef enum {
    RS_STATE_IDLE = 0,  /**< Station is free for allocation */
    RS_STATE_WAIT,      /**< Waiting for source operands (RAW dependency pending) */
    RS_STATE_READY,     /**< All operands available; ready to dispatch to functional unit */
    RS_STATE_EXEC,      /**< Actively executing in functional unit pipeline */
    RS_STATE_DONE       /**< Execution finished; waiting for CDB broadcast / arbitration */
} RSState;

/**
 * @brief Reservation Station entry.
 * 
 * Each functional unit has a set of reservation stations that buffer pending
 * operations and snoop the Common Data Bus (CDB) for operand forwarding.
 */
typedef struct {
    Tag tag;                    /**< Unique hardware identifier for this reservation station */
    bool busy;                  /**< Busy bit: true if station holds an active in-flight instruction */
    OpType op;                  /**< Operation to be performed by the functional unit */
    
    double vj;                  /**< Value of source operand 1 (valid if qj == TAG_NONE) */
    double vk;                  /**< Value of source operand 2 (valid if qk == TAG_NONE) */
    
    Tag qj;                     /**< Tag of producer RS for source operand 1 (TAG_NONE if ready) */
    Tag qk;                     /**< Tag of producer RS for source operand 2 (TAG_NONE if ready) */
    
    /* Dynamic Simulation & Pipeline State Tracking */
    int inst_id;                /**< Dynamic instruction sequence identifier / program order ID */
    int issued_cycle;           /**< Simulation cycle at which instruction was issued into this RS */
    int cycles_remaining;       /**< Functional unit execution cycles remaining */
    bool executing;             /**< Flag indicating if station is currently executing */
    RSState state;              /**< High-level lifecycle state for debugging and trace analysis */
} ReservationStation;

/**
 * @brief Convert OpType enum to human-readable mnemonic string.
 * @param op Operation type
 * @return String representation (e.g., "ADD.D")
 */
static inline const char* rs_op_to_str(OpType op) {
    switch (op) {
        case OP_ADD_D: return "ADD.D";
        case OP_SUB_D: return "SUB.D";
        case OP_MUL_D: return "MUL.D";
        case OP_DIV_D: return "DIV.D";
        default:       return "NONE ";
    }
}

/**
 * @brief Convert Tag enum to human-readable tag name string.
 * @param tag Reservation station tag
 * @return String representation (e.g., "ADD1", "MUL2", or " -- ")
 */
static inline const char* rs_tag_to_str(Tag tag) {
    switch (tag) {
        case TAG_ADD1: return "ADD1";
        case TAG_ADD2: return "ADD2";
        case TAG_ADD3: return "ADD3";
        case TAG_MUL1: return "MUL1";
        case TAG_MUL2: return "MUL2";
        case TAG_MUL3: return "MUL3";
        default:       return " -- ";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* RESERVATION_STATION_H */
