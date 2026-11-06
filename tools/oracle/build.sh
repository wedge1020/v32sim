#!/bin/sh
# *****************************************************************************
#  tools/oracle/build.sh -- build the tools that run on the stock Vircon32 console logic
#
#    tools/oracle/build.sh CHECKOUT [OUTDIR]
#
#  CHECKOUT is a https://github.com/vircon32/ComputerSoftware checkout (or the src/ComputerSoftware of a
#  bootstrap archive). OUTDIR defaults to tools/oracle/bin. Built there:
#
#    v32orun      THE headless runner (orun.cpp; header there): a bare .vbin on logging stand-ins (v32lua's
#                 audits), a real BIOS + cartridge, or a .vbin as BIOS; GPU/SPU logs, traps, BP watch, profile,
#                 per-frame cycles (-b), CFI choice. Replaces the runners of v32sim, v32c++, v32cc and v32lua.
#    refrun v32run vrun   the old command lines of those runners, as one-line wrappers over v32orun --compat=NAME:
#                 refrun = the differential tests' oracle (tests/run_gaps.sh); v32run = until HLT, one RAM word
#                 (v32c++); vrun = v32cc's (--keys, --memc, a .vbin as BIOS, STATUS halted|timeout)
#    v32gpureplay   a v32sim --gpu-trace / --spu-trace replayed on the stock GPU / SPU logic (v32orun --compat=gpureplay)
#    v32prof v32peek v32shot   per-frame CPU/GPU load and cycles per function / CPU state and RAM at a frame / PPM
#                 screenshots at chosen frames, all with scripted input (from v32c++): wrappers over v32orun --compat=prof|peek|shot
#    sysprof      whole-system profiler: BIOS, kernel and the programs in a swapped user window (from v32nix)
#    v32trace-ref the DesktopEmulator's V32TRACE recorder run headless; only when the checkout has GBtoV32's
#                 emulator-v32trace.patch applied (it is skipped otherwise): the reference for v32sim --v32trace
#    compile assemble packrom   the stock toolchain, and include/ and StandardBios.v32 beside them
#
#  These are the DesktopEmulator's own ConsoleLogic compiled unchanged with every video/audio callback
#  stubbed out, so they are the reference for what the console does. v32sim is the interactive and scriptable
#  front end; use these to ask "does the real console agree?" (docs/TOOLS.md).
#
#  ONLY="v32orun" builds just that (v32prof and v32peek come with it) (v32orun brings its wrappers). Needs g++ with C++17 and nothing else (no SDL, OpenGL, OpenAL:
#  shim/SDL.h stands in for the one function the dev tools call). wav2vircon needs a real SDL2 and is built
#  only when sdl2-config exists.
#
#  Revisions: the tools were verified against commit 5219741 (compiler v26.04.24); docs/V32SIMGAPS.md compares
#  the console logic at e75b537. They build against either.
# *****************************************************************************
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "${1:?usage: build.sh COMPUTERSOFTWARE_CHECKOUT [OUTDIR]}" && pwd)
OUT=${2:-$HERE/bin}; mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
CXX=${CXX:-g++}
ONLY=${ONLY:-"v32orun sysprof compile assemble packrom wav2vircon"}
DT="$SRC/DevelopmentTools"; CL="$SRC/DesktopEmulator/ConsoleLogic"
want() { case " $ONLY " in *" $1 "*) return 0;; esac; return 1; }

FLAGS="-O2 -std=c++17 -w -I$HERE/shim -I$DT -I$SRC -I$DT/DevToolsInfrastructure -I$SRC/VirconDefinitions"
INFRA="$DT/DevToolsInfrastructure/*.cpp"

if want compile; then
    echo "building compile..."
    # Source list as in DevelopmentTools/CMakeLists.txt (globbing CCompiler/*.cpp picks up a stale file)
    CC_FILES=""
    for f in CNodes CTokens CheckBinaryOperations CheckNodes CheckUnaryOperations CompilerInfrastructure DataTypes \
             DebugInfo EmitBinaryOperationNodes EmitExpressionNodes EmitNonExpressionNodes EmitUnaryOperationNodes \
             Globals Main MemoryPlacement Operators RegisterAllocation SourceLocation StaticValue VirconCAnalyzer \
             VirconCEmitter VirconCLexer VirconCParser VirconCPreprocessor; do CC_FILES="$CC_FILES $DT/CCompiler/$f.cpp"; done
    $CXX $FLAGS -I$DT/CCompiler $CC_FILES $INFRA -o "$OUT/compile"
fi
if want assemble; then echo "building assemble..."; $CXX $FLAGS -I$DT/Assembler $DT/Assembler/*.cpp $INFRA -o "$OUT/assemble"; fi
if want packrom; then
    echo "building packrom..."
    $CXX $FLAGS -I$DT/RomPacker -I$DT/ExternalLibraries/tinyxml2 $DT/RomPacker/*.cpp $INFRA \
         $DT/ExternalLibraries/tinyxml2/tinyxml2.cpp -o "$OUT/packrom"
fi
if want wav2vircon; then
    if command -v sdl2-config >/dev/null 2>&1; then
        echo "building wav2vircon..."
        $CXX -O2 -std=c++17 -w -I$DT -I$SRC -I$DT/DevToolsInfrastructure $(sdl2-config --cflags) -I$DT/WAV2Vircon \
             $DT/WAV2Vircon/wav2vircon.cpp $INFRA $(sdl2-config --libs) -o "$OUT/wav2vircon"
    else
        echo "skipping wav2vircon: needs SDL2 (only needed to pack sounds)"
    fi
fi
if want v32orun || want refrun || want v32run || want vrun || want v32prof || want v32peek || want v32shot || want v32gpureplay; then
    echo "building v32orun..."
    # the one runner (orun.cpp). CFI of NaN/out-of-range floats is host-dependent in the stock core, so v32orun is
    # linked with a copy of V32CPUProcessors.cpp whose float->int conversion calls v32orun_cfi() (--cfi=arm, --cfi-log).
    GEN="$OUT/.gen"; mkdir -p "$GEN"
    LINE='Register->AsInteger = (int32_t)Register->AsFloat;'
    [ "$(grep -cF "$LINE" "$CL/V32CPUProcessors.cpp")" = 1 ] || { echo "build.sh: ProcessCFI not found in V32CPUProcessors.cpp (ConsoleLogic changed)" >&2; exit 1; }
    { echo '#include <cstdint>'; echo 'extern int32_t v32orun_cfi(float f, int32_t ip);'
      sed 's|Register->AsInteger = (int32_t)Register->AsFloat;|Register->AsInteger = ::v32orun_cfi(Register->AsFloat, CPU.InstructionPointer.AsInteger);|' "$CL/V32CPUProcessors.cpp"; } > "$GEN/V32CPUProcessors.cpp"
    OTHERS=$(ls "$CL"/*.cpp | grep -v '/V32CPUProcessors.cpp$')
    $CXX -O2 -std=c++17 -w -I"$CL" -I"$SRC/DesktopEmulator" -I"$SRC/VirconDefinitions" "$HERE/orun.cpp" $OTHERS "$GEN/V32CPUProcessors.cpp" -o "$OUT/v32orun"
    rm -rf "$GEN"
    # the old runners are this program under their old command lines
    for w in refrun v32run vrun peek prof shot gpureplay; do
        n=$w; case $w in peek|prof|shot|gpureplay) n=v32$w;; esac
        printf '#!/bin/sh\nexec "$(dirname "$0")/v32orun" --compat=%s "$@"\n' $w > "$OUT/$n"; chmod +x "$OUT/$n"
    done
fi
for t in sysprof v32trace-ref; do
    want $t || continue
    if [ $t = v32trace-ref ] && [ ! -f "$CL/V32Trace.cpp" ]; then
        case " $ONLY " in *" v32trace-ref "*) echo "v32trace-ref: $CL has no V32Trace.cpp (apply GBtoV32's emulator-v32trace.patch)" >&2; exit 1;; esac
        continue
    fi
    echo "building $t..."
    $CXX -O2 -std=c++17 -w -I"$CL" -I"$SRC/DesktopEmulator" -I"$SRC/VirconDefinitions" "$HERE/$t.cpp" "$CL"/*.cpp -o "$OUT/$t"
done
if want compile && [ -d "$DT/Data/include" ]; then rm -rf "$OUT/include"; cp -r "$DT/Data/include" "$OUT/include"; fi
[ -f "$SRC/DesktopEmulator/Data/Bios/StandardBios.v32" ] && cp "$SRC/DesktopEmulator/Data/Bios/StandardBios.v32" "$OUT/StandardBios.v32"
echo "done: $OUT"
