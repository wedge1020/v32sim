; ****************************************************************************
; *  v32sim StubBios -- a Vircon32 BIOS for headless testing and debugging
; *
; *  Carries the StandardBios texture and sound, but no logo animation and no
; *  boot sound: it defines the BIOS texture regions exactly as StandardBios
; *  leaves them, puts the machine in StandardBios's hand-over state, waits one
; *  frame and jumps to the cartridge (StandardBios takes ~251 frames, this
; *  takes 1). Its error handler leaves a machine-readable record at the top of
; *  RAM and draws the error as BIOS-font text on the 10x20 grid, so v32sim's
; *  `console` / `until` / `expect console` can read it.
; *
; *  Build: bios/src/build.sh (stock `assemble -b` + `packrom`, or v32as -b).
; ****************************************************************************
;
; Memory map of the BIOS program ROM:
;   word 0   hardware error entry (the CPU jumps here with R0-R3 set, SP/BP reset)
;   word 4   power-on entry
;
; ---------------------------------------------------------------------------
; The error record (top 32 words of RAM; the handler writes nothing else,
; except the one return-address word below it at 0x3FFFDF):
;
;   0x3FFFE0  magic 0x45323356 ("V32E")
;   0x3FFFE1  error code (R0)                    0 invalid memory read ... 10 power
;   0x3FFFE2  fault address: the faulting instruction's first word
;   0x3FFFE3  next IP as the CPU reported it (R1)
;   0x3FFFE4  instruction word (R2)
;   0x3FFFE5  immediate value (R3)
;   0x3FFFE6  R4 .. 0x3FFFEF R13 at the fault (the CPU only clobbers R0-R3, BP, SP)
;   0x3FFFF0  TIM_FrameCounter at handler entry
;   0x3FFFF1  TIM_CycleCounter at handler entry
;   0x3FFFF2  number of hardware errors seen (1; 2 after a double fault)
;   0x3FFFF3  double fault: R0 (code) .. 0x3FFFF6 R3 of the second error
;   0x3FFFFF  handler state: 0x56330001 busy, 0x56330002 done, 0x56330003 double fault
; ---------------------------------------------------------------------------

%define REC_BASE      0x3FFFE0
%define REC_MAGIC     0x3FFFE0
%define REC_CODE      0x3FFFE1
%define REC_FAULT     0x3FFFE2
%define REC_NEXTIP    0x3FFFE3
%define REC_INSTR     0x3FFFE4
%define REC_IMM       0x3FFFE5
%define REC_R4        0x3FFFE6
%define REC_FRAME     0x3FFFF0
%define REC_CYCLE     0x3FFFF1
%define REC_COUNT     0x3FFFF2
%define REC_DF_R0     0x3FFFF3
%define REC_STATE     0x3FFFFF

%define MAGIC         0x45323356
%define STATE_BUSY    0x56330001
%define STATE_DONE    0x56330002
%define STATE_DOUBLE  0x56330003

%define COLOR_WHITE   0xFFFFFFFF
%define COLOR_GRAY    0xFFC0C0C0
%define COLOR_LABEL   0xFF8080FF      ; StandardBios's label color (light red)
%define COLOR_BG_ERR  0xFF501010      ; dark blue (AABBGGRR)
%define COLOR_BLACK   0xFF000000

  ; ---- word 0: hardware error entry
  jmp __error_handler          ; words 0-1
  hlt                          ; word 2
  hlt                          ; word 3

  ; ---- word 4: power-on entry
__boot:
  ; BIOS texture regions, as StandardBios leaves them
  out GPU_SelectedTexture, -1
  call __define_regions

  ; audio: StandardBios played sound -1 on channel 0; it has ended long before
  ; the hand-over, so all that remains is the assignment
  out SPU_SelectedChannel, 0
  out SPU_ChannelAssignedSound, -1

  ; screen: StandardBios's last frame is a black clear
  out GPU_ClearColor, COLOR_BLACK
  out GPU_Command, GPUCommand_ClearScreen

  in R0, CAR_Connected
  jf R0, __no_cartridge

  ; hand over at the start of a fresh frame, as StandardBios does
  wait

  ; StandardBios's hand-over state: GPU ports, registers and the stack
  ; residue of its main() (so uninitialised stack reads match too)
  out GPU_MultiplyColor, COLOR_WHITE
  out GPU_DrawingPointX, 0
  out GPU_DrawingPointY, 0
  out GPU_SelectedRegion, 0

  mov R0, 0x3FFFDB             ; copy __residue to RAM 0x3FFFDB..0x3FFFFE
  mov R1, __residue
  mov R2, 36
__boot_copy:
  mov R3, [R1]
  mov [R0], R3
  iadd R0, 1
  iadd R1, 1
  isub R2, 1
  mov R3, R2
  igt R3, 0
  jt R3, __boot_copy

  mov R0, 0
  mov R1, 0
  mov R2, 0xFB                 ; StandardBios leaves its last frame count here
  mov R3, 0
  mov R4, 0
  mov R5, 0                    ; __define_regions used R4, R5
  mov BP, 0x3FFFFD
  mov SP, 0x3FFFF0
  jmp 0x20000000

; ---------------------------------------------------------------------------
; no cartridge: say so (same words as StandardBios) and stop
__no_cartridge:
  mov SP, 0x3FFFE0
  out GPU_MultiplyColor, COLOR_WHITE
  mov R0, __str_nocart_title
  mov R1, 20
  mov R2, 40
  call __print
  out GPU_MultiplyColor, COLOR_GRAY
  mov R0, __str_nocart_text
  mov R1, 20
  mov R2, 100
  call __print
  wait
  hlt

; ---------------------------------------------------------------------------
; __define_regions: regions of texture -1 (must be selected) as StandardBios
; defines them. Font: define_region_matrix(0, 1,22, 10,41, 1,22, 32,8, 0).
; Uses R0-R5.
__define_regions:
  mov R0, 0                    ; region
  mov R2, 22                   ; y0
__dr_row:
  mov R1, 1                    ; x0
  mov R3, 32                   ; columns left
__dr_col:
  out GPU_SelectedRegion, R0
  out GPU_RegionMinX, R1
  out GPU_RegionHotspotX, R1
  out GPU_RegionMinY, R2
  out GPU_RegionHotspotY, R2
  mov R4, R1
  iadd R4, 9
  out GPU_RegionMaxX, R4
  mov R4, R2
  iadd R4, 19
  out GPU_RegionMaxY, R4
  iadd R0, 1
  iadd R1, 10
  isub R3, 1
  mov R4, R3
  igt R4, 0
  jt R4, __dr_col
  iadd R2, 20
  mov R4, R0
  ilt R4, 256
  jt R4, __dr_row

  ; the other regions, from a table of (region, minX, minY, maxX, maxY, hotX, hotY)
  mov R5, __region_table
__dr_table:
  mov R0, [R5]
  ilt R0, 0
  jt R0, __dr_end
  mov R0, [R5]
  out GPU_SelectedRegion, R0
  mov R0, [R5+1]
  out GPU_RegionMinX, R0
  mov R0, [R5+2]
  out GPU_RegionMinY, R0
  mov R0, [R5+3]
  out GPU_RegionMaxX, R0
  mov R0, [R5+4]
  out GPU_RegionMaxY, R0
  mov R0, [R5+5]
  out GPU_RegionHotspotX, R0
  mov R0, [R5+6]
  out GPU_RegionHotspotY, R0
  iadd R5, 7
  jmp __dr_table
__dr_end:
  ret

; ---------------------------------------------------------------------------
; __print: R0 = string, R1 = x, R2 = y (pixels, multiples of 10 / 20 so that
; v32sim's text console sees the characters). '\n' goes back to x, 20 lower.
; Texture -1 must be selected. Uses R0-R4.
__print:
  mov R4, R1
__print_loop:
  mov R3, [R0]
  jf R3, __print_end
  ieq R3, 10
  jt R3, __print_nl
  mov R3, [R0]
  out GPU_SelectedRegion, R3
  out GPU_DrawingPointX, R1
  out GPU_DrawingPointY, R2
  out GPU_Command, GPUCommand_DrawRegion
  iadd R1, 10
  jmp __print_next
__print_nl:
  mov R1, R4
  iadd R2, 20
__print_next:
  iadd R0, 1
  jmp __print_loop
__print_end:
  ret

; __print_hex: R0 = value, R1 = x, R2 = y: "0x" and 8 upper-case digits.
; Uses R0-R4.
__print_hex:
  out GPU_DrawingPointY, R2
  out GPU_SelectedRegion, 48   ; '0'
  out GPU_DrawingPointX, R1
  out GPU_Command, GPUCommand_DrawRegion
  iadd R1, 10
  out GPU_SelectedRegion, 120  ; 'x'
  out GPU_DrawingPointX, R1
  out GPU_Command, GPUCommand_DrawRegion
  iadd R1, 10
  mov R4, 8
__ph_loop:
  mov R3, R0
  shl R3, -28
  and R3, 15
  iadd R3, 48
  mov R2, R3
  igt R2, 57
  jf R2, __ph_digit
  iadd R3, 7                   ; 'A' - '9' - 1
__ph_digit:
  out GPU_SelectedRegion, R3
  out GPU_DrawingPointX, R1
  out GPU_Command, GPUCommand_DrawRegion
  iadd R1, 10
  shl R0, 4
  isub R4, 1
  mov R3, R4
  igt R3, 0
  jt R3, __ph_loop
  in R2, GPU_DrawingPointY     ; give y back
  ret

; __print_reg: R5 = label string, R6 = value address, R7 = x, R8 = y:
; "LABEL" at x, value at x + 40. Uses R0-R4.
__print_reg:
  mov R0, R5
  mov R1, R7
  mov R2, R8
  out GPU_MultiplyColor, COLOR_LABEL
  call __print
  out GPU_MultiplyColor, COLOR_WHITE
  mov R0, [R6]
  mov R1, R7
  iadd R1, 40
  mov R2, R8
  call __print_hex
  ret

; ---------------------------------------------------------------------------
; Hardware error handler. Uses no stack until the record is written (BP and SP
; are free: the CPU has just reset them), then calls one level deep with
; SP = 0x3FFFE0, so RAM below 0x3FFFDF is left as the program had it.
__error_handler:
  mov BP, [REC_STATE]
  ieq BP, STATE_BUSY
  jt BP, __double_fault

  mov BP, STATE_BUSY
  mov [REC_STATE], BP
  mov BP, MAGIC
  mov [REC_MAGIC], BP
  mov [REC_CODE], R0
  mov [REC_NEXTIP], R1
  mov [REC_INSTR], R2
  mov [REC_IMM], R3
  mov [0x3FFFE6], R4
  mov [0x3FFFE7], R5
  mov [0x3FFFE8], R6
  mov [0x3FFFE9], R7
  mov [0x3FFFEA], R8
  mov [0x3FFFEB], R9
  mov [0x3FFFEC], R10
  mov [0x3FFFED], R11
  mov [0x3FFFEE], R12
  mov [0x3FFFEF], R13
  in BP, TIM_FrameCounter
  mov [REC_FRAME], BP
  in BP, TIM_CycleCounter
  mov [REC_CYCLE], BP
  mov BP, 0                    ; clear the rest of the record (it held the program's stack)
  mov [0x3FFFF3], BP
  mov [0x3FFFF4], BP
  mov [0x3FFFF5], BP
  mov [0x3FFFF6], BP
  mov [0x3FFFF7], BP
  mov [0x3FFFF8], BP
  mov [0x3FFFF9], BP
  mov [0x3FFFFA], BP
  mov [0x3FFFFB], BP
  mov [0x3FFFFC], BP
  mov [0x3FFFFD], BP
  mov [0x3FFFFE], BP
  mov BP, 1
  mov [REC_COUNT], BP

  ; fault address = next IP - 1, - 1 more when the instruction has an immediate
  mov R4, R1
  isub R4, 1
  mov R5, R2
  and R5, 0x02000000
  jf R5, __eh_fault
  isub R4, 1
__eh_fault:
  mov [REC_FAULT], R4

  mov SP, 0x3FFFE0
  mov BP, SP

  ; a known drawing state, whatever the program left
  out SPU_Command, SPUCommand_StopAllChannels
  out GPU_SelectedTexture, -1
  call __define_regions
  out GPU_MultiplyColor, COLOR_WHITE
  out GPU_DrawingScaleX, 1.0
  out GPU_DrawingScaleY, 1.0
  out GPU_DrawingAngle, 0.0
  out GPU_ActiveBlending, GPUBlendingMode_Alpha
  out GPU_ClearColor, COLOR_BG_ERR
  out GPU_Command, GPUCommand_ClearScreen

  ; title and description (StandardBios's words); codes above 10 are unknown
  mov R5, [REC_CODE]
  mov R6, R5
  ilt R6, 0
  jt R6, __eh_unknown
  mov R6, R5
  igt R6, 10
  jf R6, __eh_known
__eh_unknown:
  mov R5, 11
__eh_known:
  mov R6, R5
  ieq R6, 0
  jf R6, __eh_sel1
  mov R11, __e0t
  mov R12, __e0d
  jmp __eh_seldone
__eh_sel1:
  mov R6, R5
  ieq R6, 1
  jf R6, __eh_sel2
  mov R11, __e1t
  mov R12, __e1d
  jmp __eh_seldone
__eh_sel2:
  mov R6, R5
  ieq R6, 2
  jf R6, __eh_sel3
  mov R11, __e2t
  mov R12, __e2d
  jmp __eh_seldone
__eh_sel3:
  mov R6, R5
  ieq R6, 3
  jf R6, __eh_sel4
  mov R11, __e3t
  mov R12, __e3d
  jmp __eh_seldone
__eh_sel4:
  mov R6, R5
  ieq R6, 4
  jf R6, __eh_sel5
  mov R11, __e4t
  mov R12, __e4d
  jmp __eh_seldone
__eh_sel5:
  mov R6, R5
  ieq R6, 5
  jf R6, __eh_sel6
  mov R11, __e5t
  mov R12, __e5d
  jmp __eh_seldone
__eh_sel6:
  mov R6, R5
  ieq R6, 6
  jf R6, __eh_sel7
  mov R11, __e6t
  mov R12, __e6d
  jmp __eh_seldone
__eh_sel7:
  mov R6, R5
  ieq R6, 7
  jf R6, __eh_sel8
  mov R11, __e7t
  mov R12, __e7d
  jmp __eh_seldone
__eh_sel8:
  mov R6, R5
  ieq R6, 8
  jf R6, __eh_sel9
  mov R11, __e8t
  mov R12, __e8d
  jmp __eh_seldone
__eh_sel9:
  mov R6, R5
  ieq R6, 9
  jf R6, __eh_sel10
  mov R11, __e9t
  mov R12, __e9d
  jmp __eh_seldone
__eh_sel10:
  mov R6, R5
  ieq R6, 10
  jf R6, __eh_sel11
  mov R11, __e10t
  mov R12, __e10d
  jmp __eh_seldone
__eh_sel11:
  mov R6, R5
  ieq R6, 11
  jf R6, __eh_sel12
  mov R11, __e11t
  mov R12, __e11d
  jmp __eh_seldone
__eh_sel12:
__eh_seldone:
  out GPU_MultiplyColor, COLOR_WHITE
  mov R0, R11
  mov R1, 20
  mov R2, 20
  call __print
  out GPU_MultiplyColor, COLOR_GRAY
  mov R0, R12
  mov R1, 20
  mov R2, 60
  call __print

  mov R7, 20
  mov R5, __str_fault
  mov R6, REC_FAULT
  mov R8, 120
  call __print_reg2
  mov R5, __str_nextip
  mov R6, REC_NEXTIP
  mov R8, 140
  call __print_reg2
  mov R5, __str_instr
  mov R6, REC_INSTR
  mov R8, 160
  call __print_reg2
  mov R5, __str_imm
  mov R6, REC_IMM
  mov R8, 180
  call __print_reg2

  ; R4-R8 on the left, R9-R13 on the right
  mov R9, 0                    ; index 0..9
__eh_regs:
  mov R5, R9
  imul R5, 4
  mov R10, __str_regs
  iadd R5, R10
  mov R6, R9
  iadd R6, REC_R4
  mov R8, R9
  mov R7, 20
  mov R10, R9
  ilt R10, 5
  jt R10, __eh_left
  isub R8, 5
  mov R7, 340
__eh_left:
  imul R8, 20
  iadd R8, 220
  call __print_reg
  iadd R9, 1
  mov R10, R9
  ilt R10, 10
  jt R10, __eh_regs

  ; the one-line summary for scripts: V32ERR code fault instruction immediate
  out GPU_MultiplyColor, COLOR_WHITE
  mov R0, __str_v32err
  mov R1, 20
  mov R2, 340
  call __print
  mov R0, [REC_CODE]
  mov R1, 90
  call __print_hex
  mov R0, [REC_FAULT]
  mov R1, 200
  call __print_hex
  mov R0, [REC_INSTR]
  mov R1, 310
  call __print_hex
  mov R0, [REC_IMM]
  mov R1, 420
  call __print_hex

  mov R0, STATE_DONE
  mov [REC_STATE], R0
  mov R0, [REC_CODE]           ; leave the code in R0 for a debugger looking at the halt
  wait
  hlt

; __print_reg2: like __print_reg, value at x + 200 (the long labels)
__print_reg2:
  mov R0, R5
  mov R1, R7
  mov R2, R8
  out GPU_MultiplyColor, COLOR_LABEL
  call __print
  out GPU_MultiplyColor, COLOR_WHITE
  mov R0, [R6]
  mov R1, R7
  iadd R1, 200
  mov R2, R8
  call __print_hex
  ret

; a fault inside the handler: keep the first record, note the second, stop
__double_fault:
  mov [REC_DF_R0], R0
  mov [0x3FFFF4], R1
  mov [0x3FFFF5], R2
  mov [0x3FFFF6], R3
  mov BP, 2
  mov [REC_COUNT], BP
  mov BP, STATE_DOUBLE
  mov [REC_STATE], BP
  hlt

; ---------------------------------------------------------------------------
; data

; StandardBios's final region definitions other than the font
__region_table:
  integer 256, 469,  29, 469,  29, 469,  29   ; white pixel
  integer 300,   1, 183, 278, 238, 140, 238   ; logo letters
  integer 301,   1, 240, 278, 247, 140, 232
  integer 302,   1,   1, 440,  20, 221,  20
  integer 303, 280, 191, 478, 326, 280, 191
  integer 305, 322, 116, 403, 189, 322, 116
  integer 307, 444,  26, 467,  85, 444,  26
  integer 309, 469,  26, 470,  27, 469,  26
  integer 400,   1, 183, 278, 236, 140, 236   ; the logo animation's last frame
  integer 0xFFFFFFFF           ; end

; StandardBios's stack at hand-over, RAM 0x3FFFDB..0x3FFFFE
__residue:
  integer 0x003FFFEE, 0x1000023E, 0x00000137, 0x000000A2, 0x00000140, 0x000000B5
  integer 0x00000137, 0x000000A2, 0x00000020, 0x00000008, 0x00000014, 0x0000000A
  integer 0x000000B6, 0x00000001, 0x000000C9, 0x003FFFEE, 0x100004BA, 0x000000FB
  integer 0x000000DD, 0x003FFFFD, 0x10000D6F, 0x00000000, 0x00000000, 0x00000116
  integer 0x000000EC, 0x0000008C, 0x000000EC, 0x00000016, 0x00000020, 0x00000008
  integer 0x00000000, 0xFF060606, 0x00000006, 0x00000028, 0x003FFFFF, 0x10000008

__e0t:
  string "ERROR: INVALID MEMORY READ"
__e0d:
  string "Program attempted to read from a memory address\nthat does not exist or is in a write-only device."
__e1t:
  string "ERROR: INVALID MEMORY WRITE"
__e1d:
  string "Program attempted to write on a memory address\nthat does not exist or is in a read-only device."
__e2t:
  string "ERROR: INVALID PORT READ"
__e2d:
  string "Program attempted to read from a port number\nthat does not exist or is set as write-only."
__e3t:
  string "ERROR: INVALID PORT WRITE"
__e3d:
  string "Program attempted to write on a port number\nthat does not exist or is set as read-only."
__e4t:
  string "ERROR: STACK OVERFLOW"
__e4d:
  string "Program pushed too many values in the stack\nand available RAM memory was exhausted."
__e5t:
  string "ERROR: STACK UNDERFLOW"
__e5d:
  string "Program popped too many values from the stack\nand all data stored in stack was exhausted."
__e6t:
  string "ERROR: DIVISION BY ZERO"
__e6d:
  string "Program attempted to perform a division or\nmodulus operation where the divisor was zero."
__e7t:
  string "ERROR: ARC COSINE OUT OF RANGE"
__e7d:
  string "Program attempted to perform an arc cosine\noperation when the argument was not in [-1,+1]."
__e8t:
  string "ERROR: ARC TANGENT NOT DEFINED"
__e8d:
  string "Program attempted to perform an arc tangent\noperation when both of the arguments were 0."
__e9t:
  string "ERROR: LOGARITHM OUT OF RANGE"
__e9d:
  string "Program attempted to perform a logarithm\noperation when the argument is not positive."
__e10t:
  string "ERROR: POWER HAS NO REAL SOLUTION"
__e10d:
  string "Program attempted to perform a power operation\nwhen base was negative and exponent non integer."
__e11t:
  string "UNKNOWN ERROR"
__e11d:
  string "Program caused a hardware error with an error\ncode that was not recognized by the BIOS."

__str_nocart_title:
  string "NO CARTRIDGE FOUND"
__str_nocart_text:
  string "To play a game, please power off\nyour console and insert a game\ncartridge compatible with Vircon32."
__str_fault:
  string "Fault address"
__str_nextip:
  string "Next IP"
__str_instr:
  string "Instruction"
__str_imm:
  string "Immediate value"
__str_v32err:
  string "V32ERR"
__str_regs:    ; 4 words each
  string "R4 "
  string "R5 "
  string "R6 "
  string "R7 "
  string "R8 "
  string "R9 "
  string "R10"
  string "R11"
  string "R12"
  string "R13"
