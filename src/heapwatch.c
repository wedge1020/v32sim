#include "defines.h"
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////////////
//
// heapwatch.c: --heap[=PROG.asm], the heap of a v32lua cartridge, watched from outside
//
//   at every frame end (while the cartridge runs) one line on stdout (the same format as v32orun --heap, whose
//   frame numbers differ: here TIM_FrameCounter's, as in --frame-stats, so the BIOS frames count and no line is
//   printed before the cartridge has run its first instruction; v32orun's bare mode counts from 0):
//
//       HEAP F<frame> ptr=0x%08X used=<ptr-HEAP_START> peak=<largest used so far>[ free=N blocks=N][ arena fields]
//
//   `free= blocks=` appear when the cartridge was built by a v32lua that gives outgrown blocks back to free lists
//   (FREE_MASK and FREE_BINS in the .asm): the words and the number of blocks on those lists, so `used - free` is
//   the live memory (`used` is the heap top and counts the free-listed blocks). The lists are walked read-only,
//   with a cap on the steps and a check of every address; a corrupt list prints `free=? blocks=?`.
//
//   with `v32lua --gc arena` (the arena symbols exist) the line ends with
//
//       floor=0x%08X pin=0x%08X depth=N stack=0x%08X,0x%08X     (the first `depth` marks, oldest first; `stack=-` if none)
//
//   the run's end adds
//
//       HEAP PEAK used=N ptr=0x%08X frame=F
//       HEAP END ptr=0x%08X used=N peak=N[ free=N blocks=N] frame=F[ arena fields]
//
//   Without HEAP_START (a warning on stderr, once) the lines carry ptr only: no used= / peak=.
//
//
// Where the names come from. v32lua writes `%define NAME 0xADDR` lines at the top of NAME.asm:
// HEAP_POINTER (RAM word 0), HEAP_START, and with --gc arena ARENA_FLOOR, ARENA_PIN_TOP, ARENA_DEPTH, ARENA_STACK
// (ARENA_MAX_DEPTH words); and FREE_MASK, FREE_BINS (the free lists of memory.s: bit k of FREE_MASK is set while the
// bin k, the word FREE_BINS + k, is not empty; a block's word 0 is the next block, word 1 its size in words; bin k
// holds sizes 2^k .. 2^(k+1)-1; 22 bins). They are read from NAME.asm, found next to the cartridge by the rules of every debug
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
#define  FREE_BIN_COUNT    22
#define  FREE_WALK_CAP     (HEAP_RAM_WORDS / 2)   // no list can hold more blocks than that: more is a cycle

int8_t       heap_on        = FALSE;     // --heap
const char  *heap_asm_arg   = NULL;      // --heap=PROG.asm

static uint32_t  h_ptr_addr  = 0;
static uint32_t  h_start     = 0;
static int       h_have_start = FALSE;
static int       h_arena     = FALSE;
static int       h_free      = FALSE;    // FREE_MASK and FREE_BINS are known
static uint32_t  h_free_mask = 0, h_free_bins = 0;
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
    { "ARENA_DEPTH", 0, 0 },  { "ARENA_STACK", 0, 0 }, { "ARENA_MAX_DEPTH", 0, 0 },
    { "FREE_MASK", 0, 0 },    { "FREE_BINS", 0, 0 }
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
    h_free         = (hdefs[7].found && hdefs[8].found);
    h_free_mask    = hdefs[7].value;
    h_free_bins    = hdefs[8].value;
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

//
// heap_free_text(): " free=N blocks=N" (or " free=? blocks=?") when the free lists exist, else nothing. Read only.
//
static void  heap_free_text (FILE *out)
{
    uint32_t  mask   = 0, k = 0, block = 0, size = 0, steps = 0;
    uint64_t  words  = 0, blocks = 0;
    int       bad    = FALSE;

    if (h_free == FALSE)  return;
    mask  = ram_word (h_free_mask);
    for (k = 0; (k < FREE_BIN_COUNT) && (bad == FALSE); k = k + 1)
    {
        if (((mask >> k) & 1) == 0)  continue;
        for (block = ram_word (h_free_bins + k); block != 0; block = ram_word (block))
        {
            size  = ram_word (block + 1);
            if ((block >= HEAP_RAM_WORDS - 1) || ((h_have_start == TRUE) && (block < h_start)) ||
                (size < 2) || (size < (1u << k)) || (size >= (2u << k)) || (block + size > HEAP_RAM_WORDS) ||
                (steps >= FREE_WALK_CAP))
            {
                bad  = TRUE;
                break;
            }
            steps   = steps + 1;
            words   = words + size;
            blocks  = blocks + 1;
        }
    }
    if ((mask >> FREE_BIN_COUNT) != 0)  bad  = TRUE;
    if (bad == TRUE)  fprintf (out, " free=? blocks=?");
    else              fprintf (out, " free=%llu blocks=%llu", (unsigned long long) words, (unsigned long long) blocks);
}

static void  heap_tail (FILE *out)
{
    int32_t   depth  = 0;
    int32_t   shown  = 0, i = 0;

    if (h_arena == FALSE)  return;
    depth  = (int32_t) ram_word (h_depth);
    fprintf (out, " floor=0x%08X pin=0x%08X depth=%d", ram_word (h_floor), ram_word (h_pin), (int) depth);
    shown  = (depth < 0) ? 0 : depth;
    if (shown > (int32_t) h_maxdepth)  shown = (int32_t) h_maxdepth;
    fprintf (out, " stack=");
    if (shown == 0)  fprintf (out, "-");
    for (i = 0; i < shown; i = i + 1)
    {
        fprintf (out, "%s0x%08X", (i == 0) ? "" : ",", ram_word (h_stack + (uint32_t) i));
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// heap_frame(): a frame has ended (update_frame, before the timer changes frame)
//
void  heap_frame (void)
{
    uint32_t  ptr  = 0;

    if ((heap_on == FALSE) || (h_armed == FALSE))  return;
    h_last_frame  = frames_run + 1;
    heap_poll ();                               // a pointer that changed since the last instruction is counted
    ptr  = ram_word (h_ptr_addr);
    fprintf (stdout, "HEAP F%u ptr=0x%08X", (unsigned) h_last_frame, ptr);
    if (h_have_start == TRUE)
    {
        fprintf (stdout, " used=%d peak=%d", (int) (ptr - h_start), (int) ((h_seen ? h_peak_ptr : ptr) - h_start));
    }
    heap_free_text (stdout);
    heap_tail (stdout);
    fprintf (stdout, "\n");
}

////////////////////////////////////////////////////////////////////////////////////////
//
// heap_report(): the run is over
//
void  heap_report (void)
{
    uint32_t  ptr  = 0;
    uint32_t  top  = 0;

    if ((heap_on == FALSE) || (h_armed == FALSE))  return;
    heap_poll ();
    ptr  = ram_word (h_ptr_addr);
    top  = h_seen ? h_peak_ptr : ptr;
    if (h_have_start == TRUE)
    {
        fprintf (stdout, "HEAP PEAK used=%d ptr=0x%08X frame=%u\n", (int) (top - h_start), top, (unsigned) h_peak_frame);
    }
    else
    {
        fprintf (stdout, "HEAP PEAK ptr=0x%08X frame=%u\n", top, (unsigned) h_peak_frame);
    }
    fprintf (stdout, "HEAP END ptr=0x%08X", ptr);
    if (h_have_start == TRUE)  fprintf (stdout, " used=%d peak=%d", (int) (ptr - h_start), (int) (top - h_start));
    heap_free_text (stdout);
    fprintf (stdout, " frame=%u", (unsigned) frames_run);
    heap_tail (stdout);
    fprintf (stdout, "\n");
}
