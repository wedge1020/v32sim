// --turbo test cart (tests/run_turbo_tests.sh), stock compiler dialect: polls TIM_FrameCounter and a gamepad button with no stores that
// change anything, so --turbo may fast-forward the spin; the results must equal a normal run.
#include "video.h"
#include "time.h"
#include "input.h"
int frames;
int work;
int never;
void main(void)
{
    int f; int i;
    select_gamepad(0);
    while (1) {
        f = get_frame_counter();
        for (i = 0; i < 50; i++) work += i;
        // register-only spin: reads the frame counter and a button, no stores
        while (get_frame_counter() == f) { if (gamepad_left() > 0) never++; }
        frames++;
    }
}
