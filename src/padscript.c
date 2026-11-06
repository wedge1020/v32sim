#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// padscript.c: --input=SCRIPT, a gamepad held by script, in cartridge frames
//
//   SCRIPT  =  RUN[,RUN...]          RUN  =  BUTTON[+BUTTON...]@FIRST-LAST   (or @FRAME)
//   BUTTON  =  LEFT RIGHT UP DOWN START A B X Y L R   (SELECT is X: the recorders' name)
//   @FILE      the script on the `V32_INPUT=` line of FILE (a V32TRACE .trace.txt), else its first line
//
// This is the V32_INPUT syntax of the V32TRACE recorders (spec 2.1 `input=`, the human
// report's [input] section) and of GBtoV32's and NEStoV32's profilers: frame 0 is the first
// frame the cartridge runs (the BIOS logo before it is not counted), and a button is held
// from the start of FIRST to the end of LAST. So the `input=` of a trap, given here, walks
// the cartridge back to the hit:
//
//     v32sim --batch -r --fast -x --input="$(sed -n 's/.* input=\([^ ]*\).*/\1/p' run.traps.txt | head -1)" ...
//
// --input-port=N picks the gamepad (default 0, the first: what the recorders record). The
// gamepad is connected. V32_INPUT in the environment is used when --input is not given.
// The changes are made between frames, as the `gamepad` command makes them, so the program
// sees a press in the frame it is scripted for with a count of 1.
//
////////////////////////////////////////////////////////////////////////////////////////

int8_t    *pad_input       = NULL;      // --input=SCRIPT
uint8_t    pad_input_port  = 0;         // --input-port=N

typedef struct { int32_t from, to; uint16_t mask; } prun_t;

typedef struct
{
    prun_t   *runs;
    int       nruns;
    int32_t   next_cf;                  // the cart frame that runs next (-1: the BIOS is still running)
    uint8_t   active;
    uint8_t   port;
} pstate_t;

int8_t    *pad_input2      = NULL;      // --input2=SCRIPT: the same for gamepad 2 (port 1)

static pstate_t  st[2];

static const char *const names[11] = { "LEFT", "RIGHT", "UP", "DOWN", "START", "A", "B", "X", "Y", "L", "R" };

static int  button_of (const char *s, size_t n)
{
    int  c  = 0;

    if ((n == 6) && (0 == strncasecmp (s, "SELECT", 6)))
    {
        return (7);
    }
    for (c = 0; c < 11; c = c + 1)
    {
        if ((strlen (names[c]) == n) && (0 == strncasecmp (s, names[c], n)))
        {
            return (c);
        }
    }
    return (-1);
}

static int  parse (pstate_t *s, const char *text)
{
    const char  *p     = text;
    int          room  = 0;

    if ((text == NULL) || (0 == strcmp (text, "-")) || (*text == '\0'))
    {
        return (TRUE);                          // no button held at all
    }
    while (*p != '\0')
    {
        prun_t       r    = { 0, 0, 0 };
        const char  *at   = strchr (p, '@');
        const char  *q    = p;
        char        *end  = NULL;

        if (at == NULL)
        {
            return (FALSE);
        }
        while (q < at)                          // BUTTON[+BUTTON...]
        {
            const char  *plus  = memchr (q, '+', (size_t) (at - q));
            size_t       n     = (size_t) (((plus != NULL) ? plus : at) - q);
            int          b     = button_of (q, n);

            if (b < 0)
            {
                fprintf (stderr, "[ERROR] --input: unknown button '%.*s' (LEFT RIGHT UP DOWN START A B X Y L R, SELECT = X)\n", (int) n, q);
                return (FALSE);
            }
            r.mask  = r.mask | (uint16_t) (1 << b);
            q       = (plus != NULL) ? (plus + 1) : at;
        }
        r.from  = (int32_t) strtol (at + 1, &end, 10);
        r.to    = r.from;
        if (end == at + 1)
        {
            return (FALSE);
        }
        if (*end == '-')
        {
            const char  *s  = end + 1;

            r.to  = (int32_t) strtol (s, &end, 10);
            if (end == s)
            {
                return (FALSE);
            }
        }
        if ((*end != ',') && (*end != '\0'))
        {
            return (FALSE);
        }
        if (s->nruns == room)
        {
            prun_t  *n  = realloc (s->runs, (size_t) (room = (room == 0) ? 32 : room * 2) * sizeof (prun_t));

            if (n == NULL)
            {
                return (FALSE);
            }
            s->runs  = n;
        }
        s->runs[s->nruns]  = r;
        s->nruns        = s->nruns + 1;
        p            = (*end == ',') ? (end + 1) : end;
    }
    return (TRUE);
}

static void  apply (pstate_t *s, int32_t cf)
{
    uint16_t  mask  = 0;
    int       i     = 0;

    for (i = 0; i < s->nruns; i = i + 1)
    {
        if ((cf >= s->runs[i].from) && (cf <= s->runs[i].to))
        {
            mask  = mask | s->runs[i].mask;
        }
    }
    for (i = 0; i < 11; i = i + 1)
    {
        if (((mask >> i) & 1) != (gamepad_pressed (s->port, (uint8_t) i) ? 1 : 0))
        {
            gamepad_control (s->port, (uint8_t) i, (mask >> i) & 1);
        }
    }
    gamepad_publish (s->port);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// padscript_init(): after the machine is set up: parse, connect the gamepad, frame 0's state
//
static void  init_one (pstate_t *s, const char *text, uint8_t port, const char *option, const char *key)
{
    s -> port  = port;
    if (text == NULL)
    {
        return;
    }
    if (text[0] == '@')                          // @FILE: the KEY= line of a file (a V32TRACE .trace.txt), or its first line
    {
        static char  line[65536];
        FILE        *f      = fopen (text + 1, "r");
        int          found  = FALSE;
        size_t       klen   = strlen (key);

        if (f == NULL)
        {
            fprintf (stderr, "[ERROR] %s: cannot read '%s'\n", option, text + 1);
            exit (1);
        }
        while (fgets (line, sizeof (line), f) != NULL)
        {
            line[strcspn (line, "\r\n")] = '\0';
            if ((0 == strncmp (line, key, klen)) && (line[klen] == '='))
            {
                memmove (line, line + klen + 1, strlen (line + klen + 1) + 1);
                found = TRUE;
                break;
            }
        }
        if (found == FALSE)
        {
            rewind (f);
            if ((port == 0) && (fgets (line, sizeof (line), f) != NULL))  line[strcspn (line, "\r\n")] = '\0';
            else                                                            line[0] = '\0';      // gamepad 2: no such line, no buttons
        }
        fclose (f);
        text = line;
    }
    if (port >= V32_NUM_GAMEPADS)
    {
        fprintf (stderr, "[ERROR] --input-port: 0 to %d\n", V32_NUM_GAMEPADS - 1);
        exit (1);
    }
    if (parse (s, text) == FALSE)
    {
        fprintf (stderr, "[ERROR] %s: cannot read '%s' (BUTTON[+BUTTON]@FIRST-LAST,...)\n", option, text);
        exit (1);
    }
    if ((gamepad + port) -> connected != TRUE)
    {
        gamepad_reset (port);
        (gamepad + port) -> connected  = TRUE;
    }
    s -> active   = TRUE;
    s -> next_cf  = -1;
    apply (s, 0);                                // what the cartridge's first frame sees
}

void  padscript_init (void)
{
    const char  *text  = (const char *) pad_input;

    if ((text == NULL) && (getenv ("V32_INPUT") != NULL))
    {
        text  = getenv ("V32_INPUT");
        fprintf (stderr, "[input] using V32_INPUT from the environment\n");
    }
    init_one (st, text, pad_input_port, "--input", "V32_INPUT");
    text  = (const char *) pad_input2;
    if ((text == NULL) && (getenv ("V32_INPUT2") != NULL))
    {
        text  = getenv ("V32_INPUT2");
        fprintf (stderr, "[input] using V32_INPUT2 from the environment\n");
    }
    if ((text != NULL) && (pad_input_port == 1))
    {
        fprintf (stderr, "[ERROR] --input2 drives gamepad 2 (port 1): --input-port must not be 1 as well\n");
        exit (1);
    }
    init_one (st + 1, text, 1, "--input2", "V32_INPUT2");
}

////////////////////////////////////////////////////////////////////////////////////////
//
// padscript_frame(): at the end of every frame, set up the next one
//
static void  frame_one (pstate_t *s)
{
    if (s -> active == FALSE)
    {
        return;
    }
    if (s -> next_cf < 0)
    {
        if ((((uint32_t) REG(IP)) >> 28) != V32_PAGE_CART)
        {
            return;                              // still in the BIOS: frame 0 has not run
        }
        s -> next_cf  = 0;                       // the frame that ended was the cartridge's frame 0
    }
    s -> next_cf  = s -> next_cf + 1;
    apply (s, s -> next_cf);
}

void  padscript_frame (void)
{
    frame_one (st);
    frame_one (st + 1);
}
