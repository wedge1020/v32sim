#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// padscript.c: --input=SCRIPT, a gamepad held by script, in cartridge frames
//
//   SCRIPT  =  RUN[,RUN...]          RUN  =  BUTTON[+BUTTON...]@FIRST-LAST   (or @FRAME)
//   BUTTON  =  LEFT RIGHT UP DOWN START A B X Y L R   (SELECT is X: the recorders' name)
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

static prun_t   *runs      = NULL;
static int       nruns     = 0;
static int32_t   next_cf   = -1;        // the cart frame that runs next (-1: the BIOS is still running)
static uint8_t   active    = FALSE;

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

static int  parse (const char *text)
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
        if (nruns == room)
        {
            prun_t  *n  = realloc (runs, (size_t) (room = (room == 0) ? 32 : room * 2) * sizeof (prun_t));

            if (n == NULL)
            {
                return (FALSE);
            }
            runs  = n;
        }
        runs[nruns]  = r;
        nruns        = nruns + 1;
        p            = (*end == ',') ? (end + 1) : end;
    }
    return (TRUE);
}

static void  apply (int32_t cf)
{
    uint16_t  mask  = 0;
    int       i     = 0;

    for (i = 0; i < nruns; i = i + 1)
    {
        if ((cf >= runs[i].from) && (cf <= runs[i].to))
        {
            mask  = mask | runs[i].mask;
        }
    }
    for (i = 0; i < 11; i = i + 1)
    {
        if (((mask >> i) & 1) != (gamepad_pressed (pad_input_port, (uint8_t) i) ? 1 : 0))
        {
            gamepad_control (pad_input_port, (uint8_t) i, (mask >> i) & 1);
        }
    }
    gamepad_publish (pad_input_port);
}

////////////////////////////////////////////////////////////////////////////////////////
//
// padscript_init(): after the machine is set up: parse, connect the gamepad, frame 0's state
//
void  padscript_init (void)
{
    const char  *text  = (const char *) pad_input;

    if ((text == NULL) && (getenv ("V32_INPUT") != NULL))
    {
        text  = getenv ("V32_INPUT");
        fprintf (stderr, "[input] using V32_INPUT from the environment\n");
    }
    if (text == NULL)
    {
        return;
    }
    if (pad_input_port >= V32_NUM_GAMEPADS)
    {
        fprintf (stderr, "[ERROR] --input-port: 0 to %d\n", V32_NUM_GAMEPADS - 1);
        exit (1);
    }
    if (parse (text) == FALSE)
    {
        fprintf (stderr, "[ERROR] --input: cannot read '%s' (BUTTON[+BUTTON]@FIRST-LAST,...)\n", text);
        exit (1);
    }
    if ((gamepad + pad_input_port) -> connected != TRUE)
    {
        gamepad_reset (pad_input_port);
        (gamepad + pad_input_port) -> connected  = TRUE;
    }
    active   = TRUE;
    next_cf  = -1;
    apply (0);                                   // what the cartridge's first frame sees
}

////////////////////////////////////////////////////////////////////////////////////////
//
// padscript_frame(): at the end of every frame, set up the next one
//
void  padscript_frame (void)
{
    if (active == FALSE)
    {
        return;
    }
    if (next_cf < 0)
    {
        if ((((uint32_t) REG(IP)) >> 28) != V32_PAGE_CART)
        {
            return;                              // still in the BIOS: frame 0 has not run
        }
        next_cf  = 0;                            // the frame that ended was the cartridge's frame 0
    }
    next_cf  = next_cf + 1;
    apply (next_cf);
}
