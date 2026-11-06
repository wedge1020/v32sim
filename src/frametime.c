#include "defines.h"
#include <time.h>

////////////////////////////////////////////////////////////////////////////////////////
//
// frametime.c: --time-breakdown and --frame-log=FILE (VS-13 4.4), where v32sim's own wall-clock time goes
//
//   per frame, in microseconds:
//     cpu     the program's instructions (the time between the frames' ends, less gpu and spu)
//     gpu     the GPU commands: v32sim's software GPU (draws, clears, region tables)
//     spu     the SPU port writes and the per-frame mixing step
//     output  the end-of-frame work: statistics, traces, screenshots, scripted input, gamepad
//     sleep   the wait that paces the frame to --frame-us
//     work    cpu + gpu + spu + output
//     late    work - frame_us when positive (the frame took longer than its slot), else 0
//
// Relative indicators only: v32sim's GPU is a software model (it accounts the pixels of every draw and renders for screenshots,
// it is not OpenGL), so these figures say nothing about the DesktopEmulator's speed (that is what tests/ref's v32gpureplay and the trace files are for).
// The clock is only read while one of the two options is on, and the machine is not touched.
//
////////////////////////////////////////////////////////////////////////////////////////

uint8_t   time_breakdown  = FALSE;       // --time-breakdown
int8_t   *frame_log_out   = NULL;        // --frame-log=FILE

static uint64_t  gpu_ns = 0, spu_ns = 0;
static uint64_t  gpu_at = 0, spu_at = 0;
static FILE     *flog   = NULL;

typedef struct { double v[7]; } frow_t;
static frow_t   *rows   = NULL;
static size_t    nrows  = 0, room = 0;
static const char *const cols[7] = { "cpu_us", "gpu_us", "spu_us", "output_us", "sleep_us", "work_us", "late_us" };

uint64_t  ft_now (void)
{
    struct timespec ts;

    if ((time_breakdown == FALSE) && (frame_log_out == NULL))  return (0);
    clock_gettime (CLOCK_MONOTONIC, &ts);
    return ((uint64_t) ts.tv_sec * 1000000000ULL + (uint64_t) ts.tv_nsec);
}

void  ft_gpu (uint64_t t0)  { if (t0 != 0)  gpu_ns = gpu_ns + (ft_now () - t0); }
void  ft_spu (uint64_t t0)  { if (t0 != 0)  spu_ns = spu_ns + (ft_now () - t0); }

//
// update_frame(): the frame's work ended at t_enter's start (previous_end = when the previous frame ended), the end-of-frame
// work ran until t_throttle, the pacing wait until t_end
//
void  ft_enter (void)
{
    gpu_at  = gpu_ns;
    spu_at  = spu_ns;
}

void  ft_frame (uint64_t previous_end, uint64_t t_enter, uint64_t t_throttle, uint64_t t_end)
{
    double   cpu, gpu, spu, out, slp, work, late;
    frow_t  *r  = NULL;

    if ((t_enter == 0) || (previous_end == 0))  { gpu_ns = 0;  spu_ns = 0;  gpu_at = 0;  spu_at = 0;  return; }
    gpu   = (double) gpu_at / 1000.0;
    spu   = (double) spu_at / 1000.0;
    cpu   = (double) (t_enter - previous_end) / 1000.0 - gpu - spu;
    if (cpu < 0)  cpu = 0;
    out   = (double) (t_throttle - t_enter) / 1000.0 - (double) ((gpu_ns - gpu_at) + (spu_ns - spu_at)) / 1000.0;
    gpu   = gpu + (double) (gpu_ns - gpu_at) / 1000.0;
    spu   = spu + (double) (spu_ns - spu_at) / 1000.0;
    if (out < 0)  out = 0;
    slp   = (double) (t_end - t_throttle) / 1000.0;
    work  = cpu + gpu + spu + out;
    late  = (frame_us != 0) && (work > (double) frame_us) ? (work - (double) frame_us) : 0.0;
    gpu_ns = 0;  spu_ns = 0;  gpu_at = 0;  spu_at = 0;

    if (frame_log_out != NULL)
    {
        if (flog == NULL)
        {
            flog = fopen ((char *) frame_log_out, "wb");
            if (flog == NULL)
            {
                fprintf (stderr, "[ERROR] could not write the frame log '%s'\n", frame_log_out);
                frame_log_out = NULL;
            }
            else
            {
                fprintf (flog, "frame,cpu_us,gpu_us,spu_us,output_us,sleep_us,work_us,late_us\n");
            }
        }
        if (flog != NULL)
        {
            fprintf (flog, "%u,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f,%.1f\n", (unsigned) (frames_run), cpu, gpu, spu, out, slp, work, late);
        }
    }
    if (nrows == room)
    {
        size_t   n  = (room == 0) ? 4096 : room * 2;
        frow_t  *g  = (frow_t *) realloc (rows, n * sizeof (frow_t));

        if (g == NULL)  return;
        rows  = g;
        room  = n;
    }
    r = rows + nrows;
    r -> v[0] = cpu;  r -> v[1] = gpu;  r -> v[2] = spu;  r -> v[3] = out;  r -> v[4] = slp;  r -> v[5] = work;  r -> v[6] = late;
    nrows = nrows + 1;
}

static int  cmp_d (const void *a, const void *b)
{
    double  x  = *(const double *) a;
    double  y  = *(const double *) b;

    return ((x > y) - (x < y));
}

void  ft_write (void)
{
    size_t   skip  = frame_stats_skip;
    size_t   n     = 0;
    size_t   c     = 0;
    size_t   i     = 0;
    size_t   late  = 0;
    double  *v     = NULL;

    if (flog != NULL)
    {
        fclose (flog);
        flog = NULL;
    }
    if (time_breakdown == FALSE)  return;
    if (nrows <= skip)
    {
        fprintf (stderr, "[time] no frames to report (%u run, %u skipped)\n", (unsigned) nrows, (unsigned) skip);
        return;
    }
    n  = nrows - skip;
    v  = (double *) malloc (n * sizeof (double));
    if (v == NULL)  return;
    fprintf (stderr, "[time] %u frames (first %u skipped); v32sim's own wall-clock time per frame, microseconds (relative: not the DesktopEmulator's)\n",
             (unsigned) n, (unsigned) skip);
    fprintf (stderr, "[time] %-10s %10s %10s %10s %10s\n", "", "mean", "p95", "max", "share");
    for (c = 0; c < 7; c = c + 1)
    {
        double  sum = 0.0, wsum = 0.0;

        for (i = 0; i < n; i = i + 1)  { v[i] = rows[skip + i].v[c];  sum = sum + v[i];  wsum = wsum + rows[skip + i].v[5]; }
        if (c == 6)  { for (i = 0; i < n; i = i + 1)  if (v[i] > 0)  late = late + 1; }
        qsort (v, n, sizeof (double), cmp_d);
        fprintf (stderr, "[time] %-10s %10.1f %10.1f %10.1f", cols[c], sum / (double) n, v[((n - 1) * 95) / 100], v[n - 1]);
        if (c < 4 && wsum > 0)  fprintf (stderr, " %9.1f%%", 100.0 * sum / wsum);
        fprintf (stderr, "\n");
    }
    if (frame_us != 0)  fprintf (stderr, "[time] frames whose work took longer than --frame-us (%u): %u\n", (unsigned) frame_us, (unsigned) late);
    free (v);
}
