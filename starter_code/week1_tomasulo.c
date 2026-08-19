#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#define NUM_ADD_RS   3
#define NUM_MUL_RS   2
#define NUM_INSTR    4
#define NUM_REGS     32

#define ADD_LATENCY  2
#define MUL_LATENCY  6

typedef enum { OP_ADDD, OP_MULD } OpType;

static const char *op_name(OpType op) {
    return (op == OP_ADDD) ? "ADD.D" : "MUL.D";
}

typedef struct {
    OpType op;
    int    dest;
    int    src1;
    int    src2;
} Instruction;

typedef struct {
    bool   busy;
    OpType op;
    double Vj, Vk;
    int    Qj, Qk;
    int    dest;
    int    timer;
    bool   executing;
    bool   result_ready;
    double result;
    int    tag;
    int    instr_index;
} ReservationStation;

static ReservationStation add_rs[NUM_ADD_RS];
static ReservationStation mul_rs[NUM_MUL_RS];

static int reg_status[NUM_REGS];
static double reg_file[NUM_REGS];

typedef struct {
    int issue_cycle;
    int exec_start_cycle;
    int exec_end_cycle;
    int writeback_cycle;
} InstrTiming;

static InstrTiming timing[NUM_INSTR];

static int tag_of_add_rs(int i) { return i; }
static int tag_of_mul_rs(int i) { return NUM_ADD_RS + i; }

static Instruction program[NUM_INSTR] = {
    { OP_ADDD, /*dest*/ 2,  /*src1*/ 4,  /*src2*/ 6  },
    { OP_ADDD, /*dest*/ 8,  /*src1*/ 10, /*src2*/ 12 },
    { OP_MULD, /*dest*/ 14, /*src1*/ 2,  /*src2*/ 8  },
    { OP_ADDD, /*dest*/ 16, /*src1*/ 14, /*src2*/ 4  },
};

static int next_to_issue = 0;

static bool issue(int cycle) {
    if (next_to_issue >= NUM_INSTR) return false;

    Instruction *ins = &program[next_to_issue];
    ReservationStation *pool = (ins->op == OP_ADDD) ? add_rs : mul_rs;
    int pool_size = (ins->op == OP_ADDD) ? NUM_ADD_RS : NUM_MUL_RS;

    int free_slot = -1;
    for (int i = 0; i < pool_size; i++) {
        if (!pool[i].busy) { free_slot = i; break; }
    }
    if (free_slot == -1) {
        printf("Cycle %2d: ISSUE stalled for I%d (%s) - structural hazard (no free %s RS)\n",
               cycle, next_to_issue, op_name(ins->op),
               ins->op == OP_ADDD ? "Adder" : "Multiplier");
        return false;
    }

    ReservationStation *rs = &pool[free_slot];
    rs->busy         = true;
    rs->op           = ins->op;
    rs->dest         = ins->dest;
    rs->executing    = false;
    rs->result_ready = false;
    rs->tag          = (ins->op == OP_ADDD) ? tag_of_add_rs(free_slot)
                                            : tag_of_mul_rs(free_slot);
    rs->instr_index  = next_to_issue;

    if (reg_status[ins->src1] == -1) {
        rs->Vj = reg_file[ins->src1];
        rs->Qj = -1;
    } else {
        rs->Qj = reg_status[ins->src1];
    }

    if (reg_status[ins->src2] == -1) {
        rs->Vk = reg_file[ins->src2];
        rs->Qk = -1;
    } else {
        rs->Qk = reg_status[ins->src2];
    }

    reg_status[ins->dest] = rs->tag;
    timing[next_to_issue].issue_cycle = cycle;

    printf("Cycle %2d: ISSUE   I%d (%s F%d,F%d,F%d) -> %s RS[tag=%d] Qj=%d Qk=%d\n",
           cycle, next_to_issue, op_name(ins->op),
           ins->dest, ins->src1, ins->src2,
           ins->op == OP_ADDD ? "ADD" : "MUL", rs->tag, rs->Qj, rs->Qk);

    next_to_issue++;
    return true;
}

static void execute_step(ReservationStation *rs, int latency, int cycle) {
    if (!rs->busy || rs->result_ready) return;

    if (!rs->executing) {
        if (rs->Qj == -1 && rs->Qk == -1) {
            rs->executing = true;
            rs->timer = latency;
            timing[rs->instr_index].exec_start_cycle = cycle;
            printf("Cycle %2d: EXEC    RS[tag=%d] (%s) begins execution (operands ready)\n",
                   cycle, rs->tag, op_name(rs->op));
        }
        return;
    }

    rs->timer--;
    if (rs->timer == 0) {
        rs->result_ready = true;
        rs->result = (rs->op == OP_ADDD) ? (rs->Vj + rs->Vk)
                                          : (rs->Vj * rs->Vk);
        timing[rs->instr_index].exec_end_cycle = cycle;
        printf("Cycle %2d: EXEC    RS[tag=%d] (%s) finishes execution, result=%.2f\n",
               cycle, rs->tag, op_name(rs->op), rs->result);
    }
}

static void write_result(int cycle) {
    ReservationStation *winner = NULL;

    for (int i = 0; i < NUM_ADD_RS; i++) {
        if (add_rs[i].result_ready) { winner = &add_rs[i]; break; }
    }
    if (!winner) {
        for (int i = 0; i < NUM_MUL_RS; i++) {
            if (mul_rs[i].result_ready) { winner = &mul_rs[i]; break; }
        }
    }
    if (!winner) return;

    int winner_instr = winner->instr_index;

    printf("Cycle %2d: CDB      RS[tag=%d] broadcasts result=%.2f\n",
           cycle, winner->tag, winner->result);

    reg_file[winner->dest] = winner->result;
    if (reg_status[winner->dest] == winner->tag) {
        reg_status[winner->dest] = -1;
    }
    if (winner_instr != -1) timing[winner_instr].writeback_cycle = cycle;

    ReservationStation *pools[2] = { add_rs, mul_rs };
    int sizes[2] = { NUM_ADD_RS, NUM_MUL_RS };
    for (int p = 0; p < 2; p++) {
        for (int i = 0; i < sizes[p]; i++) {
            ReservationStation *rs = &pools[p][i];
            if (!rs->busy) continue;
            if (rs->Qj == winner->tag) { rs->Vj = winner->result; rs->Qj = -1; }
            if (rs->Qk == winner->tag) { rs->Vk = winner->result; rs->Qk = -1; }
        }
    }

    winner->busy = false;
    winner->result_ready = false;
    winner->executing = false;
}

static void init_state(void) {
    for (int i = 0; i < NUM_REGS; i++) {
        reg_status[i] = -1;
        reg_file[i]   = 0.0;
    }
    reg_file[4]  = 2.0;
    reg_file[6]  = 3.0;
    reg_file[10] = 4.0;
    reg_file[12] = 5.0;

    for (int i = 0; i < NUM_ADD_RS; i++) memset(&add_rs[i], 0, sizeof(add_rs[i]));
    for (int i = 0; i < NUM_MUL_RS; i++) memset(&mul_rs[i], 0, sizeof(mul_rs[i]));
    for (int i = 0; i < NUM_INSTR; i++)  memset(&timing[i], 0, sizeof(timing[i]));
}

static bool all_done(void) {
    if (next_to_issue < NUM_INSTR) return false;
    for (int i = 0; i < NUM_ADD_RS; i++) if (add_rs[i].busy) return false;
    for (int i = 0; i < NUM_MUL_RS; i++) if (mul_rs[i].busy) return false;
    return true;
}

static void print_final_trace(void) {
    printf("\n===================== FINAL TIMING TABLE =====================\n");
    printf("Instr  Op     Dest  Issue  ExecStart  ExecEnd  Writeback\n");
    for (int i = 0; i < NUM_INSTR; i++) {
        printf(" I%d    %-5s  F%-3d  %5d  %9d  %7d  %9d\n",
               i, op_name(program[i].op), program[i].dest,
               timing[i].issue_cycle, timing[i].exec_start_cycle,
               timing[i].exec_end_cycle, timing[i].writeback_cycle);
    }
    printf("\n===================== FINAL REGISTER VALUES ==================\n");
    printf("F2  (I0 dest = F4+F6)        = %.2f   (expect 5.00)\n", reg_file[2]);
    printf("F8  (I1 dest = F10+F12)      = %.2f   (expect 9.00)\n", reg_file[8]);
    printf("F14 (I2 dest = F2*F8)        = %.2f   (expect 45.00)\n", reg_file[14]);
    printf("F16 (I3 dest = F14+F4)       = %.2f   (expect 47.00)\n", reg_file[16]);
}

int main(void) {
    init_state();

    printf("=====================================================\n");
    printf(" Tomasulo Demo - 4 instructions, ADD.D lat=%d, MUL.D lat=%d\n",
           ADD_LATENCY, MUL_LATENCY);
    printf(" I0: ADD.D F2 ,F4 ,F6\n");
    printf(" I1: ADD.D F8 ,F10,F12\n");
    printf(" I2: MUL.D F14,F2 ,F8   (RAW on I0 AND I1)\n");
    printf(" I3: ADD.D F16,F14,F4   (RAW on I2)\n");
    printf("=====================================================\n\n");

    int cycle = 0;
    int max_cycles = 100;
    while (!all_done() && cycle < max_cycles) {
        cycle++;
        issue(cycle);
        for (int i = 0; i < NUM_ADD_RS; i++) execute_step(&add_rs[i], ADD_LATENCY, cycle);
        for (int i = 0; i < NUM_MUL_RS; i++) execute_step(&mul_rs[i], MUL_LATENCY, cycle);
        write_result(cycle);
    }

    printf("\nSimulation finished after %d cycles.\n", cycle);
    print_final_trace();
    return 0;
}
