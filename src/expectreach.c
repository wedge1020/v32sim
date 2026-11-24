#include "defines.h"

////////////////////////////////////////////////////////////////////////////////////////
//
// expectreach.c: --expect-reach LABEL[,LABEL...]
//
// The run must reach one of the labels (the code labels of the debug files, as `break` and -b use them):
//
//     EXPECT REACHED <label> frame=N ip=0x%08X     stdout; the run stops, exit status 0
//     EXPECT MISSED <label>[,<label>...]           stdout; the run ended any other way (frame limit, HLT, a CPU
//                                                  error, quit...), exit status 1
//
// An unknown label is a start-up error (exit status 1, as any bad option). The check looks at the address of
// the instruction about to run, before it runs, like a breakpoint, so it is the same for single-stepping,
// run N and the headless frame limits. It only reads the IP: the machine is untouched.
//
////////////////////////////////////////////////////////////////////////////////////////

const char *expect_reach_arg   = NULL;       // --expect-reach=LIST

#define  MAX_REACH  64
static char      *reach_name[MAX_REACH];
static uint32_t   reach_addr[MAX_REACH];
static int        reach_n         = 0;
static int        reach_done      = FALSE;
uint8_t           expect_missed   = FALSE;   // read by main() for the exit status

void  expect_reach_init (void)
{
    char  *list  = NULL;
    char  *tok   = NULL;
    char  *save  = NULL;

    if (expect_reach_arg == NULL)  return;
    list  = strdup (expect_reach_arg);
    for (tok = strtok_r (list, ",", &save); tok != NULL; tok = strtok_r (NULL, ",", &save))
    {
        uint32_t  addr  = 0;

        while ((*tok == ' ') || (*tok == '\t'))  tok = tok + 1;
        if (*tok == '\0')  continue;
        if (reach_n >= MAX_REACH)
        {
            fprintf (stderr, "[ERROR] --expect-reach: at most %d labels\n", MAX_REACH);
            exit (1);
        }
        if (sym_lookup_code (tok, &addr) == FALSE)
        {
            fprintf (stderr, "[ERROR] --expect-reach: '%s' is not a known label (is there a debug file, and no -n?)\n", tok);
            exit (1);
        }
        reach_name[reach_n]  = strdup (tok);
        reach_addr[reach_n]  = addr;
        reach_n              = reach_n + 1;
    }
    free (list);
    if (reach_n == 0)
    {
        fprintf (stderr, "[ERROR] --expect-reach: no label given\n");
        exit (1);
    }
}

//
// expect_reach_check(): TRUE (once) when the instruction about to run is at one of the labels
//
int  expect_reach_check (uint32_t ip)
{
    int  k  = 0;

    for (k = 0; k < reach_n; k = k + 1)
    {
        if (reach_addr[k] == ip)
        {
            reach_done  = TRUE;
            fprintf (stdout, "EXPECT REACHED %s frame=%u ip=0x%08X\n", reach_name[k], (unsigned) (frames_run + 1), ip);
            fflush (stdout);
            return (TRUE);
        }
    }
    return (FALSE);
}

void  expect_reach_end (void)
{
    int  k  = 0;

    if ((reach_n == 0) || (reach_done == TRUE))  return;
    fprintf (stdout, "EXPECT MISSED ");
    for (k = 0; k < reach_n; k = k + 1)
    {
        fprintf (stdout, "%s%s", (k == 0) ? "" : ",", reach_name[k]);
    }
    fprintf (stdout, "\n");
    expect_missed  = TRUE;
}
