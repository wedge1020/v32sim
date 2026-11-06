// counts: a fixed number of each kind of command every frame, to check the counters of perfstats.c against known values
//   per frame: 2 define_region (12 region-definition writes), 2 set_multiply_color, 3 set_blending_mode,
//   3 zoomed + 2 rotated + 1 rotozoomed draws (4 scaled, 3 rotated), 4 gamepad reads, 1 channel assignment,
//   3 channel parameter writes (2 volume, 1 speed), 1 sound loop write is NOT a channel parameter
#include "video.h"
#include "audio.h"
#include "input.h"
#include "time.h"
int main(void)
{
    select_texture(-1);
    select_region(0);
    while (1) {
        define_region(0, 0, 63, 63, 0, 0);
        define_region(0, 0, 31, 31, 0, 0);
        set_multiply_color(make_color_rgb(255, 255, 255));
        set_multiply_color(make_color_rgb(255, 0, 0));
        set_blending_mode(0);
        set_blending_mode(1);
        set_blending_mode(0);
        for (int i = 0; i < 3; i++) draw_region_zoomed_at(10, 10);
        for (int i = 0; i < 2; i++) draw_region_rotated_at(10, 10);
        draw_region_rotozoomed_at(10, 10);
        select_gamepad(0);
        int a = gamepad_left() + gamepad_right() + gamepad_up() + gamepad_down();
        select_channel(0);
        assign_channel_sound(0, -1);
        set_channel_volume(0.5);
        set_channel_volume(0.25);
        set_channel_speed(1.0);
        end_frame();
    }
    return 0;
}
