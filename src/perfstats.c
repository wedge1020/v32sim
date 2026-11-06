#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// perfstats.c: activity counters of the console, per frame and per region (VS-13)
//
//   every counter has a column name (the --frame-stats CSV) and a stable dotted name (expect, JSON):
//
//     draws            gpu.draws             draw commands the program issued (dropped ones included)
//     dropped          gpu.dropped           draws the GPU did not render: the frame's pixel budget was spent
//     clears           gpu.clears            clear_screen commands
//     tex_switches     gpu.tex_switches      draws whose texture differs from the previous draw's
//     region_switches  gpu.region_switches   draws whose (texture, region) differs from the previous draw's
//     regions          gpu.regions           distinct (texture, region) pairs drawn
//     spu_cmds         spu.cmds              SPU commands (play, pause, stop, ...)
//     spu_plays        spu.plays             of them, play_channel
//     scaled_draws     gpu.scaled_draws      zoomed and rotozoomed draws
//     rotated_draws    gpu.rotated_draws     rotated and rotozoomed draws
//     region_defs      gpu.region_defs       writes to the region definition ports (min/max/hotspot)
//     color_changes    gpu.color_changes     writes to GPU_MultiplyColor
//     blend_changes    gpu.blend_changes     writes to GPU_ActiveBlending
//     spu_assigns      spu.assignments       channel sound assignments
//     spu_params       spu.param_writes      channel volume / speed / loop / position writes
//     movs_words       bus.movs_words        words moved by MOVS
//     sets_words       bus.sets_words        words set by SETS
//     gamepad_reads    bus.gamepad_reads     reads of the gamepad ports by the program
//     instructions     cpu.instructions      instructions executed (a MOVS / SETS word counts as one)
//     cycles           cpu.cycles            cycles the frame took (TIM_CycleCounter when it ends)
//     gpu_pixels       gpu.pixels            pixels the frame drew
//
//   every frame's counters are kept (stats on, the default), so each can be asked for as NAME.AGG
//   with AGG one of last mean min max p50 p95 p99 total (no AGG = last frame). 'stats' shows them,
//   'stats save FILE [json]' writes them, --frame-stats puts them in the CSV / JSON Lines and the
//   top regions in its summary. The old names (draws, total_draws, max_draws) keep working.
//
// The counters only watch commands go by; the machine is not changed.
//
////////////////////////////////////////////////////////////////////////////////////////

enum { C_DRAWS, C_DROPPED, C_CLEARS, C_TEXSW, C_REGSW, C_REGIONS, C_SPUCMDS, C_SPUPLAYS,
       C_SCALED, C_ROTATED, C_REGDEFS, C_COLORS, C_BLENDS, C_SPUASSIGN, C_SPUPARAM, C_MOVS, C_SETS, C_PADREADS, C_INSTR,
       C_CYCLES, C_PIXELS, P_N };

#define  C_CSV_N  C_CYCLES                       // the counters that follow the original CSV columns; cycles and pixels have columns of their own

typedef struct { const char *csv; const char *name; } cdef_t;

static const cdef_t  cdef[P_N] =
{
    { "draws", "gpu.draws" }, { "dropped", "gpu.dropped" }, { "clears", "gpu.clears" }, { "tex_switches", "gpu.tex_switches" },
    { "region_switches", "gpu.region_switches" }, { "regions", "gpu.regions" }, { "spu_cmds", "spu.cmds" }, { "spu_plays", "spu.plays" },
    { "scaled_draws", "gpu.scaled_draws" }, { "rotated_draws", "gpu.rotated_draws" }, { "region_defs", "gpu.region_defs" },
    { "color_changes", "gpu.color_changes" }, { "blend_changes", "gpu.blend_changes" }, { "spu_assigns", "spu.assignments" },
    { "spu_params", "spu.param_writes" }, { "movs_words", "bus.movs_words" }, { "sets_words", "bus.sets_words" },
    { "gamepad_reads", "bus.gamepad_reads" }, { "instructions", "cpu.instructions" }, { "cycles", "cpu.cycles" }, { "gpu_pixels", "gpu.pixels" }
};

#define  HIST_MAX_FRAMES  (4u * 1024u * 1024u)

static uint32_t   cur[P_N];
static uint32_t   last[P_N];
static uint64_t   sum[P_N];
static uint32_t   peak[P_N];
static uint32_t  *hist        = NULL;            // every frame's counters, P_N each
static size_t     hist_n      = 0;
static size_t     hist_room   = 0;
static size_t     hist_base   = 0;               // the frames before it (the BIOS logo, --frame-stats-skip) are not part of the aggregates
static int        hist_on     = TRUE;
static size_t     hist_frame0 = 0;               // frames that ran before the history began (stats reset)

#define  RT_SIZE  16384                          // power of two; ~64 distinct regions is typical, 4096 the most a cartridge can have

typedef struct { int32_t tex; int32_t region; uint32_t used; uint32_t draws; uint64_t pixels; uint64_t dropped; uint32_t stamp; } rentry_t;

static uint32_t   nframes     = 0;
static uint64_t   total_pixels = 0;
static rentry_t  *rt          = NULL;
static int32_t    prev_tex    = -2;
static int32_t    prev_reg    = -2;
static uint32_t   skip_n      = 0;                // --frame-stats-skip: the first frames are left out of the run totals
static uint32_t   stamp       = 1;                // frame number for the per-frame distinct count

static rentry_t  *region_slot (int32_t tex, int32_t region)
{
    uint32_t  h  = ((uint32_t) (tex + 2) * 2654435761u) ^ ((uint32_t) region * 40503u);
    uint32_t  n  = 0;

    if (rt == NULL)
    {
        rt  = (rentry_t *) calloc (RT_SIZE, sizeof (rentry_t));
        if (rt == NULL)
        {
            return (NULL);
        }
    }
    for (n = 0; n < RT_SIZE; n = n + 1)
    {
        rentry_t  *e  = rt + ((h + n) & (RT_SIZE - 1));

        if (e -> used == 0)
        {
            e -> used    = 1;
            e -> tex     = tex;
            e -> region  = region;
            return (e);
        }
        if ((e -> tex == tex) && (e -> region == region))
        {
            return (e);
        }
    }
    return (NULL);
}

void  perf_gpu_draw (int32_t tex, int32_t region, int32_t before, int32_t after)
{
    rentry_t  *e        = region_slot (tex, region);
    int        dropped  = ((before < 0) || (after < 0)) ? TRUE : FALSE;

    cur[C_DRAWS]  = cur[C_DRAWS] + 1;
    addrprof_event (PROFEV_GPU_DRAW);
    if (dropped == TRUE)
    {
        cur[C_DROPPED]  = cur[C_DROPPED] + 1;
    }
    if (tex != prev_tex)                         cur[C_TEXSW]  = cur[C_TEXSW] + 1;
    if ((tex != prev_tex) || (region != prev_reg))  cur[C_REGSW]  = cur[C_REGSW] + 1;
    prev_tex  = tex;
    prev_reg  = region;
    if (e != NULL)
    {
        if (e -> stamp != stamp)
        {
            e -> stamp       = stamp;
            cur[C_REGIONS]   = cur[C_REGIONS] + 1;
        }
        e -> draws  = e -> draws + 1;
        if (dropped == TRUE)  e -> dropped  = e -> dropped + 1;
        else                  e -> pixels   = e -> pixels + (uint64_t) (before - after);
    }
    if (dropped == FALSE)
    {
        total_pixels  = total_pixels + (uint64_t) (before - after);
    }
}

void  perf_gpu_clear (void)
{
    cur[C_CLEARS]  = cur[C_CLEARS] + 1;
}

void  perf_gpu_command (int32_t command)
{
    if ((command == GPUCommand_DrawRegionZoomed) || (command == GPUCommand_DrawRegionRotozoomed))   cur[C_SCALED]   = cur[C_SCALED] + 1;
    if ((command == GPUCommand_DrawRegionRotated) || (command == GPUCommand_DrawRegionRotozoomed))  cur[C_ROTATED]  = cur[C_ROTATED] + 1;
}

void  perf_spu_command (int32_t command)
{
    cur[C_SPUCMDS]  = cur[C_SPUCMDS] + 1;
    if (command == SPUCommand_PlaySelectedChannel)
    {
        cur[C_SPUPLAYS]  = cur[C_SPUPLAYS] + 1;
    }
}

//
// a write the program made to a port (not the simulator's own: ioports_set with sys_force FALSE)
//
void  perf_port_write (uint16_t port)
{
    if      ((port & 0x700) == 0x200)  addrprof_event (PROFEV_GPU_PORTWRITE);
    else if ((port & 0x700) == 0x300)  addrprof_event (PROFEV_SPU_WRITE);
    switch (port)
    {
        case GPU_MultiplyColor:        cur[C_COLORS]  = cur[C_COLORS] + 1;  break;
        case GPU_ActiveBlending:       cur[C_BLENDS]  = cur[C_BLENDS] + 1;  break;
        case GPU_RegionMinX:
        case GPU_RegionMinY:
        case GPU_RegionMaxX:
        case GPU_RegionMaxY:
        case GPU_RegionHotspotX:
        case GPU_RegionHotspotY:       cur[C_REGDEFS] = cur[C_REGDEFS] + 1;  break;
        case SPU_ChannelAssignedSound: cur[C_SPUASSIGN] = cur[C_SPUASSIGN] + 1;  break;
        case SPU_ChannelVolume:
        case SPU_ChannelSpeed:
        case SPU_ChannelLoopEnabled:
        case SPU_ChannelPosition:      cur[C_SPUPARAM] = cur[C_SPUPARAM] + 1;  break;
        default:                       break;
    }
}

void  perf_port_read (uint16_t port)
{
    if ((port >= INP_GamepadLeft) && (port <= INP_GamepadButtonR))
    {
        cur[C_PADREADS]  = cur[C_PADREADS] + 1;
    }
}

void  perf_instruction (void)             { cur[C_INSTR] = cur[C_INSTR] + 1; }
void  perf_bus_words (int sets)           { if (sets) cur[C_SETS] = cur[C_SETS] + 1; else cur[C_MOVS] = cur[C_MOVS] + 1; }

//
// the two budget counters of the frame that is ending
//
static void  sample (void)
{
    int32_t  remain  = (int32_t) ISYSPORTGET(GPU_RemainingPixels);

    cur[C_CYCLES]  = (uint32_t) ISYSPORTGET(TIM_CycleCounter);
    cur[C_PIXELS]  = (remain < 0) ? (9u * 640u * 360u) : (uint32_t) (9 * 640 * 360 - remain);
}

//
// a frame has ended (after --frame-stats has read the counters of the frame)
//
void  perf_new_frame (void)
{
    int  i  = 0;

    sample ();
    if (hist_on == TRUE)
    {
        if (hist_n == hist_room)
        {
            size_t    room = (hist_room == 0) ? 4096 : (hist_room * 2);
            uint32_t *grow = (hist_n >= HIST_MAX_FRAMES) ? NULL : (uint32_t *) realloc (hist, room * P_N * sizeof (uint32_t));

            if (grow != NULL)
            {
                hist       = grow;
                hist_room  = room;
            }
        }
        if (hist_n < hist_room)
        {
            memcpy (hist + hist_n * P_N, cur, sizeof (cur));
            hist_n  = hist_n + 1;
        }
    }
    for (i = 0; i < P_N; i = i + 1)
    {
        last[i]  = cur[i];
        sum[i]   = sum[i] + cur[i];
        if (cur[i] > peak[i])  peak[i] = cur[i];
        cur[i]   = 0;
    }
    nframes  = nframes + 1;
    stamp    = stamp + 1;
    if ((skip_n > 0) && (nframes == skip_n))
    {
        memset (sum, 0, sizeof (sum));                     // what ran so far (the BIOS logo) is not part of the run
        memset (peak, 0, sizeof (peak));
        nframes       = 0;
        total_pixels  = 0;
        hist_base     = hist_n;
        if (rt != NULL)  memset (rt, 0, RT_SIZE * sizeof (rentry_t));
        skip_n        = 0;
    }
}

void  perf_skip_frames (uint32_t n)
{
    skip_n  = n;
}

void  perf_reset (void)
{
    memset (cur, 0, sizeof (cur));
    memset (last, 0, sizeof (last));
    memset (sum, 0, sizeof (sum));
    memset (peak, 0, sizeof (peak));
    hist_frame0   = hist_frame0 + hist_n;
    hist_n        = 0;
    hist_base     = 0;
    nframes       = 0;
    total_pixels  = 0;
    prev_tex      = -2;
    prev_reg      = -2;
    if (rt != NULL)  memset (rt, 0, RT_SIZE * sizeof (rentry_t));
}

void  perf_history (int on)
{
    hist_on  = on;
}

////////////////////////////////////////////////////////////////////////////////////////
//
// aggregates over the history
//
static int  cmp_u32 (const void *a, const void *b)
{
    uint32_t  x  = *(const uint32_t *) a;
    uint32_t  y  = *(const uint32_t *) b;

    return ((x > y) - (x < y));
}

typedef struct { double mean; double total; uint32_t min; uint32_t max; uint32_t p50; uint32_t p95; uint32_t p99; size_t n; } agg_t;

static int  aggregate (int c, agg_t *a)
{
    size_t     n  = hist_n - hist_base;
    size_t     i  = 0;
    uint32_t  *v  = NULL;

    memset (a, 0, sizeof (*a));
    if ((hist_on == FALSE) || (n == 0) || (hist == NULL))
    {
        return (-1);
    }
    v  = (uint32_t *) malloc (n * sizeof (uint32_t));
    if (v == NULL)
    {
        return (-1);
    }
    for (i = 0; i < n; i = i + 1)
    {
        v[i]      = hist[(hist_base + i) * P_N + c];
        a -> total  = a -> total + v[i];
    }
    qsort (v, n, sizeof (uint32_t), cmp_u32);
    a -> n     = n;
    a -> mean  = a -> total / (double) n;
    a -> min   = v[0];
    a -> max   = v[n - 1];
    a -> p50   = v[((n - 1) * 50) / 100];
    a -> p95   = v[((n - 1) * 95) / 100];
    a -> p99   = v[((n - 1) * 99) / 100];
    free (v);
    return (0);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// --frame-stats: the columns of the frame that has just ended (called before perf_new_frame)
//
const char  *perf_csv_header (void)
{
    static char  text[512];
    int          i  = 0;

    text[0]  = '\0';
    for (i = 0; i < C_CSV_N; i = i + 1)
    {
        strcat (text, ",");
        strcat (text, cdef[i].csv);
    }
    return (text);
}

void  perf_csv_row (FILE *out)
{
    int  i  = 0;

    for (i = 0; i < C_CSV_N; i = i + 1)
    {
        fprintf (out, ",%u", cur[i]);
    }
}

//
// the same as JSON Lines members: "gpu.draws":N,...  (cycles and pixels too, they are counters)
//
void  perf_json_row (FILE *out)
{
    int  i  = 0;

    sample ();
    for (i = 0; i < P_N; i = i + 1)
    {
        fprintf (out, "%s\"%s\":%u", (i > 0) ? "," : "", cdef[i].name, cur[i]);
    }
}

void  perf_json_summary (FILE *out)
{
    int     i  = 0;
    int     k  = 0;
    agg_t   a;

    fprintf (out, "{\"type\":\"summary\",\"frames\":%u,\"stats\":{", nframes);
    for (i = 0; i < P_N; i = i + 1)
    {
        if (aggregate (i, &a) != 0)  continue;
        fprintf (out, "%s\"%s\":{\"mean\":%.4f,\"min\":%u,\"p50\":%u,\"p95\":%u,\"p99\":%u,\"max\":%u,\"total\":%.0f}",
                 (k++ > 0) ? "," : "", cdef[i].name, a.mean, a.min, a.p50, a.p95, a.p99, a.max, a.total);
    }
    fprintf (out, "}}\n");
}

////////////////////////////////////////////////////////////////////////////////////////
//
// stats save FILE [json]: every frame's counters, by column name (statdiff reads it)
//
int  perf_save (const char *path, int json)
{
    FILE    *f  = fopen (path, "wb");
    size_t   r  = 0;
    int      i  = 0;

    if (f == NULL)
    {
        return (-1);
    }
    if (json == FALSE)
    {
        fprintf (f, "frame");
        for (i = C_CYCLES; i < P_N; i = i + 1)  fprintf (f, ",%s", cdef[i].csv);
        for (i = 0; i < C_CSV_N; i = i + 1)     fprintf (f, ",%s", cdef[i].csv);
        fprintf (f, "\n");
    }
    for (r = hist_base; r < hist_n; r = r + 1)
    {
        const uint32_t  *row  = hist + r * P_N;

        if (json == TRUE)
        {
            fprintf (f, "{\"frame\":%llu", (unsigned long long) (hist_frame0 + r + 1));
            for (i = 0; i < P_N; i = i + 1)  fprintf (f, ",\"%s\":%u", cdef[i].name, row[i]);
            fprintf (f, "}\n");
        }
        else
        {
            fprintf (f, "%llu", (unsigned long long) (hist_frame0 + r + 1));
            for (i = C_CYCLES; i < P_N; i = i + 1)  fprintf (f, ",%u", row[i]);
            for (i = 0; i < C_CSV_N; i = i + 1)     fprintf (f, ",%u", row[i]);
            fprintf (f, "\n");
        }
    }
    if (json == TRUE)  perf_json_summary (f);
    fclose (f);
    return ((int) (hist_n - hist_base));
}

////////////////////////////////////////////////////////////////////////////////////////
//
// reports
//
static int  by_pixels (const void *a, const void *b)
{
    const rentry_t  *x  = (const rentry_t *) a;
    const rentry_t  *y  = (const rentry_t *) b;

    if (x -> pixels != y -> pixels)  return (x -> pixels < y -> pixels) ? 1 : -1;
    if (x -> draws != y -> draws)    return (x -> draws < y -> draws) ? 1 : -1;
    return ((x -> tex != y -> tex) ? ((x -> tex < y -> tex) ? -1 : 1) : ((x -> region < y -> region) ? -1 : ((x -> region > y -> region) ? 1 : 0)));
}

void  perf_regions (FILE *out, const char *prefix, int top)
{
    rentry_t  *list  = NULL;
    size_t     n     = 0;
    size_t     i     = 0;

    if ((rt == NULL) || (top <= 0))
    {
        return;
    }
    list  = (rentry_t *) malloc (RT_SIZE * sizeof (rentry_t));
    if (list == NULL)
    {
        return;
    }
    for (i = 0; i < RT_SIZE; i = i + 1)
    {
        if ((rt[i].used != 0) && (rt[i].draws > 0))  list[n++]  = rt[i];
    }
    qsort (list, n, sizeof (rentry_t), by_pixels);
    fprintf (out, "%s%u distinct regions drawn; by pixels charged (of %llu):\n", prefix, (unsigned) n, (unsigned long long) total_pixels);
    fprintf (out, "%s  %-12s %10s %14s %7s %8s\n", prefix, "tex:region", "draws", "pixels", "share", "dropped");
    for (i = 0; (i < n) && (i < (size_t) top); i = i + 1)
    {
        fprintf (out, "%s  %5d:%-6d %10u %14llu %6.1f%% %8llu\n", prefix, list[i].tex, list[i].region, list[i].draws,
                 (unsigned long long) list[i].pixels, (total_pixels > 0) ? 100.0 * (double) list[i].pixels / (double) total_pixels : 0.0,
                 (unsigned long long) list[i].dropped);
    }
    free (list);
}

void  perf_summary (FILE *out, const char *prefix)
{
    int    i  = 0;
    agg_t  a;

    if (nframes == 0)
    {
        return;
    }
    fprintf (out, "%s%-20s %9s %7s %7s %7s %7s %12s\n", prefix, "counter per frame", "mean", "p50", "p95", "p99", "max", "total");
    for (i = 0; i < P_N; i = i + 1)
    {
        if ((sum[i] == 0) && (i >= C_CSV_N))  continue;
        if (aggregate (i, &a) != 0)
        {
            fprintf (out, "%s%-20s %9.2f %7s %7s %7s %7u %12llu\n", prefix, cdef[i].name, (double) sum[i] / (double) nframes, "-", "-", "-", peak[i], (unsigned long long) sum[i]);
        }
        else
        {
            fprintf (out, "%s%-20s %9.2f %7u %7u %7u %7u %12.0f\n", prefix, cdef[i].name, a.mean, a.p50, a.p95, a.p99, a.max, a.total);
        }
    }
}

void  perf_show (FILE *out)
{
    int    i  = 0;
    agg_t  a;

    fprintf (out, "stats: %u frames%s; the last frame's counters, then the run's mean, p95 and maximum\n", nframes, (hist_on == TRUE) ? "" : " (history off: no percentiles)");
    for (i = 0; i < P_N; i = i + 1)
    {
        if ((sum[i] == 0) && (last[i] == 0) && (i >= C_CSV_N))  continue;
        if (aggregate (i, &a) != 0)
        {
            fprintf (out, "  %-20s %10u   mean %9.2f   max %7u\n", cdef[i].name, last[i], (nframes > 0) ? (double) sum[i] / (double) nframes : 0.0, peak[i]);
        }
        else
        {
            fprintf (out, "  %-20s %10u   mean %9.2f   p95 %7u   max %7u\n", cdef[i].name, last[i], a.mean, a.p95, a.max);
        }
    }
    fprintf (out, "  %-20s %10llu   (pixels charged to the draws that were rendered)\n", "total_pixels", (unsigned long long) total_pixels);
}

//
// a statistic by name: NAME or NAME.AGG (NAME a column or dotted name; AGG last mean min max p50 p95 p99 total), or the
// old total_NAME / max_NAME, frames, total_pixels. 0 = found
//
static int  counter_index (const char *name)
{
    int  i  = 0;

    for (i = 0; i < P_N; i = i + 1)
    {
        if ((0 == strcasecmp (name, cdef[i].csv)) || (0 == strcasecmp (name, cdef[i].name)))  return (i);
    }
    return (-1);
}

int  perf_stat (const char *name, double *value)
{
    int     c    = counter_index (name);
    char    base[96];
    char   *dot  = NULL;
    agg_t   a;

    if (0 == strcasecmp (name, "frames"))        { *value = nframes;  return (0); }
    if (0 == strcasecmp (name, "total_pixels"))  { *value = (double) total_pixels;  return (0); }
    if (c >= 0)                                  { *value = last[c];  return (0); }
    if ((0 == strncasecmp (name, "total_", 6)) && ((c = counter_index (name + 6)) >= 0))  { *value = (double) sum[c];  return (0); }
    if ((0 == strncasecmp (name, "max_", 4)) && ((c = counter_index (name + 4)) >= 0))   { *value = peak[c];  return (0); }
    snprintf (base, sizeof (base), "%s", name);
    dot  = strrchr (base, '.');
    if (dot == NULL)
    {
        return (-1);
    }
    *dot  = '\0';
    c     = counter_index (base);
    if (c < 0)
    {
        return (-1);
    }
    dot  = dot + 1;
    if (0 == strcasecmp (dot, "last"))   { *value = last[c];  return (0); }
    if (0 == strcasecmp (dot, "total"))  { *value = (double) sum[c];  return (0); }
    if (0 == strcasecmp (dot, "max"))    { *value = peak[c];  return (0); }
    if (0 == strcasecmp (dot, "mean"))   { *value = (nframes > 0) ? (double) sum[c] / (double) nframes : 0.0;  return (0); }
    if (aggregate (c, &a) != 0)
    {
        *value = 0;                                  // no frames yet, or history off
        return ((nframes == 0) ? 0 : -2);
    }
    if      (0 == strcasecmp (dot, "min"))  *value = a.min;
    else if (0 == strcasecmp (dot, "p50"))  *value = a.p50;
    else if (0 == strcasecmp (dot, "p95"))  *value = a.p95;
    else if (0 == strcasecmp (dot, "p99"))  *value = a.p99;
    else                                    return (-1);
    return (0);
}
