; G20: the clock when it starts from the host's local time (no --fixed-clock): the date,
; the time and the frame counter, once per frame for 130 frames (two clock ticks, and a
; midnight, a year change or a leap day when the run starts close to one).
; tests/run_clock.sh runs it for many instants and zones with time(NULL) faked.
;@ frames=130 dump=390@40 name=clock
  mov R1, 0x40
  mov R5, 130
__frame:
  in R0, TIM_CurrentTime
  mov [R1], R0
  in R0, TIM_CurrentDate
  mov [R1+1], R0
  in R0, TIM_FrameCounter
  mov [R1+2], R0
  iadd R1, 3
  isub R5, 1
  wait
  jt R5, __frame
  hlt
