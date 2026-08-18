/**
 * @file demo_tomasulo.c
 * @brief Cycle-accurate 4-instruction Tomasulo Algorithm Demonstration.
 * 
 * Project 5: Tomasulo on the Trotro Network
 * Instructor / Reference Implementation.
 * 
 * Instructions simulated:
 *   1. ADD.D F2, F4, F6   (Adder RS1, Latency: 2 cycles)
 *   2. ADD.D F8, F10, F12 (Adder RS2, Latency: 2 cycles)
 *   3. MUL.D F14, F2, F6  (Mult RS1,  Latency: 6 cycles, depends on F2 from Inst 1)
 *   4. SUB.D F16, F8, F2  (Adder RS3, Latency: 2 cycles, depends on F8 from Inst 2 and F2 from Inst 1)
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define NUM_REGS 32
#define NUM_ADD_RS 3
#define NUM_MUL_RS 2

#define LATENCY_ADD 2
#define LATENCY_SUB 2
#define LATENCY_MUL 6

typedef enum {
    OP_NONE = 0,
    OP_ADD_D,
    OP_SUB_D,
    OP_MUL_D,
    OP_DIV_D
} OpType;

const char* op_to_string(OpType op) {
    switch (op) {
        case OP_ADD_D: return "ADD.D";
        case OP_SUB_D: return "SUB.D";
        case OP_MUL_D: return "MUL.D";
        case OP_DIV_D: return "DIV.D";
        default:       return "NONE ";
    }
}

/* Tag identifiers */
typedef enum {
    TAG_NONE = 0,
    TAG_ADD1,
    TAG_ADD2,
    TAG_ADD3,
    TAG_MUL1,
    TAG_MUL2
} Tag;

const char* tag_to_string(Tag tag) {
    switch (tag) {
        case TAG_ADD1: return "ADD1";
        case TAG_ADD2: return "ADD2";
        case TAG_ADD3: return "ADD3";
        case TAG_MUL1: return "MUL1";
        case TAG_MUL2: return "MUL2";
        default:       return " -- ";
    }
}

typedef struct {
    int id;
    OpType op;
    int dest_reg;
    int src1_reg;
    int src2_reg;
    char text[32];
    int issue_cycle;
    int exec_start_cycle;
    int exec_end_cycle;
    int write_cdb_cycle;
    bool completed;
} Instruction;

typedef struct {
    Tag tag;
    bool busy;
    OpType op;
    double vj;
    double vk;
    Tag qj;
    Tag qk;
    int inst_id;
    int remaining_cycles;
    bool executing;
} ReservationStation;

typedef struct {
    Tag qi; // TAG_NONE if ready in register file
} RegisterStatus;

typedef struct {
    bool valid;
    Tag tag;
    double value;
    int inst_id;
} CommonDataBus;

/* Global state */
static double reg_file[NUM_REGS];
static RegisterStatus reg_status[NUM_REGS];
static ReservationStation add_rs[NUM_ADD_RS];
static ReservationStation mul_rs[NUM_MUL_RS];
static CommonDataBus cdb;

#define NUM_INSTRUCTIONS 4
static Instruction program[NUM_INSTRUCTIONS] = {
    {1, OP_ADD_D, 2,  4,  6,  "ADD.D F2, F4, F6",   0, 0, 0, 0, false},
    {2, OP_ADD_D, 8,  10, 12, "ADD.D F8, F10, F12", 0, 0, 0, 0, false},
    {3, OP_MUL_D, 14, 2,  6,  "MUL.D F14, F2, F6",  0, 0, 0, 0, false},
    {4, OP_SUB_D, 16, 8,  2,  "SUB.D F16, F8, F2",  0, 0, 0, 0, false}
};

static int pc = 0;
static int current_cycle = 0;

void init_simulator() {
    for (int i = 0; i < NUM_REGS; i++) {
        reg_file[i] = (double)i * 1.5 + 1.0;
        reg_status[i].qi = TAG_NONE;
    }
    // Specific initial values for readable arithmetic:
    reg_file[4] = 4.0;
    reg_file[6] = 2.0;
    reg_file[10] = 10.0;
    reg_file[12] = 5.0;

    for (int i = 0; i < NUM_ADD_RS; i++) {
        add_rs[i].tag = TAG_ADD1 + i;
        add_rs[i].busy = false;
        add_rs[i].op = OP_NONE;
        add_rs[i].qj = TAG_NONE;
        add_rs[i].qk = TAG_NONE;
        add_rs[i].vj = 0.0;
        add_rs[i].vk = 0.0;
        add_rs[i].remaining_cycles = 0;
        add_rs[i].executing = false;
        add_rs[i].inst_id = 0;
    }

    for (int i = 0; i < NUM_MUL_RS; i++) {
        mul_rs[i].tag = TAG_MUL1 + i;
        mul_rs[i].busy = false;
        mul_rs[i].op = OP_NONE;
        mul_rs[i].qj = TAG_NONE;
        mul_rs[i].qk = TAG_NONE;
        mul_rs[i].vj = 0.0;
        mul_rs[i].vk = 0.0;
        mul_rs[i].remaining_cycles = 0;
        mul_rs[i].executing = false;
        mul_rs[i].inst_id = 0;
    }

    cdb.valid = false;
}

ReservationStation* find_free_rs(OpType op) {
    if (op == OP_ADD_D || op == OP_SUB_D) {
        for (int i = 0; i < NUM_ADD_RS; i++) {
            if (!add_rs[i].busy) return &add_rs[i];
        }
    } else if (op == OP_MUL_D || op == OP_DIV_D) {
        for (int i = 0; i < NUM_MUL_RS; i++) {
            if (!mul_rs[i].busy) return &mul_rs[i];
        }
    }
    return NULL;
}

int get_latency(OpType op) {
    switch (op) {
        case OP_ADD_D: return LATENCY_ADD;
        case OP_SUB_D: return LATENCY_SUB;
        case OP_MUL_D: return LATENCY_MUL;
        default:       return 1;
    }
}

/* Phase 1: Write Result / CDB Broadcast */
void step_write_result() {
    cdb.valid = false;
    ReservationStation* ready_rs = NULL;

    // Find stations that finished execution (remaining_cycles == 0)
    // Priority: Oldest issued instruction
    int earliest_issue = 999999;

    for (int i = 0; i < NUM_ADD_RS; i++) {
        if (add_rs[i].busy && add_rs[i].executing && add_rs[i].remaining_cycles == 0) {
            int inst_idx = add_rs[i].inst_id - 1;
            if (program[inst_idx].issue_cycle < earliest_issue) {
                earliest_issue = program[inst_idx].issue_cycle;
                ready_rs = &add_rs[i];
            }
        }
    }

    for (int i = 0; i < NUM_MUL_RS; i++) {
        if (mul_rs[i].busy && mul_rs[i].executing && mul_rs[i].remaining_cycles == 0) {
            int inst_idx = mul_rs[i].inst_id - 1;
            if (program[inst_idx].issue_cycle < earliest_issue) {
                earliest_issue = program[inst_idx].issue_cycle;
                ready_rs = &mul_rs[i];
            }
        }
    }

    if (ready_rs != NULL) {
        int inst_idx = ready_rs->inst_id - 1;
        double result = 0.0;
        switch (ready_rs->op) {
            case OP_ADD_D: result = ready_rs->vj + ready_rs->vk; break;
            case OP_SUB_D: result = ready_rs->vj - ready_rs->vk; break;
            case OP_MUL_D: result = ready_rs->vj * ready_rs->vk; break;
            default:       result = 0.0; break;
        }

        cdb.valid = true;
        cdb.tag = ready_rs->tag;
        cdb.value = result;
        cdb.inst_id = ready_rs->inst_id;

        program[inst_idx].write_cdb_cycle = current_cycle;
        program[inst_idx].completed = true;

        // Snooping / CDB Broadcast to Register Status Table
        for (int r = 0; r < NUM_REGS; r++) {
            if (reg_status[r].qi == ready_rs->tag) {
                reg_file[r] = result;
                reg_status[r].qi = TAG_NONE;
            }
        }

        // Snooping / CDB Broadcast to Reservation Stations
        for (int i = 0; i < NUM_ADD_RS; i++) {
            if (add_rs[i].busy) {
                if (add_rs[i].qj == ready_rs->tag) {
                    add_rs[i].vj = result;
                    add_rs[i].qj = TAG_NONE;
                }
                if (add_rs[i].qk == ready_rs->tag) {
                    add_rs[i].vk = result;
                    add_rs[i].qk = TAG_NONE;
                }
            }
        }
        for (int i = 0; i < NUM_MUL_RS; i++) {
            if (mul_rs[i].busy) {
                if (mul_rs[i].qj == ready_rs->tag) {
                    mul_rs[i].vj = result;
                    mul_rs[i].qj = TAG_NONE;
                }
                if (mul_rs[i].qk == ready_rs->tag) {
                    mul_rs[i].vk = result;
                    mul_rs[i].qk = TAG_NONE;
                }
            }
        }

        // Free the reservation station
        ready_rs->busy = false;
        ready_rs->executing = false;
        ready_rs->op = OP_NONE;
    }
}

/* Phase 2: Execute */
void step_execute() {
    for (int i = 0; i < NUM_ADD_RS; i++) {
        if (add_rs[i].busy) {
            if (add_rs[i].qj == TAG_NONE && add_rs[i].qk == TAG_NONE) {
                if (!add_rs[i].executing) {
                    // Start execution this cycle
                    add_rs[i].executing = true;
                    int inst_idx = add_rs[i].inst_id - 1;
                    program[inst_idx].exec_start_cycle = current_cycle;
                }
                if (add_rs[i].remaining_cycles > 0) {
                    add_rs[i].remaining_cycles--;
                    if (add_rs[i].remaining_cycles == 0) {
                        int inst_idx = add_rs[i].inst_id - 1;
                        program[inst_idx].exec_end_cycle = current_cycle;
                    }
                }
            }
        }
    }

    for (int i = 0; i < NUM_MUL_RS; i++) {
        if (mul_rs[i].busy) {
            if (mul_rs[i].qj == TAG_NONE && mul_rs[i].qk == TAG_NONE) {
                if (!mul_rs[i].executing) {
                    // Start execution this cycle
                    mul_rs[i].executing = true;
                    int inst_idx = mul_rs[i].inst_id - 1;
                    program[inst_idx].exec_start_cycle = current_cycle;
                }
                if (mul_rs[i].remaining_cycles > 0) {
                    mul_rs[i].remaining_cycles--;
                    if (mul_rs[i].remaining_cycles == 0) {
                        int inst_idx = mul_rs[i].inst_id - 1;
                        program[inst_idx].exec_end_cycle = current_cycle;
                    }
                }
            }
        }
    }
}

/* Phase 3: Issue */
void step_issue() {
    if (pc >= NUM_INSTRUCTIONS) return;

    Instruction* inst = &program[pc];
    ReservationStation* rs = find_free_rs(inst->op);

    if (rs != NULL) {
        rs->busy = true;
        rs->op = inst->op;
        rs->inst_id = inst->id;
        rs->remaining_cycles = get_latency(inst->op);
        rs->executing = false;

        // Operand 1 (src1)
        if (reg_status[inst->src1_reg].qi == TAG_NONE) {
            rs->vj = reg_file[inst->src1_reg];
            rs->qj = TAG_NONE;
        } else {
            rs->qj = reg_status[inst->src1_reg].qi;
        }

        // Operand 2 (src2)
        if (reg_status[inst->src2_reg].qi == TAG_NONE) {
            rs->vk = reg_file[inst->src2_reg];
            rs->qk = TAG_NONE;
        } else {
            rs->qk = reg_status[inst->src2_reg].qi;
        }

        // Tag destination register in status table
        reg_status[inst->dest_reg].qi = rs->tag;

        inst->issue_cycle = current_cycle;
        pc++;
    }
}

void print_state() {
    printf("\n========================================================================================\n");
    printf("                                     CYCLE %2d\n", current_cycle);
    printf("========================================================================================\n");

    // Instruction Status
    printf("\n[Instruction Status]\n");
    printf("%-4s | %-22s | %-6s | %-14s | %-12s\n", "ID", "Instruction", "Issue", "Exec Complete", "Write Result");
    printf("-----+------------------------+--------+---------------+-------------\n");
    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        char issue_str[32] = "--", exec_str[32] = "--", write_str[32] = "--";
        if (program[i].issue_cycle > 0) snprintf(issue_str, sizeof(issue_str), "%d", program[i].issue_cycle);
        if (program[i].exec_end_cycle > 0) snprintf(exec_str, sizeof(exec_str), "%d..%d", program[i].exec_start_cycle, program[i].exec_end_cycle);
        else if (program[i].exec_start_cycle > 0) snprintf(exec_str, sizeof(exec_str), "%d..exec", program[i].exec_start_cycle);
        if (program[i].write_cdb_cycle > 0) snprintf(write_str, sizeof(write_str), "%d", program[i].write_cdb_cycle);

        printf("%-4d | %-22s | %-6s | %-14s | %-12s\n",
               program[i].id, program[i].text, issue_str, exec_str, write_str);
    }

    // Reservation Stations
    printf("\n[Reservation Stations]\n");
    printf("%-5s | %-4s | %-5s | %-7s | %-7s | %-5s | %-5s | %-5s | %-8s\n",
           "Tag", "Busy", "Op", "Vj", "Vk", "Qj", "Qk", "Rem", "State");
    printf("------+------+-------+---------+---------+-------+-------+-------+----------\n");

    for (int i = 0; i < NUM_ADD_RS; i++) {
        char vj_str[12] = "--", vk_str[12] = "--";
        if (add_rs[i].busy) {
            if (add_rs[i].qj == TAG_NONE) snprintf(vj_str, sizeof(vj_str), "%.1f", add_rs[i].vj);
            if (add_rs[i].qk == TAG_NONE) snprintf(vk_str, sizeof(vk_str), "%.1f", add_rs[i].vk);
        }
        const char* state_str = "IDLE";
        if (add_rs[i].busy) {
            if (add_rs[i].executing) state_str = (add_rs[i].remaining_cycles == 0) ? "DONE" : "EXEC";
            else state_str = "WAIT";
        }
        printf("%-5s | %-4s | %-5s | %-7s | %-7s | %-5s | %-5s | %-5d | %-8s\n",
               tag_to_string(add_rs[i].tag),
               add_rs[i].busy ? "YES" : "NO",
               add_rs[i].busy ? op_to_string(add_rs[i].op) : "--",
               vj_str, vk_str,
               tag_to_string(add_rs[i].qj),
               tag_to_string(add_rs[i].qk),
               add_rs[i].remaining_cycles,
               state_str);
    }

    for (int i = 0; i < NUM_MUL_RS; i++) {
        char vj_str[12] = "--", vk_str[12] = "--";
        if (mul_rs[i].busy) {
            if (mul_rs[i].qj == TAG_NONE) snprintf(vj_str, sizeof(vj_str), "%.1f", mul_rs[i].vj);
            if (mul_rs[i].qk == TAG_NONE) snprintf(vk_str, sizeof(vk_str), "%.1f", mul_rs[i].vk);
        }
        const char* state_str = "IDLE";
        if (mul_rs[i].busy) {
            if (mul_rs[i].executing) state_str = (mul_rs[i].remaining_cycles == 0) ? "DONE" : "EXEC";
            else state_str = "WAIT";
        }
        printf("%-5s | %-4s | %-5s | %-7s | %-7s | %-5s | %-5s | %-5d | %-8s\n",
               tag_to_string(mul_rs[i].tag),
               mul_rs[i].busy ? "YES" : "NO",
               mul_rs[i].busy ? op_to_string(mul_rs[i].op) : "--",
               vj_str, vk_str,
               tag_to_string(mul_rs[i].qj),
               tag_to_string(mul_rs[i].qk),
               mul_rs[i].remaining_cycles,
               state_str);
    }

    // CDB Broadcast
    printf("\n[Common Data Bus (CDB)]\n");
    if (cdb.valid) {
        printf("BROADCAST -> Producer: %s, Value: %.2f (Inst #%d)\n",
               tag_to_string(cdb.tag), cdb.value, cdb.inst_id);
    } else {
        printf("INACTIVE (No broadcast this cycle)\n");
    }

    // Register Status (non-empty only)
    printf("\n[Register Status (Pending Producers)]\n");
    bool any_pending = false;
    for (int r = 0; r < NUM_REGS; r++) {
        if (reg_status[r].qi != TAG_NONE) {
            printf("F%-2d: Qi = %-5s (Value in file: %.2f)\n", r, tag_to_string(reg_status[r].qi), reg_file[r]);
            any_pending = true;
        }
    }
    if (!any_pending) {
        printf("All architectural registers hold valid values in Register File.\n");
    }
}

bool all_done() {
    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        if (!program[i].completed) return false;
    }
    return true;
}

int main() {
    printf("========================================================================================\n");
    printf("               TOMASULO ALGORITHM 4-INSTRUCTION DEMO SIMULATION TRACE                   \n");
    printf("                     CPEN 315 / CPEN 733 — Project 5 Starter Demo                       \n");
    printf("========================================================================================\n");
    printf("Latencies: ADD.D = %d cycles, SUB.D = %d cycles, MUL.D = %d cycles\n",
           LATENCY_ADD, LATENCY_SUB, LATENCY_MUL);
    printf("Reservation Stations: %d Adder/Sub (ADD1-3), %d Multiplier (MUL1-2)\n",
           NUM_ADD_RS, NUM_MUL_RS);

    init_simulator();

    while (!all_done() && current_cycle < 20) {
        current_cycle++;
        
        // 3-Phase Tomasulo pipeline in reverse order for correct cycle semantics:
        // 1. Write Result / Broadcast on CDB
        // 2. Execute functional units
        // 3. Issue next instruction
        step_write_result();
        step_execute();
        step_issue();

        print_state();
    }

    printf("\n========================================================================================\n");
    printf("                              SIMULATION COMPLETED AT CYCLE %d                           \n", current_cycle);
    printf("========================================================================================\n");
    printf("\nFinal Instruction Summary Table:\n");
    printf("%-4s | %-22s | %-6s | %-14s | %-12s\n", "ID", "Instruction", "Issue", "Exec Range", "Write Result");
    printf("-----+------------------------+--------+---------------+-------------\n");
    for (int i = 0; i < NUM_INSTRUCTIONS; i++) {
        printf("%-4d | %-22s | %-6d | %2d .. %2d        | %-12d\n",
               program[i].id, program[i].text,
               program[i].issue_cycle,
               program[i].exec_start_cycle, program[i].exec_end_cycle,
               program[i].write_cdb_cycle);
    }
    printf("\nFinal Register File Values:\n");
    printf("F2  = %.2f (expected 6.00)\n", reg_file[2]);
    printf("F8  = %.2f (expected 15.00)\n", reg_file[8]);
    printf("F14 = %.2f (expected 12.00)\n", reg_file[14]);
    printf("F16 = %.2f (expected 9.00)\n", reg_file[16]);

    return 0;
}
