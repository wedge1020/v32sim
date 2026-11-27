; V32TRACE inputclock test cartridge (tests/run_v32trace_clock.sh): RAM[0x100] is a guest clock that stands still
; one frame in three (the console's frames are not the guest's) and restarts from 0 at frame 40 (a guest reset);
; at frame 60 the cartridge enters its trap routine _trap, where it waits for ever.
_start:
  mov R6, 0x100
  mov R1, 0
  mov R3, 0
__frame:
  iadd R1, 1
  mov R4, R1
  imod R4, 3
  jf R4, __hold
  iadd R3, 1
__hold:
  mov R5, R1
  isub R5, 40
  jt R5, __store
  mov R3, 0
__store:
  mov [R6], R3
  mov R5, R1
  isub R5, 60
  jf R5, _trap
  wait
  jmp __frame
_trap:
  wait
  jmp _trap
