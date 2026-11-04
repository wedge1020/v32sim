# Vircon32 C, the stock `compile` dialect (agent card)

The official compiler (ComputerSoftware `DevelopmentTools/CCompiler`, v26.04.24) reads a small C-like language. Code written for
it must follow the rules below; v32cc accepts all of this *and* ordinary C, and v32c++ rewrites ordinary C/C++ into it. The
complete list, each item confirmed against the real compiler: v32c++'s `docs/VIRCON32_QUIRKS.md` (28 entries).

## Write it this way

| rule | stock form | ordinary C |
|---|---|---|
| arrays: length before the name | `int[8] buf;` `int[4][4] m;` | `int buf[8];` |
| no `struct` keyword in a reference | `Point p;` `Point* next;` | `struct Point p;` |
| function pointer declarator | `int(int, int)* fp;` cast `(int(int)*)x` | `int (*fp)(int, int);` |
| entry point | `void main(void)` only | `int main()` |
| prototypes name their parameters | `int f(int a);` | `int f(int);` |
| null pointer | `NULL` (it is **-1**) | `0` |
| float literals | `5.0`, `0.5`, `5.` (no `.5`, no `1e3`) | `1e3`, `.5` |
| escapes | `\n \r \t \\ \' \" \xHH` only (`'\0'` reads as `'0'`!) | `\0`, octal |
| `malloc` / `free` | `#include "misc.h"` | `<stdlib.h>` |

## Not there at all

- no ternary `?:`, no comma operator; no `static`, `extern`, `volatile`, `unsigned`, `signed`, `short`, `long` (one integer type,
  32-bit signed); no K&R, no bit-fields that pack.
- parameters and return values are **one word**: no struct passed or returned by value; empty structs are rejected.
- no declaration directly after a `case` label (brace the case body).
- a function nothing calls is dropped, and the `asm { }` data inside it with it.

## Behaves differently

- `>>` is a **logical** shift (`-8 >> 1` is `0x7FFFFFFC`): the CPU has only `SHL` with a negative count.
- `x op= y` evaluates `x` twice (a side effect in the target happens twice).
- writing through a pointer a call returned (`*f() = 1`) runs the call more than once.
- an array is a pointer only in an assignment, an initializer or an argument.
- `const` is enforced on values that are merely read; globals are writable.
- string literals: control codes (BIOS glyphs 14-31) cannot be assembled.
- `char` is a whole word; strings are one character per word.

## Inline assembly and headers

`asm { "mov R0, {var}" }` blocks with `{name}` operands; the SDK headers (`video.h`, `audio.h`, `input.h`, `time.h`, `math.h`,
`string.h`, `misc.h`, `memcard.h`) are ordinary C with `asm` bodies: `clear_screen(color)`, `select_texture(id)`,
`select_region(id)`, `draw_region_at(x, y)`, `print_at(x, y, text)`, `end_frame()` (a `WAIT`), `select_gamepad(n)`,
`gamepad_left()` / `gamepad_button_a()` ... (frame counts, >0 held), `get_frame_counter()`, `rand()` / `srand(seed)` (`misc.h`), `play_sound(id)` / `play_sound_in_channel(id, ch)`.
Read the header for the exact name before using one: they are not standard C names (`gamepad_button_a`, not `gamepad_a`).

## Checking a program

`compile x.c -o x.asm -g; assemble x.asm -o x.vbin -g program; packrom x.xml -o x.v32`, then run it headless
(`v32sim --batch -r --fast -x --max-frames=600 -B StandardBios.v32 x.v32`). A self-checking test sets a global
(`int test_errors = -1;` -> 0) that `--dump=1@test_errors` reads back (V32SIM.md).
