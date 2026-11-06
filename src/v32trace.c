#include "defines.h"
#include <stdarg.h>
#include <time.h>

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
//     PREFIX.trace.txt     the human report (spec section 4.3), laid out as the DesktopEmulator
//                          recorder (GBtoV32's emulator-v32trace.patch) writes it: [events],
//                          [input], [perf], [host], series, hot spots, [markers]
//
// the same files the DesktopEmulator recorder writes, so a translator's gap-filling tool
// reads either. After a trap the cartridge is restarted from its entry point once the
// trap screen has been up for 60 frames (--v32trace-norestart turns this off), so one
// unattended run can meet several gaps. `mark [NOTE]` drops a marker (F8 in the recorder),
// `series NAME ADDR` lines of the meta are sampled every frame, and [host] uses the
// recorder's quad / video-batch model (gpu.c); host milliseconds are absent, as in the
// recorder's headless runs.
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
#define  VT_MAX_EVENTS   200
#define  VT_MAX_MARKERS  64
#define  VT_MAX_SERIES   8
#define  VF_OVERRAN      1                // the cartridge did not reach WAIT in time
#define  VF_CPUERR       2                // the CPU raised a hardware error
#define  VF_HELD         8                // the cartridge was stopped in a trap routine

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
    uint32_t  from[12], to[12];  // the last V32 control transfers, oldest first (report)
    int       nxfer;
}  hit_t;

typedef struct { uint32_t cycles; uint8_t flags; uint8_t gpu; uint16_t quads; uint16_t flushes; }  vframe_t;
typedef struct { int32_t frame; char text[320]; }  vevent_t;
typedef struct { int32_t frame; char note[512]; uint64_t *snap; }  vmark_t;
typedef struct { char name[64]; uint32_t addr; int32_t *val; }  vseries_t;

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
static uint16_t *pads2     = NULL;      // spec 2.2: the same for gamepad 2
static int      pads2_used = 0;         // gamepad 2 held a button at some point
static int32_t   npads     = 0, roompads = 0, padframe = -1;
static vframe_t *vfr       = NULL;      // report: per cart frame (same room as pads)
static int32_t   nvfr      = 0;         // frames recorded in vfr
static vevent_t  events[VT_MAX_EVENTS]; static int nevents = 0;
static vmark_t   marks[VT_MAX_MARKERS]; static int nmarks  = 0;
static vseries_t series[VT_MAX_SERIES]; static int nseries = 0;


// the controls in INP port order; the V32_INPUT names the recorders use (Y has none)
static const char *const vt_buttons[11] = { "LEFT", "RIGHT", "UP", "DOWN", "START", "A", "B", "SELECT", NULL, "L", "R" };

static uint32_t  ring_to[VT_RING];  static uint32_t ring_n = 0;
static uint32_t  ring_from[VT_RING];
static int32_t   restart_at[256];   static int nrestart_at = 0;     // cart frames of the restarts
static uint32_t  boot_frames = 0;                                   // frames before the cartridge ran
static char      started_at[32] = "";
static char      labels_path[2048] = "";
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
        else if ((0 == strcmp (key, "series")) && (nseries < VT_MAX_SERIES))
        {
            char      nm[64];
            unsigned  ad = 0;

            if (sscanf (line, "%*s %63s %x", nm, &ad) == 2)
            {
                snprintf (series[nseries].name, sizeof series[nseries].name, "%s", nm);
                series[nseries].addr = ad;
                nseries = nseries + 1;
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

static char  last_opened[2048 + 512 + 64] = "";            // the path open_beside() opened (the report names the labels file)

static FILE  *open_beside (const char *cart, const char *stem_suffix, const char *plain)
{
    char   path[2048 + 512 + 64], dir[2048], stem[512];
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
    if (f != NULL)  snprintf (last_opened, sizeof last_opened, "%s", path);
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
    snprintf (labels_path, sizeof labels_path, "%s", (v32trace_label_file != NULL) ? (char *) v32trace_label_file : last_opened);
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
    {
        time_t     now = time (NULL);
        struct tm *lt  = localtime (&now);

        strftime (started_at, sizeof started_at, "%Y-%m-%d %H:%M:%S", lt);
    }
}

static void  write_files (void);
static void  write_report (void);
static const char *report_reason = "in progress";

// room for cart frame cf in every per-frame array (pads, vfr, series); FALSE when out of memory
static int  ensure_frames (int32_t cf)
{
    int32_t   r;
    int       i;

    if (cf < roompads)  return (1);
    r = (roompads == 0) ? 4096 : roompads * 2;
    while (r <= cf)  r = r * 2;
    {
        uint16_t *np = realloc (pads, (size_t) r * sizeof (uint16_t));
        uint16_t *n2 = NULL;
        vframe_t *nv;

        if (np == NULL)  return (0);
        pads = np;
        n2 = realloc (pads2, (size_t) r * sizeof (uint16_t));
        if (n2 == NULL)  return (0);
        pads2 = n2;
        memset (pads2 + roompads, 0, (size_t) (r - roompads) * sizeof (uint16_t));
        nv = realloc (vfr, (size_t) r * sizeof (vframe_t));
        if (nv == NULL)  return (0);
        vfr = nv;
        memset (pads + roompads, 0, (size_t) (r - roompads) * sizeof (uint16_t));
        memset (vfr + roompads, 0, (size_t) (r - roompads) * sizeof (vframe_t));
        for (i = 0; i < nseries; i = i + 1)
        {
            int32_t *ns = realloc (series[i].val, (size_t) r * sizeof (int32_t));

            if (ns == NULL)  return (0);
            series[i].val = ns;
            memset (ns + roompads, 0, (size_t) (r - roompads) * sizeof (int32_t));
        }
    }
    roompads = r;
    return (1);
}

static void  add_event (int32_t frame, const char *fmt, ...)
{
    va_list  ap;

    if (nevents >= VT_MAX_EVENTS)  return;
    events[nevents].frame = frame;
    va_start (ap, fmt);
    vsnprintf (events[nevents].text, sizeof events[nevents].text, fmt, ap);
    va_end (ap);
    nevents = nevents + 1;
}

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
        h.nxfer  = (have < 12) ? (int) have : 12;
        for (i = 0; i < h.nxfer; i = i + 1)
        {
            uint32_t  slot = (ring_n - (uint32_t) h.nxfer + (uint32_t) i) % VT_RING;

            h.from[i] = ring_from[slot];
            h.to[i]   = ring_to[slot];
        }
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
            add_event (cartframe, "%s #%d: %s%s%s%s", is_trap ? "TRAP" : "event", nhits, name,
                       h.kind[0] ? " (" : "", h.kind, h.kind[0] ? ")" : "");
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
        if (cartframe < 0)  { cartframe = 0; boot_frames = frames_run; }
        if (padframe != cartframe)                   // the first instruction of a cart frame: what gamepad 1 holds
        {
            uint16_t  mask = 0;
            uint16_t  mask2 = 0;
            int       c;

            for (c = 0; c < 11; c = c + 1)
            {
                if ((gamepad != NULL) && (gamepad[0].connected) && (gamepad[0].button[c] > 0))  mask = mask | (uint16_t) (1 << c);
                if ((gamepad != NULL) && (V32_NUM_GAMEPADS > 1) && (gamepad[1].connected) && (gamepad[1].button[c] > 0))  mask2 = mask2 | (uint16_t) (1 << c);
            }
            if (ensure_frames (cartframe))
            {
                pads[cartframe]   = mask;
                pads2[cartframe]  = mask2;
                if (mask2 & 0x06FF)  pads2_used = 1;
                if (cartframe + 1 > npads)  npads = cartframe + 1;
            }
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
        ring_to[ring_n % VT_RING]   = ip;
        ring_from[ring_n % VT_RING] = prev_ip;
        ring_n                      = ring_n + 1;
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
        add_event (cartframe - 1, "the same trap came back three times in a row: not restarting again (play on from a new reset to see more)");
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
    segstart  = cartframe - 1;              // as the recorder: the frame in which the restart was decided
    if (nrestart_at < 256)  restart_at[nrestart_at++] = segstart;
    add_event (segstart, "cartridge restarted after the trap (restart %u)", restarts);
}

// spec 2.1: the gamepad 1 history of cart frames [from, to) as a V32_INPUT script, frames numbered from 'from':
// BUTTON[+BUTTON]@FIRST-LAST,... (runs of each button, grouped when they share both ends), '-' for none
static void  show_input (FILE *f, const uint16_t *pads, int32_t from, int32_t to)
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
{    write_report ();

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
                show_input (f, pads, h -> segstart, h -> frame + 1);
                if (pads2_used)                                                  // spec 2.2: only when the second gamepad was used
                {
                    fprintf (f, " input2=");
                    show_input (f, pads2, h -> segstart, h -> frame + 1);
                }
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
    report_reason = "the run ended";
    write_files ();
    vt_note ("%d distinct trap%s/event%s, %u restart%s -> %s.traps.txt, %s.blocks.txt, %s.trace.txt", nhits, nhits == 1 ? "" : "s", nhits == 1 ? "" : "s",
             restarts, restarts == 1 ? "" : "s", (char *) v32trace_out, (char *) v32trace_out, (char *) v32trace_out);
}

// called by v32trace_init's caller after the labels are known
void  v32trace_ready (void)
{
    if (v32trace_on)  assign_blocks ();
}

////////////////////////////////////////////////////////////////////////////////////////
//
// per-frame record, CPU errors, markers (the recorder's EndFrame, OnCPUError, AddMarker)
//
void  v32trace_frame_end (void)
{
    int32_t   remain = 0;
    uint32_t  used   = 0;
    double    load   = 0.0;
    int       i;

    if ((cartframe < 0) || (!ensure_frames (cartframe)))  return;
    vfr[cartframe].cycles   = (uint32_t) IPORTGET(TIM_CycleCounter);
    if (trapped)                            vfr[cartframe].flags |= VF_HELD;
    else if ((!waitflag) && (!haltflag))    vfr[cartframe].flags |= VF_OVERRAN;
    remain = (int32_t) IPORTGET(GPU_RemainingPixels);
    used   = (uint32_t) ((9 * 640 * 360) - ((remain < 0) ? 0 : remain));
    load   = 100.0 * (double) used / (double) (9 * 640 * 360);
    vfr[cartframe].gpu      = (uint8_t) ((load < 0.0) ? 0.0 : ((load > 255.0) ? 255.0 : load));
    vfr[cartframe].quads    = (uint16_t) ((gpustat_quads > 65535) ? 65535 : gpustat_quads);
    vfr[cartframe].flushes  = (uint16_t) ((gpustat_flushes + (gpustat_pending > 0 ? 1 : 0) > 65535) ? 65535
                                          : gpustat_flushes + (gpustat_pending > 0 ? 1 : 0));
    for (i = 0; i < nseries; i = i + 1)
    {
        mem_t *ram = memory + V32_PAGE_RAM;

        series[i].val[cartframe] = (series[i].addr < ram -> size) ? (int32_t) (ram -> data + series[i].addr) -> value.raw : 0;
    }
    if (cartframe + 1 > nvfr)  nvfr = cartframe + 1;
}

static const char  *error_name (int32_t code)
{
    switch (code)
    {
        case ERROR_MEMORY_READ:      return ("invalid memory read");
        case ERROR_MEMORY_WRITE:     return ("invalid memory write");
        case ERROR_PORT_READ:        return ("invalid port read");
        case ERROR_PORT_WRITE:       return ("invalid port write");
        case ERROR_STACK_OVERFLOW:   return ("stack overflow");
        case ERROR_STACK_UNDERFLOW:  return ("stack underflow");
        case ERROR_DIVISION:         return ("division error");
        default:                     return ("error");
    }
}

static int  vt_find_label (uint32_t off)            // the last label at or before a cartridge offset
{
    int  lo = 0, hi = nlabs - 1, at = -1;

    while (lo <= hi)
    {
        int  mid = (lo + hi) / 2;

        if (vlab[mid].off <= off)  { at = mid; lo = mid + 1; }  else  hi = mid - 1;
    }
    return (at);
}

static void  describe_address (uint32_t a, char *out, size_t len)
{
    uint32_t  off = a - 0x20000000;

    if (off < cart_words)
    {
        int  at = vt_find_label (off);

        if (at < 0)                       snprintf (out, len, "0x%08X", a);
        else if (vlab[at].off == off)     snprintf (out, len, "%s", vlab[at].name);
        else                              snprintf (out, len, "%s+%u", vlab[at].name, off - vlab[at].off);
        return;
    }
    if ((a >= 0x10000000) && (a < 0x20000000))  { snprintf (out, len, "bios+%X", a - 0x10000000); return; }
    snprintf (out, len, "0x%08X", a);
}

void  v32trace_cpu_error (int32_t code, uint32_t ip)
{
    char  where[512];

    if ((!v32trace_on) || (cartframe < 0))  return;
    if (ensure_frames (cartframe))  vfr[cartframe].flags |= VF_CPUERR;
    describe_address (ip, where, sizeof where);
    add_event (cartframe, "CPU hardware error %d (%s) at %s; the BIOS error screen follows", (int) code, error_name (code), where);
    write_files ();
}

static uint64_t  *aggregate (void)               // instructions per label; the last slot: before the first label
{
    uint64_t  *by = (uint64_t *) calloc ((size_t) nlabs + 1, sizeof (uint64_t));
    uint32_t   a;
    int        at = -1;

    if (by == NULL)  return (NULL);
    for (a = 0; a < cart_words; a = a + 1)
    {
        if (!counts[a])  continue;
        while ((at + 1 < nlabs) && (vlab[at + 1].off <= a))  at = at + 1;
        by[(at < 0) ? nlabs : at] += counts[a];
    }
    return (by);
}

void  v32trace_mark (const char *note)
{
    vmark_t  *m;
    int       c;

    if (!v32trace_on)
    {
        fprintf (stderr, "[ERROR] mark: no V32TRACE session (--v32trace=PREFIX and the cartridge's meta file)\n");
        return;
    }
    if (nmarks >= VT_MAX_MARKERS)  return;
    m = marks + nmarks;
    // the recorder's CurrentCartFrame(): the frame most recently begun (between two frames, the one that ended)
    m -> frame = (cartframe < 0) ? -1 : ((padframe == cartframe) ? cartframe : cartframe - 1);
    snprintf (m -> note, sizeof m -> note, "%s", (note != NULL) ? note : "");
    for (c = 0; c < ncaps; c = c + 1)            // where the game is, in the translator's terms
    {
        uint32_t  regs[16], j, cnt = caps[c].count;
        size_t    n = strlen (m -> note);

        if ((caps[c].reg >= 0) || (caps[c].ireg >= 0))  continue;
        for (j = 0; j < 16; j = j + 1)  regs[j] = (uint32_t) REG(j);
        snprintf (m -> note + n, sizeof m -> note - n, "%s[%s=", n ? " " : "", caps[c].name);
        for (j = 0; j < cnt; j = j + 1)
        {
            n = strlen (m -> note);
            snprintf (m -> note + n, sizeof m -> note - n, "%s%X", j ? "," : "", word_at (regs, caps + c, j));
        }
        n = strlen (m -> note);
        snprintf (m -> note + n, sizeof m -> note - n, "]");
    }
    m -> snap = aggregate ();
    nmarks = nmarks + 1;
    add_event (m -> frame, "marker %d%s%s", nmarks, (note && note[0]) ? ": " : "", (note != NULL) ? note : "");
    fprintf (stderr, "[v32trace] marker %d at cart frame %d\n", nmarks, (int) m -> frame);
    write_files ();
}

////////////////////////////////////////////////////////////////////////////////////////
//
// the human report: PREFIX.trace.txt (spec 4.3, the recorder's BuildReport layout)
//
static void  host_summary (FILE *f, int32_t first, int32_t last)
{
    int32_t   i, count = last - first + 1;
    uint64_t  q = 0, fl = 0;
    uint32_t  mq = 0, mf = 0;

    if ((cartframe < 0) || (last < first))  { fprintf (f, "no frames\n"); return; }
    for (i = first; i <= last && i < nvfr; i = i + 1)
    {
        if (vfr[i].flags & VF_HELD)  { count = count - 1; continue; }
        q  += vfr[i].quads;    fl += vfr[i].flushes;
        if (vfr[i].quads   > mq)  mq = vfr[i].quads;
        if (vfr[i].flushes > mf)  mf = vfr[i].flushes;
    }
    if (count <= 0)  { fprintf (f, "no frames\n"); return; }
    fprintf (f, "frames %d-%d: quads/frame avg %llu max %u, flushes/frame avg %llu max %u\n", first, last,
             (unsigned long long) (q / count), mq, (unsigned long long) (fl / count), mf);
}

static void  series_summary (FILE *f, int32_t first, int32_t last, const char *indent)
{
    int  s;

    if ((cartframe < 0) || (last < first))  return;
    for (s = 0; s < nseries; s = s + 1)
    {
        int64_t  sum = 0;
        int32_t  mx = INT32_MIN, mn = INT32_MAX, at = first, n = 0, i;

        for (i = first; i <= last && i < nvfr; i = i + 1)
        {
            int32_t  v;

            if (vfr[i].flags & VF_HELD)  continue;
            v = series[s].val[i];
            sum += v; n = n + 1;
            if (v < mn)  mn = v;
            if (v > mx)  { mx = v; at = i; }
        }
        if (n)  fprintf (f, "%sseries %s: avg %.1f min %d max %d (frame %d)\n", indent, series[s].name, (double) sum / n, (int) mn, (int) mx, (int) at);
    }
}

typedef struct { uint64_t n; size_t i; }  rank_t;
static int  cmp_rank (const void *a, const void *b)
{
    const rank_t *x = a, *y = b;

    return ((x -> n < y -> n) - (x -> n > y -> n));
}

static void  profile (FILE *f, const uint64_t *by, int32_t first, int32_t last, int top)
{
    uint64_t   total = 0, inblocks = 0;
    rank_t    *r = NULL;
    size_t     nr = 0, i;
    int        k;

    if (by == NULL)  return;
    for (i = 0; i <= (size_t) nlabs; i = i + 1)
    {
        total += by[i];
        if ((i < (size_t) nlabs) && (vlab[i].block > 0))  inblocks += by[i];
    }
    fprintf (f, "PC profile frames %d-%d, %llu instructions\n", first, last, (unsigned long long) total);
    if (total == 0)  return;
    r = (rank_t *) calloc ((size_t) nlabs + 1, sizeof (rank_t));
    if (r == NULL)  return;
    for (i = 0; i <= (size_t) nlabs; i = i + 1)  if (by[i])  { r[nr].n = by[i]; r[nr].i = i; nr = nr + 1; }
    qsort (r, nr, sizeof (rank_t), cmp_rank);
    for (k = 0; k < top && (size_t) k < nr; k = k + 1)
    {
        fprintf (f, "  %8llu %5.1f%%  %s\n", (unsigned long long) r[k].n, 100.0 * (double) r[k].n / (double) total,
                 (r[k].i < (size_t) nlabs) ? vlab[r[k].i].name : "(before first label)");
    }
    // guest blocks (bank:address), summed over their labels
    {
        uint32_t *keys = NULL; uint64_t *sum = NULL; size_t nk = 0, cap = 0, j;
        rank_t   *br;

        for (i = 0; i < (size_t) nlabs; i = i + 1)
        {
            if ((vlab[i].block <= 0) || (!by[i]))  continue;
            for (j = 0; j < nk; j = j + 1)  if (keys[j] == (uint32_t) vlab[i].block)  break;
            if (j == nk)
            {
                if (nk == cap)  { cap = cap ? cap * 2 : 256; keys = realloc (keys, cap * 4); sum = realloc (sum, cap * 8); }
                keys[nk] = (uint32_t) vlab[i].block; sum[nk] = 0; nk = nk + 1;
            }
            sum[j] += by[i];
        }
        if (npats > 0)
        {
            fprintf (f, "%s blocks: %.1f%% of instructions run in translated blocks, the rest in runtime code\n", arch,
                     100.0 * (double) inblocks / (double) total);
            br = (rank_t *) calloc (nk + 1, sizeof (rank_t));
            for (j = 0; br && j < nk; j = j + 1)  { br[j].n = sum[j]; br[j].i = j; }
            if (br)  qsort (br, nk, sizeof (rank_t), cmp_rank);
            for (k = 0; br && k < top && (size_t) k < nk; k = k + 1)
            {
                uint32_t  key = keys[br[k].i] - 1;

                fprintf (f, "  %8llu %5.1f%%  BLK %X:%04X\n", (unsigned long long) br[k].n, 100.0 * (double) br[k].n / (double) total,
                         key >> 16, key & 0xFFFF);
            }
            free (br);
        }
        free (keys); free (sum);
    }
    free (r);
}

static void  write_report (void)
{
    char        path[2048], where1[512], where2[512];
    FILE       *f;
    const char *slash = strrchr ((char *) cartfile, '/');
    char        title[65] = "";
    int32_t     cf = (cartframe < 0) ? 0 : nvfr;    // cart frames recorded
    int         i, k, nblocks = 0;
    uint32_t    last = 0;

    snprintf (path, sizeof path, "%s.trace.txt", (char *) v32trace_out);
    f = fopen (path, "w");
    if (f == NULL)  return;
    {
        FILE *c = fopen ((char *) cartfile, "rb");           // the ROM header: the title is bytes 16..79

        if (c != NULL)
        {
            if ((fseek (c, 16, SEEK_SET) == 0) && (fread (title, 1, 64, c) == 64))  title[64] = 0;  else  title[0] = 0;
            fclose (c);
        }
    }
    for (k = 0; k < nlabs; k = k + 1)  if ((vlab[k].block > 0) && ((uint32_t) vlab[k].block != last))  { nblocks++; last = (uint32_t) vlab[k].block; }
    fprintf (f, "# V32TRACE report, spec 1\n");
    fprintf (f, "# Written by v32sim %s (V32TRACE, the DesktopEmulator recorder's layout). Send this whole file.\n", VERSION);
    fprintf (f, "session: started %s local time; this copy written because: %s\n", started_at, report_reason);
    fprintf (f, "cart: file=\"%s\" title=\"%s\" words=%u fnv1a=%08X\n", slash ? slash + 1 : (char *) cartfile, title, cart_words, cart_hash);
    fprintf (f, "arch: %s\n", arch);
    {
        const char *b = (biosfile != NULL) ? strrchr ((char *) biosfile, '/') : NULL;

        fprintf (f, "bios: %s\n", (biosfile == NULL) ? "(default)" : (b ? b + 1 : (char *) biosfile));
    }
    fprintf (f, "labels: %s (%d labels, %d translated blocks)\n", labels_path, nlabs, nblocks);
    fprintf (f, "frames: total=%u boot=%u cart=%d\n", frames_run, (cartframe < 0) ? frames_run : boot_frames, cf);
    fprintf (f, "frame numbers below are cartridge frames: 0 is the first frame in which the cartridge ran\n");

    fprintf (f, "\n[events]\n");
    if ((nevents == 0) && (nhits == 0))  fprintf (f, "none\n");
    for (i = 0; i < nevents; i = i + 1)  fprintf (f, "frame %d: %s\n", (int) events[i].frame, events[i].text);
    for (i = 0; i < nhits; i = i + 1)
    {
        hit_t  *h = hits + i;
        int     c, shown = 0, from;

        fprintf (f, "%s #%d at frame %d: %s arch=%s, seen %u time%s%s%s\n", h -> event ? "EVENT" : "TRAP", i + 1, (int) h -> frame, h -> name, arch,
                 h -> count, h -> count == 1 ? "" : "s", h -> kind[0] ? ", kind " : "", h -> kind);
        for (c = 0; c < ncaps; c = c + 1)
        {
            uint32_t j, cnt = (caps[c].reg >= 0) ? 1 : caps[c].count;
            int      n;

            fprintf (f, "  capture %s=", caps[c].name);
            for (j = 0; j < cnt; j = j + 1)  fprintf (f, "%s%X", j ? "," : "", h -> vals[c][j]);
            for (n = 0; n < nkinds; n = n + 1)
                if ((0 == strcmp (kinds[n].cap, caps[c].name)) && (cnt == 1))
                {
                    int v;
                    for (v = 0; v < kinds[n].n; v = v + 1)  if (kinds[n].val[v] == h -> vals[c][0])  fprintf (f, " (%s)", kinds[n].name[v]);
                }
            fprintf (f, "\n");
        }
        if (!h -> event)
        {
            fprintf (f, "  regs:");
            for (c = 0; c < 16; c = c + 1)  fprintf (f, " R%d=%X", c, h -> regs[c]);
            fprintf (f, "\n");
        }
        from = h -> event ? ((h -> ntrail > 8) ? h -> ntrail - 8 : 0) : 0;   // an event shows its last 8 blocks
        for (k = from; k < h -> ntrail; k = k + 1)
        {
            if (!shown)  fprintf (f, "  %s block trail (oldest first):", arch);
            fprintf (f, " %X:%04X", (h -> trail[k] - 1) >> 16, (h -> trail[k] - 1) & 0xFFFF);
            shown = 1;
        }
        if (shown)  fprintf (f, "\n");
        if (!h -> event)
        {
            fprintf (f, "  V32 transfer trail (last 12, oldest first):\n");
            for (k = 0; k < h -> nxfer; k = k + 1)
            {
                describe_address (h -> from[k], where1, sizeof where1);
                describe_address (h -> to[k], where2, sizeof where2);
                fprintf (f, "    %s -> %s\n", where1, where2);
            }
        }
    }
    if (dropped)  fprintf (f, "(%u more distinct traps or events were not recorded: the limit is %d)\n", dropped, VT_MAX_HITS);
    if ((restarts) || (trapped && !v32trace_norestart))
        fprintf (f, "the cartridge was restarted %u time%s after traps; frames spent in trap routines are left out of [perf], [host] and the hot spots\n",
                 restarts, restarts == 1 ? "" : "s");

    fprintf (f, "\n[input]\n");
    fprintf (f, "# gamepad 1, cart frames 0-%d. Replay with: v32sim --input=@this_file --max-frames=%d (+ the BIOS logo) cart.v32\n", (cf > 0) ? cf - 1 : 0, cf);
    if (nrestart_at > 0)
    {
        fprintf (f, "# restarts at cart frames:");
        for (i = 0; i < nrestart_at; i = i + 1)  fprintf (f, " %d", (int) restart_at[i]);
        fprintf (f, "\n");
    }
    fprintf (f, "V32_INPUT=");
    if (cf > 0)
    {
        uint16_t  any = 0;                       // the recorder writes nothing after '=' when no button was held

        for (i = 0; i < cf && i < npads; i = i + 1)  any = any | pads[i];
        if (any & 0x06FF)  show_input (f, pads, 0, cf);    // the named buttons (Y has no name)
    }
    fprintf (f, "\n");
    if (pads2_used)                                          // spec 2.2: gamepad 2, replayed with --input-port=1
    {
        fprintf (f, "# gamepad 2 (replay with --input2=@this_file: it reads the V32_INPUT2= line)\n");
        fprintf (f, "V32_INPUT2=");
        show_input (f, pads2, 0, cf);
        fprintf (f, "\n");
    }

    fprintf (f, "\n[perf]\n");
    {
        int32_t   valid = 0;

        for (i = 0; i < cf; i = i + 1)  if (!(vfr[i].flags & VF_HELD))  valid = valid + 1;
        if (valid != cf)  fprintf (f, "(%d of %d frames were spent in trap routines and are left out)\n", cf - valid, cf);
        if (valid > 0)
        {
            uint64_t  sum = 0, gsum = 0;
            uint32_t  mx = 0, over = 0, overran = 0, errs = 0;
            int       runs = 0, step = 60, w, nw = 0;
            int32_t   worst[10];

            for (i = 0; i < cf; i = i + 1)
            {
                if (vfr[i].flags & VF_HELD)  continue;
                sum += vfr[i].cycles;  gsum += vfr[i].gpu;
                if (vfr[i].cycles > mx)  mx = vfr[i].cycles;
                if (vfr[i].cycles > V32_CYCLES_PER_FRAME)  over++;
                if (vfr[i].flags & VF_OVERRAN)  overran++;
                if (vfr[i].flags & VF_CPUERR)   errs++;
            }
            fprintf (f, "cycles/frame from frame 0: avg %llu max %u, %u of %d frames over %d\n", (unsigned long long) (sum / valid), mx, over, valid,
                     V32_CYCLES_PER_FRAME);
            fprintf (f, "frames where the cartridge missed its WAIT (dropped frames): %u; frames with a CPU error: %u; average GPU load %llu%%\n",
                     overran, errs, (unsigned long long) (gsum / valid));
            {
                char  buf[1024] = "";

                for (i = 0; i < cf; i = i + 1)
                {
                    int32_t j = i;

                    if (!(vfr[i].flags & VF_OVERRAN))  continue;
                    while ((j + 1 < cf) && (vfr[j + 1].flags & VF_OVERRAN))  j = j + 1;
                    if (runs < 20)  snprintf (buf + strlen (buf), sizeof buf - strlen (buf), " %d-%d", (int) i, (int) j);
                    runs = runs + 1;
                    i = j;
                }
                if (runs > 0)  fprintf (f, "dropped frame runs (%d):%s%s\n", runs, buf, runs > 20 ? " ..." : "");
            }
            for (i = 0; i < cf; i = i + 1)                       // the 10 most expensive frames, ties by frame number
            {
                if (vfr[i].flags & VF_HELD)  continue;
                for (w = nw; w > 0 && ((vfr[worst[w - 1]].cycles < vfr[i].cycles)); w = w - 1)  if (w < 10)  worst[w] = worst[w - 1];
                if (w < 10)  { worst[w] = i; if (nw < 10)  nw = nw + 1; }
            }
            fprintf (f, "most expensive frames:");
            for (w = 0; w < nw; w = w + 1)  fprintf (f, " %d(%u)", (int) worst[w], vfr[worst[w]].cycles);
            fprintf (f, "\n");
            while (cf / step > 120)  step = step + 60;
            fprintf (f, "timeline (%d frames per row): frames, avg cycles, max cycles, dropped, max GPU load\n", step);
            for (k = 0; k < cf; k = k + step)
            {
                int32_t   end = (k + step < cf) ? k + step : cf;
                uint64_t  rs = 0;
                uint32_t  rmx = 0, rdr = 0, rg = 0, rv = 0;

                for (i = k; i < end; i = i + 1)
                {
                    if (vfr[i].flags & VF_HELD)  continue;
                    rv++; rs += vfr[i].cycles;
                    if (vfr[i].cycles > rmx)  rmx = vfr[i].cycles;
                    if (vfr[i].gpu > rg)      rg  = vfr[i].gpu;
                    if (vfr[i].flags & VF_OVERRAN)  rdr++;
                }
                if (rv == 0)  continue;
                fprintf (f, "  %6d-%-6d %7llu %7u %4u %4u%%\n", k, (int) end - 1, (unsigned long long) (rs / rv), rmx, rdr, rg);
            }
        }
        else  fprintf (f, "the cartridge never ran\n");
    }

    fprintf (f, "\n[host]\n");
    fprintf (f, "# quads = draw commands, flushes = batches the video queue sends (a batch ends when the queue holds 20 quads or the texture, multiply colour or blending changes);\n");
    fprintf (f, "# frame ms is wall time in the emulator (absent for headless runs). A frame that takes more than 16.7 ms makes the main loop owe frames: catch-up frames.\n");
    host_summary (f, 0, cf - 1);
    series_summary (f, 0, cf - 1, "");

    fprintf (f, "\n[hot spots, whole session]\n");
    {
        uint64_t *total = aggregate ();

        profile (f, total, 0, (cf > 0) ? cf - 1 : 0, 25);
        if (nmarks > 0)
        {
            uint64_t *prev = (uint64_t *) calloc ((size_t) nlabs + 1, sizeof (uint64_t));
            uint64_t *seg  = (uint64_t *) calloc ((size_t) nlabs + 1, sizeof (uint64_t));
            int32_t   pf = 0;
            int       m;

            fprintf (f, "\n[markers]\n");
            for (m = 0; prev && seg && total && m <= nmarks; m = m + 1)
            {
                const uint64_t *now = (m < nmarks) ? marks[m].snap : total;
                int32_t         nf  = (m < nmarks) ? marks[m].frame : ((cf > 0) ? cf - 1 : 0);
                char            ind[300];

                for (i = 0; i <= nlabs; i = i + 1)  seg[i] = (now && now[i] >= prev[i]) ? now[i] - prev[i] : 0;
                if (m < nmarks)  fprintf (f, "marker %d at frame %d%s%s\n", m + 1, (int) marks[m].frame, marks[m].note[0] ? ": " : "", marks[m].note);
                fprintf (f, "segment %d: ", m + 1);
                profile (f, seg, pf, nf, 12);
                fprintf (f, "  host ");
                host_summary (f, pf, nf);
                snprintf (ind, sizeof ind, "  ");
                series_summary (f, pf, nf, ind);
                if ((m < nmarks) && (now != NULL))  { memcpy (prev, now, ((size_t) nlabs + 1) * sizeof (uint64_t)); pf = nf; }
            }
            free (prev); free (seg);
        }
        free (total);
    }
    fclose (f);
}
