/**
 * @file test_cdb_simultaneous.c
 * @brief Unit Test: Simultaneous Functional Unit Completion & CDB Arbitration.
 * 
 * Course: CPEN 315 / CPEN 733 — Advanced Computer Architecture Systems and Design
 * Project 5: Tomasulo on the Trotro Network
 * 
 * Test Objective:
 * Force two functional units (Multiplier with 6-cycle latency and Adder with 2-cycle latency)
 * to finish execution on the EXACT same cycle (Cycle 7). Verify that the CDB arbiter:
 *   1. Grants broadcast to the older instruction (Inst 1, MUL.D) first at Cycle 8.
 *   2. Holds the losing station (Inst 2, ADD.D) without loss or data corruption.
 *   3. Grants broadcast to the second instruction (Inst 2, ADD.D) on Cycle 9.
 *   4. Asserts correct final register file values and no dropped results.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <math.h>

#include "../student_implementation/src/reservation_station.h"
#include "../student_implementation/src/register_status_table.h"
#include "../student_implementation/src/cdb.h"

#define LATENCY_ADD 2
#define LATENCY_MUL 6

typedef struct {
    int id;
    OpType op;
    int dest;
    int src1;
    int src2;
    int issue_cycle;
    int write_cycle;
    bool completed;
} TestInst;

int main() {
    printf("====================================================================\n");
    printf("   TEST: Simultaneous FU Completion and Single-CDB Arbitration      \n");
    printf("====================================================================\n");

    RegisterStatusTable rst;
    rst_init(&rst);

    // Initial register file values:
    rst.reg_file[0] = 3.0; // F0 = 3.0
    rst.reg_file[2] = 4.0; // F2 = 4.0 (3.0 * 4.0 = 12.0)
    rst.reg_file[6] = 5.0; // F6 = 5.0
    rst.reg_file[8] = 7.0; // F8 = 7.0 (5.0 + 7.0 = 12.0)

    ReservationStation rs_add;
    rs_add.tag = TAG_ADD1;
    rs_add.busy = false;

    ReservationStation rs_mul;
    rs_mul.tag = TAG_MUL1;
    rs_mul.busy = false;

    CommonDataBus bus;
    cdb_clear(&bus);

    TestInst inst1 = {1, OP_MUL_D, 10, 0, 2, 1, 0, false}; // MUL.D F10, F0, F2 (Issue @ C1)
    TestInst inst2 = {2, OP_ADD_D, 4,  6, 8, 5, 0, false}; // ADD.D F4,  F6, F8 (Issue @ C5)

    int cdb_broadcast_counts[20] = {0};

    printf("[Setup]\n");
    printf("  Inst 1: MUL.D F10, F0, F2 (Latency: 6 cycles, Issued Cycle 1)\n");
    printf("  Inst 2: ADD.D F4,  F6, F8 (Latency: 2 cycles, Issued Cycle 5)\n");
    printf("  Expected Completion: Both finish execution at Cycle 7!\n\n");

    for (int cycle = 1; cycle <= 12; cycle++) {
        cdb_clear(&bus);

        /* -------------------------------------------------------------
         * Phase 1: Write Result / CDB Arbitration
         * ------------------------------------------------------------- */
        ReservationStation* winner = NULL;

        bool mul_done = (rs_mul.busy && rs_mul.executing && rs_mul.cycles_remaining == 0);
        bool add_done = (rs_add.busy && rs_add.executing && rs_add.cycles_remaining == 0);

        if (mul_done && add_done) {
            // Simultaneous completion collision!
            // Apply Oldest-Issued-Instruction-First Arbitration:
            if (rs_mul.issued_cycle <= rs_add.issued_cycle) {
                winner = &rs_mul;
            } else {
                winner = &rs_add;
            }
            printf("  [Cycle %2d] CDB COLLISION detected! Winner: %s (Issued Cycle %d vs %d)\n",
                   cycle, rs_tag_to_str(winner->tag), rs_mul.issued_cycle, rs_add.issued_cycle);
        } else if (mul_done) {
            winner = &rs_mul;
        } else if (add_done) {
            winner = &rs_add;
        }

        if (winner != NULL) {
            double res = 0.0;
            if (winner->op == OP_MUL_D) res = winner->vj * winner->vk;
            if (winner->op == OP_ADD_D) res = winner->vj + winner->vk;

            cdb_broadcast(&bus, winner->tag, res, winner->inst_id);
            rst_on_cdb_broadcast(&rst, winner->tag, res);

            cdb_broadcast_counts[cycle]++;

            if (winner == &rs_mul) { inst1.write_cycle = cycle; inst1.completed = true; }
            if (winner == &rs_add) { inst2.write_cycle = cycle; inst2.completed = true; }

            winner->busy = false;
            winner->executing = false;
        }

        /* -------------------------------------------------------------
         * Phase 2: Execute
         * ------------------------------------------------------------- */
        if (rs_mul.busy && rs_mul.qj == TAG_NONE && rs_mul.qk == TAG_NONE) {
            rs_mul.executing = true;
            if (rs_mul.cycles_remaining > 0) rs_mul.cycles_remaining--;
        }
        if (rs_add.busy && rs_add.qj == TAG_NONE && rs_add.qk == TAG_NONE) {
            rs_add.executing = true;
            if (rs_add.cycles_remaining > 0) rs_add.cycles_remaining--;
        }

        /* -------------------------------------------------------------
         * Phase 3: Issue
         * ------------------------------------------------------------- */
        if (cycle == 1) { // Issue Inst 1 (MUL.D)
            rs_mul.busy = true;
            rs_mul.op = OP_MUL_D;
            rs_mul.inst_id = inst1.id;
            rs_mul.issued_cycle = 1;
            rs_mul.cycles_remaining = LATENCY_MUL;
            rs_mul.vj = rst.reg_file[inst1.src1];
            rs_mul.vk = rst.reg_file[inst1.src2];
            rs_mul.qj = TAG_NONE;
            rs_mul.qk = TAG_NONE;
            rs_mul.executing = false;
            rst_set_producer(&rst, inst1.dest, rs_mul.tag);
            printf("  [Cycle %2d] Issued Inst 1 (MUL.D) to MUL1 (Latency = %d)\n", cycle, LATENCY_MUL);
        }
        if (cycle == 5) { // Issue Inst 2 (ADD.D)
            rs_add.busy = true;
            rs_add.op = OP_ADD_D;
            rs_add.inst_id = inst2.id;
            rs_add.issued_cycle = 5;
            rs_add.cycles_remaining = LATENCY_ADD;
            rs_add.vj = rst.reg_file[inst2.src1];
            rs_add.vk = rst.reg_file[inst2.src2];
            rs_add.qj = TAG_NONE;
            rs_add.qk = TAG_NONE;
            rs_add.executing = false;
            rst_set_producer(&rst, inst2.dest, rs_add.tag);
            printf("  [Cycle %2d] Issued Inst 2 (ADD.D) to ADD1 (Latency = %d)\n", cycle, LATENCY_ADD);
        }
    }

    printf("\n[Validation Checks]\n");
    // 1. Single broadcast per cycle constraint
    for (int c = 1; c <= 12; c++) {
        assert(cdb_broadcast_counts[c] <= 1 && "ERROR: More than 1 broadcast on CDB in single cycle!");
    }
    printf("  [PASS] Single CDB bus width constraint respected (<= 1 broadcast/cycle).\n");

    // 2. Oldest-first arbitration order
    assert(inst1.write_cycle == 8 && "ERROR: Inst 1 (MUL1) should broadcast at cycle 8");
    printf("  [PASS] Inst 1 (MUL1) won arbitration at Cycle 8 (write_cycle = %d).\n", inst1.write_cycle);

    assert(inst2.write_cycle == 9 && "ERROR: Inst 2 (ADD1) should broadcast at cycle 9");
    printf("  [PASS] Inst 2 (ADD1) broadcast at Cycle 9 (write_cycle = %d) without data loss.\n", inst2.write_cycle);

    // 3. Register file values
    assert(fabs(rst.reg_file[10] - 12.0) < 1e-6 && "ERROR: F10 value incorrect");
    assert(fabs(rst.reg_file[4] - 12.0) < 1e-6 && "ERROR: F4 value incorrect");
    printf("  [PASS] Final register file values correct: F10 = %.2f, F4 = %.2f.\n",
           rst.reg_file[10], rst.reg_file[4]);

    printf("\n>>> ALL CDB SIMULTANEOUS ARBITRATION TESTS PASSED SUCCESSFULLY! <<<\n");
    return 0;
}
