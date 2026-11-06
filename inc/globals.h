#ifndef  _GLOBALS_H
#define  _GLOBALS_H

typedef signed   long long int slli;
typedef unsigned long long int ulli;
typedef struct timespec        TimeSpec;

// Simplify opcode display
struct instruction_opcode
{
    uint8_t  name[7];
};
typedef struct instruction_opcode opcode_t;

union  word_type
{
    int32_t   i32;
    float     f32;
    uint32_t  raw;
};
typedef union word_type word_t;

////////////////////////////////////////////////////////////////////////////////////////
//
// data0: shorthand union to "overload" member elements in the linked_list
// struct (fmt vs active)
//
union  data0
{
    uint8_t   fmt;
    uint8_t   active;
};
typedef union data0     d0_t;

////////////////////////////////////////////////////////////////////////////////////////
//
// data1: shorthand union to "overload" member elements in the linked_list
// struct (number vs frames)
//
union  data1
{
    uint32_t  number;
    uint32_t  frames;
};
typedef union data1     d1_t;

////////////////////////////////////////////////////////////////////////////////////////
//
// data2: shorthand union to "overload" member elements in the linked_list
// struct (line vs cycles)
//
union  data2
{
    uint32_t  line;
    uint32_t  cycles;
};
typedef union data2     d2_t;

////////////////////////////////////////////////////////////////////////////////////////
//
// data3: shorthand union to "overload" member elements in the linked_list
// struct (count vs ???)
//
union  data3
{
    uint32_t  count;
};
typedef union data3     d3_t;

////////////////////////////////////////////////////////////////////////////////////////
//
// data4: shorthand union to "overload" member elements in the linked_list
// struct (subcall vs ???)
//
union  data4
{
    uint32_t  subcall;
};
typedef union data4     d4_t;

typedef struct linked_list linked_l;
struct linked_list
{
    int8_t    *name;  // to store filename
    int8_t    *cname;
    int8_t    *label; // to label display points
    uint8_t    type;
    uint8_t    space;
    d0_t       item0;
    d1_t       item1;
    d2_t       item2;
    d3_t       item3;
    d4_t       item4;
    float      time;
    void      *pointer;
    void     **dpointer;
    word_t     data;
    linked_l  *next;
    linked_l  *end;
};

struct data_type
{
    word_t   value;
    uint8_t  flag;
    uint8_t  mode;
    uint8_t  qty;
    uint8_t  fmt;
    int8_t  *name;
    int8_t  *alias;
};
typedef struct data_type data_t;

struct memory_type
{
    uint8_t   type;
    data_t   *data;
    uint8_t   flag;
    uint32_t  firstaddr;
    uint32_t  last_addr;
    uint32_t  size;
    uint32_t  extra;       // words allocated beyond size (a ROM's texture pixels)
    uint32_t  checksum;
};
typedef struct memory_type mem_t;

extern FILE      *display;
extern FILE      *devnull;
extern FILE      *debug;
extern FILE      *verbose;
extern int8_t    *biosfile;
extern int8_t    *cartfile;
extern int8_t    *memcfile;
extern int8_t    *commandfile;
extern int8_t    *biosasmdebug;
extern int8_t    *cartasmdebug;
extern int8_t    *bioscdebug;
extern int8_t    *cartcdebug;
extern int8_t    *debugdir;
extern uint8_t    sourceflag;
extern int8_t     sys_error;
extern mem_t     *memory;
extern data_t    *reg;
extern int8_t    *token_label;
extern wpoint_t  *wpoint;

////////////////////////////////////////////////////////////////////////////////////////
//
// Variables related to IOPorts
//
extern data_t   **ioports;
extern vtex_t    *bios_vtex;  // for managing textures and regions within textures
extern vtex_t    *cart_vtex;  // for managing textures and regions within textures
extern gamepad_t *gamepad;
extern uint8_t    sys_reg_show;

extern uint8_t    action;
extern uint8_t    modeflag;
extern uint8_t    biosasmdebugflag;
extern uint8_t    bioscdebugflag;
extern uint8_t    cartasmdebugflag;
extern uint8_t    cartcdebugflag;
extern uint8_t    profileflag;
extern uint8_t    runflag;
extern uint8_t    colorflag;
extern uint8_t    branchflag;
extern uint8_t    ignoreflag;
extern uint8_t    retflag;
extern uint8_t    derefaddr;
extern uint8_t    errorcheck;
extern uint8_t    nextflag;
extern uint64_t   insn_count;
#define  BP_TEMP    9           // linked_l.space of a one-shot breakpoint (tbreak)
extern uint8_t    haltflag;
extern uint8_t    turboflag;
extern uint8_t    waitflag;
extern uint32_t   frame_us;
extern uint32_t   max_frames;
extern uint8_t    fixed_clock;                 // --fixed-clock: TIM ports start at a fixed date/time
extern uint32_t   fixed_year, fixed_day, fixed_secs;
extern uint8_t    rng_seed_set;                // --seed: RNG_CurrentValue generator seeded at startup
extern uint32_t   rng_seed;
extern uint32_t   frames_run;
extern int8_t    *profile_out;                 // --profile-out=FILE: per-address execution counts
extern uint8_t    profile_bin;                 // --profile-format=bin
extern uint32_t   profile_top;                 // --profile-top=N
extern uint8_t    fixed_cycles;                // --fixed-cycles: TIM_CycleCounter reads 0
extern uint8_t    frame_limit;
extern uint8_t    batchflag;                   // --batch: quiet scripted mode (runctl.c)
extern FILE      *status_out;                  // acknowledgements, stop reasons: stdout, or stderr with --batch
extern uint32_t   script_errors;               // command lines that failed to parse (--batch)
extern uint32_t   check_failures;              // failed expect / timed-out until
extern uint8_t    quit_status_set;             // 'quit N'
extern uint8_t    quit_status;
extern int8_t    *pad_input;                  // --input=SCRIPT (V32_INPUT syntax)
extern uint8_t    pad_input_port;             // --input-port=N
extern int8_t    *frame_stats_out;            // --frame-stats=FILE
extern uint32_t   frame_stats_top;
extern uint32_t   frame_stats_skip;           // --frame-stats-skip=N
extern uint32_t   frame_stats_top;            // --frame-stats-top=N
extern int8_t    *pad_input2;                 // --input2=SCRIPT (padscript.c)
extern int8_t     frame_stats_json;           // --stats-format=json
extern uint8_t    profile_by_function;         // --profile-by=function
extern int8_t    *symbolfile;                  // --symbol-file=FILE
extern uint8_t    exitstatus;
extern uint8_t    exitflag;
extern int8_t    *dumpspec;
extern uint8_t    consoleflag;
extern int8_t    *kbdinput;
extern int32_t    kbdport;
extern int32_t    mouseport;
extern uint8_t    wordsize;
extern uint32_t   profcount;
extern uint32_t   profcountsub;
extern uint32_t   profcountopcode[64];
extern uint32_t   rom_offset;
extern uint32_t   cpu_instruction;             // the console's Instruction / ImmediateValue registers
extern uint32_t   cpu_immediate;
extern uint8_t    fetch_fault;                 // 0 none, 1 instruction word, 2 immediate word unreadable
extern uint32_t   sys_error_ip;                // R1 override for the error being raised
extern uint8_t    sys_error_ip_set;
extern uint32_t   seek_word;
extern uint32_t   watch_word;
extern linked_l  *bpoint; // breakpoint list
extern linked_l  *dpoint; // display list
extern linked_l  *lpoint; // label list
extern linked_l  *mpoint; // tracking allocated memory
extern linked_l  *ppoint; // subroutine profiling list
extern linked_l  *csub;   // current subroutine
extern linked_l  *tpoint;
extern opcode_t   lookup[64];
extern linked_l  *next_tpoint;

////////////////////////////////////////////////////////////////////////////////////////
//
// Function prototypes
//
size_t    get_filesize   (int8_t *);
uint32_t  get_word       (FILE *);
void      put_word       (uint32_t,    uint8_t);
uint8_t   decode         (uint32_t,    uint32_t,    float,    uint8_t);
void      decode_display (uint32_t,    uint32_t,    float,    uint8_t);
uint8_t   decode_check   (uint32_t,    uint32_t,    float,    uint8_t);
void      decode_process (uint32_t,    uint32_t,    float,    uint8_t);
void      init_ioports   (void);                              // initialize IOPorts
uint8_t   ioports_chk    (uint16_t,    uint8_t,     uint8_t);
int16_t   ioports_num    (uint8_t *);
data_t   *ioports_ptr    (uint16_t);
word_t    ioports_get    (uint16_t,    uint8_t);              // get value from port
uint8_t   ioports_set    (uint16_t,    int32_t,     float,    uint8_t); // set value to port
void      update_ioports (void);
void      init_memory    (void);                              // initialize memory
uint8_t   alloc_memory   (int32_t);
void      load_command   (void);
uint32_t  load_labels    (uint8_t *,   uint8_t,     uint8_t);
uint8_t   load_memory    (uint32_t,    int8_t *);             // load file into memory
uint8_t   unload_memory  (uint32_t);
uint8_t   memory_chk     (uint32_t,    uint8_t,     uint8_t);
void      dump_words    (int8_t *);                                  // --dump=N[@ADDR|@LABEL]
word_t   *memory_get     (uint32_t,    uint8_t);
word_t   *memory_texel   (uint32_t);                            // ROM texture pixel (beyond the CPU-visible size)
void      memory_set     (uint32_t,    uint32_t,    uint8_t); // set value to memory
void      fmemory_set    (uint32_t,    float,       uint8_t);
void      init_registers (void);
void      sigcrash       (int32_t);
void      source_add_root (const char *);
const char *source_line  (const char *, uint32_t);
void      source_overlay (uint32_t);
int       state_wr32     (FILE *, uint32_t);
int       state_rd32     (FILE *, uint32_t *);
uint8_t   state_save     (const char *);
uint8_t   state_restore  (const char *);
int       gpu_save       (FILE *);
int       gpu_restore    (FILE *);
int       spu_save       (FILE *);
int       spu_restore    (FILE *);
void      spu_reset      (void);
void      spu_write      (uint16_t, int32_t, float);
void      spu_new_frame  (void);
void      spu_sound_load (int32_t, uint32_t, uint32_t *);
void      spu_cart_insert (uint32_t);
void      spu_cart_remove (void);
void      memc_save      (void);
void      gpu_reset      (void);
void      gpu_new_frame  (void);
int       gpu_console_show (FILE *);
char     *gpu_console_render (int, size_t *);
int       gpu_console_auto (void);
int       gpu_console_set_color (const char *);
int       gpu_console_set_cursor (const char *);
void      gpu_console_set_cursor_codes (const uint8_t *, int);
void      gpu_console_status (FILE *);
extern uint32_t console_gen;
void      gpu_command    (int32_t);
char     *gpu_console_snapshot (void);                       // the screen as plain text (until)
int       gpu_console_match (const char *, uint32_t);        // is the text on the screen (until, expect console)
void      gpu_console_attrs (FILE *, int);                   // color grids (console --attrs)
uint8_t   gpu_screenshot (const char *);
uint8_t   gpu_screenshot_plain (const char *);                 // without grid or outlines
int       gpu_grid_set   (int, int, int, int32_t, int, int); // on (-1 keep), dx, dy (0 keep), rgb (-1 keep), alpha % (-1 keep), labels (-1 keep)
int       cli_highlight  (const char *);                     // --highlight=TEX:REGION[:COLOR][,...]   (command.c)
int       cli_screengrid (const char *);                     // --screengrid[=SPEC]
int       shots_set_frames (const char *);                   // --screenshot-at=FRAMES (shots.c)
void      shots_set_file (const char *);                     // --screenshot-file=PATTERN
void      shots_frame (void);
void      gpu_grid_show  (FILE *);
void      gpu_noextra    (int);
int       gpu_highlight_add    (int32_t, int32_t, uint32_t);   // texture, region, 0xRRGGBB
int       gpu_highlight_remove (int32_t, int32_t);
void      gpu_highlight_clear  (void);
void      gpu_highlight_enable (int);
void      gpu_highlight_show   (FILE *);
word_t   *reg_get        (uint8_t,     uint8_t);
int8_t   *reg_get_name   (uint8_t);
void      reg_set        (uint8_t,     uint32_t,    uint8_t);
word_t   *reg_get        (uint8_t,     uint8_t);
void      reg_set        (uint8_t,     uint32_t,    uint8_t);
void      output_reg     (uint8_t,     uint8_t,     uint8_t,  uint8_t *);
void      output_mem     (uint32_t,    uint8_t,     uint8_t,  uint8_t *);
void      output_iop     (uint32_t,    uint8_t,     uint8_t *);
void      output_mem_named (const char *, uint32_t,   uint8_t,  uint8_t,  uint8_t *);
void      output_val     (const char *, uint32_t,     uint8_t);
word_t   *new_word_i32   (uint32_t *,  uint8_t);
void      cycle_begin    (void);                              // count an instruction's cycle (before it runs)
void      update_cycle   (void);                              // end the frame when its cycles are used up
void      timer_change_frame (void);                          // the timer's start-of-frame step
void      addrprof_init  (void);                               // address profile (addrprofile.c)
uint8_t   addrprof_enabled (void);
void      addrprof_count (uint32_t);
void      addrprof_write (void);
uint32_t  locals_load_cart (const char *);                   // local variables, NAME.vbin.vars (locals.c)
uint32_t  locals_load_file (const char *);
uint8_t   locals_have    (void);
void      locals_show    (const char *);
int       locals_lookup  (const char *, uint32_t *, uint32_t *);
int       srcdebug_selected (uint32_t *, uint32_t *, int *);   // pc, bp, number of the selected frame
const char *srcdebug_func_at (uint32_t);   // pc, bp, number of the selected frame
void      output_lua_value (uint32_t);
void      padscript_init (void);                               // --input=SCRIPT (padscript.c)
void      padscript_frame (void);
void      gpustat_change (void);                               // V32TRACE host-cost model (gpu.c)
extern uint32_t   gpustat_quads, gpustat_flushes, gpustat_pending;
void      v32trace_frame_end (void);                           // V32TRACE: record the frame that ended (v32trace.c)
void      v32trace_cpu_error (int32_t, uint32_t);
void      v32trace_mark  (const char *);
void      framestats_frame (void);                             // per-frame CPU / GPU load (framestats.c)
void      perf_gpu_draw (int32_t, int32_t, int32_t, int32_t);  // perfstats.c: GPU / SPU counters
void      perf_gpu_clear (void);
void      perf_spu_command (int32_t);
void      perf_new_frame (void);
void      perf_reset (void);
void      perf_skip_frames (uint32_t);
const char *perf_csv_header (void);
void      perf_csv_row (FILE *);
void      perf_regions (FILE *, const char *, int);
void      perf_summary (FILE *, const char *);
void      perf_show (FILE *);
int       perf_stat (const char *, double *);
void      perf_gpu_command (int32_t);
void      perf_port_write (uint16_t);
void      perf_port_read (uint16_t);
void      perf_instruction (void);
void      perf_bus_words (int);
void      perf_history (int);
void      perf_json_row (FILE *);
void      perf_json_summary (FILE *);
int       perf_save (const char *, int);
void      framestats_write (void);
void      v32trace_init  (void);                               // V32TRACE (v32trace.c)
void      v32trace_ready (void);
void      v32trace_instr (uint32_t);
void      v32trace_frame (void);
void      v32trace_write (void);
extern int8_t   *v32trace_out;
extern int8_t   *v32trace_meta_file;
extern int8_t   *v32trace_label_file;
extern uint8_t   v32trace_norestart;
extern uint8_t   v32trace_on;
void      update_frame   (void);                              // updating FrameCounter
uint32_t  word2int       (word_t *);
float     word2float     (word_t *);
linked_l *listnode       (uint8_t,     uint32_t);
linked_l *list_add       (linked_l *,  linked_l *);
linked_l *add_list       (linked_l *,  linked_l *);
linked_l *list_grab      (linked_l **, linked_l *);
uint32_t  displayshow    (linked_l *,  uint8_t);
void      show_sysregs   (void);
void      display_config (uint8_t);
uint8_t  *show_size      (uint32_t);
void      process_args   (int32_t,     int8_t  **);
void      usage          (int8_t   *);
void      help           (uint8_t);
uint8_t  *get_input      (FILE     *,  const uint8_t *);
FILE     *open_search_file (const char *, const char *, char *, size_t);
uint32_t  sym_load_cart  (const char *);                      // symbols.c: data symbols of a C program
uint32_t  sym_load_file  (const char *);
int       sym_lookup     (const char *, uint32_t *, uint32_t *);
int       sym_name_for   (uint32_t,    char *,      size_t);
void      runctl_init    (void);                              // runctl.c: batch mode, run/until/expect
void      runctl_reset   (void);
void      runctl_run     (uint32_t);
void      runctl_until   (const char *, uint32_t,   uint32_t);
void      runctl_frame   (void);
void      runctl_expect_value   (wloc_t, uint8_t, wloc_t, uint8_t);
void      runctl_expect_stat    (const char *, const char *, const char *);
void      runctl_expect_console (const char *, uint32_t);
void      status_printf  (const char *, ...);
uint8_t   prompt         (uint32_t);
linked_l *find_label     (linked_l *,  int8_t *);
linked_l *find_value     (linked_l *,  uint32_t);
linked_l *find_label_at  (linked_l *,  uint32_t);
linked_l *find_ptr       (linked_l *,  void     *);
slli      timediff_ns    (TimeSpec *,  TimeSpec *);
void      gamepad_io     (uint16_t);
void      gamepad_control (uint8_t,    uint8_t,     uint8_t);
uint8_t   gamepad_pressed (uint8_t,    uint8_t);
void      gamepad_publish (uint8_t);
void      gamepad_reset  (uint8_t);
void      gamepad_connect (uint8_t,    uint8_t);
void     *ralloc         (size_t,      size_t,      uint8_t);
void      rfree          (void     *);
void      sigint         (int32_t);
void      srcdebug_invalidate (void);                          // srcdebug.c: source-level debugging (V32DBG)
int       srcdebug_have  (void);
int       srcdebug_describe (uint32_t, char *, size_t, int);
int       srcdebug_backtrace (void);
void      srcdebug_frame (int, uint32_t, int);
void      srcdebug_cancel (void);
int       srcdebug_step  (int);
int       srcdebug_finish (void);
int       srcdebug_until (const char *, uint32_t, int);
int       srcdebug_rearm (void);
int       srcdebug_check (void);
int       srcdebug_resolve (const char *, uint32_t, int, uint32_t *, int, uint32_t *);
int       srcdebug_function (const char *, uint32_t *);
void      srcdebug_list  (const char *, uint32_t, int, const char *);
const char *source_lang  (const char *);                       // "LUA", "C" or "ASM", by the file's extension
void      source_suppress (uint32_t);                          // source.c: do not print the overlay at this IP once

#endif
