#include "defines.h"
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////////////
//
// heapwatch.c: --heap[=PROG.asm], the heap of a v32lua cartridge, watched from outside
//
//   at every frame end (while the cartridge runs) one line on stdout:
//
//       HEAP F<frame> ptr=0x%08X used=<ptr-HEAP_START> peak=<largest used so far>
//
//   and, when the cartridge was built with `v32lua --gc arena` (the arena symbols exist), also
//
//       ... floor=0x%08X pin=0x%08X depth=N stack=0x..,0x..          (the first `depth` marks, oldest first)
//
//   the run's end adds   HEAP PEAK used=N words ptr=0x.. frame=F   and   HEAP END used=N words ptr=0x.. frame=F
//   (with the arena fields on the END line too). Frame numbers are TIM_FrameCounter's, as in --frame-stats
//   (the BIOS frames count; no line is printed before the cartridge has run its first instruction).
//
// Where the names come from. v32lua writes `%define NAME 0xADDR` lines at the top of NAME.asm:
// HEAP_POINTER (RAM word 0), HEAP_START, and with --gc arena ARENA_FLOOR, ARENA_PIN_TOP, ARENA_DEPTH, ARENA_STACK
// (ARENA_MAX_DEPTH words). They are read from NAME.asm, found next to the cartridge by the rules of every debug
// file (open_search_file), or from the file named by --heap=PROG.asm. A name that file lacks is looked up
// among the data symbols (--symbol-file, NAME.asm.sym). HEAP_POINTER defaults to RAM word 0. Without HEAP_START
// the lines carry only ptr (a warning on stderr says so, once).
//
// The peak is exact: the heap pointer is read after every instruction the cartridge runs (a read through the
// forced path, which has no side effects), not only at frame ends. Nothing is written: the machine is untouched.
//
////////////////////////////////////////////////////////////////////////////////////////

#define  HEAP_RAM_WORDS    0x00400000u
#define  HEAP_STACK_MAX    16

int8_t       heap_on        = FALSE;     // --heap
const char  *heap_asm_arg   = NULL;      // --heap=PROG.asm

static uint32_t  h_ptr_addr  = 0;
static uint32_t  h_start     = 0;
static int       h_have_start = FALSE;
static int       h_arena     = FALSE;
static uint32_t  h_floor = 0, h_pin = 0, h_depth = 0, h_stack = 0, h_maxdepth = HEAP_STACK_MAX;
static int       h_armed     = FALSE;
static uint32_t  h_peak_ptr  = 0;        // the highest valid heap pointer seen
static uint32_t  h_peak_frame = 0;
static int       h_seen      = FALSE;    // a valid pointer has been seen
static uint32_t  h_last_frame = 0;

typedef struct { const char *name; uint32_t value; int found; } hdef_t;
static hdef_t  hdefs[] =
{
    { "HEAP_POINTER", 0, 0 }, { "HEAP_START", 0, 0 }, { "ARENA_FLOOR", 0, 0 }, { "ARENA_PIN_TOP", 0, 0 },
    { "ARENA_DEPTH", 0, 0 },  { "ARENA_STACK", 0, 0 }, { "ARENA_MAX_DEPTH", 0, 0 }
};
#define  NHDEFS  (sizeof (hdefs) / sizeof (hdefs[0]))

static void  heap_scan_asm (FILE *fptr)
{
    char      line[1024];
    char     *p      = NULL;
    char     *name   = NULL;
    char     *end    = NULL;
    size_t    k      = 0;

    while (fgets (line, sizeof (line), fptr) != NULL)
    {
        for (p = line; (*p == ' ') || (*p == '\t'); p = p + 1) ;
        if (0 != strncmp (p, "%define", 7))  continue;
        for (p = p + 7; (*p == ' ') || (*p == '\t'); p = p + 1) ;
        name = p;
        for (end = name; (*end != '\0') && (*end != ' ') && (*end != '\t'); end = end + 1) ;
        if (*end == '\0')  continue;
        *end = '\0';
        for (p = end + 1; (*p == ' ') || (*p == '\t'); p = p + 1) ;
        if (!isdigit ((unsigned char) *p))  continue;
        for (k = 0; k < NHDEFS; k = k + 1)
        {
            if (0 == strcmp (name, hdefs[k].name))
            {
                hdefs[k].value  = (uint32_t) strtoul (p, NULL, 0);
                hdefs[k].found  = TRUE;
            }
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// heap_init(): find the symbols (after the cartridge and its symbol files are loaded)
//
void  heap_init (void)
{
    char    found[2048];
    FILE   *fptr  = NULL;
    size_t  k     = 0;
    uint32_t  w   = 0, a = 0;

    if (heap_on == FALSE)  return;
    if (heap_asm_arg != NULL)
    {
        fptr = fopen (heap_asm_arg, "r");
        if (fptr == NULL)
        {
            fprintf (stderr, "[ERROR] --heap: could not read '%s'\n", heap_asm_arg);
            exit (1);
        }
    }
    else if (cartfile != NULL)
    {
        fptr = open_search_file ((const char *) cartfile, ".asm", found, sizeof (found));
    }
    if (fptr != NULL)
    {
        heap_scan_asm (fptr);
        fclose (fptr);
    }
    for (k = 0; k < NHDEFS; k = k + 1)          // what the .asm lacked: the data symbols
    {
        if ((hdefs[k].found == FALSE) && (sym_lookup (hdefs[k].name, &a, &w) == TRUE))
        {
            hdefs[k].value  = a;
            hdefs[k].found  = TRUE;
        }
    }
    h_ptr_addr     = hdefs[0].value;            // RAM word 0 when nothing names it
    h_have_start   = hdefs[1].found;
    h_start        = hdefs[1].value;
    h_arena        = (hdefs[2].found && hdefs[3].found && hdefs[4].found && hdefs[5].found);
    h_floor        = hdefs[2].value;
    h_pin          = hdefs[3].value;
    h_depth        = hdefs[4].value;
    h_stack        = hdefs[5].value;
    if (hdefs[6].found && (hdefs[6].value > 0) && (hdefs[6].value < 4096))  h_maxdepth = hdefs[6].value;
    if (h_have_start == FALSE)
    {
        fprintf (stderr, "[WARNING] --heap: HEAP_START is unknown (no %%define for it in the cartridge's .asm%s): only ptr is shown\n",
                 (heap_asm_arg == NULL) ? "; --heap=PROG.asm names the file" : "");
    }
}

static uint32_t  ram_word (uint32_t address)
{
    word_t  *w  = memory_get (address, TRUE);

    return ((w == NULL) ? 0 : (uint32_t) w->i32);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// heap_poll(): after every instruction. Cheap: one RAM read, a compare.
//
void  heap_poll (void)
{
    uint32_t  ptr  = 0;

    if (h_armed == FALSE)
    {
        if (((rom_offset & V32_PAGE_MASK) >> 28) != V32_PAGE_CART)  return;
        h_armed  = TRUE;                        // the BIOS has its own use of low RAM: watch the cartridge only
    }
    ptr  = ram_word (h_ptr_addr);
    if ((ptr >= HEAP_RAM_WORDS) || ((h_have_start == TRUE) && (ptr < h_start)))  return;
    if ((h_seen == FALSE) || (ptr > h_peak_ptr))
    {
        h_seen         = TRUE;
        h_peak_ptr     = ptr;
        h_peak_frame   = frames_run + 1;
    }
}

static void  heap_tail (FILE *out)
{
    uint32_t  depth  = 0, i = 0;

    if (h_arena == FALSE)  return;
    depth  = ram_word (h_depth);
    fprintf (out, " floor=0x%08X pin=0x%08X depth=%u", ram_word (h_floor), ram_word (h_pin), (unsigned) depth);
    if (depth > h_maxdepth)  depth = h_maxdepth;
    fprintf (out, " stack=");
    for (i = 0; i < depth; i = i + 1)
    {
        fprintf (out, "%s0x%08X", (i == 0) ? "" : ",", ram_word (h_stack + i));
    }
}

static void  heap_line (const char *what, uint32_t frame, int with_peak)
{
    uint32_t  ptr  = ram_word (h_ptr_addr);

    heap_poll ();                               // a pointer that changed since the last instruction is counted
    if (what != NULL)
    {
        fprintf (stdout, "HEAP %s", what);
    }
    else
    {
        fprintf (stdout, "HEAP F%u", (unsigned) frame);
    }
    if (h_have_start == TRUE)
    {
        if (what == NULL)
        {
            fprintf (stdout, " ptr=0x%08X used=%d", ptr, (int) (ptr - h_start));
            if (with_peak)  fprintf (stdout, " peak=%d", (int) ((h_seen ? h_peak_ptr : ptr) - h_start));
        }
        else
        {
            fprintf (stdout, " used=%d words ptr=0x%08X frame=%u", (int) (ptr - h_start), ptr, (unsigned) frame);
        }
    }
    else
    {
        fprintf (stdout, " ptr=0x%08X", ptr);
        if (what != NULL)  fprintf (stdout, " frame=%u", (unsigned) frame);
    }
    if (what == NULL || strcmp (what, "END") == 0)  heap_tail (stdout);
    fprintf (stdout, "\n");
}

////////////////////////////////////////////////////////////////////////////////////////
//
// heap_frame(): a frame has ended (update_frame, before the timer changes frame)
//
void  heap_frame (void)
{
    if ((heap_on == FALSE) || (h_armed == FALSE))  return;
    h_last_frame  = frames_run + 1;
    heap_line (NULL, h_last_frame, TRUE);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// heap_report(): the run is over
//
void  heap_report (void)
{
    uint32_t  ptr  = 0;

    if ((heap_on == FALSE) || (h_armed == FALSE))  return;
    heap_poll ();
    ptr  = ram_word (h_ptr_addr);
    if (h_have_start == TRUE)
    {
        fprintf (stdout, "HEAP PEAK used=%d words ptr=0x%08X frame=%u\n",
                 (int) ((h_seen ? h_peak_ptr : ptr) - h_start), h_seen ? h_peak_ptr : ptr, (unsigned) h_peak_frame);
    }
    else
    {
        fprintf (stdout, "HEAP PEAK ptr=0x%08X frame=%u\n", h_seen ? h_peak_ptr : ptr, (unsigned) h_peak_frame);
    }
    heap_line ("END", frames_run, FALSE);
}
