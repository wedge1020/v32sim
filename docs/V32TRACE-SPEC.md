# V32TRACE SPEC, version 2.1

A translator-neutral interface between a Vircon32 cartridge built by an `XtoV32` trans-assembler and the tools that run it: the
Vircon32 DesktopEmulator's trace recorder and the `v32sim` simulator. A translator that follows this spec gets, from the same patched
emulator and without any emulator change: play-and-record sessions, trap capture with guest state, several traps and events in one
session, guest-level hot spots, host cost measurements, and machine-readable output for its own gap-filling tools.

The key words MUST, SHOULD and MAY are used as in RFC 2119.

## 1. Roles

| role | does |
|---|---|
| translator | builds the cartridge and writes the **build files** (section 2) |
| recorder | a program that runs the cartridge and reads the build files: the patched DesktopEmulator (`V32Trace`) or `v32sim --v32trace`. Writes the **session files** (section 4). Knows no guest machine |
| consumer | a script or tool of the translator that reads the session files and fixes gaps (section 6) |

A recorder only reads V32 state: CPU registers, RAM words, instruction addresses. Everything about the guest is described by the translator.

## 2. Build files

All three MUST sit in the folder of the `.v32` and be written by the same build. `<stem>` is the `.v32` file name without extension.

| file | content |
|---|---|
| `<stem>.v32` | the cartridge |
| `<stem>.labels.txt` or `labels.txt` | one `0xADDRESS name` per line: V32 address (`0x20000000` + program word offset), label name. A recorder tries `<stem>.labels.txt` first |
| `<stem>.v32trace.meta` or `v32trace.meta` | the description in section 3. A recorder tries `<stem>.v32trace.meta` first |

Without a meta file a recorder still reports cycles, dropped frames, host cost and hot spots (by label if labels exist), but detects no trap
and names no guest location.

## 3. The meta file

Text, UTF-8 or ASCII, one directive per line: a keyword then arguments separated by spaces. `#` starts a comment line. Keywords are lower
case. Numbers are hexadecimal unless a directive says otherwise. A recorder MUST ignore a keyword it does not know; a translator MAY use extra
keywords for its own tools.

### 3.1 Required

| directive | meaning |
|---|---|
| `format N` | the spec version, `2` for this document. MUST be the first directive. A recorder accepts the versions it implements (this one accepts 1 and 2: a version 1 file is a version 2 file without the directives marked "2") and MUST otherwise ignore the whole file and say so in its report |
| `words N` | decimal. The number of program words of the `.v32` just built |
| `arch NAME` | the guest name (`gb`, `nes`, `sms`, `a2600`, `snes`, ...). Lower case, no spaces. Written to every report and trap line |
| `trap LABEL` | V32 label of a routine the cartridge enters when it cannot continue. When the first instruction at that label executes the recorder captures a trap (section 4.1). More than one `trap` line MAY be given |

### 3.2 Cartridge identity (2)

| directive | meaning |
|---|---|
| `fnv1a HEX` | the FNV-1a hash of the program ROM, computed over its words: `h = 2166136261`, then for each 32-bit program word `w`, `h = (h xor w) * 16777619` (mod 2^32). SHOULD be given |

**The identity check**: if `words` differs from the loaded cartridge's program size, or `fnv1a` is given and differs from its hash, the recorder
ignores the meta file and the labels, and says `meta: IGNORED` in the report. This protects against files left from another build.

### 3.3 Locating guest code

| directive | meaning |
|---|---|
| `block PATTERN [packed] [bank=HEX] [bankadd=HEX]` | The first label of a translated guest block encodes its guest location. PATTERN is a `scanf` pattern matching the whole label name. Default form: two hex numbers, **bank** then **address** (`_b%x_%x`). `bank=HEX`: one number, the address, in that fixed bank. `packed`: one number, `bank << 16 \| address`. `bankadd=HEX` is added to the bank. Several `block` lines MAY be given and are tried in order. Bank is at most 16 bits, address at most 16 bits |
| `continue PREFIX` | labels named PREFIX followed by a digit belong to the block above them |
| `continue PREFIX*` | labels starting with PREFIX belong to the block above them |

A guest location is written everywhere as `bank:address` in hex (`6:88BF`, `0:0150`); an architecture without banks uses bank 0. Labels that
match no `block` and no `continue` end the current block (runtime code).

### 3.4 Capturing state

| directive | meaning |
|---|---|
| `capture NAME ADDR COUNT [index=R<n>[&MASK]] [volatile]` | read COUNT (1..64) 32-bit RAM words at V32 RAM word address ADDR. With `index=`, the address is ADDR + (R<n> & MASK), for words addressed through a guest register such as a guest stack pointer. `volatile` (2): the value changes from one occurrence of the same problem to the next (a frame counter) and is left out of the identity of a trap or event (section 4.1) |
| `register NAME R<n> [MASK] [volatile]` | the value of CPU register R<n> (R0..R15) masked with MASK |
| `kinds CAPTURE VALUE=name ...` | names for the values of a one-word capture (`kinds trapkind 1=no-block 2=ram-code-mismatch`). A hit whose capture has a named value gets `kind=name` in `traps.txt` (2) and the name in the human report |

NAME is 1..63 characters, no spaces or `=`, and MUST NOT be one of the reserved names of section 4.2. Captures are taken at every trap and event and,
for plain `capture` lines without `index=`, at every marker (section 4). `register` and indexed captures are not sampled at markers.

A translator SHOULD capture at least: the guest program counter or trapped target, the mapped bank or banks, and a trap kind if the cartridge has
more than one reason to trap. It MUST keep NAMEs stable between versions, because consumers parse them.

### 3.5 Soft events (2)

| directive | meaning |
|---|---|
| `event NAME LABEL` | when the first instruction at LABEL executes, the recorder captures an **event**: the same capture as a trap, but the cartridge goes on. For conditions a translator wants counted and located without stopping: an unimplemented opcode treated as a no-op, a fallback path taken, a RAM-code mismatch it can survive |

### 3.6 Restart after a trap (2)

| directive | meaning |
|---|---|
| `resume [LABEL]` | the cartridge MAY be restarted after a trap (section 4.1). With no LABEL it restarts at the cartridge's first instruction; with LABEL at that label |

Without `resume` a recorder MUST NOT restart the cartridge.

### 3.7 Per-frame measurement

| directive | meaning |
|---|---|
| `series NAME ADDR` | sample the RAM word at ADDR at the end of every frame (at most 8). The report gives average, minimum and maximum, per session and per marker stretch. Values are signed decimal. Optional for a recorder (`v32sim` writes it too) |

### 3.8 Example (NEStoV32)

```
format 2
words 286768
fnv1a 2D1C45C2
arch nes
rom prg_crc32=EFB09075 chr_crc32=00000000 mapper=2 prg8=16
trap _halt
block _b%x_%x
continue _t
capture trappc 8006 1
capture trapkind 8007 1
capture frame 8005 1 volatile
capture prgbank8 808C 4
resume
kinds trapkind 1=no-block 2=ram-code-mismatch 3=bank-not-built
```

`rom` is not interpreted (free text for humans and consumers).

## 4. Session files

The DesktopEmulator user records with **Console > Record V32TRACE** (F7), plays, drops markers with **F8**, and stops with F7 (or resets, powers
off, quits; each new trap, event and marker also writes the files, so closing the window after a trap loses nothing). Files go to the emulator's
`Data/Traces/` folder; `<cart>_<stamp>` is the cartridge file name and the local date and time. `v32sim --v32trace=PREFIX` writes `PREFIX.traps.txt`
and `PREFIX.blocks.txt` instead.

| file | content |
|---|---|
| `<cart>_<stamp>.trace.txt` | the human report (section 4.3). DesktopEmulator and `v32sim` (`PREFIX.trace.txt`) |
| `<cart>_<stamp>.traps.txt` | machine-readable trap and event record (section 4.2). Written when the session has at least one |
| `<cart>_<stamp>.blocks.txt` | every translated guest block that ran: header `# <arch> blocks that ran ...`, then `bank:address count` per line, count = instructions executed. Written when labels were found |

**Cartridge frame** numbers count from 0 at the first frame the cartridge ran (the BIOS intro is not counted).

### 4.1 Traps, events and restarts

A **hit** is a trap or an event. A recorder records at most 64 distinct hits per session; further distinct ones are counted as dropped.

- **Identity.** Two hits are the same when they are both traps or both events of the same name and every non-`volatile` capture has the same
  words. The same hit again increases `count` of the recorded one; its other data (frame, registers, trail) is the first occurrence's.
- **A trap stops the cartridge**: it spins in its trap routine. While it does, the recorder counts nothing: no instruction counts, no jump trail,
  and the frames are marked as spent in a trap and left out of every measurement (perf, host, series, hot spots). Events are not captured while stopped.
- **An event** does not stop anything and is captured wherever its label runs.
- **Restart.** When the meta file has `resume`, restarting is enabled (the DesktopEmulator has the menu item **Restart cartridge after a trap**, on by
  default; `v32sim` restarts unless `--v32trace-norestart`), and the cartridge has been in a trap for 60 frames (long enough to see the trap screen), the
  recorder restarts it: R0..R15 are set to 0, SP and BP to the last RAM word, the instruction pointer to the cartridge's first instruction (or the
  `resume` label), the CPU's halted and waiting states are cleared. **Nothing else is reset**: RAM, GPU and SPU keep their state, so a translator's
  start code MUST be able to run again over a used machine.
- **Give-up.** If the same trap (by identity) comes back three times in a row, the recorder stops restarting for the rest of the session and notes it.
  A session that restarts therefore ends when the player ends it or the cartridge keeps failing in the same place.
- A restart does not end the session and does not start a new file; a console reset or power cycle does.

### 4.2 traps.txt

```
# V32TRACE-traps 2 cart=<file> fnv1a=<hash of the program ROM> restarts=<n>
TRAP  n=<k> cartframe=<N> count=<C> arch=<arch> [kind=<name>] <NAME>=<hex>[,<hex>...] ... trail=<bank:addr>[,<bank:addr>...]
EVENT n=<k> cartframe=<N> count=<C> arch=<arch> name=<event> [kind=<name>] <NAME>=<hex>[,<hex>...] ... trail=...
```

Revision 2.1 (additive) appends two fields to every line, after `trail`: `segstart=<N> input=<script>`. `segstart` is the cart frame at which the cartridge was
last (re)started before the hit (0 when it was never restarted). `input` is the gamepad 1 history of the cart frames `segstart` .. `cartframe` as a
`V32_INPUT` script (`BUTTON[+BUTTON]@FROM-TO[,...]`, frames numbered from `segstart`, so frame 0 is the first frame of that run of the cartridge; `-` when no
button was held). It is the way to *get back to the hit*: feed it to `v32sim` (`V32_INPUT=<script>`) or, after the translator's own renaming (V32 `L` is NES
Select; `R`, `X`, `Y` are unmapped; the guest sees a press a few frames after the cart frame it was held in), to the translator's profiler. A script spans
only one run of the cartridge because a restart starts the guest over; the whole-session script in the human report (4.3) is one timeline across restarts and
does not replay in a profiler. Button names are the recorder's (`LEFT RIGHT UP DOWN START A B SELECT L R`); Y has none. The `input` value never contains
whitespace. Recorders that do not know the fields (spec 2) omit them; a consumer that needs them MUST fall back to the human report's `[input]` section and
its restart events, as `tools/trace2input.py` does.

One line per distinct hit, in order of first occurrence. After the first token, split on whitespace, then each field on the first `=`. Names are exactly
the `capture` and `register` NAMEs; values are hex words (no prefix), comma separated for COUNT > 1. `trail` is the last 32 guest blocks entered
(8 for an event), oldest first (`-` when empty). **Reserved names**, which a translator MUST NOT use as capture NAMEs: `n`, `cartframe`, `count`, `arch`,
`name`, `kind`, `trail`, `segstart`, `input`. A consumer MUST ignore fields it does not know.

### 4.3 The human report

Sections: header (`cart:` file, title, words, fnv1a of the program ROM; `arch:`; `meta:` problems; `labels:`), `[events]` (every hit in full, restarts, CPU
errors, markers), `[input]` (a `V32_INPUT=` script for gamepad 1 over the whole session, and from 2.1 a `# restarts at cart frames:` line), `[perf]` (cycles per frame against the 250,000 budget, frames where the cartridge missed
its WAIT, expensive frames, timeline), `[host]` (quads and video-queue flushes per frame, host milliseconds per frame, catch-up frames; the cost the stock
emulator pays), series, `[hot spots]` (instructions per label and per guest block, whole session and per marker stretch). A marker's note carries the plain
captures at that moment: `[NAME=hex]`. The layout of the human report is NOT part of this spec: consumers MUST use `traps.txt` and `blocks.txt`.

## 5. Conformance

A translator conforms when:
1. it writes the three build files together, with `words` (and `fnv1a`) matching the `.v32`;
2. every `trap` and `event` names a label that exists in the labels file;
3. every `block` pattern matches the real label names, and runtime labels do not match;
4. the capture NAMEs it documents are the ones its consumer reads;
5. if it writes `resume`, the cartridge's start code runs correctly over a used machine (RAM, GPU and SPU not reset).

Check: build a cartridge that traps, record a session, read `traps.txt`. A missing guest location in `trail` means a `block` or `continue` line is
missing; `meta: IGNORED` means `words` or `fnv1a` is stale; a session that restarts into garbage means the start code assumes a fresh machine.

## 6. Consumers

A consumer is translator-specific and reads only `traps.txt` and `blocks.txt`. Two examples:

| translator | consumer | rule |
|---|---|---|
| NEStoV32 | `tools/gapfill.py BASE.prof OUT.prof *.traps.txt` | kind 1: the PC in the mapped bank becomes profile coverage; kind 3: the bank is marked seen in that slot; kind 2 (RAM code) is reported |
| GBtoV32 | `gbroots.py *.traps.txt` | pc < 4000: bank 0; 4000..7FFF: the captured bank; >= 8000: RAM code, not fixable by `--root` |

Profile or root formats stay private to each translator. Sharing coverage between users (by ROM hash) is the translator's concern; the spec only asks
that `rom` carry whatever identifies the ROM.

An unattended loop needs no emulator: run the cartridge in `v32sim --v32trace=run` with scripted input, give `run.traps.txt` to the consumer, rebuild,
repeat until `run.traps.txt` has no `TRAP` line.

A human run can feed the same loop: play to the problem in the emulator, then turn the trap's `input` into the profiler's script and merge the new profile with
the old one (NEStoV32: `NESPROF_INPUT="$(tools/trace2input.py run.traps.txt)" build/NESprof game.nes run.prof "$(tools/trace2input.py run.traps.txt --frames)"`,
then `tools/profmerge.py`). What the human did to reach a trap is thereby replayable without them, and later runs can start from it. NEStoV32 wraps the steps in `tools/gaps_from_input.sh GAME.nes BASE.prof run.traps.txt OUT [--extend ...]`: the profiler never traps, so one pass covers every gap on the recorded path (and on `--extend` keystrokes beyond the trap) instead of one gap per restart. After the trap the replay keeps running (`--tail`, default 600 frames) and can press random buttons (`--wander N --seeds K`), because the recording stops at the trap and the code behind it is otherwise never executed.

## 7. Implementations

| implementation | supports |
|---|---|
| DesktopEmulator patch (`V32Trace`) | everything in this spec |
| `v32sim --v32trace=PREFIX` (`--v32trace-meta=FILE`, `--v32trace-labels=FILE`, `--v32trace-norestart`) | traps, events, restart, identity check, `traps.txt`, `blocks.txt`, `segstart=`/`input=` (`--input=SCRIPT` replays), `series`, markers (`mark`), the human report `PREFIX.trace.txt` (`--input=@FILE` replays its `V32_INPUT=` line) and the host cost (by the patch's model). On the gap ROM the three session files equal the DesktopEmulator recorder's. Not yet: `input2=`. The simulator is not cycle-exact: trust the guest address first; the frame number matched the console's on the gap ROM |

## 8. Relation to V32DBG

V32DBG describes source-level debugging of programs written in a source language (line tables, locals). V32TRACE describes guest code produced by a
translator and the gaps in it. They share nothing at the file level and do not conflict: `labels.txt` is a V32TRACE build file, the `.debug` files are
V32DBG's. A later version MAY let a recorder name hot spots by V32DBG source line when a debug file sits beside the cartridge.

## 9. Limits

- Gamepad 1 only; the Y button is not recorded. A script recorded after a restart replays from a clean guest, not from the restarted machine that kept RAM, GPU and SPU state. Loading a savestate while recording makes the trace discontinuous.
- Recording costs about 12% emulation time on a heavy cartridge; off, the CPU only tests a null pointer.
- Captured values are V32 RAM words and CPU registers; guest state kept elsewhere cannot be captured.
- The jump trail holds the last 512 control transfers; the block trail is derived from it.
- A restart is not a reset: it keeps RAM, GPU and SPU state (section 4.1).

## 10. Candidates for version 3

Naming hot spots by V32DBG line; gamepad 2 (`input2=`); a meta directive naming the guest button map and input delay so a generic consumer can do what `trace2input.py` does for NES; a `restart-state` directive listing RAM ranges
a recorder zeroes on restart.

## 11. Changes

Version 2.1: `segstart` and `input` per hit in `traps.txt` (the script that walks to the hit since the last restart), the `# restarts at cart frames:` line in the report. Old files remain valid.

Version 2: `fnv1a` identity check; several traps and events per session with identity and counting; `event`; `resume` and restart after a trap; `volatile`;
`kind=` in `traps.txt` and symbolic kinds; `traps.txt` format 2 (`n`, `count`, `name`, `kind`; several lines); `v32sim` as a second recorder; trap frames excluded
from measurements. A version 1 recorder file set remains valid input.

Version 1: first version. Replaces the GBtoV32-specific trace patch; the recorder has no built-in knowledge of any guest machine.
