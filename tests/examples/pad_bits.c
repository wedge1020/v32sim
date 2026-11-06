/* pad_bits.c -- records, from the first frame on that shows any control of gamepad 1 pressed, the 11 controls of every
   frame as a number (Left = bit 10 ... R = bit 0). Used to check the v32mouse device against the v32io test vectors. */
#include "input.h"
#include "video.h"
#include "time.h"
int test_errors = -1;
int[64] st;
int f;
int b(int v) { if (v > 0) return 1; return 0; }
int bits(void)
{
    select_gamepad(1);
    return b(gamepad_left()) * 1024 + b(gamepad_right()) * 512 + b(gamepad_up()) * 256 + b(gamepad_down()) * 128
         + b(gamepad_button_start()) * 64 + b(gamepad_button_a()) * 32 + b(gamepad_button_b()) * 16 + b(gamepad_button_x()) * 8
         + b(gamepad_button_y()) * 4 + b(gamepad_button_l()) * 2 + b(gamepad_button_r());
}
void main(void)
{
    while(bits() == 0) end_frame();          /* frame 0 is the first frame that shows anything */
    for(f = 0; f < 64; f++)
    {
        st[f] = bits();
        end_frame();
    }
    test_errors = 0;
    while(1) end_frame();
}
