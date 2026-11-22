#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// turbo.c: --turbo (see README "Turbo mode"). Besides what it switches on elsewhere (no
// real-time pacing, a full GPU pixel queue is dropped instead of painted: gpu.c), turbo
// fast-forwards SPIN LOOPS: a program that polls (typically TIM_FrameCounter, or a button)
// until the frame changes burns the rest of the frame's 250000 cycles one iteration at a
// time. When a loop is seen to repeat exactly, the iterations that cannot change anything
// are skipped in one step.
//
// "Repeats exactly" means, at two taken branches to the same address: all 16 registers are
// equal, every RAM word stored to in between holds again the value it had at the first
// branch (a CALL and a RET leave the stack as they found it), and in between no port was
// written, no volatile port was read (TIM_CycleCounter, RNG_CurrentValue), no card memory was
// written and no frame ended. Such a loop computes the same
// thing again and again until the cycle budget runs out, so the skipped iterations are
// replaced by their cost: TIM_CycleCounter, the instruction count and the per-frame
// instruction / gamepad-read / memory-card counters advance as they would have. The last
// iterations are always run for real, so the frame ends at the same instruction with the
// same registers as without --turbo; a loop whose exit depends on something that changes
// within the frame (the cycle counter, a store) is never skipped.
//
// The skipping is off whenever something would see the skipped instructions one by one:
// breakpoints, watchpoints, single-stepping and source stepping, the profilers, the
// V32TRACE recorder, 'watch for opcode', -v / -D output.
//
////////////////////////////////////////////////////////////////////////////////////////

uint8_t   turbo_mode       = FALSE;        // --turbo
uint8_t   turbo_report     = FALSE;        // --turbo=report: a summary line at exit
uint32_t  spin_epoch       = 0;            // bumped by anything a repeating loop could not repeat over
uint64_t  spin_skipped     = 0;            // cycles fast-forwarded (statistics, --turbo=report prints it at exit)

#define  SPIN_REGS    16
#define  SPIN_MISSES  8
#define  SPIN_LOG     32                   // distinct RAM words a loop may store to and still count as repeating

uint8_t   spin_tracking    = FALSE;        // a snapshot is live: stores are logged (memory.c calls spin_note_store)

static struct { data_t *cell; int32_t old; }  slog[SPIN_LOG];
static int  nlog  = 0;

//
// a store is about to change *cell: remember what it held at the snapshot, once per word
//
void  spin_note_store (data_t *cell)
{
    int  i  = 0;

    for (i = 0; i < nlog; i = i + 1)
    {
        if (slog[i].cell == cell)
        {
            return;
        }
    }
    if (nlog == SPIN_LOG)
    {
        spin_epoch  = spin_epoch + 1;                 // too many different words: give up on this snapshot
        return;
    }
    slog[nlog].cell  = cell;
    slog[nlog].old   = cell -> value.i32;
    nlog             = nlog + 1;
}

static int  memory_restored (void)
{
    int  i  = 0;

    for (i = 0; i < nlog; i = i + 1)
    {
        if (slog[i].cell -> value.i32 != slog[i].old)
        {
            return (FALSE);
        }
    }
    return (TRUE);
}

static struct
{
    uint8_t   valid;
    uint32_t  target;
    uint32_t  epoch;
    uint32_t  cycles;
    uint64_t  insns;
    uint32_t  perf[3];
    int32_t   regs[SPIN_REGS];
    int       misses;
}  snap;

void  turbo_reset (void)                   // a frame ended, or the machine was changed by hand
{
    snap.valid     = FALSE;
    spin_tracking  = FALSE;
}

void  turbo_spin_observe (void)
{
    uint32_t   target  = (uint32_t) rom_offset;
    int32_t   *cycles  = &(*(ioports + ((TIM_CycleCounter & 0x0700) >> 8)))[TIM_CycleCounter & 0x00FF].value.i32;
    int        i       = 0;

    if ((runflag == FALSE) || (bpoint != NULL) || (wpoint != NULL) || (profileflag == TRUE) ||
        (addrprof_active == TRUE) || (v32trace_on == TRUE) || (srcdebug_stepping == TRUE) ||
        (watch_word != 0xFFFFFFFF) || (seek_word != 0xFFFFFFFF) || (verbose != devnull) || (debug != devnull) ||
        (nextflag == TRUE))
    {
        snap.valid     = FALSE;
        spin_tracking  = FALSE;
        return;
    }

    if ((snap.valid == TRUE) && (snap.epoch == spin_epoch))
    {
        if (snap.target == target)
        {
            for (i = 0; i < SPIN_REGS; i = i + 1)
            {
                if (snap.regs[i] != REG(i))
                {
                    break;
                }
            }
            if ((i == SPIN_REGS) && (memory_restored () == TRUE))
            {
                //
                // the loop repeats: one trip costs 'period' cycles. Skip all the whole trips
                // that leave at least one cycle of the budget (the frame must end by a
                // real instruction)
                //
                uint32_t  now     = (uint32_t) *cycles;
                uint32_t  period  = now - snap.cycles;

                if ((period > 0) && (now < V32_CYCLES_PER_FRAME - 1))
                {
                    uint32_t  k  = (V32_CYCLES_PER_FRAME - 1 - now) / period;

                    if (k > 0)
                    {
                        uint32_t  perf[3];

                        perf_repeat_snapshot (perf);
                        *cycles         = (int32_t) (now + k * period);
                        insn_count      = insn_count + (uint64_t) k * (insn_count - snap.insns);
                        perf_repeat_add (snap.perf, perf, k);
                        spin_skipped    = spin_skipped + (uint64_t) k * period;
                    }
                }
                snap.valid     = FALSE;             // look afresh (the remaining trips run for real)
                spin_tracking  = FALSE;
                return;
            }
        }
        if (++snap.misses < SPIN_MISSES)
        {
            return;
        }
    }

    snap.valid     = TRUE;
    spin_tracking  = TRUE;
    nlog           = 0;
    snap.target  = target;
    snap.epoch   = spin_epoch;
    snap.cycles  = (uint32_t) *cycles;
    snap.insns   = insn_count;
    snap.misses  = 0;
    perf_repeat_snapshot (snap.perf);
    for (i = 0; i < SPIN_REGS; i = i + 1)
    {
        snap.regs[i]  = REG(i);
    }
}
