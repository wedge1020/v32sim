/* hello_text.c -- the smallest cartridge for the harness examples: text on the BIOS-font screen, a self-check, HLT.
   tests/run_harness_tests.sh builds it with tools/v32mkcart.sh and runs it through v32sim-selfcheck, v32sim-golden,
   v32sim-peek and v32sim-addrprof. Built with -DKEEP_RUNNING it never halts, so a session can look at the screen. */
#include "video.h"
#include "time.h"
#include "string.h"

int test_errors = -1;
int counter;
int[32] message;

int add_up(int n)
{
    int i, sum;
    sum = 0;
    for (i = 1; i <= n; i++) sum += i;
    return sum;
}

void main(void)
{
    int f;
    strcpy(message, "hello, v32sim");
    for (f = 0; f < 10; f++)
    {
        clear_screen(color_black);
        select_texture(-1);
        print_at(10, 20, message);
        counter = add_up(100);
        end_frame();
    }
    if (counter == 5050) test_errors = 0; else test_errors = 1;
#ifdef KEEP_RUNNING
    /* a cartridge that halts ends the simulator run: to look at its screen from a session, keep it alive */
    while (1) { print_at(10, 20, message); end_frame(); }
#endif
}
