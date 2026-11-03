/* pad_log.c -- records, every frame, how long gamepad 1 has held Left and A (negative: not held). The input-replay check
   runs it under v32peek (stock emulator) and under v32sim with the same script converted by tools/vprof2cmd.py. */
#include "input.h"
#include "video.h"
#include "time.h"
int test_errors = -1;
int[300] la;
int[300] ab;
int f;
void main(void)
{
    for(f = 0; f < 300; f++)
    {
        select_gamepad(1);
        la[f] = gamepad_left();
        ab[f] = gamepad_button_a();
        end_frame();
    }
    test_errors = 0;
    while(1) end_frame();
}
