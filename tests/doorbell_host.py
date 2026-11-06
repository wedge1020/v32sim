#!/usr/bin/env python3
"""A minimal v32dev host for the doorbell test: it answers every change of channel 0's c2h_head by moving the host part's
h2c_head by one, and keeps its heartbeat moving. usage: doorbell_host.py CARD SECONDS [DELAY_SECONDS]"""
import os, struct, sys, time
card, seconds = sys.argv[1], float(sys.argv[2])
delay = float(sys.argv[3]) if len(sys.argv) > 3 else 0.0
START = 262144 - 16384
HOST0 = START + 64
CART0 = HOST0 + 4 * 2040
def off(w): return 8 + 4 * w
fd = os.open(card, os.O_RDWR)
def rd(w): return struct.unpack("<I", os.pread(fd, 4, off(w)))[0]
def wr(w, v): os.pwrite(fd, struct.pack("<I", v & 0xFFFFFFFF), off(w))
wr(HOST0 + 1, 1)                                   # flags: host present
last = rd(CART0 + 1)
beat = 0
end = time.time() + seconds
while time.time() < end:
    beat += 1
    wr(HOST0 + 2, beat)
    c = rd(CART0 + 1)
    if c != last:
        last = c
        if delay: time.sleep(delay)
        wr(HOST0 + 3, rd(HOST0 + 3) + 1)           # a message for the cart
    time.sleep(0.0002)
