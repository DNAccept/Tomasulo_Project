/* =====================================================================
 * starter_tomasulo.c
 *
 * PROJECT 5 — Tomasulo on the Trotro Network
 * WEEK 2 deliverable (C/C++ Implementation Lead):
 *   "Implement reservation stations and register-status table in
 *    starter_tomasulo.c; implement the issue() function."
 *
 * Status of the three-phase pipeline in THIS file:
 *   issue()        -> DONE   (Week 2 requirement)
 *   execute()      -> TODO   (Week 3: functional-unit latencies,
 *                              CDB arbitration for simultaneous
 *                              completions)
 *   write_result() -> TODO   (Week 3: CDB broadcast() )
 *
 * Data structures implemented this week (per Part II, Project 5,
 * Section H "Architecture & Design Tasks"):
 *   - Reservation station: busy bit, op, Qj/Qk (source tags or ready
 *     values), unique tag identifying this station as a producer.
 *   - Register-status table: architectural register -> producing RS
 *     tag (or -1 if the register file already holds the current
 *     value).
 *
 * WEEK 2 CRITICAL GATE (Testing/Validation Lead sign-off required
 * before proceeding to Week 3): the hand-traced WAR/WAW example
 *     ADD.D F2,F4,F6  ->  MUL.D F2,F2,F8
 * must issue with the correct tags before this file is considered
 * complete for the week. See validate_war_waw_example() below and
 * the assertions in main().
 *
 * Build:   gcc -Wall -Wextra -std=c11 -o starter_tomasulo starter_tomasulo.c
 * Run:     ./starter_tomasulo
 * ===================================================================== */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>

/* ---------------------------------------------------------------------
 * Configuration (extend as your team's seed dictates)
 * ------------------------------------------------------------------- */
#define NUM_ADD_RS   3      /* reservation stations for the Adder      */
#define NUM_MUL_RS   2      /* reservation stations for the Multiplier */
#define NUM_REGS     32     /* F0..F31                                 */
#define MAX_INSTR    64     /* room for the full seeded kernel         */

#define ADD_LATENCY  2       /* per project brief */
#define MUL_LATENCY  6       /* per project brief */

typedef enum { OP_ADDD, OP_MULD, OP_SUBD, OP_DIVD } OpType;

static const char *op_name(OpType op) {
    switch (op) {
        case OP_ADDD: return "ADD.D";
        case OP_MULD: return "MUL.D";
        case OP_SUBD: return "SUB.D";
        case OP_DIVD: return "DIV.D";
    }
    return "?";
}

/* ---------------------------------------------------------------------
 * Instruction (static program / loaded kernel)
 * ------------------------------------------------------------------- */
typedef struct {
    OpType op;
    int    dest;
    int    src1;
    int    src2;
} Instruction;

static Instruction program[MAX_INSTR];
static int program_len = 0;
static int next_to_issue = 0;

/* ---------------------------------------------------------------------
 * Reservation station
 * ------------------------------------------------------------------- */
typedef struct {
    bool   busy;
    OpType op;
    double Vj, Vk;   /* operand values, once known                    */
    int    Qj, Qk;   /* producing RS tag, or -1 if value already known */
    int    dest;     /* destination architectural register            */
    int    tag;      /* this station's own identity                   */
    int    instr_index;

    /* --- fields used starting Week 3 (execute / write-result) --- */
    bool   executing;
    int    timer;
    bool   result_ready;
    double result;
} ReservationStation;

static ReservationStation add_rs[NUM_ADD_RS];
static ReservationStation mul_rs[NUM_MUL_RS];

/* ---------------------------------------------------------------------
 * Register-status table
 *   reg_status[r] == -1   -> register file holds the current value
 *   reg_status[r] == tag  -> reservation station <tag> will produce
 *                            the next value for register r
 * ------------------------------------------------------------------- */
static int    reg_status[NUM_REGS];
static double reg_file[NUM_REGS];

static int tag_of_add_rs(int i) { return i; }
static int tag_of_mul_rs(int i) { return NUM_ADD_RS + i; }

/* ---------------------------------------------------------------------
 * init_state()
 * ------------------------------------------------------------------- */
static void init_state(void) {
    for (int i = 0; i < NUM_REGS; i++) {
        reg_status[i] = -1;
        reg_file[i]   = 0.0;
    }
    for (int i = 0; i < NUM_ADD_RS; i++) memset(&add_rs[i], 0, sizeof(add_rs[i]));
    for (int i = 0; i < NUM_MUL_RS; i++) memset(&mul_rs[i], 0, sizeof(mul_rs[i]));
    next_to_issue = 0;
}

/* ---------------------------------------------------------------------
 * load_kernel()
 *   Week 3+: replace this with a real loader reading the team's
 *   seeded kernel emitted by gen_tomasulo_kernel.py. For Week 2 we
 *   hand-load the required WAR/WAW validation example plus a couple
 *   of extra instructions so issue() has something non-trivial to do.
 * ------------------------------------------------------------------- */
static void load_kernel(void) {
    program_len = 0;
    /* Required Week-2 validation checkpoint instructions: */
    program[program_len++] = (Instruction){ OP_ADDD, /*dest*/2, /*s1*/4, /*s2*/6 };
    program[program_len++] = (Instruction){ OP_MULD, /*dest*/2, /*s1*/2, /*s2*/8 };
    /* A couple of extra instructions to exercise multiple RS slots: */
    program[program_len++] = (Instruction){ OP_ADDD, /*dest*/10, /*s1*/12, /*s2*/14 };
    program[program_len++] = (Instruction){ OP_SUBD, /*dest*/16, /*s1*/10, /*s2*/2  };
}

/* =======================================================================
 * issue()  --  IMPLEMENTED (Week 2 requirement)
 *
 * Contract:
 *   - Issues at most one instruction per call, in strict program order.
 *   - Returns true if an instruction issued, false otherwise (either
 *     the program is exhausted, or a structural hazard stalled issue
 *     because no reservation station of the needed type is free).
 *   - Snapshots each source operand: if the register-status table
 *     shows no pending producer (-1), the CURRENT register-file value
 *     is captured into Vj/Vk. Otherwise the PRODUCING TAG is captured
 *     into Qj/Qk, and the value is filled in later by write_result()
 *     (Week 3) when that tag broadcasts on the CDB.
 *   - Performs register renaming: reg_status[dest] is overwritten with
 *     this reservation station's own tag. This is precisely what
 *     eliminates WAR and WAW hazards -- any later instruction reading
 *     or rewriting the same architectural register gets THIS
 *     instruction's tag (or a still-newer one), never a stale one.
 * ===================================================================== */
static bool issue(int cycle) {
    if (next_to_issue >= program_len) return false;

    Instruction *ins = &program[next_to_issue];

    bool is_add = (ins->op == OP_ADDD || ins->op == OP_SUBD);
    ReservationStation *pool = is_add ? add_rs : mul_rs;
    int pool_size            = is_add ? NUM_ADD_RS : NUM_MUL_RS;

    /* Find a free reservation station of the correct functional-unit
     * type. If none is free, this is a structural hazard: issue must
     * stall (per the project brief's "report a structural-hazard
     * stall if exceeded" requirement). */
    int free_slot = -1;
    for (int i = 0; i < pool_size; i++) {
        if (!pool[i].busy) { free_slot = i; break; }
    }
    if (free_slot == -1) {
        printf("Cycle %2d: ISSUE stalled for I%d (%s) - structural hazard: "
               "no free %s reservation station\n",
               cycle, next_to_issue, op_name(ins->op),
               is_add ? "Adder" : "Multiplier");
        return false;
    }

    ReservationStation *rs = &pool[free_slot];
    rs->busy        = true;
    rs->op          = ins->op;
    rs->dest        = ins->dest;
    rs->instr_index = next_to_issue;
    rs->tag         = is_add ? tag_of_add_rs(free_slot) : tag_of_mul_rs(free_slot);
    rs->executing    = false;
    rs->result_ready = false;

    /* --- Operand 1 --- */
    if (reg_status[ins->src1] == -1) {
        rs->Vj = reg_file[ins->src1];
        rs->Qj = -1;
    } else {
        rs->Qj = reg_status[ins->src1];
    }

    /* --- Operand 2 --- */
    if (reg_status[ins->src2] == -1) {
        rs->Vk = reg_file[ins->src2];
        rs->Qk = -1;
    } else {
        rs->Qk = reg_status[ins->src2];
    }

    /* --- Register renaming: eliminates WAR/WAW on ins->dest --- */
    reg_status[ins->dest] = rs->tag;

    printf("Cycle %2d: ISSUE   I%d (%s F%d,F%d,F%d) -> %s RS[tag=%d] "
           "Qj=%d Qk=%d Vj=%s Vk=%s\n",
           cycle, next_to_issue, op_name(ins->op),
           ins->dest, ins->src1, ins->src2,
           is_add ? "ADD" : "MUL", rs->tag, rs->Qj, rs->Qk,
           rs->Qj == -1 ? "known" : "pending",
           rs->Qk == -1 ? "known" : "pending");

    next_to_issue++;
    return true;
}

/* =======================================================================
 * execute()  --  TODO (Week 3)
 *
 * Requirements for Week 3:
 *   - A station may begin execution only once Qj == -1 && Qk == -1.
 *   - Once started, count down the functional-unit latency
 *     (ADD_LATENCY / MUL_LATENCY).
 *   - On reaching zero, compute the result and mark result_ready,
 *     but do NOT write it anywhere yet -- that is write_result()'s job.
 * ===================================================================== */
static void execute(int cycle) {
    (void)cycle;
    /* TODO (Week 3):
     *   for each busy, non-executing station with Qj==-1 && Qk==-1:
     *       start executing, set timer = latency for its op type
     *   for each executing station:
     *       timer--; if timer == 0: compute result, result_ready = true
     */
}

/* =======================================================================
 * write_result()  --  TODO (Week 3)
 *
 * Requirements for Week 3:
 *   - At most ONE reservation station may broadcast on the CDB per
 *     cycle -- this is where your team's CDB arbitration policy
 *     (Week 3 architecture task) is implemented for the case of two
 *     functional units completing in the same cycle.
 *   - Broadcasting station's result must:
 *       1. Update reg_file[dest] and clear reg_status[dest] if it
 *          still points at this producer's tag.
 *       2. Be snooped by every OTHER busy reservation station: any
 *          Qj/Qk matching the broadcasting tag captures the value and
 *          clears to -1.
 *       3. Free the producing station (busy = false).
 * ===================================================================== */
static void write_result(int cycle) {
    (void)cycle;
    /* TODO (Week 3):
     *   pick at most one result_ready station (apply CDB arbitration
     *   policy if more than one is ready);
     *   commit to reg_file / reg_status;
     *   snoop-update every other busy station's Qj/Qk;
     *   free the producing station.
     */
}

/* ---------------------------------------------------------------------
 * validate_war_waw_example()
 *
 * WEEK 2 CRITICAL GATE. Hand-traces:
 *     I0: ADD.D F2, F4, F6
 *     I1: MUL.D F2, F2, F8      (reuses F2 as both a source and dest)
 *
 * Expected behaviour straight out of issue():
 *   - I0 issues into some ADD RS (call its tag T0); reg_status[F2] = T0.
 *   - I1 issues into a MUL RS. Its src1 (F2) sees reg_status[F2] == T0,
 *     so Qj = T0 (NOT a stale/incorrect value). Its dest (F2) then gets
 *     RENAMED: reg_status[F2] is overwritten with I1's own MUL tag.
 *   - This is exactly what prevents a WAR hazard: a hypothetical third
 *     instruction reading F2 after I1 issues will correctly wait on
 *     I1's tag, never on I0's stale producer.
 * ------------------------------------------------------------------- */
static void validate_war_waw_example(void) {
    init_state();
    program_len = 0;
    program[program_len++] = (Instruction){ OP_ADDD, 2, 4, 6 };
    program[program_len++] = (Instruction){ OP_MULD, 2, 2, 8 };

    printf("\n--- WEEK 2 VALIDATION CHECKPOINT: ADD.D F2,F4,F6 -> "
           "MUL.D F2,F2,F8 ---\n");

    int cycle = 1;
    bool ok0 = issue(cycle++);
    assert(ok0 && "I0 should issue successfully");
    int add_tag = add_rs[0].tag;               /* I0's producing tag   */
    assert(reg_status[2] == add_tag &&
           "reg_status[F2] must point at I0's ADD RS tag after I0 issues");

    bool ok1 = issue(cycle++);
    assert(ok1 && "I1 should issue successfully");
    assert(mul_rs[0].Qj == add_tag &&
           "I1's Qj must equal I0's tag (true RAW dependency correctly "
           "captured)");
    assert(reg_status[2] == mul_rs[0].tag &&
           "reg_status[F2] must be RENAMED to I1's own MUL RS tag "
           "(this is the WAR-hazard-eliminating step)");
    assert(reg_status[2] != add_tag &&
           "reg_status[F2] must NO LONGER point at I0's stale tag");

    printf("PASS: register renaming correctly eliminates the WAR hazard "
           "on F2.\n");
    printf("      I0 tag=%d, I1 tag=%d, reg_status[F2] now = %d\n\n",
           add_tag, mul_rs[0].tag, reg_status[2]);
}

/* ---------------------------------------------------------------------
 * main()
 * ------------------------------------------------------------------- */
int main(void) {
    /* Required Week-2 gate: must pass before the team proceeds. */
    validate_war_waw_example();

    /* Demonstrate issue() over the small Week-2 kernel. execute() and
     * write_result() are TODO stubs this week, so reservation stations
     * will never free up -- this is expected; Week 3 completes the
     * pipeline. */
    init_state();
    load_kernel();

    printf("--- Issuing the Week 2 sample kernel (execute/write-result "
           "not yet implemented) ---\n");
    int cycle = 0;
    while (next_to_issue < program_len && cycle < 20) {
        cycle++;
        issue(cycle);
        execute(cycle);       /* TODO stub */
        write_result(cycle);  /* TODO stub */
    }

    printf("\nIssued %d of %d instructions before reservation stations "
           "filled (expected, since execute()/write_result() are not "
           "yet implemented).\n", next_to_issue, program_len);
    return 0;
}
