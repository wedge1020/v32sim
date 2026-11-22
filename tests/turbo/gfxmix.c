// --turbo / lazy GPU test cart (tests/run_turbo_tests.sh), stock compiler dialect: every kind of draw (zoomed, rotated, rotozoomed, three blending modes, multiply colours).
#include "video.h"
#include "time.h"
#include "math.h"
#include "string.h"
int f;
void main(void)
{
    int i;
    select_texture(-1);
    select_region(65);
    define_region(0, 0, 9, 19, 5, 10);
    while (1) {
        clear_screen(make_color_rgba(20, 40 + (f & 31), 60, 255));
        set_blending_mode(0);
        for (i = 0; i < 40; i++) {
            set_multiply_color(make_color_rgba((i*37+f)&255, (i*91)&255, (i*53)&255, 255 - (i*5)));
            set_drawing_point(20 + i*15, 30 + (i*7 + f)%250);
            draw_region();
        }
        set_multiply_color(make_color_rgba(255,255,255,128));
        for (i = 0; i < 10; i++) {
            set_drawing_point(100 + i*40, 100);
            set_drawing_scale(1.0 + i*0.37, 0.5 + i*0.9);
            draw_region_zoomed();
        }
        set_multiply_color(make_color_rgba(255,200,100,255));
        for (i = 0; i < 10; i++) {
            set_drawing_point(60 + i*50, 220);
            set_drawing_angle(0.1 * (i + f));
            draw_region_rotated();
        }
        set_blending_mode(1);
        set_multiply_color(make_color_rgba(60,80,40,255));
        for (i = 0; i < 6; i++) {
            set_drawing_point(300 + i*30, 180);
            set_drawing_scale(3.0 - i*0.7, -2.0 + i*0.5);
            set_drawing_angle(0.3 * i - 0.01*f);
            draw_region_rotozoomed();
        }
        set_blending_mode(2);
        set_multiply_color(make_color_rgba(90,30,30,255));
        for (i = 0; i < 6; i++) {
            set_drawing_point(200 + i*60, 300);
            set_drawing_scale(2.0, 2.0);
            draw_region_zoomed();
        }
        set_blending_mode(0);
        set_multiply_color(make_color_rgba(255,255,255,255));
        f++;
        end_frame();
    }
}
