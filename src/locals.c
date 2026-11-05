#include "defines.h"
#include <ctype.h>

////////////////////////////////////////////////////////////////////////////////////////
//
// locals.c: local variables and their values (V32DBG section 4)
//
// The compilers describe every named local with a comment line in the assembly
// (`;@var NAME loc=BP-3 type=int`, inside `;@scope {` ... `}` blocks: v32cc -g, v32lua -g),
// and `v32as -g` turns those into NAME.vbin.vars beside NAME.vbin.debug, with the address
// range in which each variable is visible (only the assembler knows the addresses):
//
//     scope,START,END,DEPTH[,FUNCTION]
//     var,START,END,NAME,LOC,TYPE,FLAGS,DEPTH[,FUNCTION]
//
// START is the first address at which the variable is visible, END the first address at
// which it is not. LOC is `BP+N` / `BP-N` (a frame word), `Rn` (a register, after
// v32opt's promotion), `@ADDR` or a symbol (a word in RAM), or `-` (optimized out). TYPE
// is v32cc's `int uint float bool ptr T[N] T[] struct:TAG:WORDS union:TAG:WORDS` or
// v32lua's `lua` (a NaN-boxed Lua value); FLAGS `boxed` (the word holds a pointer to the
// value: a Lua local captured by a closure) and `optimized-out`, joined by `+`.
//
// What this module gives the debugger:
//
//     locals [NAME]       the variables visible at the selected frame (see `frame`,
//                         `up`, `down`), each with its type and value
//     print NAME, x NAME, expect NAME ..., watch [NAME] ...   a local of the selected
//                         frame is found before a global of the same name (as in gdb);
//                         its address is fixed when the command is given
//
////////////////////////////////////////////////////////////////////////////////////////

#define  MAX_SHOWN_WORDS    16                   // elements of an array / words of a struct shown

typedef struct
{
    uint32_t   start;
    uint32_t   end;
    char      *name;
    char      *loc;
    char      *type;
    char      *flags;
    uint32_t   depth;
    char      *func;
} lvar_t;

static lvar_t   *vars        = NULL;
static size_t    nvars       = 0;
static size_t    room        = 0;
static char      vars_file[2048] = "";

static char  *dupstr (const char *s)
{
    char  *d  = malloc (strlen (s) + 1);

    if (d != NULL)
    {
        strcpy (d, s);
    }
    return (d);
}

static int  parse_u32 (const char *s, uint32_t *v)
{
    char           *end  = NULL;
    unsigned long   x    = 0;

    if ((s == NULL) || (*s == '\0'))
    {
        return (FALSE);
    }
    x  = strtoul (s, &end, 0);
    if (*end != '\0')
    {
        return (FALSE);
    }
    *v  = (uint32_t) x;
    return (TRUE);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// locals_load_file(): read one .vbin.vars file; the number of variables it gave
//
uint32_t  locals_load_file (const char *path)
{
    FILE      *fptr   = NULL;
    char       line[2048];
    char      *f[10];
    int        nf     = 0;
    char      *p      = NULL;
    uint32_t   tally  = 0;
    lvar_t     v;

    fptr  = fopen (path, "r");
    if (fptr == NULL)
    {
        return (0);
    }
    while (fgets (line, sizeof (line), fptr) != NULL)
    {
        line[strcspn (line, "\r\n")]  = '\0';
        if (strncmp (line, "var,", 4) != 0)       // scope lines are not needed: each var has its range
        {
            continue;
        }
        nf  = 0;
        for (p = line; (p != NULL) && (nf < 10); nf = nf + 1)
        {
            f[nf]  = p;
            p      = strchr (p, ',');
            if (p != NULL)
            {
                *p  = '\0';
                p   = p + 1;
            }
        }
        if (nf < 8)
        {
            continue;
        }
        memset (&v, 0, sizeof (v));
        if ((parse_u32 (f[1], &v.start) == FALSE) || (parse_u32 (f[2], &v.end) == FALSE) ||
            (parse_u32 (f[7], &v.depth) == FALSE) || (f[3][0] == '\0'))
        {
            continue;
        }
        v.name   = dupstr (f[3]);
        v.loc    = dupstr (f[4]);
        v.type   = dupstr (f[5]);
        v.flags  = dupstr (f[6]);
        v.func   = dupstr ((nf > 8) ? f[8] : "");
        if (nvars == room)
        {
            size_t   r  = (room == 0) ? 64 : (room * 2);
            lvar_t  *n  = realloc (vars, r * sizeof (lvar_t));

            if (n == NULL)
            {
                break;
            }
            vars  = n;
            room  = r;
        }
        vars[nvars]  = v;
        nvars        = nvars + 1;
        tally        = tally + 1;
    }
    fclose (fptr);
    return (tally);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// locals_load_cart(): NAME.vbin.vars next to the V32 file (or where the debug files are)
//
uint32_t  locals_load_cart (const char *datafile)
{
    FILE      *fptr   = NULL;
    uint32_t   tally  = 0;

    if (datafile == NULL)
    {
        return (0);
    }
    fptr  = open_search_file (datafile, ".vbin.vars", vars_file, sizeof (vars_file));
    if (fptr == NULL)
    {
        return (0);
    }
    fclose (fptr);
    tally  = locals_load_file (vars_file);
    fprintf (verbose, "LOADING LOCALS FROM: %s (%u)\n", vars_file, tally);
    return (tally);
}

uint8_t  locals_have (void)
{
    return (nvars > 0);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// the variables visible at an address: for two of the same name the innermost block wins
//
static int  visible (uint32_t pc, size_t i)
{
    size_t       j     = 0;
    const char  *func  = NULL;

    if ((pc < vars[i].start) || (pc >= vars[i].end))
    {
        return (FALSE);
    }
    // the range alone is not enough: v32as ends a function's variables at ';@fn' or ';@src -',
    // which a C program without --emit-hints does not have, so they would run on into the
    // functions after it. The function of the instruction (from the debug file) must match.
    func  = srcdebug_func_at (pc);
    if ((func != NULL) && (vars[i].func[0] != '\0') && (strcmp (func, vars[i].func) != 0))
    {
        return (FALSE);
    }
    for (j = 0; j < nvars; j = j + 1)
    {
        if ((j == i) || (pc < vars[j].start) || (pc >= vars[j].end) || (strcmp (vars[j].name, vars[i].name) != 0))
        {
            continue;
        }
        if ((vars[j].depth > vars[i].depth) || ((vars[j].depth == vars[i].depth) && (vars[j].start > vars[i].start)))
        {
            return (FALSE);                      // shadowed
        }
    }
    return (TRUE);
}

static int  has_flag (const lvar_t *v, const char *flag)
{
    const char  *p  = v -> flags;
    size_t       n  = strlen (flag);

    while ((p != NULL) && (*p != '\0'))
    {
        if ((strncmp (p, flag, n) == 0) && ((p[n] == '\0') || (p[n] == '+')))
        {
            return (TRUE);
        }
        p  = strchr (p, '+');
        p  = (p != NULL) ? (p + 1) : NULL;
    }
    return (FALSE);
}

static int  rd (uint32_t addr, uint32_t *value)
{
    word_t  *w  = NULL;

    if (memory_chk (addr, FLAG_READ, TRUE) == FALSE)
    {
        return (FALSE);
    }
    w  = memory_get (addr, TRUE);
    if (w == NULL)
    {
        return (FALSE);
    }
    *value  = (uint32_t) w -> i32;
    return (TRUE);
}

// the '[' that makes a type an array: the first one outside the {members} of a struct / union type
static const char  *top_bracket (const char *type)
{
    int  depth  = 0;

    for ( ; *type != '\0'; type = type + 1)
    {
        if (*type == '{')  depth = depth + 1;
        if (*type == '}')  depth = depth - 1;
        if ((*type == '[') && (depth == 0))  return (type);
    }
    return (NULL);
}

// how many words the type covers (1 for scalars and unknown types)
static uint32_t  type_words (const char *type)
{
    const char  *br   = top_bracket (type);
    const char  *col  = NULL;
    const char  *brace = strchr (type, '{');         // struct:TAG:WORDS{x:int@0;...}: the members are not part of the size
    uint32_t     n    = 0;
    uint32_t     el   = 1;

    if ((strncmp (type, "struct:", 7) == 0) || (strncmp (type, "union:", 6) == 0))
    {
        for (col = type + 6; (*col != '\0') && (col != brace); col = col + 1) ;
        for (col = col - 1; (col > type) && (*col != ':'); col = col - 1) ;
        if ((col != NULL) && (br == NULL))
        {
            return ((uint32_t) strtoul (col + 1, NULL, 10) ?: 1);
        }
    }
    if (br != NULL)
    {
        char  base[128];

        snprintf (base, sizeof (base), "%.*s", (int) (br - type), type);
        el  = type_words (base);
        n   = (uint32_t) strtoul (br + 1, NULL, 10);
        return ((n == 0) ? el : (n * el));
    }
    return (1);
}

typedef enum { WHERE_NONE = 0, WHERE_MEM, WHERE_REG, WHERE_GONE } where_t;

// where the variable's (first) word is, in frame (pc, bp); for a register, only frame 0 knows
static where_t  locate (const lvar_t *v, uint32_t bp, int frame, uint32_t *addr, int *rnum)
{
    const char  *l  = v -> loc;
    uint32_t     a  = 0;
    uint32_t     w  = 0;

    if ((l == NULL) || (strcmp (l, "-") == 0) || has_flag (v, "optimized-out"))
    {
        return (WHERE_GONE);
    }
    if ((strncmp (l, "BP+", 3) == 0) || (strncmp (l, "BP-", 3) == 0))
    {
        a  = (uint32_t) strtoul (l + 3, NULL, 0);
        *addr  = (l[2] == '+') ? (bp + a) : (bp - a);
        return (WHERE_MEM);
    }
    if (((l[0] == 'R') || (l[0] == 'r')) && isdigit ((unsigned char) l[1]))
    {
        *rnum  = atoi (l + 1);
        if ((frame != 0) || (*rnum < 0) || (*rnum > 15))
        {
            return (WHERE_NONE);
        }
        return (WHERE_REG);
    }
    if (l[0] == '@')
    {
        l  = l + 1;
    }
    if (isdigit ((unsigned char) l[0]))
    {
        *addr  = (uint32_t) strtoul (l, NULL, 0);
        return (WHERE_MEM);
    }
    if (sym_lookup (l, addr, &w) == TRUE)
    {
        return (WHERE_MEM);
    }
    return (WHERE_NONE);
}

static void  show_scalar (const char *type, uint32_t w)
{
    float  f  = 0.0f;

    if (strcmp (type, "uint") == 0)
    {
        fprintf (stdout, "%u", w);
    }
    else if (strcmp (type, "float") == 0)
    {
        memcpy (&f, &w, sizeof (f));
        fprintf (stdout, "%g", (double) f);
    }
    else if (strcmp (type, "bool") == 0)
    {
        fprintf (stdout, (w != 0) ? "true" : "false");
    }
    else if (strcmp (type, "ptr") == 0)
    {
        if (w == 0xFFFFFFFF)
        {
            fprintf (stdout, "NULL (-1)");
        }
        else
        {
            char  name[160];

            fprintf (stdout, "0x%.8X", w);
            if (sym_name_for (w, name, sizeof (name)) == TRUE)
            {
                fprintf (stdout, " <%s>", name);
            }
        }
    }
    else if (strcmp (type, "lua") == 0)
    {
        output_lua_value (w);
    }
    else if (strcmp (type, "int") == 0)
    {
        fprintf (stdout, "%d", (int32_t) w);
    }
    else
    {
        fprintf (stdout, "0x%.8X", w);           // a type this debugger does not know: the raw word
    }
}

// show_members(): a struct / union type written as struct:TAG:WORDS{name:type@offset;...} printed as
// {x = 3, y = 5}; a member that is itself an aggregate is shown as raw words. FALSE if the text is not that.
static int  show_members (const char *type, uint32_t addr)
{
    const char  *p      = strchr (type, '{');
    int          first  = TRUE;
    int          depth  = 0;
    const char  *seg    = NULL;
    const char  *q      = NULL;

    if (p == NULL)  return (FALSE);
    fprintf (stdout, "{");
    seg  = p + 1;
    for (q = seg; ; q = q + 1)
    {
        if (*q == '{')                   { depth = depth + 1;  continue; }
        if ((*q == '}') && (depth > 0))  { depth = depth - 1;  continue; }
        if (((*q == ';') || (*q == '}') || (*q == '\0')) && (depth == 0))
        {
            char         name[64];
            char         mtype[256];
            const char  *colon  = memchr (seg, ':', (size_t) (q - seg));
            const char  *at     = NULL;
            const char  *t      = NULL;
            uint32_t     off    = 0;
            uint32_t     w      = 0;
            uint32_t     mw     = 0;
            uint32_t     k      = 0;

            for (t = seg; t < q; t = t + 1)  if (*t == '@')  at = t;       // the last '@' is the member's own offset
            if ((colon != NULL) && (at != NULL) && (at > colon))
            {
                snprintf (name,  sizeof (name),  "%.*s", (int) (colon - seg), seg);
                snprintf (mtype, sizeof (mtype), "%.*s", (int) (at - colon - 1), colon + 1);
                off  = (uint32_t) strtoul (at + 1, NULL, 0);
                fprintf (stdout, "%s%s = ", first ? "" : ", ", name);
                first  = FALSE;
                mw     = type_words (mtype);
                if ((mw == 1) && (strchr (mtype, '[') == NULL) && (strncmp (mtype, "struct:", 7) != 0) && (strncmp (mtype, "union:", 6) != 0))
                {
                    if (rd (addr + off, &w) == FALSE)  fprintf (stdout, "<?>");
                    else                               show_scalar (mtype, w);
                }
                else
                {
                    fprintf (stdout, "{");
                    for (k = 0; (k < mw) && (k < 4); k = k + 1)
                    {
                        fprintf (stdout, "%s", (k > 0) ? ", " : "");
                        if (rd (addr + off + k, &w) == FALSE)  fprintf (stdout, "<?>");
                        else                                   fprintf (stdout, "0x%.8X", w);
                    }
                    fprintf (stdout, (mw > 4) ? ", ...}" : "}");
                }
            }
            seg = q + 1;
            if ((*q == '}') || (*q == '\0'))  break;
        }
    }
    fprintf (stdout, "}");
    return (TRUE);
}

static void  show_value (const lvar_t *v, uint32_t bp, int frame)
{
    uint32_t     addr   = 0;
    int          rn     = 0;
    uint32_t     w      = 0;
    uint32_t     n      = 0;
    uint32_t     i      = 0;
    const char  *type   = ((v -> type != NULL) && (strcmp (v -> type, "-") != 0)) ? v -> type : "-";
    char         base[128];
    const char  *br     = top_bracket (type);
    where_t      where  = locate (v, bp, frame, &addr, &rn);

    if (where == WHERE_GONE)
    {
        fprintf (stdout, "<optimized out>");
        return;
    }
    if (where == WHERE_NONE)
    {
        fprintf (stdout, "<in %s: not known in this frame>", v -> loc);
        return;
    }
    if (where == WHERE_REG)
    {
        w  = (uint32_t) REG(rn);
    }
    else if (rd (addr, &w) == FALSE)
    {
        fprintf (stdout, "<cannot read 0x%.8X>", addr);
        return;
    }
    if (has_flag (v, "boxed"))                   // the slot holds a pointer to the value's box
    {
        if (rd (w, &w) == FALSE)
        {
            fprintf (stdout, "<box at 0x%.8X cannot be read>", w);
            return;
        }
    }
    if ((br == NULL) && (strncmp (type, "struct:", 7) != 0) && (strncmp (type, "union:", 6) != 0))
    {
        show_scalar (type, w);
        return;
    }
    if (where == WHERE_REG)
    {
        fprintf (stdout, "<aggregate in a register?>");
        return;
    }
    if ((br == NULL) && (strchr (type, '{') != NULL) && show_members (type, addr))
    {
        return;                                  // a struct / union whose members the compiler told us
    }
    // an array (of scalars: element by element) or a struct / union (raw words)
    n  = type_words (type);
    snprintf (base, sizeof (base), "%.*s", (br != NULL) ? (int) (br - type) : (int) strlen (type), type);
    if ((br != NULL) && (br[1] == ']'))
    {
        n  = 1;                                  // T[]: the length is not known; show the first
    }
    fprintf (stdout, "{");
    for (i = 0; (i < n) && (i < MAX_SHOWN_WORDS); i = i + 1)
    {
        if (rd (addr + i, &w) == FALSE)
        {
            fprintf (stdout, "%s<?>", (i > 0) ? ", " : "");
            break;
        }
        fprintf (stdout, "%s", (i > 0) ? ", " : "");
        if ((br != NULL) && (type_words (base) == 1))
        {
            show_scalar (base, w);
        }
        else
        {
            fprintf (stdout, "0x%.8X", w);
        }
    }
    if (n > MAX_SHOWN_WORDS)
    {
        fprintf (stdout, ", ... (%u words)", n);
    }
    if ((br != NULL) && (br[1] == ']'))
    {
        fprintf (stdout, ", ...");
    }
    fprintf (stdout, "}");
}

////////////////////////////////////////////////////////////////////////////////////////
//
// locals_show(): the `locals [NAME]` command
//
void  locals_show (const char *only)
{
    uint32_t  pc     = 0;
    uint32_t  bp     = 0;
    int       frame  = 0;
    size_t    i      = 0;
    int       shown  = 0;

    if (nvars == 0)
    {
        fprintf (stderr, "[ERROR] no local variable information is loaded (NAME.vbin.vars: v32cc -g or v32lua -g, then v32as -g)\n");
        return;
    }
    srcdebug_selected (&pc, &bp, &frame);
    for (i = 0; i < nvars; i = i + 1)
    {
        if ((only != NULL) && (strcmp (vars[i].name, only) != 0))
        {
            continue;
        }
        if (visible (pc, i) == FALSE)
        {
            continue;
        }
        fprintf (stdout, "%s = ", vars[i].name);
        show_value (&vars[i], bp, frame);
        fprintf (stdout, "    (%s, %s%s%s)\n", vars[i].type, vars[i].loc,
                 (strcmp (vars[i].flags, "-") != 0) ? ", " : "",
                 (strcmp (vars[i].flags, "-") != 0) ? vars[i].flags : "");
        shown  = shown + 1;
    }
    if (shown == 0)
    {
        if (only != NULL)
        {
            fprintf (stderr, "[ERROR] no local '%s' is visible in frame #%d\n", only, frame);
        }
        else
        {
            fprintf (stdout, "No locals.\n");
        }
    }
}

////////////////////////////////////////////////////////////////////////////////////////
//
// locals_lookup(): a local of the selected frame that lives in memory -> its address and
// size in words. Used by the command parser before the globals (sym_lookup)
//
int  locals_lookup (const char *name, uint32_t *addr, uint32_t *words)
{
    uint32_t  pc     = 0;
    uint32_t  bp     = 0;
    int       frame  = 0;
    int       rn     = 0;
    size_t    i      = 0;

    if ((nvars == 0) || (name == NULL))
    {
        return (FALSE);
    }
    srcdebug_selected (&pc, &bp, &frame);
    for (i = 0; i < nvars; i = i + 1)
    {
        if ((strcmp (vars[i].name, name) != 0) || (visible (pc, i) == FALSE))
        {
            continue;
        }
        if (locate (&vars[i], bp, frame, addr, &rn) != WHERE_MEM)
        {
            return (FALSE);                      // a register or optimized out: `locals NAME` shows it
        }
        if (has_flag (&vars[i], "boxed") && (rd (*addr, addr) == FALSE))
        {
            return (FALSE);
        }
        if (words != NULL)
        {
            *words  = type_words (vars[i].type);
        }
        return (TRUE);
    }
    return (FALSE);
}
