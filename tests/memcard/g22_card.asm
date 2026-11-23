; G22: the memory card as the cartridge sees it: the connected port, the first words of
; the v32dev mailbox region (header, emulator flag and heartbeat words, host area) over
; four frames, then program writes to the plain area, the header, the host area and the
; cart area, read back. tests/run_memcard.sh runs it on cards of several kinds and also
; compares the card file each emulator leaves behind.
  mov R6, 0x40
  in R0, MEM_Connected
  mov [R6], R0
  iadd R6, 1
  mov R9, 4
__frame:
  mov R3, 0x3003C000
  mov R4, 80
__read:
  mov R0, [R3]
  mov [R6], R0
  iadd R3, 1
  iadd R6, 1
  isub R4, 1
  jt R4, __read
  mov R0, 0x1234
  mov [0x30000010], R0
  mov R0, 0x77
  mov [0x3003C00A], R0
  mov R0, 0x55
  mov [0x3003C040], R0
  mov R0, 0x66
  mov [0x3003DFE0], R0
  mov R0, 0x88
  mov [0x3003DFF4], R0
  mov R0, [0x30000010]
  mov [R6], R0
  mov R0, [0x3003C00A]
  mov [R6+1], R0
  mov R0, [0x3003C040]
  mov [R6+2], R0
  mov R0, [0x3003DFE0]
  mov [R6+3], R0
  mov R0, [0x3003DFF4]
  mov [R6+4], R0
  iadd R6, 5
  wait
  isub R9, 1
  jt R9, __frame
  hlt
