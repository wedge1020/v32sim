// --turbo test cart (tests/run_turbo_tests.sh), stock compiler dialect: never clears, 12 full-screen draws a frame (9 are accepted), so the pending pixel queue fills up.
#include "video.h"
#include "time.h"
void main(void)
{
    int i;
    select_texture(-1);
    select_region(0);
    define_region(0, 0, 639, 359, 0, 0);
    while (1) {
        for (i = 0; i < 12; i++) draw_region_at(0, 0);
        end_frame();
    }
}
