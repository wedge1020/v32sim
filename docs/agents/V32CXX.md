# v32c++ -- C++ and C to Vircon32 C (agent card)

A cfront-style transpiler: ordinary C++ or C in (`v32c++`, `v32c` for C input), the stock compiler's Vircon32 C out. The
project's `AGENTS.md` is the full guide. Docs: `README.md`, `docs/VIRCON32_QUIRKS.md` (the stock dialect, rule by rule, and what
v32c++ does about each), `docs/C_INPUT.md`, `docs/V32IO.md`, `docs/STRING.md`, `docs/GENERICS.md`, `docs/INLINE_ASSEMBLY_SUPPORT.md`,
`docs/NATIVE_PASSTHROUGH.md` (each also in Spanish and French).

```
v32c++ game.cpp -o out/game.c            # then: compile out/game.c -o game.asm; assemble; packrom (Make.sh is written)
v32c a.c b.c lib.v32a                    # C input: several files linked by v32c++ itself; --mklib makes libraries
v32c++ --target=standard x.cpp           # print standard C instead (for comparing with gcc)
```

## Works (just write it)

classes, inheritance, virtual functions, constructors / destructors, operators, references, `const`, objects by value (lowered to
hidden pointers), `new`/`delete`, namespaces, `auto`, range `for`, `std::string`, `std::array<T,N>`, `std::vector<T>` (fixed
per-type forms), `static`/`extern`/`volatile`, any float literal, ternary and comma operators, `switch` fall-through, `goto`,
`va_list`, inline `asm { "..." }` with `{&var}` / `{*p}` operands, `__attribute__((rom))` const arrays, `<v32/*.hpp>` wrappers
(keyboard and mouse: `docs/V32IO.md`). C input adds designated initializers, compound literals, `_Static_assert`, `__atomic_*`.

## Does not (clear error)

templates (only the fixed std forms), exceptions, RTTI, multiple inheritance, lambdas, `enum class`, `constexpr`, `mutable`,
nested classes and class-scope enums, `static` class members, `T *&`, 2-D array parameters, objects through `...`.

## Differs from C++

`unsigned`, `short`, `long` are one 32-bit `int`; `double` is `float` (signed `>>` and unsigned `< > / %` are rewritten to behave;
`--no-signed-shift`, `--no-unsigned-ops`); `new T[n]` skips constructors; `goto` out of a scope skips destructors; a class in a
namespace keeps its bare C name (do not name yours `String`, `Keyboard`, `Mouse`); an SDK pointer's null is -1; the SDK compiler
drops functions nothing calls; it warns about unused arguments and stops after 10 warnings (`(void)x;` or
`--silence-unused-args`).

## Tests and tools

`make test`, then `tools/vircon32/check.sh` (compiles every output with the real toolchain and runs the self-checking samples:
`int test_errors = -1;` set to 0). The headless tools it used (`v32run`, `v32prof`, `v32shot`, `v32peek`) now live in v32sim
`tools/oracle/`; `v32sim/tools/v32sim-selfcheck.sh` runs the same samples in v32sim (and `--oracle` on the stock core).
No `-g` source markers yet: debugging by source line needs v32opt's `v32srcmark.py` on the `compile -g` output.
