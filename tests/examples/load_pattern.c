/* load_pattern.c -- a cartridge whose CPU load changes every frame (2000 + 1500 * (frame % 50) loop iterations),
   for the frame-statistics check: v32sim --frame-stats must agree with the stock-emulator v32prof row by row. */
#include "video.h"
#include "time.h"
#include "input.h"
int test_errors = -1;
int f;
void main(void)
{
    int i, n;
    for(f = 0; f < 300; f++)
    {
        n = 2000 + (f % 50) * 1500;
        for(i = 0; i < n; i++) test_errors = -1;
        clear_screen(color_black);
        end_frame();
    }
    test_errors = 0;
}
