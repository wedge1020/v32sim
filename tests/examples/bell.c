// bell: the --v32dev-doorbell test cartridge. Rings channel 0 (a store to the cart part's c2h_head), then spins until the
// header's reply counter (word 10) moves. Results, in RAM words from 0x00200000:
//   [0] header word 8 (emulator flags)   [1] spin count (100000 = gave up)   [2] counter change   [3] 1 = done
#include "time.h"
int main(void)
{
    volatile int *hdr  = (volatile int *) 0x3003C000;
    volatile int *c2h  = (volatile int *) 0x3003E021;
    volatile int *r    = (volatile int *) 0x00200000;
    int before, n = 0;
    for (int i = 0; i < 5; i++) end_frame();         // let the host's heartbeat show
    r[0]    = hdr[8];
    before  = hdr[10];
    *c2h    = *c2h + 1;
    while (hdr[10] == before && n < 100000) n++;
    r[1]    = n;
    r[2]    = hdr[10] - before;
    r[3]    = 1;
    while (1) end_frame();
    return 0;
}
