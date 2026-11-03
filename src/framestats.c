#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// framestats.c: --frame-stats=FILE, the per-frame load of the console's two budgets
//
//   one CSV row per frame:  frame,cycles,cpu_pct,gpu_pixels,gpu_pct,end
//     cycles      instructions run in the frame, a WAIT counted (TIM_CycleCounter when the frame ends)
//     cpu_pct     cycles as a percentage of the 250,000-cycle budget
//     gpu_pixels  pixels the frame drew, of the GPU_RemainingPixels capacity (9 screens)
//     gpu_pct     gpu_pixels as a percentage of that capacity
//     end         w the program waited for the next frame, b the cycle budget ran out
//
//   when the run ends a summary goes to stderr (mean, 95th percentile, maximum and the
//   frames at 100% of each budget). --frame-stats-skip=N leaves the first N frames out of
//   the summary (the BIOS logo takes about 250), the CSV keeps every frame.
//
// It is what v32prof (v32cxx tools/vircon32) prints, measured inside the simulator, so any
// run can be measured with the scripted input and commands of a v32sim session. The
// counters only read ports: the emulated machine is not changed.
//
////////////////////////////////////////////////////////////////////////////////////////

#define  FS_GPU_CAPACITY   (9 * 640 * 360)

int8_t    *frame_stats_out   = NULL;     // --frame-stats=FILE
uint32_t   frame_stats_skip  = 0;        // --frame-stats-skip=N

static FILE      *fs_file    = NULL;
static uint32_t  *fs_cpu     = NULL;     // cycles of every frame, for the summary
static uint32_t  *fs_gpu     = NULL;     // pixels of every frame
static size_t     fs_count   = 0;
static size_t     fs_room    = 0;

////////////////////////////////////////////////////////////////////////////////////////
//
// framestats_frame(): called when a frame ends, before the timer and the GPU start the next
//
void  framestats_frame (void)
{
    uint32_t  cycles  = 0;
    int32_t   remain  = 0;
    uint32_t  pixels  = 0;

    if (frame_stats_out == NULL)
    {
        return;
    }
    if (fs_file == NULL)
    {
        fs_file = fopen ((char *) frame_stats_out, "wb");
        if (fs_file == NULL)
        {
            fprintf (stderr, "[ERROR] could not write the frame statistics '%s'\n", frame_stats_out);
            frame_stats_out = NULL;
            return;
        }
        fprintf (fs_file, "frame,cycles,cpu_pct,gpu_pixels,gpu_pct,end\n");
    }

    cycles  = IPORTGET(TIM_CycleCounter);
    remain  = (int32_t) IPORTGET(GPU_RemainingPixels);
    pixels  = (remain < 0) ? FS_GPU_CAPACITY : (uint32_t) (FS_GPU_CAPACITY - remain);

    fprintf (fs_file, "%u,%u,%.2f,%u,%.2f,%c\n", (unsigned) (frames_run + 1), (unsigned) cycles,
             100.0 * (double) cycles / (double) V32_CYCLES_PER_FRAME,
             (unsigned) pixels, 100.0 * (double) pixels / (double) FS_GPU_CAPACITY,
             (waitflag == TRUE) ? 'w' : 'b');

    if (fs_count == fs_room)
    {
        size_t  room  = (fs_room == 0) ? 4096 : (fs_room * 2);
        void   *cpu   = realloc (fs_cpu, room * sizeof (uint32_t));
        void   *gpu   = realloc (fs_gpu, room * sizeof (uint32_t));

        if ((cpu == NULL) || (gpu == NULL))
        {
            return;                      // the CSV is complete, only the summary stops growing
        }
        fs_cpu   = cpu;
        fs_gpu   = gpu;
        fs_room  = room;
    }
    fs_cpu[fs_count]  = cycles;
    fs_gpu[fs_count]  = pixels;
    fs_count          = fs_count + 1;
}

static int  cmp_u32 (const void *a, const void *b)
{
    uint32_t  x  = *(const uint32_t *) a;
    uint32_t  y  = *(const uint32_t *) b;

    return ((x > y) - (x < y));
}

static void  summary_line (const char *name, uint32_t *v, size_t n, double budget)
{
    double    sum    = 0.0;
    size_t    index  = 0;
    size_t    full   = 0;
    uint32_t  p95    = 0;

    for (index = 0; index < n; index = index + 1)
    {
        sum = sum + (double) v[index];
        if ((double) v[index] >= budget)
        {
            full = full + 1;
        }
    }
    qsort (v, n, sizeof (uint32_t), cmp_u32);
    p95 = v[((n - 1) * 95) / 100];
    fprintf (stderr, "[frame-stats] %s: mean %.2f%%  p95 %.2f%%  max %.2f%%  frames at 100%%: %u\n", name,
             100.0 * (sum / (double) n) / budget, 100.0 * (double) p95 / budget,
             100.0 * (double) v[n - 1] / budget, (unsigned) full);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// framestats_write(): the run is over: close the CSV and print the summary
//
void  framestats_write (void)
{
    size_t  skip  = frame_stats_skip;

    if (fs_file != NULL)
    {
        fclose (fs_file);
        fs_file = NULL;
    }
    if ((fs_cpu == NULL) || (fs_count <= skip))
    {
        if (frame_stats_out != NULL)
        {
            fprintf (stderr, "[frame-stats] no frames to summarize (%u run, %u skipped)\n",
                     (unsigned) fs_count, (unsigned) skip);
        }
        return;
    }
    fprintf (stderr, "[frame-stats] %u frames (first %u skipped), csv %s\n",
             (unsigned) (fs_count - skip), (unsigned) skip, frame_stats_out);
    summary_line ("cpu", fs_cpu + skip, fs_count - skip, (double) V32_CYCLES_PER_FRAME);
    summary_line ("gpu", fs_gpu + skip, fs_count - skip, (double) FS_GPU_CAPACITY);
}
