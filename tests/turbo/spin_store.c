// --turbo test cart (tests/run_turbo_tests.sh), stock compiler dialect: the spin loop stores (polls++) every trip, so it is NOT skippable.
#include "video.h"
#include "time.h"
#include "input.h"
int frames;
int polls;
int work;
int pad_seen;
void main(void)
{
    int f; int i;
    while (1) {
        f = get_frame_counter();
        // some real work this frame
        for (i = 0; i < 200; i++) work += i ^ frames;
        // spin until the next frame (no WAIT): polls TIM_FrameCounter
        while (get_frame_counter() == f) { polls++; }
        frames++;
    }
}
