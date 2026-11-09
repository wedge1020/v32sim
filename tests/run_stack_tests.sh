#!/bin/bash
# tests/run_stack_tests.sh -- the whole development stack against v32sim: every front end's -g chain, plain and through v32opt.
#
#   tests/run_stack_tests.sh TOOLCHAIN_DIR          (make check runs it as the "stack" suite)
#
#   C        v32cc -g -> v32as -g
#   C++      v32c++ -g --compiler=v32cc (#line markers) -> v32cc -g --target-compat=v32cxx -> v32as -g
#   Lua      v32lua -g -> v32as -g
#   each at -O0 (no v32opt), -O2 and -O3 (v32opt between the compiler and the assembler)
#
# examples/stack.{c,cpp,lua} are the same program in three languages: add(3, 4) with a local that is bumped (by a macro in C and C++),
# a loop, a dead variable. For each build v32sim must
#   * stop at FILE:LINE of the *source* language (break), name the function and the line in `bt`,
#   * show the locals true: at -O0 every value exact; at -O2/-O3 a value is either the true one or `<optimized out>` -- never a
#     wrong one (a register nobody writes any more, a variable the optimizer folded away),
#   * in C and C++: put a macro's statements on the line that uses the macro (the #define line has no code).
# A language whose tool is missing from the toolchain directory is reported as skipped, never as a pass.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$HERE/..; EX=$HERE/examples
SIM=${V32SIM:-$ROOT/bin/v32sim}
TC=${1:-${V32TC:?usage: run_stack_tests.sh TOOLCHAIN_DIR}}; TC=$(cd "$TC" && pwd); export V32TC=$TC
BIOS=${V32BIOS:-$TC/StandardBios.v32}
W=${WORK:-$(mktemp -d)}; mkdir -p "$W"
pass=0; fail=0; skip=0
ok()   { pass=$((pass+1)); echo "  ok    $1"; }
bad()  { fail=$((fail+1)); echo "  FAIL  $1"; }
skipt(){ skip=$((skip+1)); echo "  skip  $1 ($2)"; }
[ -x "$SIM" ] || { echo "no v32sim at $SIM"; exit 2; }
have() { [ -x "$TC/$1" ]; }

# session CART SCRIPT: the commands on stdin, the output on stdout
session() { printf "$2" | "$SIM" --batch --fast -x --max-frames=3000 -B "$BIOS" -L src "$1" 2>&1; }

# values OUT "name=VALUE ..." EXACT: each name's line must show VALUE (a regex), or, unless EXACT, <optimized out>
values() {
    local out=$1 spec=$2 exact=$3 w n v line r=0
    for w in $spec; do
        n=${w%%=*}; v=${w#*=}
        line=$(echo "$out" | grep -E "^$n = " | head -1)
        if echo "$line" | grep -qE "^$n = $v( |\$)"; then :
        elif [ "$exact" = 0 ] && echo "$line" | grep -q "^$n = <optimized out>"; then :
        else r=1; echo "        $n: wanted $v, got '${line:-no line}'"; fi
    done
    return $r
}

# case NAME FILE LINE1 "vars at LINE1" LINE2 "vars at LINE2" FUNCRE  (cart in $W/NAME/..)
check_cart() {
    local label=$1 cart=$2 file=$3 l1=$4 v1=$5 l2=$6 v2=$7 fre=$8 exact=$9 out o1 o2
    out=$(session "$cart" "break $file:$l1\ncontinue\nlocals\nbt\nbreak $file:$l2\ncontinue\nlocals\nquit\n")
    o1=$(echo "$out" | sed -n '1,/^#0/p'); o2=$(echo "$out" | sed -n '/^\[breakpoint\] triggered.*:'"$l2"' /,$p')
    if echo "$out" | grep -qE "^#0  $fre \($file:$l1\)"; then ok "$label: bt names the function and the $file line"
    else bad "$label: bt: $(echo "$out" | grep '^#0' | head -1)"; fi
    local r; r=$(values "$o1" "$v1" "$exact"); if [ -z "$r" ]; then ok "$label: locals at $file:$l1"; else bad "$label: locals at $file:$l1"; echo "$r"; fi
    r=$(values "$o2" "$v2" "$exact"); if [ -z "$r" ]; then ok "$label: locals at $file:$l2"; else bad "$label: locals at $file:$l2"; echo "$r"; fi
}

build() {  # build LABEL SRC LEVEL -> sets CART
    local label=$1 src=$2 lvl=$3 dir; dir="$W/$label"; mkdir -p "$dir"; cp "$EX/$src" "$dir/"
    CART=
    if ( cd "$dir" && sh "$ROOT/tools/v32mkcart.sh" -o out $([ "$lvl" != 0 ] && echo -O "$lvl") "$src" >/dev/null 2>"$dir/build.err" ); then
        CART=$dir/out/${src%.*}.v32
    else bad "$label: build failed: $(tail -2 "$dir/build.err" | tr '\n' ' ')"; fi
}

for lvl in 0 2 3; do
    [ "$lvl" != 0 ] && have v32opt || [ "$lvl" = 0 ] || { skipt "-O$lvl (all languages)" "no v32opt in the toolchain directory"; continue; }
    exact=1; [ "$lvl" != 0 ] && exact=0
    echo "== C, -O$lvl"
    if have v32cc && have v32as; then
        build "c-O$lvl" stack.c "$lvl"
        if [ -n "$CART" ]; then
            check_cart "C -O$lvl" "$CART" stack.c 7 "a=3 b=4 sum=8" 16 "r=14 dead=6 i=4" "add" "$exact"
            o=$(session "$CART" "break stack.c:6\nbreak stack.c:1\nquit\n")
            echo "$o" | grep -q "(stack.c:6 in add)" && echo "$o" | grep -q "line 1 has no code" \
                && ok "C -O$lvl: the macro's statement is on the line that uses it, the #define line has no code" || bad "C -O$lvl macro lines: $(echo "$o" | head -3 | tr '\n' '|')"
        fi
    else skipt "C" "no v32cc/v32as"; fi
    echo "== C++, -O$lvl"
    if have v32c++ && have v32cc && have v32as && { [ -d "$TC/../include/v32c++" ] || [ -d "$TC/include/v32c++" ] || [ -n "${V32CXX_INCLUDE:-}" ]; }; then
        build "cpp-O$lvl" stack.cpp "$lvl"
        if [ -n "$CART" ]; then
            check_cart "C++ -O$lvl" "$CART" stack.cpp 9 "a=3 b=4 s=8" 21 "r=14 dead=6 i=4" ".*add.*" "$exact"
            o=$(session "$CART" "break stack.cpp:8\nbreak stack.cpp:1\nquit\n")
            echo "$o" | grep -q "(stack.cpp:8 in " && echo "$o" | grep -q "line 1 has no code" \
                && ok "C++ -O$lvl: the macro's statement is on the line that uses it" || bad "C++ -O$lvl macro lines: $(echo "$o" | head -3 | tr '\n' '|')"
        fi
    else skipt "C++" "needs v32c++ with v32cc/v32as and its headers (TOOLCHAIN/include/v32c++ or \$V32CXX_INCLUDE)"; fi
    echo "== Lua, -O$lvl"
    if have v32lua && have v32as; then
        build "lua-O$lvl" stack.lua "$lvl"
        [ -n "$CART" ] && check_cart "Lua -O$lvl" "$CART" stack.lua 5 "a=3.0000 b=4.0000 sum=8.0000" 12 "r=18.0000 dead=10.0000" "add" "$exact"
    else skipt "Lua" "no v32lua in the toolchain directory"; fi
done
echo "== $pass passed, $fail failed, $skip skipped"
[ $fail = 0 ]
