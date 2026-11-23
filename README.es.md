# v32sim

[Versión en inglés / in English](README.md) | [Versión en francés / en français](README.fr.md)

**Referencia de la línea de órdenes:** [docs/USAGE.es.md](docs/USAGE.es.md) (cada opción, variable de entorno y estado de salida), o `man v32sim`.

**v32sim** es un depurador y simulador en modo texto para la consola de fantasía
Vircon32. Su propósito es hacer observable la máquina: ejecutar la CPU paso a
paso, instrucción por instrucción, inspeccionar y modificar registros, memoria y
IOPorts, poner puntos de ruptura y puntos de vigilancia, perfilar la ejecución
y manejar la máquina desde archivos de órdenes para pruebas automatizadas.

No es un reproductor: no hay ventana ni salida de audio. Modela la CPU, el
sistema de memoria, los IOPorts, una GPU por software (de modo que `screenshot`
puede escribir la pantalla en un PNG) y una SPU (estado de canales y de sonido,
pero no se reproduce nada) con la fidelidad suficiente para estudiar, depurar y
trastear con software de Vircon32, y para automatizar las pruebas de cartuchos.

## Estado actual

- Las 64 instrucciones de la CPU están implementadas y se comprueban ejecutando
  los mismos cartuchos en v32sim y en la lógica de la consola original
  (`tests/run_gaps.sh`, `run_fuzz.sh`, `run_romcheck.sh`, `run_clock.sh`, `run_memcard.sh`; ver
  [docs/V32SIMGAPS.md](docs/V32SIMGAPS.md)): el temporizador y el RNG, la pila,
  el bus de memoria, la temporización de los errores, una palabra por ciclo en
  `MOVS`/`SETS`/`CMPS`, la limitación de puertos de la GPU y la validación de la
  ROM coinciden con la consola. Solo difieren el comportamiento indefinido del
  anfitrión (`INT_MIN / -1` y los desplazamientos >= 32 en anfitriones distintos de x86-64) y el renderizado a nivel de
  píxel
- Los registros (R0-R15 con sus alias, más los registros del sistema IP/IR/IV) y
  las cuatro páginas de memoria (RAM, BIOS, CART, MEMCARD) se comportan como el
  hardware
- Los errores de sistema (acceso no válido a memoria o puertos, división por
  cero, etc.) registran el estado de la máquina en R0-R3, restablecen SP/BP a la
  última palabra de la RAM y redirigen la ejecución al manejador de errores de la
  BIOS en `0x10000000`
- Grupos de IOPorts: TIM (con años bisiestos), RNG, INP (mandos), CAR, MEM, GPU,
  SPU
- **GPU**: un renderizador por software con un búfer de imagen de 640x360:
  borrado, dibujo, zoom, rotación, rotozoom, mezcla (alfa/suma/resta), color
  multiplicador y el presupuesto de píxeles por cuadro (`GPU_RemainingPixels`);
  `screenshot` lo escribe en un PNG
- **SPU**: 16 canales con selección de sonido, volumen, velocidad, bucle y estado
  de posición que avanzan cada cuadro, y se cargan los datos VSND de la BIOS y
  del cartucho; no hay salida de audio
- **Tarjeta de memoria**: se validan la cabecera `V32-MEMC` y el tamaño; los
  cambios llegan al archivo al final de cada cuadro (así que un simulador
  terminado a la fuerza pierde como mucho un cuadro), como hace el
  DesktopEmulator. Una tarjeta formateada para **v32dev** mantiene un buzón
  activo, de modo que los programas que usan la biblioteca v32dev
  (almacenamiento, serie, ...) hablan con un proceso anfitrión real
  `v32dev.py serve` (ver *Tarjeta de memoria y v32dev*)
- **Teclado v32io**: un `v32kbd` en cualquier puerto de mando, alimentado desde
  el indicador (`kbd type`, `kbd key`) o desde el anfitrión (un terminal, una
  tubería, una FIFO o un archivo), ver *El teclado v32io*
- **Ratón v32io**: un `v32mouse` en otro puerto de mando, manejado desde el
  indicador (`mouse move`, `mouse click`), ver *El ratón v32io*
- **Puntos de vigilancia** sobre registros, IOPorts y palabras de memoria,
  comparados con un número o con otro registro, IOPort o palabra de memoria;
  disparados por flanco
- **Guardar / restaurar** el estado completo de la máquina
- **Ensamblador en vivo**: `asm` ensambla una instrucción y la parchea en la
  memoria, o la ejecuta una vez
- **Depuración a nivel de código fuente** (V32DBG): `step`/`next` (en los modos
  de código fuente), `finish` y `until` avanzan por línea de código, `break`/`tbreak ARCHIVO:LÍNEA`,
  `list` y `bt` con nombres de función y líneas, para Lua, C y ensamblador; ver
  [Depuración a nivel de código fuente](#depuración-a-nivel-de-código-fuente)
- **Superposición de código fuente**: la línea de código en ensamblador, C o Lua
  correspondiente al IP actual se imprime en el indicador
- Interfaz de órdenes construida con **flex/bison** (`lexer.l`, `parser.y`), y un
  segundo par flex/bison para el ensamblador (`asm_lexer.l`, `asm_parser.y`): las
  órdenes pueden abreviarse, `#` inicia un comentario y los errores de sintaxis
  indican el token y la columna que fallan
- `ignore` salta la instrucción actual; un manejador de **SIGSEGV/SIGBUS/SIGFPE/SIGABRT**
  imprime el estado del simulador antes de morir
- **Modo Lua** experimental (`--langmode=lua`): las palabras de memoria se
  inspeccionan en busca de etiquetas de valor de Lua (cadenas, tablas,
  funciones, nil/true/false) al mostrar con `/B` o `/s`
- **Guiones** (`--batch`): símbolos (etiquetas y globales de C) donde se acepte
  un número, `run N`, `until "TEXTO"`, `expect`, `quit N`, estados de salida,
  `console ARCHIVO` y `console --attrs`, `watch/d` con signo y perfiles por
  función; ver [Guiones](#guiones)
- **Archivos de depuración** (etiquetas y números de línea) se cargan
  automáticamente (ver [Archivos de depuración](#archivos-de-depuración)); `-n`
  lo desactiva

- **Repositorio central de herramientas**: los guiones, las herramientas de
  emulador original («oráculo») y los bancos de pruebas que otros proyectos de
  Vircon32 usan para probar, depurar, perfilar y simular sus programas viven
  también aquí: ver [docs/TOOLS.md](docs/TOOLS.md),
  [docs/TESTING.md](docs/TESTING.md), [docs/PROFILING.md](docs/PROFILING.md) y,
  para un agente que trabaja en otro proyecto,
  [docs/AGENTS-SNIPPET.md](docs/AGENTS-SNIPPET.md); `AGENTS.md` es para trabajar
  en el propio v32sim
- **Variables locales** (`locals`, `info locals`; V32DBG sección 4): las
  variables del marco seleccionado con sus tipos y valores, a partir del
  `NAME.vbin.vars` que `v32as -g` escribe para la salida de `v32cc -g` y
  `v32lua -g`
- **Mando con guion por cuadro del cartucho** (`--input`, la sintaxis
  `V32_INPUT` de los registradores V32TRACE)
- **Estadísticas de cuadros** (`--frame-stats=ARCHIVO`): carga de CPU y GPU por
  cuadro en CSV, con un resumen de media / p95 / máximo

## Tabla de contenidos

- [Estructura del proyecto](#estructura-del-proyecto)
- [Requisitos](#requisitos)
- [Compilación](#compilación)
- [Instalación](#instalación)
- [Uso](#uso)
- [Mapa de memoria](#mapa-de-memoria)
- [El depurador](#el-depurador)
- [Órdenes](#órdenes)
- [Depuración a nivel de código fuente](#depuración-a-nivel-de-código-fuente)
- [Tarjeta de memoria y v32dev](#tarjeta-de-memoria-y-v32dev)
- [El ratón v32io](#el-ratón-v32io)
- [El teclado v32io](#el-teclado-v32io)
- [La consola de texto](#la-consola-de-texto)
- [Ejecución sin supervisión](#ejecución-sin-supervisión)
- [Velocidad y modo turbo](#velocidad-y-modo-turbo)
- [Guiones](#guiones)
- [Pruebas](#pruebas)
- [IOPORTS](#ioports)
- [Limitaciones conocidas y funciones previstas](#limitaciones-conocidas-y-funciones-previstas)

## Estructura del proyecto

```
.
├── Makefile              definiciones de compilación
├── README.md             este archivo (README.es.md, README.fr.md: traducciones al español y al francés)
├── LICENSE
├── TODO.md
├── src/                  todos los fuentes en C
│   ├── lexer.l           analizador léxico flex para las líneas del indicador y de archivos de órdenes
│   ├── parser.y          gramática bison; rellena una orden analizada (cmd_t)
│   ├── command.c         tabla de órdenes, abreviaturas y todos los ejecutores
│   ├── asm_lexer.l       analizador léxico flex del ensamblador en vivo (prefijo `asm`)
│   ├── asm_parser.y      gramática bison del ensamblador en vivo (prefijo `asm`)
│   ├── assembly.c        tabla de mnemónicos, símbolos, codificación de instrucciones
│   ├── gpu.c             GPU por software, búfer de imagen, captura de pantalla PNG
│   ├── spu.c             modelo de canales y sonido de la SPU, carga de VSND
│   ├── savestate.c       guardar / restaurar el estado de la máquina
│   ├── source.c          superposición de líneas de código en ensamblador / C / Lua
│   ├── srcdebug.c        depuración a nivel de código fuente: tabla de líneas, pasos por línea/finish/until,
│                         marcos y bt, break ARCHIVO:LÍNEA, list
│   ├── watchpoint.c      puntos de vigilancia de registros / IOPorts / memoria
│   ├── args.c help.c     opciones de la línea de órdenes, el texto de uso y el `help` de la sesión
│   └── input.c           bucle del indicador, readline, entrada de archivos de órdenes, archivos de depuración
├── inc/                  todas las cabeceras (command.h, version.h, ...)
├── obj/                  objetos, archivos de dependencias y los generados
│                         lexer.c / parser.c / parser.h y sus equivalentes asm_*
├── bin/                  el binario v32sim compilado (generado)
├── bios/                 archivos de datos de la BIOS
├── gen/                  salida de flex/bison, incluida en el repositorio (`make gen`), para compilar sin flex/bison
├── tools/                guiones y herramientas de oráculo para probar otros proyectos (docs/TOOLS.md)
│   └── oracle/           herramientas en C++ sobre el ConsoleLogic original: v32orun (lo envuelven refrun v32run vrun v32prof v32peek v32shot) sysprof
├── tests/                los conjuntos de pruebas (ver Pruebas más abajo), fuentes de cartuchos, casos diferenciales (tests/gaps)
├── docs/                 USAGE.md (referencia de la línea de órdenes, con USAGE.es.md / USAGE.fr.md en español y francés),
│                         TOOLS, TESTING, PROFILING, PERFORMANCE, las especificaciones y los documentos de estado;
│                         agents/ (una ficha por herramienta), proposals/ (peticiones abiertas a los otros proyectos)
├── man/                  páginas de manual: v32sim.1 (v32sim.es.1, v32sim.fr.1)
├── AGENTS.md CLAUDE.md   notas para agentes (CLAUDE.md importa AGENTS.md)
└── cart/                 archivos de datos de cartuchos
```

## Requisitos

- Un compilador de C (GCC o Clang)
- **GNU readline**: edición de la línea del indicador, teclas de cursor e
  historial de órdenes (en macOS la función de historial forma parte de
  readline; en Linux `libhistory` es una biblioteca aparte: el Makefile se
  encarga de ambos casos)
- **flex** y **bison** (cualquier versión razonablemente reciente; la gramática
  evita directivas específicas de una versión, así que sirven el bison 2.3 y el
  flex 2.5.35 de serie de macOS)
- libm (estándar en Linux y macOS)

Ya no se usan las expresiones regulares POSIX, y la salida PNG no depende de
ninguna biblioteca (el escritor de capturas de pantalla está integrado).

## Compilación

```
make            # compilación por defecto -> bin/v32sim
make LTO=       # sin optimización en el enlazado (por defecto: -O2 -flto si el compilador lo admite; OPT=-O3 etc.)
make debug      # compilación -DDEBUG -g -O0 -> bin/v32sim
make asan       # compilación con AddressSanitizer + UBSan, para cazar errores de memoria
make gen        # actualiza gen/ (la salida de flex/bison) tras editar un archivo .l o .y
make version    # imprime la cadena de versión (make version VERSION=X sella inc/version.h, TODO.md y la línea .TH de las páginas de manual)
make test       # tests/run_kbd_sync_tests.sh, run_cli_tests.sh, run_v32dev_server_tests.sh: no necesitan cadena de herramientas
make check TC=DIR [V32IO=DIR] [SRCMARK=FILE]   # todos los conjuntos de pruebas desde un directorio de cadena de herramientas (tools/v32sim-check.sh)
make test-harness                              # tests/run_harness_tests.sh (V32TC, V32BIOS, ORACLE en el entorno)
make oracle COMPUTERSOFTWARE=DIR               # compila las herramientas sobre la lógica de consola original (tools/oracle/build.sh)
make archive    # ../v32sim-project-YYYYMMDD.zip del árbol limpio
make clean      # elimina el contenido de obj/ (incluido el analizador generado) y bin/v32sim
```

Sin flex ni bison se usa la salida de `gen/` ya incluida en el repositorio
(como hace `tools/build-nogen.sh`). No añadas `-ffast-math` ni `-march=native`:
la GPU y las instrucciones de coma flotante deben dar el mismo último bit en
cualquier anfitrión.

Los archivos objeto y de dependencias van a `obj/`; el binario va a
`bin/v32sim`. Se siguen las dependencias de las cabeceras, así que editar una
cabecera de `inc/` recompila todos los fuentes que la incluyen. Los archivos
objeto **no** se recompilan cuando cambian las opciones: ejecuta `make clean`
al cambiar entre las compilaciones por defecto, `debug` y `asan`.

## Instalación

```
make install      # el binario a ~/bin/bin.<arch>/ (o a ~/bin/ si no existe); el árbol tools/ a ~/.local/lib/v32sim/tools;
                  # la página de manual (comprimida con gzip) a ~/.local/share/man/man1
make sysinstall   # el binario a /usr/local/bin/; el árbol tools/ a /usr/local/Vircon32/v32tools/v32sim/tools;
                  # la página de manual (comprimida con gzip) a /usr/local/share/man/man1
make uninstall    # y make sysuninstall
```

`sysinstall` admite `PREFIX` (por defecto `/usr/local`), `VROOT` (por defecto
`$(PREFIX)/Vircon32/v32tools`), `MANDIR` (por defecto `$(PREFIX)/share/man/man1`)
y `DESTDIR`; `install` admite `USRLIB` (por defecto `~/.local/lib/v32sim`) y
`USRMAN` (por defecto `~/.local/share/man/man1`). `DEST_LANG=en|es|fr` instala
la página de ese idioma (`man/v32sim.es.1` ...) como `v32sim.1`, de modo que
`man v32sim` la muestra; sin definir, o con `all`, instala la inglesa. Las
herramientas de `tools/` buscan `v32sim` junto a ellas (`../bin/v32sim`, en una
copia de trabajo) y luego en `$PATH`; otros proyectos llegan a ellas mediante
`$V32SIM_HOME/tools`. Ambos objetivos eliminan lo que una versión anterior
instaló en `share/v32sim`.

## Uso

```
Usage: v32sim [OPTION]... [CARTFILE.v32]
Debugger/Simulator for Vircon32 Fantasy Console (version VERSION)

Mandatory arguments to long options are mandatory for short options too.

 -B, --biosfile=FILE       load this BIOS V32 file as BIOS
 -b, --break=OFFSET|LABEL|FILE:LINE
                           set a breakpoint (repeatable); a name that is a known label is that label even when
                           it is also hex (add, face), otherwise a hex OFFSET; FILE:LINE as 'break' does
 -C, --command-file=FILE   load this file with sim commands
 -c, --colors              enable colorized output
 -d, --deref-addr          output address of dereference
 -D, --debug               enable simulator debug output (development)
 -e, --errorcheck          enable runtime error checking
     --bios-asm-debug=FILE load BIOS asm labels from FILE
     --bios-c-debug=FILE   load BIOS C labels from FILE
     --cart-asm-debug=FILE load CART asm labels from FILE
     --cart-c-debug=FILE   load CART C labels from FILE
     --debug-dir=DIR       also look for debug files in DIR (first)
     --source              show the source line for each stop (asm; C with -L C)
 -E, --entry-point=OFFSET  set simulator entry point
 -L, --langmode=MODE       label/source mode: C, lua (Lua values in memory dumps) or src (any
                           language); the source line is shown at each stop with --source
 -M, --memcfile=FILE       load this file as a MEMCARD
 -n, --no-debug            do not process any debug files
 -p, --profile             enable instruction profiling
 -r, --run                 do not enable single-step mode
 -S, --bios-start          start at the BIOS code start (the default)
 -w, --watch-for=OPCODE    run until OPCODE is encountered
 -v, --verbose             enable more verbose output
 -V, --version             display version information and exit
     --v32kbd[=PORT]       emulate a v32kbd keyboard on gamepad PORT (default 0)
     --kbd-input=SOURCE    host key source: stdin, fd:N, FIFO/pipe/file (implies --v32kbd)
     --kbd-sync            v32kbd: deliver the next key event only after the program has read the gamepad
                           since the last one (the default replaces it, as the real keyboard does)
     --warn-halt           say on stderr when the program executes HLT (the emulator then stops)
     --halt-status=N       exit status N (1-255) when the run ends at HLT (and nothing else went wrong)
     --v32mouse[=PORT]     emulate a v32mouse on gamepad PORT (default 1); drive it with the
                           'mouse' command
     --fast                do not throttle to real time (frames run as fast as possible)
     --turbo[=report]      --fast, plus: spin loops that cannot change anything are fast-forwarded and a full GPU
                           pixel queue is dropped instead of painted (screenshots may then be incomplete; README "Turbo mode");
                           =report prints what was skipped to stderr at exit
     --gpu-eager           paint every GPU command at once instead of when a screenshot or save needs the pixels
                           (same pictures; for comparing, and for timing the painting itself)
     --frame-us=N          wall-clock microseconds per frame (default 16666; 0 = --fast)
     --max-frames=N        stop after N frames
     --fixed-clock[=Y,D,S] TIM_CurrentDate/Time start at year Y, day D (0 = Jan 1st), S seconds
                           (default 2026,0,0) instead of the host clock (reproducible runs)
     --seed=N              seed the RNG_CurrentValue generator with N at startup
     --tty                 full-screen interactive mode: the text console on this terminal, keys
                           to a v32kbd (port 0 unless --v32kbd=N); Ctrl-] = debugger
     --console-color=MODE  text console colors: none, 16, 256, true, auto (--tty: auto)
     --cursor-color=COLOR  recolor the cursor block (BIOS character 20) on the text console
     --console             print the BIOS-font text screen when the run ends
     --dump=N[@ADDR]       (or ADDR:N) after the run print N RAM words (signed, one per line) from ADDR
                           (hex, or a label; NAME also tries global_NAME), default RAM start
     --profile-out=FILE    when the run ends write how many times each instruction address was
                           executed (needs no prompt and no -p); debug labels add a third column
     --profile-format=FMT  text (default) or bin (V32PROF1 header, then LE32 address,count pairs)
     --profile-top=N       print the N hottest addresses to stderr when the run ends
     --profile-event=KIND  what the profile counts: instr (default), gpu-draw, gpu-portwrite, spu-write, cycles
     --time-breakdown      at exit, where v32sim's own wall-clock time goes per frame (cpu / gpu / spu / output)
     --frame-log=FILE      write that breakdown for every frame to FILE (also without --time-breakdown)
     --v32dev-doorbell[=MS] v32dev mailbox: header flag bit 1, per-channel reply counters at header words 10..13, and a store to
                           a cart part's c2h_head rings: flush, give the host MS ms (2) to answer, import its reply, go on
     --v32dev-host=N=SPEC  serve channel N of the card's v32dev mailbox natively: storage:FILE[,bytes][,ro][,sector=N][,create=S],
                           serial:stdio|echo[,hup=N]|tcp=PORT[,in=FILE][,out=FILE][,hangup]; --v32dev-host-force formats over other data.
                           Environment: V32DEV_SERVER=1 serves V32DEV_CH0..3 (a SPEC each; the option wins) with the mailbox in memory only
     --gpu-trace=FILE      record the program's GPU port writes by frame (binary, docs/TRACE-FORMAT.md); --spu-trace=FILE: SPU
     --trace-dump=FILE     print a trace file as text and exit
     --profile-by=KIND     address (default) or function: with function, --profile-top and the text
                           --profile-out add up the addresses of each function (the closest label)
     --input=SCRIPT        hold gamepad buttons by cartridge frame: BUTTON[+BUTTON]@FIRST-LAST,... or @FILE (V32_INPUT
                           syntax, frame 0 = the cart's first; V32TRACE input=); --input-port=N (default 0)
     --input2=SCRIPT       the same for gamepad 2 (port 1; V32TRACE input2=, @FILE reads its V32_INPUT2= line)
     --screenshot-at=FRAMES  write the screen after these cartridge frames (N, A-B, A-B/STEP, comma
                           separated); frame 0 is the first one the cartridge runs, after the BIOS hands over
                           (the BIOS logo takes ~250 frames, and --max-frames counts them too)
     --screenshot-file=PAT   file name for those; %d = the frame (default v32sim-shot-%05d.png)
     --screengrid[=SPEC]     grid over screenshots: DX[xDY],color=C,alpha=N,labels|nolabels (default 40x40)
     --highlight=T:R[:COLOR] outline texture T region R (or bios:R) in screenshots; comma separated
     --noextra             no grid and no outlines in screenshots
     --frame-stats=FILE    write one CSV row per frame (cycles, CPU %, GPU pixels, GPU %) and print a summary
                           (mean, p95, max, frames at 100%) to stderr when the run ends
     --stats=FILE          the same as --frame-stats
     --stats-format=json   --frame-stats FILE as JSON Lines (stable dotted names, a summary line at the end)
     --frame-stats-top=N   list the N regions charged the most GPU pixels in that summary (default 8, 0 = none)
     --stats-top=N         the same as --frame-stats-top
     --frame-stats-skip=N  leave the first N frames (the BIOS logo, ~250) out of that summary
     --stats-skip=N        the same as --frame-stats-skip
     --v32trace=PREFIX     V32TRACE (spec 2.1): read the translator's <cart>.v32trace.meta and labels, watch its trap
                           routines and events, write PREFIX.traps.txt, .blocks.txt and the .trace.txt report; after a trap the
                           cartridge restarts (--v32trace-norestart: stay in the trap); --v32trace-meta=FILE and
                           --v32trace-labels=FILE name the files when they are not beside the cartridge
     --fixed-cycles        reads of TIM_CycleCounter return 0 (programs seeding from it repeat)
     --symbol-file=FILE    also read data symbols from FILE (v32cc's NAME.asm.sym format); the ones
                           next to the V32 file are read automatically
 -q, --batch, --quiet     for scripts: commands from stdin or -C without prompt or echo; no banner,
                           instruction line, display list or register summary; stdout holds only what
                           print, console, ... print; acknowledgements and stop reasons go to stderr;
                           a failed expect or timed-out until ends the run with status 1
 -x, --exit-status         exit status tells how the program ended: 0 HLT, 2 frame limit,
                           3 system error (stops instead of entering the BIOS handler)
                           (always: 1 a check failed or a bad option, 4 --batch could not parse a command, 5 --dump failed,
                           N 'quit N'; at startup 11 a file cannot be opened, 12 not read, 21 not a V32 file)
 -h, --help                display this information

FILE   is any path plus the filename desired
OFFSET is the full 32-bit/4-byte memory address (hex)
OPCODE is the full 32-bit/4-byte instruction hex
```

Este es el resultado de `--help`; [docs/USAGE.es.md](docs/USAGE.es.md) (y `man v32sim`) describen cada opción, las variables de
entorno, los archivos y los estados de salida. Las opciones que llevan un argumento lo admiten como `--opt=VALOR` o `--opt VALOR`;
las opcionales (`--v32kbd`, `--v32mouse`, `--turbo`, `--fixed-clock`, `--screengrid`, `--v32dev-doorbell`) solo como `--opt=VALOR`.
`--quiet` es otro nombre de `--batch`, y `--stats`, `--stats-skip` y `--stats-top` lo son de las opciones `--frame-stats`. `-w OPCODE` implica `-r`,
`--tty` implica `-r` y `--kbd-input` implica `--v32kbd`. Un CARTFILE igual a `-` significa que no hay cartucho: la BIOS se ejecuta sola.
`-b OFFSET|LABEL|FILE:LINE` admite lo que admite la orden `break` y se cumple una vez cargados los archivos de depuración: un nombre que
es una etiqueta (o función) conocida es esa etiqueta aunque también sea un número hexadecimal (`add`, `face`, `dead`; escriba `0xdead`
para la dirección); si no, es un OFFSET hexadecimal, con o sin `0x`; un valor que no es nada de eso se notifica por stderr. `-L MODE` debe
ser `C`, `lua` o `src`; otro valor es un error (mensaje por stderr, código de salida 1). `-S` empieza en el inicio del código de la BIOS,
que es lo predeterminado (`-E` empieza en otro sitio).
`--screenshot-at` cuenta cuadros del cartucho: el cuadro 0 es el primer cuadro que termina con el contador de programa en el cartucho, es decir,
después de que la BIOS le ha cedido el control (el logotipo de la BIOS estándar dura unos 250 cuadros); `--max-frames` cuenta todos los cuadros
desde el encendido, logotipo incluido, así que hay que darle cuadros suficientes (`--max-frames=400` para el cuadro 120 del cartucho). Los cuadros
que la ejecución no llegó a alcanzar se notifican por stderr al final (`[WARNING] --screenshot-at: ...`).

`v32sim --version` imprime `v32sim ` y la versión (`AAAAMMDD`, con `-dev` en una compilación de desarrollo). La versión es la
definición `VERSION` de `inc/version.h`; se muestra en el banner, en el texto de uso, en la orden `version` del indicador y en
`make version`. Se puede cambiar al compilar con `-DVERSION=\"...\"`.

Ejemplos:

```
v32sim cart.v32                     # empieza en el depurador
v32sim -r cart.v32                  # solo ejecuta el cartucho
v32sim -b main -C setup.cmds cart.v32
v32sim -L lua -r cart.v32           # muestra valores de Lua
v32sim -q --fast cart.v32 < t.cmd   # ejecución con guion, ver Guiones
```

Por defecto el simulador carga `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32`
(la ubicación de instalación estándar de ComputerSoftware de Vircon32); se cambia con `--biosfile`.
La ejecución empieza en la primera instrucción de la BIOS (`0x10000004`) salvo que `--entry-point` indique otra cosa.

## Mapa de memoria

| Página | Primera dirección | Notas |
|--------|-------------------|-------|
| RAM  | `0x00000000` | 16 MB / 4M palabras, lectura y escritura |
| BIOS | `0x10000000` | punto de entrada en `0x10000004`, manejador de errores en `0x10000000` |
| CART | `0x20000000` | ROM de programa del cartucho |
| MEMC | `0x30000000` | tarjeta de memoria |

## El depurador

Cuando no se inicia con `--run`, el simulador se detiene en la primera
instrucción y presenta el indicador `v32sim>`. Desde aquí puedes avanzar paso a
paso (`step`), saltar subrutinas (`next`), ejecutar libremente (`continue`) y
disponer que la ejecución vuelva a detenerse en puntos de ruptura, puntos de
vigilancia o al encontrar un opcode vigilado.

- **Ctrl-C (SIGINT)** saca del modo de ejecución y vuelve al indicador
- **EOF en stdin** (p. ej. Ctrl-D, o una tubería de entrada cerrada) termina
  limpiamente; útil al manejar v32sim desde guiones
- Ante un **error de sistema**, el estado de la máquina se guarda en R0-R3, SP/BP
  se borran, la ejecución se redirige al manejador de errores de la BIOS en
  `0x10000000` y el simulador se detiene en el indicador para que puedas
  inspeccionar el fallo

### Archivos de órdenes

`--command-file=ARCHIVO` (o `-C`) entrega al simulador un archivo de texto plano
con **una orden válida del indicador por línea** (cualquier orden: puntos de
ruptura, preparación de la lista de visualización, `continue`, etc.). Las líneas
en blanco y las líneas (o finales de línea) que empiezan por `#` son comentarios.
Esta es la forma principal de automatizar sesiones; combínala con `--run` y con
stdin/stdout redirigidos para pruebas con guion.

## Órdenes

Las órdenes no distinguen mayúsculas de minúsculas y pueden abreviarse con un prefijo de una longitud
mínima que `src/command.c` fija para cada una (`s` = step, `n` = next, `c` = continue,
`b` = break, `p` = print, `x` = x, `se` = set, `bt` = backtrace, `pro` = profile, `in` = inventory,
`lo` = load, ...). Una línea en blanco repite el último step/next/continue. Las letras sueltas son `b` break, `c` continue,
`d` display, `g` gamepad, `h` help, `i` ignore, `l` label, `n` next, `p` print, `q` quit, `r` replace, `s` step, `w` watch,
`x` x y `?` help. Cuando varias órdenes empiezan por las mismas letras gana la primera de la tabla de `src/command.c`, así que `r` es
`replace`, no `run` (`ru` es `run`); en los guiones, escriba el nombre entero ante la duda.

| Orden      | Descripción |
| ---------- | ----------- |
| `break [0xMEM\|ETIQUETA\|ARCHIVO:LÍNEA\|LÍNEA]` | pone un punto de ruptura de ejecución (dirección, etiqueta, función o línea de código; sin argumento: los lista) |
| `tbreak ...` | lo mismo, y se elimina al alcanzarlo (sin argumento: lista los puntos de ruptura, como `break`) |
| `unbreak #` | elimina un punto de ruptura por índice |
| `continue` | reanuda la ejecución hasta el siguiente disparador |
| `step` | ejecuta la instrucción actual y se detiene en la siguiente; en los modos de código fuente (`set mode = src\|C\|lua`) avanza hasta la siguiente línea de código, entrando en las llamadas |
| `next` | salta las subrutinas; en los modos de código fuente avanza hasta la siguiente línea de código, y las llamadas se ejecutan hasta su retorno |
| `finish` | ejecuta hasta que retorna la función del marco seleccionado |
| `until [ARCHIVO:LÍNEA\|LÍNEA]` | avanza hasta una línea posterior de la función (sale de un bucle), o hasta esa línea |
| `list [ARCHIVO:LÍNEA\|LÍNEA\|FUNCIÓN]` | muestra el código fuente alrededor de la línea actual (o de esa) |
| `frame [N]` / `up [N]` / `down [N]` | selecciona un marco de pila |
| `print/fmt XYZ` | muestra una vez XYZ (registro, memoria, IOPort, símbolo) |
| `x/fmt SÍMBOLO\|0xADDR [CANTIDAD]` | muestra CANTIDAD palabras desde un símbolo (por defecto: toda la variable de C) |
| `display/fmt XYZ [ETIQUETA]` | añade un elemento a la lista de visualización que se muestra en cada parada |
| `undisplay #` | elimina un elemento de la lista de visualización por índice |
| `label [0xMEM_ADDR ETIQUETA]` | lista o fija una etiqueta para un desplazamiento de memoria |
| `unlabel #` | elimina una etiqueta por índice |
| `watch [LOC OP VALOR [ETIQUETA]]` | pone un punto de vigilancia sobre un registro, IOPort o palabra de memoria (sin argumentos: los lista) |
| `watchlist` | lista los puntos de vigilancia activos |
| `run N` / `wait N` | continúa durante N cuadros y luego se detiene |
| `until "TEXTO" [timeout N] [rows N]` | continúa hasta que la consola de texto muestre TEXTO |
| `expect [print] LOC OP VALOR` / `expect console "TEXTO"` | comprueba un valor o la pantalla; un fallo fija el estado de salida 1 |
| `console [ARCHIVO]` / `console --attrs[=fg\|bg\|all] [ARCHIVO]` | muestra o escribe la pantalla de texto (con cuadrículas de color) |
| `unwatch #\|ETIQUETA` | elimina un punto de vigilancia |
| `backtrace` (`bt`) | lista las llamadas a subrutinas, la más reciente primero; con información de líneas de Lua/C, los marcos BP como `#0 f (archivo:línea)` |
| `locals [NOMBRE]` / `info locals [NOMBRE]` | variables locales del marco seleccionado con tipos y valores (ver *Variables locales*) |
| `profile` | muestra el informe de perfilado (requiere `-p`) |
| `inventory` | resumen de los recursos del sistema |
| `gamepad [# [ORDEN]]` | ver/seleccionar/simular el estado del mando (`gamepad 1` selecciona el mando 1) |
| `kbd [attach [PUERTO]\|detach\|type TEXTO\|key TECLA [MODO]\|input ORIGEN\|off\|clear\|sync [on\|off]]` | el teclado v32kbd (ver *El teclado v32io*) |
| `mouse [attach [PUERTO]\|detach\|move DX DY\|click\|down\|up [BOTÓN]\|wait N\|clear]` | el ratón v32mouse (ver *El ratón v32io*) |
| `mailbox [CANAL]` | el buzón v32dev de la tarjeta de memoria: actividad, anillos, descriptores, datos pendientes |
| `dev [info\|format [force] [N=TYPE ...]\|attach N SPEC\|detach N\|swap N SPEC]` | el anfitrión v32dev nativo (ver *Tarjeta de memoria y v32dev*) |
| `mark [NOTA]` | con `--v32trace`: deja una marca en el informe de la sesión |
| `load cart:ruta` / `load memc:ruta` | carga un componente en tiempo de ejecución |
| `unload bios\|cart\|memc` | descarga una página de memoria |
| `replace [IP:0xADDR] [IR:0x...] [IV:0x...]` | parchea la instrucción (y el inmediato) en IP/ADDR de la memoria |
| `asm [@0xADDR\|-x] INSTRUCCIÓN` | ensambla una instrucción: la parchea (en IP o en ADDR), o la ejecuta una vez con `-x` |
| `screenshot [--plain] [ARCHIVO]` | escribe la pantalla de la GPU en un archivo PNG |
| `stats [show\|regions [N]\|reset\|on\|off\|save ARCHIVO [json]\|expect NOMBRE[.AGG] OP VALOR]` | contadores de GPU / SPU / bus (dibujos, descartados, cambios de región, palabras de MOVS...) por cuadro y por región; `expect stats NOMBRE.AGG OP VALOR` |
| `highlight [TEX:REGIÓN [COLOR]]` | perfila una región en las capturas de pantalla (`clear`, `on`, `off`; también llamada `boundbox`) |
| `screengrid [on\|off] [DX [DY]] [color C] [alpha N] [labels on\|off]` | superpone una cuadrícula a las capturas de pantalla |
| `noextra [on\|off]` | capturas de pantalla sin cuadrícula ni perfiles (`screenshot --plain` para una sola) |
| `save [ARCHIVO]` / `restore ARCHIVO` | escribe / restaura el estado completo de la máquina |
| `set [NOMBRE=VALOR]` | fija opciones, registros, memoria o IOPorts (sin argumentos: lista las opciones) |
| `ignore` | salta la instrucción actual (no se ejecuta y los contadores no avanzan) |
| `version` | muestra la versión del simulador |
| `help [ORDEN]` / `?` | ayuda de las órdenes |
| `quit [N]` | sale del simulador (con estado de salida N) |

### Sufijos de formato

`print` y `display` admiten un sufijo de formato. Los operandos numéricos son hexadecimales: hasta
3 dígitos es un IOPort, más es una dirección de memoria (`ram:0x75` es la palabra 0x75 de la RAM; `print/x 0x205`,
`print GPU_ClearColor`, `print R3`, `print reg`, `print [SP]`):

- `/X` hexadecimal en mayúsculas (por defecto), `/x` hexadecimal en minúsculas
- `/u` decimal sin signo, `/d` decimal con signo, `/o` octal, `/b` binario
- `/f` coma flotante
- `/B` booleano (TRUE/FALSE)
- `/D` decodifica el valor como una instrucción
- `/s` cadena (una palabra de 32 bits por carácter; en modo Lua, desempaqueta las
  cadenas etiquetadas)

### Puntos de vigilancia

`watch  LOCALIZACIÓN  OP  VALOR  [ETIQUETA]` se detiene cuando la comparación *pasa a ser*
verdadera. `LOCALIZACIÓN` es un registro (R0-R15, BP, SP, CR, SR, DR, IP, IR, IV), un
IOPort (un símbolo, o hexadecimal de hasta 3 dígitos) o una palabra de memoria (hexadecimal con más
dígitos); `OP` es uno de `=`, `==`, `!=`, `<`, `>`, `<=`, `>=`; `VALOR` es un
número (hexadecimal, binario, octal, decimal), un registro, un símbolo de IOPort o el
contenido de `[0xMEM]`, `[IOPORT]` o `[REG]`; la ETIQUETA es opcional:

```
watch SP <= 0x20000000 stack_underflow
watch 0x00001000 != 0
watch TIM_FrameCounter = 60
watch R1 > [0x00000010]
```

Los puntos de vigilancia se disparan por flanco: una condición que ya es verdadera cuando se fija,
o que sigue siéndolo, no vuelve a detener la ejecución hasta que haya sido falsa entretanto.
Se comprueban tras cada instrucción y siempre que cambia el estado de la máquina
(así que se ve un valor que cambia y vuelve a cambiar dentro de una misma instrucción).
Se eliminan con `unwatch #` o `unwatch ETIQUETA`; `watchlist` (o `watch` solo)
los lista. Las comparaciones son de 32 bits sin signo.

### Ensamblado en vivo

`asm` ensambla una instrucción con la sintaxis del ensamblador de Vircon32:

```
asm mov R1, 0x10                       # parchea la instrucción en el IP actual
asm @0x20000100 out GPU_Command, GPUCommand_ClearScreen
asm -x iadd R0, -1                     # se ejecuta una vez, la memoria queda intacta
```

Los operandos son registros (`R0`-`R15`, `CR`, `SR`, `DR`, `BP`, `SP`), números
(decimal, `0xHEX`, `0bBINARIO`, `0OCTAL`, flotantes), etiquetas de los archivos de
depuración cargados, nombres de IOPorts y las constantes `GPUCommand_*`, `GPUBlendingMode_*`,
`SPUCommand_*` y `SPUChannelState_*`. Un entero decimal dado a una instrucción de
coma flotante se convierte (`fadd R4, 1` suma 1.0); los hexadecimales, binarios y octales
son patrones de bits en bruto. Un parche no puede ser más largo que la instrucción que
reemplaza; uno más corto se rellena con `MOV R0, R0` (una instrucción nula). Se ha comprobado
que la salida ensamblada de cada instrucción de la BIOS coincide con su código
máquina palabra por palabra.

### Capturas de pantalla

`screenshot [ARCHIVO]` escribe el búfer de imagen de la GPU de 640x360 en un PNG (por defecto
`v32sim.SELLO.png`). El renderizador es un modelo por software de la GPU de la consola
(muestreo por vecino más próximo, el mismo presupuesto de píxeles y la misma mezcla), de modo que una captura
muestra lo que el cartucho ha dibujado hasta ahora en este cuadro.

`highlight TEXTURA:REGIÓN [COLOR]` (también escrita `boundbox`) marca una región (`bios:REGIÓN` para la textura de la BIOS; COLOR es un nombre o RRGGBB, rojo por defecto): cada
quad que el programa dibuja desde ella, con escala o rotación incluidas, recibe en las capturas un contorno de un píxel justo por fuera. Los contornos
se pintan solo en el PNG, nunca en la pantalla que ve el programa; cubren los quads dibujados en este cuadro o en el anterior (un clear_screen
olvida los anteriores), y un dibujo que la GPU descartó por falta de píxeles no tiene ninguno. `highlight` los lista, `highlight clear [TEX:REGIÓN]`
los olvida, `highlight off|on` los oculta y los muestra. Solo se perfilan los dibujos hechos después de la orden: deja que transcurra un cuadro antes de la captura.

`screengrid [on|off] [DX [DY]] [color C] [alpha PORCENTAJE]` mezcla una cuadrícula tenue en las capturas, para juzgar dónde se dibujó cada cosa: verde al
55 % cada 40 x 40 píxeles por defecto (celdas cuadradas de 16 x 9; 8, 10, 20 y 40 dividen exactamente 640 y 360; `screengrid 32 30` da celdas de 20 x 12).
Los contornos de `highlight` se dibujan encima. `noextra [on|off]` deja la cuadrícula y los contornos fuera de todas las capturas
conservando sus ajustes, y `screenshot --plain [ARCHIVO]` lo hace para una sola. La cuadrícula lleva las coordenadas de píxel en sus líneas (`screengrid labels off`
las desactiva); las líneas más próximas entre sí que una etiqueta las comparten. Nada de esto toca la pantalla que ve el programa.

### Guardar y restaurar

`save [ARCHIVO]` escribe el estado de la máquina (registros, IOPorts, RAM, tarjeta de memoria,
regiones y búfer de imagen de la GPU, SPU, temporizadores, mandos; por defecto
`v32sim.SELLO.sav`), y `restore ARCHIVO` lo vuelve a cargar. El archivo es
little-endian con las series de ceros comprimidas, y registra sumas de comprobación de la BIOS y
del cartucho: solo se restaura en una máquina que tenga las mismas. Los puntos de ruptura,
los puntos de vigilancia, la lista de visualización, las etiquetas y los datos de perfilado forman parte de la
sesión de depuración y no se guardan.

### Lista de visualización

La lista de visualización se vuelve a mostrar cada vez que el simulador se detiene. Los elementos pueden ser
registros (desreferenciables con `[...]`), direcciones o rangos de memoria, o
IOPorts (numéricos como `0x205` o simbólicos como `GPU_SelectedTexture`). Si
no se da etiqueta a un elemento de IOPort, se usa su nombre simbólico.

### Etiquetas

`label 0x10000040 main` asocia un nombre a un desplazamiento; las etiquetas pueden usarse
para puntos de ruptura (`-b main` o `break main`) y como operandos de `asm`.
Las etiquetas también se cargan automáticamente de los archivos de depuración del ensamblador y de C (ver más abajo).

### Archivos de depuración

Junto a cada archivo V32, v32sim busca sus archivos de depuración: `NAME.vbin.debug` (el del ensamblador: `v32as -g`, o el
`assemble -g` original) y `NAME.asm.debug` (el archivo auxiliar del compilador de C), luego `NAME.vbin.vars`, `NAME.vbin.sym` / `NAME.asm.sym` y `NAME.asm`
(ver *Variables locales* y *Símbolos*). Cada uno se busca en este orden: el directorio dado con `--debug-dir` (si lo hay), el directorio del archivo V32,
su `obj/`, su `../obj/`, el `obj/` del directorio actual y el directorio actual. `--bios-asm-debug` y las demás nombran un archivo
explícitamente. `-n` lo desactiva todo. Los archivos de depuración contienen una entrada `OFFSET,ARCHIVO,LÍNEA[,ETIQUETA]`
por instrucción.

Con `--source` (o `set source=true`) la línea de código de la instrucción actual
se imprime en cada parada: líneas de ensamblador por defecto, las líneas de C o Lua
con `-L src` (cualquier lenguaje; `-L C` y `-L lua` son alias que además conservan
sus antiguos efectos adicionales) o `set mode = src`. La etiqueta del lenguaje (`ASM`, `C`, `LUA`)
sale de la extensión del archivo. Los nombres de archivo en los archivos de depuración son relativos al directorio
en que se compiló el programa, así que se prueban tal cual y relativos a los
directorios donde se encontraron los archivos de depuración y V32 (y a sus padres).

### Set

```
set color=true      set deref=true       set debug=true
set verbose=true    set errorchk=true    set profile=true
set source=true     set mode=C           # superposición de código fuente, y qué código (asm, src, C o lua)
set R4=0x4004       set IP=0x10000040    # IP es un salto
set 0x00224466=0x71
set GPU_SelectedTexture=-1
```

Los booleanos admiten true/false, on/off, yes/no, 1/0 (`color` también como `colors`, `deref` como `derefaddr`, `errorchk` como `errorcheck` o `error`).
`set` sin argumentos lista los ajustes actuales. Los destinos de memoria y de IOPort se validan.

### Load/Unload

`load  cart:ruta`  y  `load   memc:ruta`  intercambian componentes en tiempo de ejecución
(sobre todo para cambiar la tarjeta de memoria). `unload bios|cart|memc` libera la
página. `load` comprueba primero que el archivo se puede abrir, y `unload` rechaza una página
que no está cargada. `load bios:ruta` reemplaza la BIOS (se recargan sus etiquetas
y se refresca la instrucción leída).

## Depuración a nivel de código fuente

Los archivos de depuración llevan el archivo de código y la línea de cada instrucción (la convención es [docs/V32DBG-SPEC.md](docs/V32DBG-SPEC.md)):
`v32as -g` escribe `DIRECCIÓN,ARCHIVO,LÍNEA[,FUNCIÓN]` a partir de las marcas `;@src ARCHIVO:LÍNEA` de un
programa en Lua (`v32lua -g`) o en C, y el archivo auxiliar del compilador de C se asocia a las direcciones
como antes. v32sim mantiene una tabla de líneas ordenada con lo que encuentre y construye sobre ella las
órdenes a nivel de línea. `set mode = asm` (el valor por defecto) mantiene `step` y `next` como pasos de instrucción. En los
modos de código fuente (`set mode = src|C|lua`, o `-L src|C|lua`) los mismos `step`, `next`, `s`, `n` y la
repetición con línea en blanco avanzan por **línea de código**; sin información de líneas cargada siguen siendo
pasos de instrucción.

| Orden | Hace |
| ----- | ---- |
| `step` (modos de código fuente) | ejecuta hasta que cambia la línea de código (otro archivo, línea o función, o un salto atrás al inicio de una línea: una nueva pasada de un bucle), **entrando** en las llamadas |
| `next` (modos de código fuente) | lo mismo, pero un `CALL` se ejecuta hasta su retorno (el puntero de pila debe volver adonde estaba: seguro con recursión; no hace falta ver ningún `RET`) |
| `finish` | ejecuta hasta que retorna el marco seleccionado; funciona también en la primera instrucción de una función (ver *Marcos de pila*) |
| `until` | como `next`, pero solo se detiene en una línea **posterior** de esta función (o cuando retorna): sale de un bucle |
| `until ARCHIVO:LÍNEA` | avanza hasta esa línea, o hasta que retorna la función actual |
| `break ARCHIVO:LÍNEA`, `break LÍNEA` | la dirección más baja de la línea; `break -a` todas ellas. Una línea sin código pasa a la siguiente que lo tenga, y lo avisa |
| `tbreak ...` | de un solo uso |
| `list`, `list LÍNEA`, `list ARCHIVO:LÍNEA`, `list FUNCIÓN` | diez líneas de código, con `=>` marcando la actual; `list` otra vez continúa |
| `bt`, `frame N`, `up`, `down` | los marcos, y el marco seleccionado (que usan `list` y `finish`) |

### La pila de depuración: C, C++ y Lua, optimizado o no

v32sim es el final de una cadena de depuración compartida por todas las herramientas de lenguaje de Vircon32:

```
source ->  v32cc -g | v32c++ -g --compiler=v32cc | v32lua -g   ->  [v32opt -O2/-O3]  ->  v32as -g  ->  v32sim
```

`tools/v32mkcart.sh [-O NIVEL] FUENTE.c|.cpp|.lua|.asm` ejecuta la cadena adecuada (define `V32TC` con el
directorio de la cadena de herramientas) y deja `NAME.vbin.debug`, `.vbin.vars` y `.asm.sym` junto al cartucho. Los números de línea
siguen el archivo que escribiste (en las macros de C/C++, la línea del *uso*, no la de la cabecera). Tras `v32opt`,
una variable que se mantuvo en un registro se sigue mediante sus registros `;@home`; aquella cuyo registro
ha desaparecido muestra `<optimized out>`. `tests/run_stack_tests.sh` comprueba todo esto en un programa en tres
lenguajes con -O0/-O2/-O3 (forma parte de `make check`). Estado por herramienta: [docs/V32DBG-STATUS.md](docs/V32DBG-STATUS.md).

`break LÍNEA` es un número **decimal** inferior a `0x10000000` cuando hay información de líneas cargada;
las direcciones se escriben en hexadecimal (`break 0x20000012`), como antes. `break FUNCIÓN` pone el
punto de ruptura en la primera instrucción de la función (no en cada línea, aunque todas las
entradas del ensamblador llevan el nombre de la función). ARCHIVO puede ser un nombre base (`bench.lua`) o cualquier final
de la ruta.

Cada parada imprime una línea, `[step] bench.lua:4 in add:   local s = a + b` (a stderr en
modo `--batch`, como los demás motivos de parada; `bt`, `list` y `frame` imprimen en stdout).

### Marcos de pila

Tanto v32cc como v32lua usan marcos `PUSH BP; MOV BP,SP`, de modo que tras el prólogo la dirección
de retorno está en `[BP+1]` y el BP del llamador en `[BP]`. El marco de la función en ejecución se
reconoce por sus instrucciones, así que también es correcto en su primera instrucción (aún no se ha
apilado nada), tras `PUSH BP` y en `POP BP; RET`. Se supone que el código sin código fuente (ver más abajo)
no tiene marco y que se entra en él con un `CALL`: la dirección de retorno está en la cima de la pila, si
la palabra que hay allí es la dirección posterior a un `CALL`. El recorrido termina en una dirección que no es código,
o en un BP ilegible o no creciente.

`backtrace` usa estos marcos cuando los datos de depuración son de Lua o C (un archivo que no es
`.asm`); para los programas en ensamblador sigue listando los destinos de `CALL`, como siempre.
El código compilado con `-O2` y `omit-frame-pointers` no tiene cadena: `next` y `until` no
la necesitan (usan la instrucción `CALL` y SP), `finish` y `bt` sí.

### Variables locales

`locals [NOMBRE]` (también `info locals`) lista las variables visibles en el marco seleccionado (`frame`, `up`, `down`) con
sus tipos y valores; `print NOMBRE`, `x NOMBRE`, `display`, `watch` y `expect` encuentran una local de ese marco antes que una global del
mismo nombre (su dirección queda fija cuando se da la orden). La información está en `NAME.vbin.vars`, que `v32as -g` escribe
a partir de las líneas `;@var` / `;@scope` de `v32cc -g` y `v32lua -g`, y se busca igual que los demás archivos de depuración:

```
v32sim> locals
a = 3    (int, BP+2)
arr = {3, 4, 7, 12}    (int[4], BP-4)
p = {0x00000003, 0x00000005}    (struct:pt:2, BP-8)
q = 0x003FFFE2    (ptr, BP-9)
i = 99    (int, BP-10)
```

Tipos: `int uint float bool ptr`, matrices `T[N]` (se muestran 16 elementos), `struct:`/`union:` (miembros por nombre, `p = {x = 3, y = 5}`, cuando el compilador los escribe en `type=`; si no, como palabras en bruto), `lua` (valores
con NaN-boxing: números, cadenas, nil/true/false, tablas, funciones; una local `boxed`, capturada por una clausura, se lee a través de su
caja). Una local que el optimizador movió a un registro (`loc=R5`) se muestra solo en el marco 0; `-`/`optimized-out` lo indican. De dos
variables con el mismo nombre gana la del bloque más interno. Una variable se muestra solo en la función en que el archivo de depuración sitúa la
instrucción, porque v32as termina los rangos de una función de C al final del programa cuando v32cc no escribe `;@fn` (sin
`--emit-hints`).

### Código sin fuente

Una entrada cuyo archivo es `-` (o cuya línea es 0), p. ej. `0x20000060,-,0`, indica que las
instrucciones desde esa dirección hasta la siguiente entrada no tienen código fuente: una biblioteca de tiempo de ejecución. Es
lo que significa `;@src -`. `step` atraviesa ese código sin detenerse, `bt` muestra
`?? [0x...]` y `finish` retorna desde él. `v32as -g` escribe una entrada así por cada rango `;@src -`;
un archivo de depuración sin ellas (un `v32as` más antiguo) hace que la última línea anterior a la biblioteca se extienda hasta la siguiente entrada.

## Tarjeta de memoria y v32dev

Un archivo de tarjeta de memoria es `V32-MEMC` seguido de 262144 palabras little-endian. El
simulador lo mantiene sincronizado con la memoria de la tarjeta como hace el DesktopEmulator:
cada escritura marca su página de 256 palabras, y las páginas sucias se escriben al
final de cada cuadro.

**v32dev** (el transporte anfitrión-dispositivo del proyecto v32io) usa las últimas 16384
palabras de la tarjeta como un buzón con cuatro canales. El lado del anfitrión es un proceso
aparte (`v32dev.py serve CARD --ch1 storage:disk.img`) que lee y escribe el *archivo* de la
tarjeta mientras se ejecuta el programa. Para que eso funcione, el simulador hace el papel del
emulador:

  * la **cabecera** (indicador de actividad y latido, palabras 8 y 9 de la región)
    se mantiene solo en memoria y nunca se escribe en el archivo
  * el **área del anfitrión** se vuelve a leer del archivo al comienzo de cada
    cuadro, y nunca se escribe; el programa no puede pisar lo que escribió el anfitrión
  * de las partes **del cartucho**, las 16 palabras de control de cada canal (los índices
    del anillo) se escriben después de todo lo demás, de modo que un anfitrión que ve un índice nuevo
    ve también los datos que anuncia
  * una tarjeta sin cabecera válida (o con `V32DEV_DISABLE` definida como cualquier valor salvo `0`) es una
    tarjeta ordinaria; nada de esto se aplica

`mailbox [CANAL]` muestra todo el estado: actividad, latidos, índices de anillo,
descriptores y datos pendientes. `restore` deja intactos la cabecera y el área del
anfitrión de un buzón activo.

El conjunto de pruebas con cartuchos reales de v32dev del proyecto v32io (`dev_rom_test.py`)
pasa sin cambios contra v32sim: ver *Pruebas*.

**`--v32dev-doorbell[=MS]`** (desactivada por defecto; P-E2 de v32nix, interfaz acordada con v32nix el 2026-11-06): con ella activada, la palabra 8 de la cabecera
(banderas del emulador, solo en memoria) recibe el bit 1 = «timbre admitido» (el bit 0 como antes), y las palabras 10..13 de la cabecera (`0x3003C000 + 10 + c`) cuentan, por
canal, los mensajes del anfitrión al cartucho que se han hecho visibles: +1 cada vez que una importación de la parte del anfitrión encuentra que `h2c_head` se ha movido. Un almacenamiento del programa que
cambia el `c2h_head` de una parte del cartucho (`0x3003E020 + c*0x7F8 + 1`) es el timbre: se escriben las páginas sucias (datos, luego palabras de confirmación), un anfitrión
que está activo dispone de hasta MS milisegundos de reloj de pared (por defecto 2; 0 = sin espera) para responder, se vuelve a leer la parte del anfitrión del canal y la CPU
continúa sin cargar ciclos. Un anfitrión que responde más tarde se importa en el límite del cuadro, donde el contador se mueve entonces: el código debe funcionar
con la respuesta tanto temprana como en el límite. La espera es de reloj de pared, así que una ejecución con un anfitrión externo que responde no es repetible ciclo a ciclo:
deja la opción desactivada en las pruebas diferenciales (desactivada, la cabecera, la palabra de banderas y la temporización son idénticas bit a bit a las de antes).
`mailbox` muestra anillos, esperas y respuestas tempranas. Prueba: `tests/examples/bell.c` con `tests/doorbell_host.py` (sección del banco de pruebas
«--v32dev-doorbell», necesita `V32IO=` el proyecto v32io para `v32dev.py format`). El P-E1 de v32nix (interrupción del temporizador) no está construido: v32nix
lo rechazó (la expropiación D30/D37 funciona sin él; registrado en `docs/PROPOSALS-STATUS.md`).

**`--v32dev-host=N=SPEC`** (repetible; opcional; VS-19): un **anfitrión v32dev nativo dentro del simulador**, para dispositivos de almacenamiento y serie, con el
comportamiento de `tools/v32dev.py` del proyecto v32io (la referencia; el anfitrión en Python y el parche del DesktopEmulator permanecen en v32io, esta es la
tercera implementación). Trabaja directamente sobre la memoria de la tarjeta: sin ida y vuelta por el archivo de la tarjeta, sin segundo proceso, sin reloj de pared. El canal N (0..3) se atiende
desde el primer cuadro; la tarjeta (`-M ARCHIVO`) debe ser una tarjeta v32dev escribible, o una cuyos últimos 64 KiB sean todo ceros (entonces se formatea, como hace
`v32dev.py format`, con la cabecera escrita la última); si hay otros datos ahí se niega, salvo que se dé `--v32dev-host-force`. Con `V32DEV_DISABLE` definida
(con cualquier valor salvo `0`) la tarjeta sigue siendo ordinaria y el anfitrión nativo no se inicia, con un aviso. SPEC, con la misma sintaxis que `v32dev.py serve` (las opciones de
`storage:` se leen de derecha a izquierda, como en el servidor del DesktopEmulator, así que un nombre de archivo puede contener comas):

  * `storage:ARCHIVO[,bytes][,ro][,sector=N][,create=SECTORES]` un archivo como dispositivo de bloques (modo de palabra: 4 bytes por palabra, sectores de 128 palabras; `bytes`: un byte
    por palabra, sectores de 512 palabras; operaciones READ/WRITE/FLUSH, estado OK/EINVAL/ERANGE/EROFS/EIO, descriptor `[sector_words, sectors, flags, max_sectors]`)
  * `serial:stdio` la salida del programa a la salida estándar (8 bits bajos); la entrada desde la entrada estándar al ejecutar sin supervisión (`-r`; fin de la entrada = colgado)
  * `serial:tcp=PORT` un puerto TCP de este ordenador solamente (127.0.0.1): un cliente a la vez (uno nuevo sustituye al anterior), la salida mientras nadie está conectado se descarta, y el programa ve un colgado cuando el cliente se va, hasta que se conecta el siguiente (no admite otra opción que `out=ARCHIVO`)
  * `serial:echo[,hup=N]` lo que escribe el programa vuelve; cuelga tras N palabras
  * extensiones de v32sim para el puerto serie: `in=ARCHIVO` (se entrega al inicio y en cada reapertura del canal), `out=ARCHIVO` (salida del programa, 8 bits bajos), `hangup` (cuelga cuando
    se han entregado los datos de `in=`); se combinan con las demás

El anfitrión se ejecuta en cada límite de cuadro (un latido por cuadro, luego cada canal hasta que no queda trabajo). Con `--v32dev-doorbell` un anillo se
responde **de inmediato** (T = 0, sin vaciado, sin espera, sin reloj de pared), de modo que una ejecución con anfitrión nativo es repetible (las pruebas comparan dos tarjetas byte a
byte). El número de sesión es un contador, no la hora. `mailbox [N]` muestra los contadores del anfitrión nativo por canal. **`save` / `restore` conservan el anfitrión nativo**: el archivo de estado termina con las
colas, los contadores y el número de sesión del anfitrión, y una restauración devuelve toda la región del buzón, lado del anfitrión incluido, de modo que una ejecución partida en dos (guardar, nuevo
proceso, restaurar, continuar) es idéntica a la ejecución completa, para almacenamiento y para puerto serie, con y sin timbre (comprobado en cada cuadro de la
conversación). Un archivo de estado sin ese bloque, o guardado con otros tipos de dispositivo nativo, se restaura como antes (se conserva el lado del anfitrión actual, un
mensaje recibido a medias se descarta, con un aviso en el segundo caso). La imagen de disco es un archivo ajeno al estado: restaura con la imagen tal como estaba (se avisa si el número de sectores es
distinto). **Orden `dev`** (`help dev`): `dev [info]` (las líneas de `v32dev.py info`), `dev format [force]` (el
`v32dev.py format` integrado: la tarjeta pasa a ser un buzón ahora, con los dispositivos configurados anotados), `dev attach N SPEC`, `dev detach N`, `dev swap N SPEC` en tiempo de
ejecución (el programa ve desaparecer al anfitrión, y una sesión nueva cuando vuelve, como un medio cambiado); en `--batch` una orden `dev` que falla termina la ejecución con
estado 4. Prueba: `tests/native_host_test.py` (la ejecuta `tests/run_tests.sh`) ejecuta los cartuchos reales de v32io (`storage_test`, `serial_test`) contra él y contra `v32dev.py serve`: mismo informe,
misma imagen.

**`V32DEV_SERVER` y `V32DEV_CH0`..`V32DEV_CH3`** (el servidor v32dev integrado del DesktopEmulator del proyecto v32io, `emulator/v32dev-server`: las mismas
variables con el mismo significado, de modo que una misma línea de órdenes y un mismo entorno sirven para ambos emuladores): con `V32DEV_SERVER` definida con cualquier valor salvo `0`, v32sim atiende la
tarjeta cargada con los dispositivos de `V32DEV_CH0` a `V32DEV_CH3` (una SPEC cada una, como arriba; una variable vacía o sin definir deja su canal sin atender, una incorrecta
se informa y se deja fuera). Sin definir o con `0`, las variables no se miran. La tarjeta **no necesita formato** y **ningún buzón llega nunca al archivo de la tarjeta**: si los
últimos 64 KiB no contienen nada, la cabecera se pone ahí solo en la memoria (el programa ve un buzón activo), toda la región queda fuera de cada guardado, y un programa
que se cae no deja nada. Una tarjeta que tiene datos en esos 64 KiB se deja en paz y no se atiende (un aviso lo dice); `V32DEV_DISABLE` mantiene la tarjeta
ordinaria; una tarjeta formateada con `v32dev.py format` también funciona (el servidor ocupa el lugar del anfitrión, y el área de anfitrión del archivo no se lee). Una tarjeta ausente no es un
error: no hay nada que atender. `--v32dev-host=N=SPEC` sigue funcionando y gana a `V32DEV_CHN` en su canal; con `V32DEV_SERVER` activada, también mantiene el
buzón fuera del archivo, y sin ella (o con `V32DEV_SERVER=0`) formatea el archivo de la tarjeta como antes. En modo servidor nada es fatal (un dispositivo que no se puede
abrir, un puerto ocupado: el canal queda sin atender, con un mensaje), `dev format` no tiene nada que hacer, y `dev attach|detach|swap` cambian canales durante la ejecución.
No se ha tomado del emulador, porque v32sim no tiene menú ni archivo de configuración: las entradas de menú (modo de bytes, solo lectura, imagen nueva), el archivo de configuración, el
puerto TCP por defecto `3200 + N` (dé `serial:tcp=PORT`) y `serial:echo` solo por su nombre. **`dev format [force] [N=TYPE ...]`** también admite los tipos de canal de `v32dev.py format --chN`
(`none storage block serial char pipe tcp udp netplay socket`, o un número), y `dev info` nombra cada tipo como hace `v32dev.py info`. Prueba: `tests/run_v32dev_server_tests.sh`
(sin cadena de herramientas; 84 comprobaciones) y, con el proyecto v32io, sus escenarios integrados de `dev_rom_test.py` mediante `tests/v32sim_runner.sh`.

## El ratón v32io

El ratón, `v32mouse`, son dos contadores de 12 posiciones (X en Left/Right/X/Y, Y en Up/Down/L/R: un trit y un par con código Gray, de modo que cambia un
control por paso) y tres botones (Start = central, A = izquierdo, B = derecho). Un programa lee los contadores una vez por cuadro, así que el
dispositivo envía como mucho 5 pasos por eje y cuadro; un movimiento más largo lleva varios cuadros. v32sim emula uno en cualquier puerto salvo el del teclado:

    v32sim --v32mouse=1 -B bios.v32 cart.v32            (luego se maneja desde el indicador o desde un guion --batch)

`mouse attach [PUERTO]` (por defecto 1), `mouse detach`, `mouse move DX DY`, `mouse click|down|up [left|right|middle]` (un clic es pulsar, dos
cuadros, soltar), `mouse wait N` (cuadros), `mouse clear`, `mouse` (estado: contadores, botones, cola). Las acciones esperan en una cola de 4096 entradas y
se entregan una por cuadro, en orden, antes de que se ejecute el cuadro. Restaurar un estado guardado olvida las acciones en cola y toma los contadores del
mando restaurado. `tests/vectors/mouse_*.vec` (del proyecto v32io) se comprueban cuadro a cuadro con
`tests/run_mouse_vectors.py`, que ejecuta `run_harness_tests.sh`.

## El teclado v32io

Los dispositivos v32io se ven como un mando para el programa. El teclado, `v32kbd`, usa
Left/Right como estroboscopio, Up/Down para pulsar/soltar, y Start A B X Y L R como un
código de tecla de 7 bits, un evento de tecla por cuadro. v32sim emula uno en cualquier puerto:

    v32sim --v32kbd=1 --kbd-input=stdin -B bios.v32 cart.v32
    v32sim --kbd-input=/tmp/keys cart.v32            (una FIFO, tubería o archivo)

o desde el indicador: `kbd attach 1`, `kbd type "ls -l\n"`, `kbd key ctrl+c`,
`kbd key f1 down`, `kbd input ARCHIVO`, `kbd sync on|off`, `kbd` (estado), `kbd clear`.

El texto se convierte en eventos de tecla como lo produce un teclado estadounidense (shift se mantiene
pulsado a lo largo de los caracteres consecutivos que lo necesitan); los caracteres UTF-8 que tiene la fuente
de la BIOS (acentos, dibujo de cuadros...) se envían como *caracteres tecleados* de v32io. Un terminal
usado como entrada se pone en modo en bruto mientras se ejecuta el programa: se traducen las teclas de flecha, Supr,
F1-F12 y las combinaciones con Ctrl y Alt, y **Ctrl-]** vuelve al indicador del
depurador (que recupera el terminal). El texto que llega más rápido que un
evento por cuadro espera; no se pierde nada.

**Lo que ve el programa (y cuándo pierde una tecla).** Como en el adaptador real y en el DesktopEmulator, se entrega un evento de tecla
por cuadro, *al comienzo del cuadro, mire o no el programa el mando*. Los controles conservan solo el
último evento: el estroboscopio (Left/Right) cambia en cada evento, de modo que un programa que no ha leído el mando entre dos eventos no
ve nunca el primero, y con un número par de eventos entre medias no ve ningún cambio. Teclear `cat` en un programa que
está ocupado unos cuadros (una ida y vuelta de v32dev, un cálculo largo, un programa que sondea con menos frecuencia que una vez por cuadro) da
`at`, `t` o nada: cada tecla es un evento de pulsación y un evento de liberación, dos cuadros. v32sim no pone en cola para el programa por
su cuenta, porque la consola real tampoco lo hace. Un invitado debe leer su v32kbd **en cada cuadro** (lo hace el `v32kbd_probe()` de la
biblioteca), también mientras espera otra cosa, o el guion no debe teclear hasta que el programa esté listo (`until` su
indicador). Dos ayudas:

  * `kbd` muestra `replaced before the program read them: N` (eventos entregados cuando el anterior no se había leído: esos
    se pierden), de modo que una prueba puede distinguir una tecla perdida de una errata al teclear. Solo observa.
  * `--kbd-sync` / `kbd sync on|off` (desactivado por defecto): el simulador retiene cada evento hasta que el programa ha ejecutado un `IN` sobre
    un control del mando del teclado desde el anterior, de modo que no se reemplaza nada; un programa que lee en cada cuadro recibe el
    mismo evento por cuadro que antes. Un programa que nunca lee no recibe más eventos (esperan en la cola; `kbd` muestra los
    cuadros retenidos). La ejecución ya no es lo que haría el teclado real mientras el programa está ocupado, así que úsalo para averiguar si
    una tecla perdida es la causa, o en guiones, no en ejecuciones diferenciales sensibles a la temporización.

`tests/kbd_probe.py` escribe el cartucho de sondeo (ocupado durante N cuadros, luego registra los eventos que ve) y
`tests/run_kbd_sync_tests.sh` comprueba todo esto sin cadena de herramientas.

## La consola de texto

Los caracteres dibujados con la textura de la BIOS (textura -1, región = código de carácter) se
guardan también como texto. Se reconocen dos disposiciones:

  * la **fuente de la BIOS** tal como la dispone `print_at()`: celdas de 10x20 píxeles, 64x18
  * el **terminal de v32c++** (`libc/v32term.h`): 80x24 celdas de 8x15 píxeles, dibujadas
    ligeramente escaladas a partir de regiones que la biblioteca define sobre la textura de la BIOS; el dibujo de
    «papel» de una celda la deja en blanco y el dibujo del glifo que sigue fija su carácter

Se muestra la disposición dibujada en último lugar. `console` la imprime (UTF-8; los caracteres de dibujo de cuadros
de la BIOS pasan a ser Unicode), `--console` la imprime al terminar la ejecución, y
ClearScreen la vacía.

**Colores.** Se conserva el color multiplicador de la GPU de cada dibujo: el color del glifo es el
primer plano de la celda y, en el terminal de 80x24, su dibujo de «papel» es el fondo
(así que funciona el vídeo inverso). Se muestran con secuencias de color ANSI simples, sin necesidad
de una biblioteca curses, reducidas al nivel que elijas: `none`, `16`, `256` o
`true` (24 bits), con el color más próximo por distancia RGB. `--console-color=MODO` o
`console color MODO` lo fija; `auto` sigue `NO_COLOR`, `COLORTERM` y `TERM`.
Sin fijar, `--tty` usa auto y el volcado de `console` es texto sin formato (así puede ir a un
archivo). El texto blanco y el fondo negro son los valores por defecto del propio terminal.

**Cursor.** Un programa dibuja su cursor él mismo, normalmente el bloque sólido (carácter 20
de la BIOS). `--cursor-color=COLOR` / `console cursor COLOR [CÓDIGO...]` cambia el color de
esas celdas (un nombre, `#RRGGBB` u `off`); se pueden listar más códigos de carácter, como el contorno
de una caja hueca, después del color.

### Modo interactivo

    v32sim --tty -B bios.v32 v32nix.v32            teclado en el mando 0
    v32sim --tty --v32kbd=1 -B bios.v32 cart.v32   teclado en el mando 1

`--tty` ejecuta el programa con la consola de texto en tu terminal (pantalla alternativa,
repintada cuando cambia el texto) y tus teclas yendo a un v32kbd emulado, de modo que un
programa de texto como v32nix o V32DOS se puede usar ahí mismo. Necesita un
terminal en stdin y stdout. **Ctrl-]** pasa al indicador del depurador
(se devuelve el terminal; la última pantalla queda como texto en el historial de desplazamiento);
`continue` vuelve al programa. Ctrl-C y las demás teclas de control van al
programa, no a v32sim. El programa debe leer un v32kbd en ese puerto de mando
(biblioteca `v32kbd_init` / `v32io`); uno que lea el teclado en pantalla de la
fuente de teclas por defecto de v32term no verá tus teclas.

## Ejecución sin supervisión

  * `--fast` / `--frame-us=N`: no duerme para mantener 60 cuadros por segundo (o
    espera N microsegundos por cuadro). `--max-frames=N` se detiene tras N cuadros, contados desde el encendido (el logotipo de la BIOS, unos 250 cuadros, incluido).
  * `-x` / `--exit-status`: el estado de salida indica cómo terminó la ejecución: 0 el
    programa se detuvo, 2 se alcanzó el límite de cuadros, 3 un error de sistema (la ejecución
    se detiene ahí en lugar de entrar en el manejador de errores de la BIOS).
  * Las órdenes de `-C ARCHIVO` las lee el indicador (después de leer la primera instrucción),
    así que `step`, `next` y `continue` actúan; con `-r` se ejecutan una vez al inicio.
  * `--dump=N[@ADDR|@ETIQUETA]` (o `ADDR:N`): al terminar la ejecución, imprime `N` palabras de memoria como
    enteros con signo, una por línea, antes del banner final de registros. `ADDR` es
    hexadecimal (por defecto: el inicio de la RAM); una etiqueta se busca tal cual y luego como
    `global_ETIQUETA`, de modo que una global `counter` de v32cc se puede volcar como
    `--dump=1@counter` cuando se carga su archivo de depuración. Junto con `-r -x --fast
    --max-frames=N` esto es un modo sin interfaz de «ejecutar hasta HLT y volcar la RAM», como
    `vrun -d N` (v32orun --compat=vrun, usado por v32cc).
  * `--fixed-clock[=AÑO,DÍA,SEGUNDOS]` y `--seed=N` hacen reproducibles las ejecuciones: los puertos de
    fecha/hora empiezan desde un valor fijo en lugar del reloj del anfitrión, y el
    RNG se inicializa al arrancar. Dos ejecuciones del mismo cartucho dan entonces volcados
    de RAM idénticos (un programa que se inicializa a partir de `TIM_CycleCounter` sigue difiriendo
    entre compilaciones de distinta velocidad, porque el número de ciclos difiere).

### Estadísticas de cuadros

`--frame-stats=ARCHIVO` escribe una fila CSV por cuadro, `frame,cycles,cpu_pct,gpu_pixels,gpu_pct,end` (ciclos frente al presupuesto de 250.000,
píxeles de GPU frente a la capacidad de 9 pantallas; `end` es `w` para un cuadro terminado por `WAIT`, `b` cuando se agotó el presupuesto), e
imprime un resumen (media, percentil 95, máximo, cuadros al 100 %) en stderr al terminar la ejecución. `--frame-stats-skip=N`
deja fuera del resumen los primeros N cuadros (el logotipo de la BIOS ocupa unos 250). Tras esas columnas vienen los contadores de GPU / SPU
(`draws,dropped,clears,tex_switches,region_switches,regions,spu_cmds,spu_plays`, luego `scaled_draws,rotated_draws,region_defs,color_changes,
blend_changes,spu_assigns,spu_params,movs_words,sets_words,gamepad_reads,instructions,memcard_accesses,spu_playing`), y el resumen los lista (media, p50, p95, p99,
máximo, total) junto con las regiones a las que se cargaron más píxeles. `--stats-format=json` escribe JSON Lines en su lugar (un objeto por cuadro con los nombres
estables `gpu.draws`, `cpu.cycles`, ..., y una línea `{"type":"summary"...}` al final); `--stats`, `--stats-skip` y `--stats-top` son las mismas
opciones que las de `--frame-stats`. En una sesión `stats` muestra los contadores, `expect stats gpu.draws.p95 <= 600` (o
`stats expect ...`) comprueba uno (`.last .mean .min .max .p50 .p95 .p99 .total`), `stats save ARCHIVO [json]` escribe cada cuadro. Solo lee el estado; los números son iguales a
los de `v32prof`, cuadro por cuadro. Ver [docs/PROFILING.md](docs/PROFILING.md).

### Perfil de direcciones

`--profile-out=ARCHIVO` escribe, al terminar la ejecución, cuántas veces se ejecutó la instrucción de cada
dirección. No necesita indicador, ni `-p`, ni archivos de depuración, y funciona con
`-r -x --max-frames=N` (una ejecución detenida por el límite de cuadros también lo escribe) igual que con una
ejecución que llega a `HLT`:

```
# v32sim address profile v1
# cycles 403 frames 0
0x00000004 100
0x00000006 100
```

  * Una línea por dirección ejecutada (BIOS incluida), ordenada por dirección; la dirección es la
    de la primera palabra de la instrucción. `# cycles` es el número de instrucciones ejecutadas
    (un `WAIT` cuenta una vez), de modo que los recuentos suman esa cifra. Con un archivo de depuración asm del
    cartucho cargado, una tercera columna nombra la etiqueta que cubre la dirección.
  * `--profile-format=bin` escribe la línea `V32PROF1` y luego pares `dirección, cuenta`
    de 32 bits little-endian.
  * `--profile-top=N` imprime en stderr las N direcciones más calientes (con etiquetas, cuando se conocen).
  * `--profile-by=function` suma los recuentos sobre las direcciones de cada función (la
    etiqueta que las cubre en los archivos de depuración cargados; las direcciones de la BIOS sin etiqueta
    forman un solo grupo): `--profile-top=N` lista entonces las N funciones más calientes con su
    proporción y el número de direcciones, y el `--profile-out` de texto tiene una línea por
    función (`# v32sim function profile v1`, luego `0xPRIMERA CUENTA NDIR NOMBRE`). Carga
    el mapa de C (`-L C`, o el `.vbin.debug` hecho con `v32as --map`) para obtener nombres de C;
    sin etiquetas la salida lo dice. El formato binario no cambia.
  * `--fixed-cycles` hace que las lecturas de `TIM_CycleCounter` de un programa devuelvan 0, de modo que el código que
    se inicializa a partir de ahí se comporta igual entre compilaciones; con `--fixed-clock` y `--seed` el
    perfil es reproducible, byte a byte.

### V32TRACE (trampas y eventos de un cartucho traducido)

`--v32trace=PREFIJO` convierte la ejecución en un registrador V32TRACE (especificación versión 2.1, [docs/V32TRACE-SPEC.md](docs/V32TRACE-SPEC.md)). Un traductor
(NEStoV32, GBtoV32, ...) escribe `<cart>.v32trace.meta` y `<cart>.labels.txt` junto al cartucho; v32sim los lee (o
`--v32trace-meta=ARCHIVO`, `--v32trace-labels=ARCHIVO`), vigila las rutinas de trampa y los eventos que nombran, y escribe `PREFIJO.traps.txt` (una
línea TRAP/EVENT por cada trampa o evento distinto, con el estado capturado por el traductor, los últimos bloques de invitado a los que se entró y, desde la 2.1,
`segstart=` e `input=`: el historial del mando 1 desde el último reinicio como un guion `V32_INPUT`) y `PREFIJO.blocks.txt` (los bloques de
invitado que se ejecutaron, con recuentos de instrucciones) cada vez que se encuentra uno nuevo y al terminar la ejecución. Una herramienta del traductor para rellenar huecos lee
esos archivos, de modo que un ciclo de compilar-ejecutar-corregir no necesita ventana de emulador (el `make test-trace` de GBtoV32 se ejecuta contra este simulador: todo pasa).

Tras una trampa el cartucho se reinicia 60 cuadros después (registros y pila restablecidos, salto al inicio del cartucho o a la etiqueta `resume` del archivo meta;
se conservan RAM, GPU y SPU) de modo que una sola ejecución pueda encontrar varios huecos; la misma trampa tres veces seguidas detiene esto.
`--v32trace-norestart` mantiene el cartucho en la trampa. Un archivo meta hecho para otra compilación del cartucho (difieren `words` o `fnv1a`)
se ignora con una nota en stderr.

v32sim escribe además el informe legible del registrador, `PREFIJO.trace.txt` (especificación 4.3), con la disposición del registrador del DesktopEmulator: `[events]`
(nuevos aciertos, reinicios, rendición, errores de la CPU), `[input]` (el mando 1 de la sesión como una línea `V32_INPUT=`, con los cuadros de reinicio),
`[perf]` (medias de carga de CPU, tramos de cuadros descartados, los cuadros más costosos, una línea de tiempo), `[host]` (quads de GPU y vaciados de dibujo por
cuadro, el modelo de coste de anfitrión del registrador y cada `series NOMBRE ADDR` que nombra el archivo meta), `[hot spots, whole session]` (las
etiquetas y bloques de invitado principales) y `[markers]`. La orden `mark [NOTA]` fija una marca (la marca de tecla F del registrador) en el cuadro actual,
con las capturas simples del archivo meta en su nota; el informe divide entonces sus números por los segmentos entre marcas. El
`trace2input.py --session` de GBtoV32 lo lee, y `--input=@PREFIJO.trace.txt` reproduce su línea `V32_INPUT=`. Comprobado línea a línea contra
el registrador real (`tools/oracle/v32trace-ref`, más abajo) con la ROM de huecos de GBtoV32 en tres escenarios (entrada con dos marcas, una
trampa repetida hasta la rendición, `--v32trace-norestart`): `traps.txt`, `blocks.txt` y `trace.txt` son iguales, salvo el comentario de reproducción y
el orden de los puntos calientes con recuentos iguales (`run_harness_tests.sh`). `input2=` (mando 2) se escribe después de `input=` en cada línea, y `V32_INPUT2=` en el informe, solo cuando el segundo mando mantuvo un botón pulsado
(así una traza de un jugador sigue siendo igual a la del registrador); `--input2=GUION` / `--input2=@PREFIJO.trace.txt` la reproduce.

Para volver a un acierto, da su `input=` a `--input` (más abajo): el mando se reproduce por cuadro del cartucho y la misma trampa
vuelve en el mismo `cartframe`. Comprobado contra la lógica de consola original con la ROM de huecos de GBtoV32: con A pulsado desde el cuadro 40 del cartucho, la
rutina de trampa empieza en el cuadro 41 del cartucho en ambos.

### Mando con guion (--input)

`--input=GUION` mantiene pulsados botones del mando por **cuadro del cartucho** (el cuadro 0 es el primero que ejecuta el cartucho, tras el logotipo
de la BIOS): `BOTÓN[+BOTÓN...]@PRIMERO-ÚLTIMO[,...]` (o `@CUADRO`), con los botones `LEFT RIGHT UP DOWN START A B X Y L R` (`SELECT` es X, el
nombre de los registradores). Esta es la sintaxis `V32_INPUT` de los registradores V32TRACE y de los perfiladores de los traductores; se usa `V32_INPUT` del
entorno cuando no se da `--input`. `--input=@ARCHIVO` toma el guion de la línea `V32_INPUT=` de ARCHIVO (un
informe `.trace.txt`). `--input-port=N` elige el mando (por defecto 0); queda conectado. `--input2=GUION` hace lo mismo para el mando 2 (puerto 1),
con `V32_INPUT2` en el entorno (y `@ARCHIVO` lee la línea `V32_INPUT2=`). Los cambios ocurren
entre cuadros, como los hace la orden `gamepad`: un botón mantenido desde el cuadro F vale 1 en el cuadro F.

## Velocidad y modo turbo

La rapidez con que se ejecuta v32sim importa para las ejecuciones de pruebas (un arranque y compilación de v32nix son cientos de millones de instrucciones). Lo que
lo hace rápido, en el orden en que se fue descubriendo (cifras en `docs/PERFORMANCE.md`):

  * **Nada de trabajo para mensajes que nadie lee.** El texto de `--debug` / `--verbose` se formateaba para cada instrucción y
    se tiraba a `/dev/null` (hasta 20261122); `DPRINTF` (inc/globals.h) ahora comprueba primero el flujo. Las comprobaciones de memoria construyen sus textos
    de error solo cuando hay un error.
  * **Caminos cortos para el caso común**: las lecturas y escrituras de RAM/ROM, la captura de instrucciones, las lecturas de puertos y el contador de ciclos
    se saltan las comprobaciones genéricas; el bucle principal no llama al código de puntos de vigilancia, de pasos por línea de código ni de perfil de direcciones salvo que uno esté armado.
  * **Los nodos de backtrace** (CALL/RET) salen de una lista libre y sus etiquetas se buscan cuando se pide `backtrace`.
  * **Pintado diferido.** El búfer de imagen solo lo lee una captura de pantalla o un estado guardado, así que un dibujo o un borrado se pone en cola
    y se pinta cuando se necesitan los píxeles (o cuando hay 16384 en cola). Un borrado opaco descarta todo lo que había en cola antes,
    de modo que un programa que borra en cada cuadro no pinta más que lo que pide una captura. El pintado es exactamente lo que
    daría pintar de inmediato (`--gpu-eager` lo hace, para comparar); además se volvió más rápido (los dibujos alineados con los ejes encuentran sus
    columnas y filas de origen una sola vez, se saltan los texels transparentes, los productos de color por canal salen de una tabla).
  * **Opciones de compilación**: `-O2` y `-flto` por defecto; `-ffp-contract=off` mantiene idénticos los resultados en coma flotante en anfitriones con multiplicación-suma fusionada.

Todo eso está activado en el modo por defecto y no cambia ningún resultado (los conjuntos diferenciales contra la lógica de consola original, las
capturas de pantalla y las estadísticas son idénticos byte a byte a los de la compilación 20261109, anterior a estos cambios; la única diferencia visible es que `backtrace`
resuelve una etiqueta cuando se muestra, no cuando ocurrió la llamada).

`--turbo[=report]` es para ejecuciones en las que nadie mira los píxeles ni el reloj. Implica `--fast` (nunca duerme) y añade dos
cosas que pueden diferir de una ejecución normal:

  * **Avance rápido de los bucles de espera.** Un programa que sondea (típicamente `TIM_FrameCounter`, o un botón) hasta que cambia el cuadro consume
    los 250000 ciclos del cuadro una vuelta tras otra. Cuando se ve que un bucle se repite exactamente (en dos saltos tomados a la misma
    dirección los 16 registros son iguales, cada palabra de RAM en que se escribió entretanto vuelve a tener su valor anterior - un CALL y un RET dejan
    la pila como la encontraron - y no se escribió ningún puerto, no se leyó ningún puerto volátil (`TIM_CycleCounter`, `RNG_CurrentValue`),
    no se escribió memoria de la tarjeta ni terminó ningún cuadro), se saltan vueltas enteras de una sola vez: `TIM_CycleCounter`, el recuento de instrucciones y
    los contadores por cuadro de instrucciones / lecturas de mando / lecturas de tarjeta avanzan como habrían avanzado, y las últimas vueltas se ejecutan siempre de
    verdad, de modo que el cuadro termina en la misma instrucción con los mismos registros y la misma memoria. **Ningún resultado cambia** (comprobado en
    RAM, registros, estado de salida y `--frame-stats`); lo que no se ve son las instrucciones individuales, así que el salto está desactivado
    mientras algo vaya a vigilarlas: puntos de ruptura, puntos de vigilancia, `-w`, paso a paso y paso a paso por código fuente, `-p` y `--profile-out/-top`,
    V32TRACE, `-v` / `-D`. Un bucle que cuenta o guarda algo nuevo en cada vuelta nunca se salta.
  * **Una cola de GPU llena se descarta, no se pinta.** Si hay 16384 dibujos en cola sin captura de pantalla ni guardado entre medias (un programa
    que nunca borra), se descartan los más antiguos. No cambia nada de lo que el programa puede observar (la contabilidad de dibujos, la consola de texto,
    los contadores y trazas de la GPU usan las órdenes, no los píxeles), pero una `screenshot` o `save` posterior muestra solo lo que
    se puso en cola después, y lo dice (`warning: --turbo skipped N draws`). Los programas que borran su pantalla, y cualquier ejecución de la que
    se tome una captura al menos cada 16384 dibujos, son exactos.

`--turbo=report` imprime en stderr al salir lo que se omitió. Turbo nunca cambia el reloj de la máquina: el contador de ciclos, el
contador de cuadros, la fecha y la hora avanzan como en una ejecución normal.

## Guiones

Todo lo que necesita un guion para manejar una ejecución sin interfaz y para decir qué fue mal. Un guion
es la entrada estándar del simulador (o `-C ARCHIVO`).

### Modo por lotes

`--batch` (`-q`, `--quiet`) hace que stdout contenga exactamente lo que pidió el guion:

  * sin indicador `v32sim>` ni eco de las órdenes, sin la línea de instrucción `[ADDR] HH HH HH HH OPC`
    tras una parada, sin banner de registros al final, sin banner de fin de
    ejecución (`regs`, `print` y `console` siguen imprimiendo, en stdout);
  * los acuses (`[watch] ...`, `[console] wrote ...`, por qué se detuvo una ejecución) y todos los
    diagnósticos van a stderr, de modo que el stdout de un guion nunca tiene que filtrarse;
  * una línea en blanco se omite y el fin de la entrada termina la ejecución (sin bucle infinito);
  * el primer `expect` fallido, o un `until` que agota su tiempo, termina la ejecución.

Estado de salida (siempre; `-x` añade los códigos de límite de cuadros y de error de sistema): 0 éxito;
1 un `expect` falló o un `until` agotó su tiempo; 2 se alcanzó el límite de cuadros (`-x`);
3 un error de sistema (`-x`); 4 no se pudo analizar una línea de órdenes (modo por lotes); 5 `--dump` no pudo imprimir lo
pedido (especificación incorrecta, símbolo desconocido, memoria ilegible; solo cuando nada más fijó un estado); `quit N`
prevalece sobre todos ellos. Una ejecución que termina porque el programa ejecutó `HLT` es un 0 sin más (un final limpio) salvo que
`--halt-status=N` diga otra cosa; `--warn-halt` imprime `[HLT] ...` en stderr cuando ocurre. Una ejecución que no puede empezar sale antes
con 1 (una opción o un valor de opción incorrectos, un archivo de `-C` que no se puede abrir, `--tty` sin terminal, un `--v32dev-host` incorrecto), 11 (no se puede abrir un archivo de BIOS, cartucho o tarjeta), 12 (no se puede leer) o 21 (no es un archivo V32 válido).
Los fallos internos (memoria agotada, un acceso no válido a un IOPort) imprimen `[ERROR]` en stderr y salen con 4, 5, 7, 13, 14 o 15 (4 y 5 son también
los estados anteriores; 2 es también el estado cuando no se puede abrir `/dev/null`); no deberían ocurrir. Sin `--batch` no cambia nada de la salida interactiva
(los guiones que filtran las líneas del indicador y de instrucción siguen funcionando).

### Símbolos

Se acepta un nombre dondequiera que se acepte un número: `print`, `display`, `watch` (destino y lado
derecho), `expect`, `x`, `break` y `--dump`. Un símbolo se busca como símbolo de datos,
luego como etiqueta de los archivos de depuración cargados, cada uno tal cual y luego como `global_NOMBRE`
(el nombre que da el ensamblador a una global de C), y por último sin distinguir mayúsculas.

  * Los símbolos de datos vienen de `NAME.asm.sym` (escrito por `v32cc -g`, con el tamaño de cada
    variable) y de `NAME.vbin.sym` (`v32as -g`; se lee en segundo lugar, de modo que sus direcciones prevalecen) o, cuando
    no existe ninguno, de las líneas `%define global_NOMBRE DIRECCIÓN` de `NAME.asm`, que se encuentra junto
    al archivo V32 o como los archivos de depuración (ver [Archivos de depuración](#archivos-de-depuración)).
    `--symbol-file=ARCHIVO` nombra explícitamente un archivo `.asm.sym`. `-n` los desactiva.
  * `print counter` muestra la palabra en la dirección que representa `counter`, con el
    nombre en la cabecera: `counter(0x00000002): 0x00000096`; `[counter]` es lo mismo;
    `[[ptr]]` sigue un puntero; `&counter` es la propia dirección; `buf+2` y `buf-1`
    suman una constante (se admite una constante con `+`/`-`).
  * `x/d buf` lista las palabras de una variable (por defecto toda la matriz),
    `x/D work 10` diez instrucciones de una función.
  * `watch [counter] == 6`, `watch/d [neg] < 0` y la lista de puntos de vigilancia nombran la
    variable (`counter`, `buf+2`) en lugar de una dirección.

### Esperar y comprobar

  * `run N` (alias `wait N`): continúa durante N cuadros. Un punto de ruptura o de vigilancia que
    se dispare antes la detiene antes; un `continue` posterior no tiene límite.
  * `until "TEXTO" [timeout N] [rows N]`: continúa hasta que la consola de texto muestre TEXTO
    (en cualquier punto de una fila; `^` y `$` anclan al inicio y al final de una fila), mirando
    solo las últimas N filas cuando se da `rows`. TEXTO es literal: `.`, `*`, `[`, `(`, `\`
    se representan a sí mismos, y `^` / `$` cuentan solo como primer / último carácter. Con el
    prefijo `regex:` es una expresión regular extendida POSIX, probada en cada fila (así
    `until "regex:^(ok|done)$"`); una no válida se comunica una vez y nunca coincide.
    `expect console` admite los mismos patrones. La pantalla se comprueba al final de cada
    cuadro y tiene que haber cambiado desde la orden (así lo que ya está ahí no
    termina la espera). Tiempo límite por defecto: 3600 cuadros.
  * `expect LOC OP VALOR` (como `watch`, evaluado ahora; `expect print R0 == 5` también
    funciona) y `expect console "TEXTO" [rows N]`. Una comprobación que pasa es silenciosa en modo por lotes.
  * `watch/d` (o `watch/D`) compara como números de 32 bits con signo: `watch/d [neg] < 0`.
    La lista lo marca `[signed]`. `watch` a secas sigue siendo sin signo.

### Consola a un archivo

`console ARCHIVO` escribe la pantalla de texto en ARCHIVO. `console --attrs[=fg|bg|all] [ARCHIVO]`
añade, después del texto, cuadrículas `-- fg --` y `-- bg --` con las mismas filas y columnas
que el texto; cada celda es el dígito hexadecimal del color ANSI más próximo (0 negro, 1 rojo,
2 verde, 3 amarillo, 4 azul, 5 magenta, 6 cian, 7 gris claro, 8-F versiones brillantes, F
blanco), `.` para un blanco en la cuadrícula de primer plano. Una prueba puede comprobar que una palabra es roja.
`screenshot ARCHIVO` escribe los píxeles como PNG.

### Ejemplo

```
$ cat session.cmd
kbd attach 2
until "^MEM:\>$" rows 1 timeout 1500
run 60
kbd type "ver\n"
run 120
expect console "Vircon32 Disk Operating System"
console --attrs=fg screen.txt
quit
$ v32sim --batch --fast -B StandardBios.v32 --v32kbd dos.v32 < session.cmd; echo $?
```

## Pruebas

`tests/run_tests.sh TOOLCHAIN V32IO_PROJECT` compila los cartuchos de prueba con
las DevTools de Vircon32 y ejecuta: el conjunto de v32dev mediante
`tests/v32sim_runner.sh` (un sucedáneo de `dev_rom_runner`: 98 comprobaciones de dispositivos
de almacenamiento y serie, caída y recuperación del anfitrión, tarjetas con y sin formato, y los escenarios del servidor v32dev: `V32DEV_SERVER`, `serial:stdio`, `serial:tcp`), las
comprobaciones de v32kbd (órdenes, entrada del anfitrión con secuencias de escape, caracteres tecleados,
orden de los eventos), el viaje de ida y vuelta del desensamblado de inmediatos en coma flotante, `load bios:` y
la persistencia de la tarjeta cuando se mata el simulador con `kill -9`.

`tests/run_batch_tests.sh [TOOLCHAIN]` compila `tests/batch/prog.c` con `v32cc`, `v32as`
y `packrom` (de TOOLCHAIN o del PATH, con `StandardBios.v32` o `$V32BIOS`) y
comprueba las funciones de guion: símbolos, `x`, `run`, `until`, `expect`, `quit N`, los estados
de salida, el punto de vigilancia con signo, `console ARCHIVO`/`--attrs`, `--profile-by=function`, y que el
indicador interactivo no cambia.

`tests/run_src_tests.sh [V32AS_DIR]` comprueba las órdenes a nivel de código fuente (54 comprobaciones) con
`tests/src/bench.asm`, ensamblador escrito como lo escribe `v32lua -g`, solo con `v32as`: el
cartucho lo hace `tests/mkcart.py`, y la BIOS es `tests/gaps/bios.asm`.

`tests/run_kbd_sync_tests.sh` (`make test`; 22 comprobaciones, sin cadena de herramientas: entrega de v32kbd con un programa ocupado, `--kbd-sync`, `--warn-halt`,
`--halt-status`, `regex:`) y `tests/run_cli_tests.sh` (también `make test`; 128 comprobaciones, sin cadena de herramientas: la línea de
órdenes, los textos de ayuda y los códigos de salida) y `tests/run_v32dev_server_tests.sh` (también `make test`; 84 comprobaciones, sin cadena de herramientas ni
v32io: `V32DEV_SERVER`/`V32DEV_CHn`, `serial:tcp`, el análisis de SPEC, los tipos de `dev format`).

`tests/run_turbo_tests.sh TOOLCHAIN` (solo `compile`, `assemble`, `packrom` y `StandardBios.v32` originales; 45 comprobaciones) cubre `--turbo`, `--turbo=report`
y `--gpu-eager`: resultados iguales con y sin turbo en un cartucho que sondea, nada omitido cuando el bucle guarda algo, capturas de pantalla
idénticas en diferido / inmediato / turbo, y el aviso de cola descartada.

`tests/run_harness_tests.sh` (`V32TC`, `V32BIOS`, y opcionalmente `ORACLE` y `V32SRCMARK`) comprueba los guiones de `tools/` y su
concordancia con la lógica de consola original: autocomprobación superada/fallida en ambos motores, símbolos de un `.asm` del compilador original,
`--frame-stats` contra `v32prof`, reproducción del mando contra `v32peek`, `bt` a nivel de código fuente en un cartucho compilado con el compilador original,
sesiones de pantalla de referencia, `v32sim-peek.sh`, `v32sim-addrprof.py`, `locals` en cartuchos de C y Lua, `--input` y el ejecutor
del corpus (98 comprobaciones en total con `ORACLE` y `V32IO` definidas; 2 más necesitan `V32SRCMARK` y un árbol de GBtoV32). `make oracle COMPUTERSOFTWARE=DIR` compila
las herramientas de oráculo.

`tests/run_gaps.sh TOOLS`, `run_romcheck.sh TOOLS`, `run_fuzz.sh TOOLS`, `run_clock.sh TOOLS` (el reloj de arranque en varias zonas horarias alrededor de los cambios de horario de verano) y `run_memcard.sh TOOLS` (tarjetas de memoria y buzones v32dev) son los conjuntos diferenciales contra la lógica de consola original
(TOOLS: el directorio de salida de `tools/oracle/build.sh`; ver `docs/V32SIMGAPS.md`), y `tests/run_stack_tests.sh TOOLCHAIN` comprueba la pila
de depuración (C, C++ y Lua, cada uno con -O0, -O2 y -O3). `make check TC=DIR` ejecuta todos los conjuntos desde un directorio de cadena de herramientas e imprime una
línea de resumen; un conjunto cuyas entradas faltan se informa como SKIP, nunca como superado. Las recetas están en [docs/TESTING.md](docs/TESTING.md).

## IOPORTS

Todos los IOPorts existen y siguen la implementación del DesktopEmulator:

- **TIM**: fecha/hora (se tienen en cuenta los años bisiestos; el reloj de arranque es la hora local del anfitrión, como en la consola, también a través de los cambios de horario de verano), contadores de cuadros y de ciclos
- **RNG, INP, CAR, MEM**: se comportan como cabe esperar
- **GPU**: `GPU_ClearColor`, `GPU_SelectedTexture`, `GPU_SelectedRegion`,
  `GPU_Command` (ClearScreen, DrawRegion, DrawRegionZoomed, DrawRegionRotated,
  DrawRegionRotozoomed), `GPU_MultiplyColor`, `GPU_ActiveBlending`,
  `GPU_DrawingPointX/Y`, `GPU_DrawingScaleX/Y`, `GPU_DrawingAngle`,
  `GPU_RegionMinX/MinY/MaxX/MaxY`, `GPU_RegionHotspotX/Y` y
  `GPU_RemainingPixels` se renderizan en el búfer de imagen
- **SPU**: `SPU_GlobalVolume`, `SPU_SelectedSound`, `SPU_SoundLength`
  (solo lectura), `SPU_SoundPlayWithLoop`, `SPU_SoundLoopStart/End`,
  `SPU_SelectedChannel`, `SPU_ChannelState` (solo lectura),
  `SPU_ChannelAssignedSound`, `SPU_ChannelVolume`, `SPU_ChannelSpeed`,
  `SPU_ChannelLoopEnabled`, `SPU_ChannelPosition`, `SPU_Command`; los canales
  avanzan 735 muestras por cuadro, pero no se mezcla ni se reproduce nada

Se cargan todas las texturas y sonidos presentes en un archivo V32; `inventory` los
lista. Desde el indicador, los puertos se pueden manipular con `set` e inspeccionar con
`print`/`display`.

## Limitaciones conocidas y funciones previstas

- **SPU**: sin salida de audio (tampoco captura a WAV)
- **Renderizado**: la rasterización de quads rotados y escalados del renderizador por software no se ha comparado con la salida de OpenGL
  (`docs/V32SIMGAPS.md` G14); `INT_MIN / -1` es indefinido en el anfitrión y difiere de la consola (G12); los desplazamientos de 32 o más también son indefinidos en el anfitrión, pero coinciden con la consola en x86-64
- **Consola de texto**: solo se capturan, como caracteres, los dibujos con la textura de la BIOS; se ignoran el alfa, el parpadeo y el color de borrado del invitado, y
  el texto dibujado con la fuente propia de un programa no se captura. `until` y `expect console` comparan texto, no colores
- **Símbolos**: solo las variables con una dirección numérica (globales de v32cc y de v32lua) son símbolos de datos; las variables locales de una función y los
  miembros de estructuras no son símbolos (las locales tienen su propia tabla, `locals`; un miembro solo tiene nombre dentro del `type=` de su variable), y una
  entrada de `.asm.sym` sin tamaño cuenta como una palabra
- Los modos de visualización de Lua y C (`-L`) son experimentales. Las variables locales (`locals`, V32DBG sección 4) muestran escalares, matrices, estructuras
  por nombre de miembro, globales con su `type=`, valores de Lua, globales de Lua y upvalues de clausuras; un miembro de estructura no se puede nombrar en una
  expresión (`print p.x`; `x p` muestra las palabras), un miembro que es a su vez un agregado se muestra como palabras en bruto, las tablas de Lua anidadas solo
  muestran su dirección, y `display NOMBRE` conserva la dirección que tenía el nombre en lugar de seguir el marco
- El lector de archivos de depuración está escrito a mano (podría pasar a flex)
- El inicio por defecto es la BIOS (`0x10000004`), no el cartucho

Consulta TODO.md para los puntos pendientes.
