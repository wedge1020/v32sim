#include "defines.h"
#include <stdarg.h>

////////////////////////////////////////////////////////////////////////////////////////
//
// V32TRACE for the simulator (spec: V32TRACE-SPEC.md, version 2).
//
// With --v32trace=PREFIX the run reads the translator's <stem>.v32trace.meta and labels
// file, watches the trap routines and soft events the meta names, and when the run ends
// (and at every new trap) writes
//
//     PREFIX.traps.txt     one TRAP / EVENT line per distinct trap or event (spec section 4.2)
//     PREFIX.blocks.txt    every translated guest block that ran, "bank:address count"
//
// the same files the DesktopEmulator recorder writes, so a translator's gap-filling tool
// reads either. After a trap the cartridge is restarted from its entry point once the
// trap screen has been up for 60 frames (--v32trace-norestart turns this off), so one
// unattended run can meet several gaps. Not in this implementation: `series`, markers,
// the human report, host cost.
//
// The module never changes what the console does, except for the restart.
//

#define  VT_MAX_HITS     64
#define  VT_HOLD_FRAMES  60
#define  VT_RING         512
#define  VT_MAX_CAPS     32
#define  VT_MAX_SPECS    32
#define  VT_MAX_PATS     8
#define  VT_MAX_CONT     16
#define  VT_MAX_KINDS    16

int8_t   *v32trace_out        = NULL;       // --v32trace=PREFIX
int8_t   *v32trace_meta_file  = NULL;       // --v32trace-meta=FILE
int8_t   *v32trace_label_file = NULL;       // --v32trace-labels=FILE
uint8_t   v32trace_norestart  = FALSE;      // --v32trace-norestart
uint8_t   v32trace_on         = FALSE;

typedef struct { char name[64]; uint32_t addr; uint32_t count; int reg; int ireg; uint32_t mask; int vol; }  cap_t;
typedef struct { int trap; char name[64]; char label[192]; }  spec_t;
typedef struct { char fmt[192]; int packed; int fixed; uint32_t add; }  pat_t;
typedef struct { char cap[64]; int n; uint32_t val[16]; char name[16][48]; }  kind_t;
typedef struct { uint32_t off; char *name; int block; }  lab_t;
typedef struct
{
    int       event;
    char      name[64];
    char      key[1024];
    char      kind[48];
    int32_t   frame;
    int32_t   segstart;        // spec 2.1: cart frame of the last (re)start before the hit
    uint32_t  count;
    uint32_t  regs[16];
    uint32_t  vals[VT_MAX_CAPS][64];
    uint32_t  trail[32];       // guest block keys, oldest first
    int       ntrail;
}  hit_t;

static char      arch[64]     = "unknown";
static cap_t     caps[VT_MAX_CAPS];     static int ncaps = 0;
static spec_t    specs[VT_MAX_SPECS];   static int nspecs = 0;
static pat_t     pats[VT_MAX_PATS];     static int npats = 0;
static char      cont[VT_MAX_CONT][64]; static int ncont = 0;
static kind_t    kinds[VT_MAX_KINDS];   static int nkinds = 0;
static int       have_resume = 0, have_words = 0, have_hash = 0;
static char      resume_label[192];
static uint32_t  resume_off = 0, meta_words = 0, meta_hash = 0;

static lab_t    *vlab = NULL;           static int nlabs = 0;
static uint32_t  cart_words = 0, cart_hash = 0;
static uint32_t *counts = NULL;         // instructions executed per cart word
static uint32_t *watch  = NULL;         // bit per cart word
static uint32_t  watch_off[VT_MAX_SPECS];

static hit_t     hits[VT_MAX_HITS];     static int nhits = 0;
static uint32_t  dropped = 0, restarts = 0;
static int       trapped = 0, held = 0, gave_up = 0, repeat_run = 0;
static char      last_key[1024];
static int32_t   cartframe = -1;        // -1: the cartridge has not run yet
static int32_t   segstart  = 0;         // spec 2.1: cart frame of the last restart (0: never restarted)
static uint16_t *pads      = NULL;      // spec 2.1: gamepad 1 buttons held in each cart frame (bit per control)
static int32_t   npads     = 0, roompads = 0, padframe = -1;

// the controls in INP port order; the V32_INPUT names the recorders use (Y has none)
static const char *const vt_buttons[11] = { "LEFT", "RIGHT", "UP", "DOWN", "START", "A", "B", "SELECT", NULL, "L", "R" };

static uint32_t  ring_to[VT_RING];  static uint32_t ring_n = 0;
static uint32_t  prev_ip = 0, expect_ip = 0;  static int have_prev = 0;

static void  vt_note (const char *fmt, ...)
{
    va_list  ap;

    va_start (ap, fmt);
    fprintf (stderr, "[v32trace] ");
    vfprintf (stderr, fmt, ap);
    fprintf (stderr, "\n");
    va_end (ap);
}

static int  has_word (const char *line, const char *word)
{
    const char *p = strstr (line, word);

    return (p != NULL);
}

static void  parse_meta (FILE *f)
{
    char  line[1024], key[64], a[192];

    while (fgets (line, sizeof line, f) != NULL)
    {
        unsigned  x = 0, y = 0, z = 0;

        if ((line[0] == '#') || (line[0] == '\n'))
        {
            continue;
        }
        a[0] = 0;
        if (sscanf (line, "%63s %191s", key, a) < 1)
        {
            continue;
        }
        if (0 == strcmp (key, "format"))
        {
            if ((atoi (a) != 1) && (atoi (a) != 2))
            {
                vt_note ("unsupported v32trace.meta format %s: ignored", a);
                ncaps = nspecs = npats = 0;
                return;
            }
        }
        else if (0 == strcmp (key, "words"))   { have_words = 1; meta_words = (uint32_t) strtoul (a, NULL, 10); }
        else if (0 == strcmp (key, "fnv1a"))   { have_hash = 1;  meta_hash  = (uint32_t) strtoul (a, NULL, 16); }
        else if (0 == strcmp (key, "arch"))    { snprintf (arch, sizeof arch, "%s", a); }
        else if ((0 == strcmp (key, "trap")) && (a[0]) && (nspecs < VT_MAX_SPECS))
        {
            specs[nspecs].trap = 1;
            snprintf (specs[nspecs].name,  sizeof specs[nspecs].name,  "trap");
            snprintf (specs[nspecs].label, sizeof specs[nspecs].label, "%s", a);
            nspecs = nspecs + 1;
        }
        else if ((0 == strcmp (key, "event")) && (nspecs < VT_MAX_SPECS))
        {
            char  l[192];

            if (sscanf (line, "%*s %63s %191s", a, l) == 2)
            {
                specs[nspecs].trap = 0;
                snprintf (specs[nspecs].name,  sizeof specs[nspecs].name,  "%s", a);
                snprintf (specs[nspecs].label, sizeof specs[nspecs].label, "%s", l);
                nspecs = nspecs + 1;
            }
        }
        else if (0 == strcmp (key, "resume"))  { have_resume = 1; snprintf (resume_label, sizeof resume_label, "%s", a); }
        else if ((0 == strcmp (key, "block")) && (a[0]) && (npats < VT_MAX_PATS))
        {
            pat_t *p = pats + npats;

            snprintf (p -> fmt, sizeof p -> fmt, "%s", a);
            p -> packed = has_word (line, " packed");
            p -> fixed  = -1;
            p -> add    = 0;
            if (strstr (line, " bank=")    != NULL)  p -> fixed = (int) strtoul (strstr (line, " bank=") + 6, NULL, 16);
            if (strstr (line, " bankadd=") != NULL)  p -> add   = (uint32_t) strtoul (strstr (line, " bankadd=") + 9, NULL, 16);
            npats = npats + 1;
        }
        else if ((0 == strcmp (key, "continue")) && (a[0]) && (ncont < VT_MAX_CONT))
        {
            snprintf (cont[ncont], sizeof cont[ncont], "%s", a);
            ncont = ncont + 1;
        }
        else if (((0 == strcmp (key, "capture")) || (0 == strcmp (key, "register"))) && (ncaps < VT_MAX_CAPS))
        {
            cap_t *c = caps + ncaps;
            char   nm[64];

            memset (c, 0, sizeof *c);
            c -> reg = -1; c -> ireg = -1; c -> mask = 0xFFFFFFFF; c -> count = 1;
            c -> vol = has_word (line, " volatile");
            if (0 == strcmp (key, "register"))
            {
                if (sscanf (line, "%*s %63s R%u %x", nm, &x, &y) >= 2 && x < 16)
                {
                    snprintf (c -> name, sizeof c -> name, "%s", nm);
                    c -> reg = (int) x;
                    if (sscanf (line, "%*s %*s R%*u %x", &y) == 1)  c -> mask = y;
                    ncaps = ncaps + 1;
                }
            }
            else if (sscanf (line, "%*s %63s %x %u", nm, &x, &z) == 3 && z > 0 && z <= 64)
            {
                const char *ix = strstr (line, "index=R");

                snprintf (c -> name, sizeof c -> name, "%s", nm);
                c -> addr = x; c -> count = z;
                if ((ix != NULL) && (sscanf (ix, "index=R%u&%x", &x, &y) >= 1) && x < 16)
                {
                    c -> ireg = (int) x;
                    if (sscanf (ix, "index=R%*u&%x", &y) == 1)  c -> mask = y;
                }
                ncaps = ncaps + 1;
            }
        }
        else if ((0 == strcmp (key, "kinds")) && (nkinds < VT_MAX_KINDS))
        {
            kind_t *k = kinds + nkinds;
            char   *tok = strtok (line, " \t\r\n");

            memset (k, 0, sizeof *k);
            tok = strtok (NULL, " \t\r\n");
            if (tok != NULL)
            {
                snprintf (k -> cap, sizeof k -> cap, "%s", tok);
                while (((tok = strtok (NULL, " \t\r\n")) != NULL) && (k -> n < 16))
                {
                    char *eq = strchr (tok, '=');

                    if (eq != NULL)
                    {
                        *eq = 0;
                        k -> val[k -> n] = (uint32_t) strtoul (tok, NULL, 16);
                        snprintf (k -> name[k -> n], 48, "%s", eq + 1);
                        k -> n = k -> n + 1;
                    }
                }
                nkinds = nkinds + 1;
            }
        }
    }
}

static FILE  *open_beside (const char *cart, const char *stem_suffix, const char *plain)
{
    char   path[2048], dir[2048], stem[512];
    const char *slash = strrchr (cart, '/'), *dot;
    FILE  *f;

    snprintf (dir, sizeof dir, "%.*s", (int) (slash ? slash - cart + 1 : 0), cart);
    snprintf (stem, sizeof stem, "%s", slash ? slash + 1 : cart);
    dot = strrchr (stem, '.');
    if (dot != NULL)  stem[dot - stem] = 0;
    snprintf (path, sizeof path, "%s%s%s", dir, stem, stem_suffix);
    f = fopen (path, "r");
    if (f == NULL)
    {
        snprintf (path, sizeof path, "%s%s", dir, plain);
        f = fopen (path, "r");
    }
    return (f);
}

static int  cmp_lab (const void *a, const void *b)
{
    const lab_t *x = (const lab_t *) a, *y = (const lab_t *) b;

    return ((x -> off > y -> off) - (x -> off < y -> off));
}

static int  match_block (const char *name, uint32_t *key)
{
    int  i;

    for (i = 0; i < npats; i = i + 1)
    {
        char      f[256];
        unsigned  a = 0, b = 0;
        int       used = 0, got;

        snprintf (f, sizeof f, "%s%%n", pats[i].fmt);
        if ((pats[i].packed) || (pats[i].fixed >= 0))
        {
            got = sscanf (name, f, &a, &used);
            if ((got != 1) || (used != (int) strlen (name)))  continue;
            *key = pats[i].packed ? a : ((((uint32_t) pats[i].fixed + pats[i].add) << 16) | (a & 0xFFFF));
            return (1);
        }
        got = sscanf (name, f, &a, &b, &used);
        if ((got != 2) || (used != (int) strlen (name)) || (b > 0xFFFFFF))  continue;
        *key = ((a + pats[i].add) << 16) | b;
        return (1);
    }
    return (0);
}

static int  is_cont (const char *name)
{
    int  i;

    for (i = 0; i < ncont; i = i + 1)
    {
        size_t  n = strlen (cont[i]);

        if ((n > 0) && (cont[i][n - 1] == '*'))
        {
            if (0 == strncmp (name, cont[i], n - 1))  return (1);
        }
        else if ((0 == strncmp (name, cont[i], n)) && (name[n] >= '0') && (name[n] <= '9'))
        {
            return (1);
        }
    }
    return (0);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// setup: called once the BIOS and the cartridge are loaded
//
void  v32trace_init (void)
{
    FILE      *mf = NULL, *lf = NULL;
    uint32_t   i;
    mem_t     *cart = memory + V32_PAGE_CART;

    if ((v32trace_out == NULL) || (cartfile == NULL) || (cart -> size == 0))
    {
        return;
    }
    cart_words = cart -> size;
    cart_hash  = 2166136261u;
    for (i = 0; i < cart_words; i = i + 1)
    {
        cart_hash = (cart_hash ^ (cart -> data + i) -> value.raw) * 16777619u;
    }
    mf = (v32trace_meta_file != NULL) ? fopen ((char *) v32trace_meta_file, "r")
                                      : open_beside ((char *) cartfile, ".v32trace.meta", "v32trace.meta");
    if (mf == NULL)
    {
        vt_note ("no v32trace.meta beside the cartridge: nothing to watch");
        return;
    }
    parse_meta (mf);
    fclose (mf);
    if (((have_words) && (meta_words != cart_words)) || ((have_hash) && (meta_hash != cart_hash)))
    {
        vt_note ("v32trace.meta belongs to another build of the cartridge (words %u/%u, hash %08X/%08X): ignored",
                 meta_words, cart_words, meta_hash, cart_hash);
        return;
    }
    lf = (v32trace_label_file != NULL) ? fopen ((char *) v32trace_label_file, "r")
                                       : open_beside ((char *) cartfile, ".labels.txt", "labels.txt");
    if (lf == NULL)
    {
        vt_note ("no labels file beside the cartridge: nothing to watch");
        return;
    }
    {
        char      line[512], name[400];
        unsigned  addr;
        int       cap = 0;

        while (fgets (line, sizeof line, lf) != NULL)
        {
            if ((sscanf (line, "%x %399s", &addr, name) == 2) && ((addr >> 28) == 2) && ((addr & 0x0FFFFFFF) < cart_words))
            {
                if (nlabs == cap)
                {
                    cap  = cap ? cap * 2 : 4096;
                    vlab = (lab_t *) realloc (vlab, (size_t) cap * sizeof (lab_t));
                }
                vlab[nlabs].off   = addr & 0x0FFFFFFF;
                vlab[nlabs].name  = strdup (name);
                vlab[nlabs].block = 0;
                nlabs             = nlabs + 1;
            }
        }
        fclose (lf);
    }
    qsort (vlab, (size_t) nlabs, sizeof (lab_t), cmp_lab);
    counts = (uint32_t *) calloc (cart_words, sizeof (uint32_t));
    watch  = (uint32_t *) calloc ((cart_words >> 5) + 1, sizeof (uint32_t));
    for (i = 0; i < (uint32_t) nspecs; i = i + 1)
    {
        int  k;

        watch_off[i] = 0xFFFFFFFF;
        for (k = 0; k < nlabs; k = k + 1)
        {
            if (0 == strcmp (vlab[k].name, specs[i].label))
            {
                watch_off[i] = vlab[k].off;
                watch[vlab[k].off >> 5] |= (1u << (vlab[k].off & 31));
                break;
            }
        }
        if (watch_off[i] == 0xFFFFFFFF)  vt_note ("label %s (meta line %d) is not in the labels file", specs[i].label, (int) i);
    }
    if ((have_resume) && (resume_label[0]))
    {
        int  k;

        for (k = 0; k < nlabs; k = k + 1)  if (0 == strcmp (vlab[k].name, resume_label))  resume_off = vlab[k].off;
    }
    v32trace_on = (nspecs > 0) ? TRUE : FALSE;
}

static void  write_files (void);

static uint32_t  word_at (const uint32_t *regs, const cap_t *c, uint32_t i)
{
    mem_t *ram = memory + V32_PAGE_RAM;
    uint32_t a;

    if (c -> reg >= 0)  return (regs[c -> reg] & c -> mask);
    a = c -> addr + ((c -> ireg >= 0) ? (regs[c -> ireg] & c -> mask) : 0) + i;
    return ((a < ram -> size) ? (ram -> data + a) -> value.raw : 0);
}

static void  capture_hit (int is_trap, const char *name)
{
    hit_t   h;
    int     i, n = 0, known = 0;
    char   *k;

    memset (&h, 0, sizeof h);
    h.event = !is_trap;
    snprintf (h.name, sizeof h.name, "%s", name);
    h.frame = cartframe;
    h.segstart = segstart;
    h.count = 1;
    for (i = 0; i < 16; i = i + 1)  h.regs[i] = (uint32_t) REG(i);
    snprintf (h.key, sizeof h.key, "%s", name);
    for (i = 0; i < ncaps; i = i + 1)
    {
        uint32_t j, cnt = (caps[i].reg >= 0) ? 1 : caps[i].count;

        for (j = 0; j < cnt; j = j + 1)
        {
            h.vals[i][j] = word_at (h.regs, caps + i, j);
            if ((!caps[i].vol) && (strlen (h.key) < sizeof h.key - 12))
            {
                k = h.key + strlen (h.key);
                snprintf (k, 12, ",%X", h.vals[i][j]);
            }
        }
    }
    for (n = 0; n < nkinds; n = n + 1)
        for (i = 0; i < ncaps; i = i + 1)
            if ((0 == strcmp (kinds[n].cap, caps[i].name)) && (caps[i].count == 1) && (h.kind[0] == 0))
            {
                int  v;

                for (v = 0; v < kinds[n].n; v = v + 1)
                    if (kinds[n].val[v] == h.vals[i][0])  snprintf (h.kind, sizeof h.kind, "%s", kinds[n].name[v]);
            }
    // the last 32 guest blocks entered (oldest first), from the control transfers
    {
        uint32_t  have = (ring_n < VT_RING) ? ring_n : VT_RING, t;
        uint32_t  tmp[32];
        int       cnt = 0;
        uint32_t  last = 0xFFFFFFFF;

        for (t = have; t > 0 && cnt < 32; t = t - 1)
        {
            uint32_t  to = ring_to[(ring_n - have + t - 1) % VT_RING] - 0x20000000;
            int       lo = 0, hi = nlabs - 1, at = -1;

            while (lo <= hi)
            {
                int  mid = (lo + hi) / 2;

                if (vlab[mid].off <= to)  { at = mid; lo = mid + 1; }  else  hi = mid - 1;
            }
            if ((at >= 0) && (vlab[at].block > 0) && ((uint32_t) vlab[at].block != last))
            {
                tmp[cnt++] = (uint32_t) vlab[at].block;
                last = (uint32_t) vlab[at].block;
            }
        }
        for (i = 0; i < cnt; i = i + 1)  h.trail[i] = tmp[cnt - 1 - i];
        h.ntrail = cnt;
    }
    for (i = 0; i < nhits; i = i + 1)
    {
        if ((hits[i].event == h.event) && (0 == strcmp (hits[i].key, h.key)))
        {
            hits[i].count = hits[i].count + 1;
            known = 1;
            break;
        }
    }
    if (!known)
    {
        if (nhits >= VT_MAX_HITS)  dropped = dropped + 1;
        else
        {
            hits[nhits] = h;
            nhits       = nhits + 1;
            vt_note ("%s #%d at cart frame %d: %s%s%s", is_trap ? "TRAP" : "event", nhits, (int) h.frame, name,
                     h.kind[0] ? " " : "", h.kind);
        }
        write_files ();
    }
    if (is_trap)
    {
        trapped    = 1;
        held       = 0;
        repeat_run = (0 == strcmp (h.key, last_key)) ? repeat_run + 1 : 0;
        snprintf (last_key, sizeof last_key, "%s", h.key);
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// block numbering: the same rules as the DesktopEmulator recorder. vlab[].block holds
// the block key + 1 (0: runtime code) so that "no block" and "block 0:0000" differ
//
static void  assign_blocks (void)
{
    int       k, cur = 0;
    uint32_t  key;

    for (k = 0; k < nlabs; k = k + 1)
    {
        if (match_block (vlab[k].name, &key))  { cur = (int) (key + 1); vlab[k].block = cur; }
        else if ((cur != 0) && (is_cont (vlab[k].name)))  vlab[k].block = cur;
        else  { cur = 0; vlab[k].block = 0; }
    }
}

void  v32trace_instr (uint32_t ip)
{
    uint32_t  off = ip - 0x20000000;

    if (off < cart_words)
    {
        if (cartframe < 0)  cartframe = 0;
        if (padframe != cartframe)                   // the first instruction of a cart frame: what gamepad 1 holds
        {
            uint16_t  mask = 0;
            int       c;

            for (c = 0; c < 11; c = c + 1)
            {
                if ((gamepad != NULL) && (gamepad[0].connected) && (gamepad[0].button[c] > 0))  mask = mask | (uint16_t) (1 << c);
            }
            if (cartframe >= roompads)
            {
                int32_t   r = (roompads == 0) ? 4096 : roompads * 2;
                uint16_t *n;

                while (r <= cartframe)  r = r * 2;
                n = realloc (pads, (size_t) r * sizeof (uint16_t));
                if (n != NULL)  { memset (n + roompads, 0, (size_t) (r - roompads) * sizeof (uint16_t)); pads = n; roompads = r; }
            }
            if (cartframe < roompads)  { pads[cartframe] = mask; if (cartframe + 1 > npads)  npads = cartframe + 1; }
            padframe = cartframe;
        }
        if (!trapped)  counts[off] = counts[off] + 1;
        if (((watch[off >> 5] >> (off & 31)) & 1) && (!trapped))
        {
            int  i;

            for (i = 0; i < nspecs; i = i + 1)
            {
                if (watch_off[i] == off)  { capture_hit (specs[i].trap, specs[i].name); break; }
            }
        }
    }
    if ((have_prev) && (ip != expect_ip) && (!trapped))
    {
        ring_to[ring_n % VT_RING] = ip;
        ring_n                    = ring_n + 1;
    }
    have_prev = 1;
    prev_ip   = ip;
    expect_ip = ip + 1 + (((REG(IR)) & 0x02000000) ? 1 : 0);
}

// once per frame: count cartridge frames, hold a trap screen, then run the cartridge again
void  v32trace_frame (void)
{
    int  i;

    if (cartframe >= 0)  cartframe = cartframe + 1;
    if ((!trapped) || (v32trace_norestart) || (gave_up))  return;
    held = held + 1;
    if (held < VT_HOLD_FRAMES)  return;
    if (repeat_run >= 2)
    {
        gave_up = 1;
        vt_note ("the same trap came back three times in a row: not restarting again");
        return;
    }
    for (i = 0; i < 16; i = i + 1)  REG(i) = 0;
    REG(SP) = (int32_t) ((memory + V32_PAGE_RAM) -> size - 1);
    REG(BP) = REG(SP);
    REG(IP) = (int32_t) (0x20000000 + resume_off);
    rom_offset = (uint32_t) REG(IP);       // the run loop fetches from rom_offset; the WAIT being processed must not step it on
    branchflag = TRUE;
    trapped   = 0;
    have_prev = 0;
    restarts  = restarts + 1;
    segstart  = cartframe;
}

// spec 2.1: the gamepad 1 history of cart frames [from, to) as a V32_INPUT script, frames numbered from 'from':
// BUTTON[+BUTTON]@FIRST-LAST,... (runs of each button, grouped when they share both ends), '-' for none
static void  show_input (FILE *f, int32_t from, int32_t to)
{
    typedef struct { int32_t a, b; uint16_t mask; } run_t;
    run_t   *runs = NULL;
    int      nruns = 0, room = 0, c, i, j, any = 0;
    int32_t  fr, start;

    if (to > npads)  to = npads;
    if (from < 0)  from = 0;
    for (c = 0; c < 11; c = c + 1)
    {
        if (vt_buttons[c] == NULL)  continue;
        start = -1;
        for (fr = from; fr <= to; fr = fr + 1)
        {
            int held = (fr < to) && (pads[fr] & (1 << c));

            if (held && (start < 0))  start = fr;
            if ((!held) && (start >= 0))
            {
                for (i = 0; i < nruns; i = i + 1)  if ((runs[i].a == start - from) && (runs[i].b == fr - 1 - from))  break;
                if (i == nruns)
                {
                    if (nruns == room)
                    {
                        run_t *n = realloc (runs, (size_t) (room = room ? room * 2 : 64) * sizeof (run_t));
                        if (n == NULL)  { free (runs); fprintf (f, "-"); return; }
                        runs = n;
                    }
                    runs[nruns].a = start - from; runs[nruns].b = fr - 1 - from; runs[nruns].mask = 0; nruns = nruns + 1;
                }
                runs[i].mask = runs[i].mask | (uint16_t) (1 << c);
                start = -1;
            }
        }
    }
    for (i = 1; i < nruns; i = i + 1)              // ordered by (first, last), as the recorder's map is
    {
        run_t k = runs[i];
        for (j = i; (j > 0) && ((runs[j - 1].a > k.a) || ((runs[j - 1].a == k.a) && (runs[j - 1].b > k.b))); j = j - 1)  runs[j] = runs[j - 1];
        runs[j] = k;
    }
    for (i = 0; i < nruns; i = i + 1)
    {
        int first = 1;

        fprintf (f, "%s", any ? "," : "");
        for (c = 0; c < 11; c = c + 1)
        {
            if ((runs[i].mask & (1 << c)) && (vt_buttons[c] != NULL))  { fprintf (f, "%s%s", first ? "" : "+", vt_buttons[c]); first = 0; }
        }
        fprintf (f, "@%d-%d", runs[i].a, runs[i].b);
        any = 1;
    }
    if (!any)  fprintf (f, "-");
    free (runs);
}

static void  show_key (FILE *f, uint32_t key)
{
    fprintf (f, "%X:%04X", key >> 16, key & 0xFFFF);
}

static void  write_files (void)
{
    char   path[2048];
    FILE  *f;
    int    i, k;

    if ((v32trace_out == NULL) || (!v32trace_on))  return;
    if (nhits > 0)
    {
        snprintf (path, sizeof path, "%s.traps.txt", (char *) v32trace_out);
        f = fopen (path, "w");
        if (f != NULL)
        {
            const char *slash = strrchr ((char *) cartfile, '/');

            fprintf (f, "# V32TRACE-traps 2 cart=%s fnv1a=%08X restarts=%u\n", slash ? slash + 1 : (char *) cartfile, cart_hash, restarts);
            for (i = 0; i < nhits; i = i + 1)
            {
                hit_t *h = hits + i;
                int    c;
                uint32_t j;

                fprintf (f, "%s n=%d cartframe=%d count=%u arch=%s", h -> event ? "EVENT" : "TRAP", i + 1, (int) h -> frame, h -> count, arch);
                if (h -> event)  fprintf (f, " name=%s", h -> name);
                if (h -> kind[0])  fprintf (f, " kind=%s", h -> kind);
                for (c = 0; c < ncaps; c = c + 1)
                {
                    uint32_t cnt = (caps[c].reg >= 0) ? 1 : caps[c].count;

                    fprintf (f, " %s=", caps[c].name);
                    for (j = 0; j < cnt; j = j + 1)  fprintf (f, "%s%X", j ? "," : "", h -> vals[c][j]);
                }
                fprintf (f, " trail=");
                if (h -> ntrail == 0)  fprintf (f, "-");
                for (k = 0; k < h -> ntrail; k = k + 1)
                {
                    if (k)  fprintf (f, ",");
                    show_key (f, h -> trail[k] - 1);
                }
                fprintf (f, " segstart=%d input=", (int) h -> segstart);        // spec 2.1
                show_input (f, h -> segstart, h -> frame + 1);
                fprintf (f, "\n");
            }
            fclose (f);
        }
    }
    // blocks that ran: instructions per guest block, ascending by location
    {
        snprintf (path, sizeof path, "%s.blocks.txt", (char *) v32trace_out);
        f = fopen (path, "w");
        if (f != NULL)
        {
            uint64_t *sum = NULL;
            uint32_t *keys = NULL;
            int       nk = 0, capk = 0;
            const char *slash = strrchr ((char *) cartfile, '/');

            fprintf (f, "# %s blocks that ran (bank:address in hex, instructions executed)\n# cart=%s fnv1a=%08X\n", arch,
                     slash ? slash + 1 : (char *) cartfile, cart_hash);
            for (k = 0; k < nlabs; k = k + 1)
            {
                uint32_t  end = (k + 1 < nlabs) ? vlab[k + 1].off : cart_words, a;
                uint64_t  n = 0;
                int       at;

                if (vlab[k].block == 0)  continue;
                for (a = vlab[k].off; a < end; a = a + 1)  n = n + counts[a];
                for (at = 0; at < nk; at = at + 1)  if (keys[at] == (uint32_t) vlab[k].block)  break;
                if (at == nk)
                {
                    if (nk == capk)
                    {
                        capk = capk ? capk * 2 : 1024;
                        keys = (uint32_t *) realloc (keys, (size_t) capk * 4);
                        sum  = (uint64_t *) realloc (sum,  (size_t) capk * 8);
                    }
                    keys[nk] = (uint32_t) vlab[k].block;
                    sum[nk]  = 0;
                    nk       = nk + 1;
                }
                sum[at] = sum[at] + n;
            }
            // ascending by key (insertion order is by label address, so sort)
            for (i = 1; i < nk; i = i + 1)
            {
                uint32_t kk = keys[i]; uint64_t ss = sum[i];

                for (k = i - 1; k >= 0 && keys[k] > kk; k = k - 1)  { keys[k + 1] = keys[k]; sum[k + 1] = sum[k]; }
                keys[k + 1] = kk; sum[k + 1] = ss;
            }
            for (i = 0; i < nk; i = i + 1)
            {
                if (sum[i] == 0)  continue;
                show_key (f, keys[i] - 1);
                fprintf (f, " %llu\n", (unsigned long long) sum[i]);
            }
            free (keys); free (sum);
            fclose (f);
        }
    }
}

void  v32trace_write (void)
{
    if (!v32trace_on)  return;
    write_files ();
    vt_note ("%d distinct trap%s/event%s, %u restart%s -> %s.traps.txt, %s.blocks.txt", nhits, nhits == 1 ? "" : "s", nhits == 1 ? "" : "s",
             restarts, restarts == 1 ? "" : "s", (char *) v32trace_out, (char *) v32trace_out);
}

// called by v32trace_init's caller after the labels are known
void  v32trace_ready (void)
{
    if (v32trace_on)  assign_blocks ();
}
