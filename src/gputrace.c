#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// gputrace.c: --gpu-trace=FILE and --spu-trace=FILE (VS-13 4.3), the program's writes to the
// GPU and SPU ports, frame by frame, in a file another program can replay
// (tools/oracle/orun.cpp --compat=gpureplay does, on the DesktopEmulator's own GPU / SPU logic).
//
// Format "V32TRACE" version 1, little endian, documented in docs/TRACE-FORMAT.md:
//
//   header   8 bytes "V32TRACE", u16 version (1), u16 kind (1 GPU, 2 SPU), u32 0
//   then records, a tag byte and a payload:
//     'N' u16 count          cartridge textures (GPU) / cartridge sounds (SPU) loaded; first record
//     'T' i16 tex, u16 wide, u16 high, u16 n, n x (u16 region, 6 x i16 minX minY maxX maxY hotX hotY)
//                            GPU: a texture (-1 = the BIOS') and its regions that are not all zero
//     'S' i32 id, u32 length, i32 loop, i32 loopstart, i32 loopend      SPU: a sound (-1 = the BIOS')
//     'W' u16 port, u32 value            a write by the program (value = the bits written, floats as bits)
//     'F' u32 frame                      a frame ended (frames_run before it was counted)
//     'E' u32 frames                     the run ended
//
// The textures' pixels and the sounds' samples are not in the file: what the writes cost the GPU
// and SPU logic does not depend on them.
//
////////////////////////////////////////////////////////////////////////////////////////

int8_t   *gpu_trace_out  = NULL;     // --gpu-trace=FILE
int8_t   *spu_trace_out  = NULL;     // --spu-trace=FILE

static FILE     *tf[2]    = { NULL, NULL };
static int       tstate[2] = { 0, 0 };     // 0 not started, 1 running, -1 failed / closed

static void  put16 (FILE *f, uint32_t v)  { fputc (v & 0xFF, f);  fputc ((v >> 8) & 0xFF, f); }
static void  put32 (FILE *f, uint32_t v)  { put16 (f, v & 0xFFFF);  put16 (f, v >> 16); }

static FILE *start (int k)
{
    const char  *name  = (const char *) ((k == 0) ? gpu_trace_out : spu_trace_out);

    if (tstate[k] == 1)  return (tf[k]);
    if ((name == NULL) || (tstate[k] < 0))  return (NULL);
    tf[k]  = fopen (name, "wb");
    if (tf[k] == NULL)
    {
        fprintf (stderr, "[ERROR] could not write the trace '%s'\n", name);
        tstate[k]  = -1;
        return (NULL);
    }
    tstate[k]  = 1;
    fwrite ("V32TRACE", 1, 8, tf[k]);
    put16 (tf[k], 1);
    put16 (tf[k], (uint32_t) (k + 1));
    put32 (tf[k], 0);
    if (k == 0)
    {
        uint32_t  n  = (cart_vtex != NULL) ? cart_vtex -> qty : 0;
        uint32_t  t  = 0;
        uint32_t  r  = 0;

        fputc ('N', tf[k]);
        put16 (tf[k], n);
        for (t = 0; t <= n; t = t + 1)
        {
            vtex_t  *v  = (t == 0) ? bios_vtex : ((cart_vtex != NULL) ? cart_vtex + (t - 1) : NULL);
            uint32_t cnt = 0;

            if ((v == NULL) || (v -> region == NULL))  continue;
            for (r = 0; r < 4096; r = r + 1)
            {
                const region_t  *g  = v -> region + r;
                if (g -> minX || g -> minY || g -> maxX || g -> maxY || g -> hotX || g -> hotY)  cnt = cnt + 1;
            }
            fputc ('T', tf[k]);
            put16 (tf[k], (t == 0) ? 0xFFFF : (t - 1));
            put16 (tf[k], v -> wide);
            put16 (tf[k], v -> high);
            put16 (tf[k], cnt);
            for (r = 0; r < 4096; r = r + 1)
            {
                const region_t  *g  = v -> region + r;
                if (g -> minX || g -> minY || g -> maxX || g -> maxY || g -> hotX || g -> hotY)
                {
                    put16 (tf[k], r);
                    put16 (tf[k], (uint16_t) g -> minX);  put16 (tf[k], (uint16_t) g -> minY);
                    put16 (tf[k], (uint16_t) g -> maxX);  put16 (tf[k], (uint16_t) g -> maxY);
                    put16 (tf[k], (uint16_t) g -> hotX);  put16 (tf[k], (uint16_t) g -> hotY);
                }
            }
        }
    }
    else
    {
        spu_trace_snapshot (tf[k]);
    }
    return (tf[k]);
}

void  gputrace_write_u16 (FILE *f, uint32_t v)  { put16 (f, v); }
void  gputrace_write_u32 (FILE *f, uint32_t v)  { put32 (f, v); }

//
// ioports_set(): the program wrote a port (any value the program gave, accepted or not)
//
void  gputrace_write (uint16_t port, int32_t i32, float f32)
{
    int       k     = ((port & 0x700) == 0x200) ? 0 : (((port & 0x700) == 0x300) ? 1 : -1);
    FILE     *f     = NULL;
    uint32_t  bits  = (uint32_t) i32;

    if ((k < 0) || (((k == 0) ? gpu_trace_out : spu_trace_out) == NULL))  return;
    f  = start (k);
    if (f == NULL)  return;
    switch (port)
    {
        case GPU_DrawingScaleX:  case GPU_DrawingScaleY:  case GPU_DrawingAngle:
        case SPU_GlobalVolume:   case SPU_ChannelVolume:  case SPU_ChannelSpeed:
            memcpy (&bits, &f32, 4);
            break;
        default:
            break;
    }
    fputc ('W', f);
    put16 (f, port);
    put32 (f, bits);
}

void  gputrace_frame (void)
{
    int  k  = 0;

    for (k = 0; k < 2; k = k + 1)
    {
        FILE  *f  = start (k);

        if (f != NULL)
        {
            fputc ('F', f);
            put32 (f, frames_run);
        }
    }
}

void  gputrace_close (void)
{
    int  k  = 0;

    for (k = 0; k < 2; k = k + 1)
    {
        if (tstate[k] == 1)
        {
            fputc ('E', tf[k]);
            put32 (tf[k], frames_run);
            fclose (tf[k]);
            tf[k]      = NULL;
            tstate[k]  = -1;
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// --trace-dump=FILE: the text form of a trace
//
static int  get8 (FILE *f)  { return (fgetc (f)); }
static uint32_t  get16 (FILE *f)  { uint32_t a = (uint32_t) fgetc (f); uint32_t b = (uint32_t) fgetc (f); return (a | (b << 8)); }
static uint32_t  get32 (FILE *f)  { uint32_t a = get16 (f); uint32_t b = get16 (f); return (a | (b << 16)); }

int  gputrace_dump (const char *path)
{
    FILE      *f   = fopen (path, "rb");
    char       magic[9];
    uint32_t   ver = 0;
    uint32_t   kind = 0;
    int        tag = 0;

    if (f == NULL)
    {
        fprintf (stderr, "[ERROR] --trace-dump: cannot read '%s'\n", path);
        return (1);
    }
    init_ioports ();                                  // for the port names
    memset (magic, 0, sizeof (magic));
    if ((fread (magic, 1, 8, f) != 8) || (0 != strcmp (magic, "V32TRACE")))
    {
        fprintf (stderr, "[ERROR] --trace-dump: '%s' is not a v32sim trace\n", path);
        fclose (f);
        return (1);
    }
    ver   = get16 (f);
    kind  = get16 (f);
    (void) get32 (f);
    printf ("V32TRACE version %u %s\n", ver, (kind == 1) ? "gpu" : "spu");
    if (ver != 1)
    {
        fprintf (stderr, "[ERROR] --trace-dump: version %u is not known\n", ver);
        fclose (f);
        return (1);
    }
    while ((tag = get8 (f)) != EOF)
    {
        switch (tag)
        {
            case 'N':  printf ("N %u\n", get16 (f));  break;
            case 'T':
            {
                int32_t   tex  = (int16_t) get16 (f);
                uint32_t  w    = get16 (f);
                uint32_t  h    = get16 (f);
                uint32_t  n    = get16 (f);
                uint32_t  i    = 0;

                printf ("T tex=%d size=%ux%u regions=%u\n", tex, w, h, n);
                for (i = 0; i < n; i = i + 1)
                {
                    uint32_t  r = get16 (f);
                    int16_t   a = (int16_t) get16 (f), b = (int16_t) get16 (f), c = (int16_t) get16 (f), d = (int16_t) get16 (f);
                    int16_t   x = (int16_t) get16 (f), y = (int16_t) get16 (f);

                    printf ("  R %u (%d,%d)-(%d,%d) hot=(%d,%d)\n", r, a, b, c, d, x, y);
                }
                break;
            }
            case 'S':
            {
                int32_t  id = (int32_t) get32 (f);
                uint32_t len = get32 (f);
                int32_t  lp = (int32_t) get32 (f), ls = (int32_t) get32 (f), le = (int32_t) get32 (f);

                printf ("S id=%d length=%u loop=%d start=%d end=%d\n", id, len, lp, ls, le);
                break;
            }
            case 'W':
            {
                uint32_t  p = get16 (f);
                uint32_t  v = get32 (f);

                printf ("W 0x%03X %s = 0x%08X (%d)\n", p, ioports_name (p), v, (int32_t) v);
                break;
            }
            case 'F':  printf ("F %u\n", get32 (f));  break;
            case 'E':  printf ("E %u\n", get32 (f));  break;
            default:
                fprintf (stderr, "[ERROR] --trace-dump: unknown record '%c' (0x%02X)\n", tag, tag);
                fclose (f);
                return (1);
        }
    }
    fclose (f);
    return (0);
}
