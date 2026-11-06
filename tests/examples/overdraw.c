// overdraw: 12 full-screen draws every frame; the GPU draws 9 screens a frame, so 3 are dropped each frame
// (tests/run_harness_tests.sh: v32orun counts them, in bare mode from its model and in console mode from the real GPU)
#include "video.h"
#include "time.h"
int main(void)
{
    select_texture(-1);
    select_region(0);
    define_region(0, 0, 639, 359, 0, 0);
    while (1) {
        for (int i = 0; i < 12; i++) draw_region_at(0, 0);
        end_frame();
    }
    return 0;
}
