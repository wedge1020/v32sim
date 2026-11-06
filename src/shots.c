#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// shots.c: --screenshot-at=FRAMES, --screenshot-file=PATTERN
//
//   FRAMES  =  ITEM[,ITEM...]     ITEM  =  N   |   FIRST-LAST   |   FIRST-LAST/STEP
//
// Cartridge frames, as --input counts them: frame 0 is the first frame the cartridge runs
// (the BIOS logo before it is not counted). The screenshot is the screen as that frame
// left it, written as 'screenshot' would write it, with the grid and the outlines from
// 'screengrid' and 'highlight' unless 'noextra' is on. PATTERN is a file name; a %d in it
// is the frame number (%05d works too). Without %d, ".png" gets "-FRAME" before it
// when more than one frame is asked for. Default: v32sim-shot-%05d.png.
//
////////////////////////////////////////////////////////////////////////////////////////

#define  MAX_ITEMS  64

typedef struct { int32_t from, to, step; } item_t;

static item_t     items[MAX_ITEMS];
static int        nitems    = 0;
static int        nframes   = 0;
static const char *pattern  = "v32sim-shot-%05d.png";
static int32_t    next_cf   = -1;                       // the cart frame that just ended, once the cart runs

static int  wanted (int32_t cf)
{
    int  i  = 0;

    for (i = 0; i < nitems; i = i + 1)
    {
        if ((cf >= items[i].from) && (cf <= items[i].to) && (((cf - items[i].from) % items[i].step) == 0))
        {
            return (TRUE);
        }
    }
    return (FALSE);
}

int  shots_set_frames (const char *spec)
{
    char  *copy  = strdup (spec);
    char  *p     = NULL;
    int    ok    = 1;

    nitems   = 0;
    nframes  = 0;
    for (p = strtok (copy, ","); (p != NULL) && (ok == 1); p = strtok (NULL, ","))
    {
        item_t  it  = { 0, 0, 1 };
        char   *e   = NULL;

        it.from  = (int32_t) strtol (p, &e, 10);
        it.to    = it.from;
        if ((e == p) || (it.from < 0) || (nitems >= MAX_ITEMS))  { ok = 0;  break; }
        if (*e == '-')
        {
            char  *q  = e + 1;
            it.to  = (int32_t) strtol (q, &e, 10);
            if ((e == q) || (it.to < it.from))  { ok = 0;  break; }
            if (*e == '/')
            {
                q  = e + 1;
                it.step  = (int32_t) strtol (q, &e, 10);
                if ((e == q) || (it.step < 1))  { ok = 0;  break; }
            }
        }
        if (*e != '\0')  { ok = 0;  break; }
        items[nitems]  = it;
        nitems         = nitems + 1;
        nframes        = nframes + ((it.to - it.from) / it.step) + 1;
    }
    free (copy);
    if (ok == 0)
    {
        nitems  = 0;
        return (-1);
    }
    return (0);
}

void  shots_set_file (const char *pat)
{
    pattern  = pat;
}

//
// once per frame, as it ends, before the GPU starts the next one
//
void  shots_frame (void)
{
    char  name[512];
    char  fmt[512];
    char *dot  = NULL;

    if (nitems == 0)
    {
        return;
    }
    if (next_cf < 0)
    {
        if ((((uint32_t) REG(IP)) >> 28) != V32_PAGE_CART)
        {
            return;                                          // still in the BIOS
        }
        next_cf  = 0;
    }
    else
    {
        next_cf  = next_cf + 1;
    }
    if (wanted (next_cf) == FALSE)
    {
        return;
    }
    if (strchr (pattern, '%') != NULL)
    {
        snprintf (fmt, sizeof (fmt), "%s", pattern);
    }
    else if ((nframes > 1) && ((dot = strrchr (pattern, '.')) != NULL))
    {
        snprintf (fmt, sizeof (fmt), "%.*s-%%d%s", (int) (dot - pattern), pattern, dot);
    }
    else if (nframes > 1)
    {
        snprintf (fmt, sizeof (fmt), "%s-%%d", pattern);
    }
    else
    {
        snprintf (fmt, sizeof (fmt), "%s", pattern);
    }
    snprintf (name, sizeof (name), fmt, (int) next_cf);       // fmt is checked by the user: one %d at most
    if (gpu_screenshot (name) == TRUE)
    {
        status_printf ("[screenshot] frame %d: wrote %s\n", (int) next_cf, name);
    }
    else
    {
        fprintf (stderr, "[ERROR] screenshot: could not write '%s'\n", name);
    }
}
