[Versión en inglés / in English](USAGE.md) | [Versión en francés / en français](USAGE.fr.md)

# v32sim: referencia de la línea de órdenes

v32sim ejecuta un cartucho de Vircon32 en sus propios modelos de CPU, GPU y SPU, y o bien te da un indicador de depurador paso a
paso, o bien lo ejecuta sin supervisión y presenta un informe. Esta página trata la línea de órdenes, el entorno, los archivos y
el estado de salida. El lenguaje de órdenes del depurador (`step`, `break`, `print`, `watch`, `kbd`, ...) está en el
[README](../README.es.md) («El depurador», «Órdenes», «Guiones»); `help` en el indicador lista todas las órdenes y
`help ORDEN` explica una. Qué herramienta responde a qué pregunta: [TOOLS.md](TOOLS.md). Recetas de pruebas:
[TESTING.md](TESTING.md). Perfilado: [PROFILING.md](PROFILING.md).

## Sinopsis

```
v32sim [OPCIÓN]... [CARTFILE.v32]
v32sim --version | --help
```

`CARTFILE.v32` es el cartucho. Da `-` (o nada) para no usar cartucho: la BIOS se ejecuta sola. Las opciones pueden ir antes o
después del cartucho. Las opciones largas con argumento admiten `--nombre=VALOR` (o `--nombre VALOR`); `[=X]` más abajo significa
que el valor es opcional y debe ir pegado con `=`. Los números dados como OFFSET, OPCODE o ADDR son hexadecimales (`0x` es
opcional).

## Inicio rápido

Depurar un cartucho (el indicador aparece en la primera instrucción; `help` lista las órdenes):

```
v32sim -B StandardBios.v32 cart.v32
v32sim -B StandardBios.v32 -b main cart.v32      # punto de ruptura en la etiqueta main, luego `continue`
v32sim -B StandardBios.v32 --source -L src cart.v32   # muestra la línea de código C / Lua en cada parada
```

Los archivos de depuración (etiquetas, líneas de código, símbolos) se encuentran automáticamente junto al cartucho; créalos con
`v32as -g` y `v32cc -g` / `v32lua -g`.

Ejecutar sin interfaz, comprobar el estado de salida, imprimir unas palabras de RAM (`-r` ejecutar, `-q` silencioso, `-x` estado de
salida significativo):

```
v32sim -B StandardBios.v32 -r -q -x --fast --max-frames=600 --dump=4@counter cart.v32 </dev/null; echo $?
```

Recetas:

```
# captura de pantalla tras los cuadros 60 y 120 del cartucho (v32sim-shot-00060.png, ...)
v32sim -B B.v32 -r -q --fast --max-frames=400 --screenshot-at=60,120 cart.v32 </dev/null

# reproducir un guion de mando (A pulsado en los cuadros 40-44 del cartucho, Start en el 100) y luego capturar la pantalla
v32sim -B B.v32 -r -q --fast --max-frames=450 --input='A@40-44,START@100' --screenshot-at=150 cart.v32 </dev/null

# estadísticas de cuadros: una fila CSV por cuadro, resumen (media, p95, máximo, regiones) en stderr
v32sim -B B.v32 -r -q --fast --max-frames=1800 --frame-stats=load.csv --frame-stats-skip=250 cart.v32 </dev/null

# las funciones más calientes
v32sim -B B.v32 -r -q --fast --max-frames=1800 --profile-by=function --profile-top=20 cart.v32 </dev/null

# un guion de órdenes del depurador desde un archivo, y en stdout solo lo que imprimen `print`/`console`
v32sim -B B.v32 --batch --fast -x --v32kbd dos.v32 < session.cmd

# la pantalla de texto del programa al terminar; ejecuciones reproducibles
v32sim -B B.v32 -r -q --fast -x --max-frames=900 --console --fixed-clock --seed=1 --fixed-cycles cart.v32 </dev/null

# un programa de texto en tu terminal (Ctrl-] vuelve al indicador)
v32sim --tty -B B.v32 v32nix.v32

# una sesión V32TRACE: informes de trampas y eventos PREFIX.traps.txt, .blocks.txt, .trace.txt
v32sim -B B.v32 -r -q --fast --max-frames=3000 --v32trace=out/run cart.v32 </dev/null
```

Sin `--batch`, un guion canalizado a stdin se muestra con el indicador; da `-q` cuando un programa lee la salida. Una ejecución con
`-r` con órdenes en stdin o con `-C` ejecuta esas órdenes una vez al inicio.

## Opciones

Las opciones marcadas (forma corta) tienen una: `-B`, etc. «Por defecto» es lo que se aplica cuando la opción no está presente.

### Carga y BIOS

| Opción | Significado |
|---|---|
| `-B, --biosfile=ARCHIVO` | Archivo V32 de la BIOS. Por defecto: `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32` (la instalación del DesktopEmulator). |
| `-M, --memcfile=ARCHIVO` | Archivo de tarjeta de memoria (`V32-MEMC` + 262144 palabras). Los cambios se escriben de vuelta al final de cada cuadro. Ver *Tarjeta de memoria* más abajo. |
| `-E, --entry-point=OFFSET` | Empieza a ejecutar en OFFSET (hex) en lugar de en el inicio de la BIOS. |
| `-S, --bios-start` | Empieza en el inicio del código de la BIOS (lo predeterminado; se conserva por compatibilidad, y `-E` lo anula). |
| `CARTFILE.v32` o `-` | El cartucho; `-` = ninguno. |
| `-V, --version` | Imprime `v32sim VERSIÓN` y termina. |
| `-h, --help` | Imprime el texto de uso y termina (estado 0). |

### Información de depuración y código fuente

| Opción | Significado |
|---|---|
| `--cart-asm-debug=ARCHIVO`, `--cart-c-debug=ARCHIVO` | Etiquetas / tabla de líneas del cartucho desde ARCHIVO (del ensamblador `.vbin.debug`; del compilador `.asm.debug`). |
| `--bios-asm-debug=ARCHIVO`, `--bios-c-debug=ARCHIVO` | Lo mismo para la BIOS. |
| `--debug-dir=DIR` | Busca también archivos de depuración en DIR, que se prueba primero. Sin estas opciones, los archivos de depuración se buscan junto al archivo V32, en su `obj/` y `../obj/`, en `./obj` y en `.`. |
| `--symbol-file=ARCHIVO` | Símbolos de datos (el `NOMBRE.asm.sym` de v32cc). Los `NOMBRE.asm.sym` / `NOMBRE.vbin.sym` junto al cartucho se leen automáticamente. |
| `-n, --no-debug` | No procesa ningún archivo de depuración (etiquetas, líneas, símbolos). |
| `--source` | Muestra la línea de código fuente de cada parada (ensamblador; C o Lua con `-L`). |
| `-L, --langmode=MODO` | `C`, `lua` o `src`: el lenguaje fuente de la superposición. `C` y `src` activan también `--source`; `lua` muestra valores de Lua en los volcados de memoria. `step`/`next` avanzan entonces por línea de código. Cualquier otro valor es un error (mensaje por stderr, estado de salida 1). |
| `-d, --deref-addr` | Muestra la dirección de una desreferencia. |
| `-c, --colors` | Salida en color en el indicador. |

### Puntos de ruptura, entrada y vigilancia

| Opción | Significado |
|---|---|
| `-b, --break=OFFSET\|ETIQUETA\|ARCHIVO:LÍNEA` | Pone un punto de ruptura (repetible), como hace la orden `break` una vez cargados los archivos de depuración. Un nombre que es una etiqueta (o función) conocida es esa etiqueta aunque también sea un número hexadecimal (`add`, `face`, `dead`; escriba `0xdead` para la dirección); si no, el valor es un OFFSET hexadecimal, con o sin `0x`. `ARCHIVO:LÍNEA` necesita información de líneas. Un valor que no es nada de eso se notifica por stderr. |
| `-w, --watch-for=OPCODE` | Ejecuta (`-r`) hasta que se ejecute esta palabra de instrucción de 32 bits. |
| `-C, --command-file=ARCHIVO` | Ejecuta las órdenes del depurador de ARCHIVO. El indicador las lee después de obtener la primera instrucción (así que `step`, `next`, `continue` actúan); con `-r` se ejecutan una vez al inicio. |
| `-e, --errorcheck` | Comprobación de errores en tiempo de ejecución. |
| `-p, --profile` | Perfilado de instrucciones; `profile` muestra el informe. |

### Control de ejecución, velocidad y turbo

| Opción | Significado |
|---|---|
| `-r, --run` | No empieza en modo paso a paso: ejecuta hasta un punto de ruptura, un punto de vigilancia, HLT o el límite de cuadros. |
| `--max-frames=N` | Se detiene tras N cuadros, contados desde el encendido (el logotipo de la BIOS, unos 250 cuadros, incluido; estado de salida 2 con `-x`). |
| `--fast` | No limita la velocidad al tiempo real. |
| `--frame-us=N` | Microsegundos de reloj de pared por cuadro. Por defecto 16666; 0 = `--fast`. |
| `--turbo[=report]` | `--fast`, más avance rápido de los bucles de espera y descarte de una cola de píxeles de la GPU llena en lugar de pintarla (las capturas de pantalla pueden salir incompletas). `=report` imprime en stderr al salir lo que se omitió. README «Velocidad y modo turbo». Desactivado mientras algo vigile instrucciones individuales. |
| `--gpu-eager` | Pinta cada orden de la GPU de inmediato (mismas imágenes; para comparar y medir tiempos). |
| `--fixed-clock[=A,D,S]` | `TIM_CurrentDate/Time` empiezan en el año A, el día D (0 = 1 de enero) y S segundos (por defecto `2026,0,0`) en lugar del reloj del anfitrión. |
| `--seed=N` | Inicializa `RNG_CurrentValue` con N al arrancar. |
| `--fixed-cycles` | Las lecturas de `TIM_CycleCounter` devuelven 0. |
| `--warn-halt` | Avisa por stderr cuando el programa ejecuta HLT (el emulador se detiene entonces). |
| `--halt-status=N` | Estado de salida N (1-255) cuando la ejecución termina en HLT y nada más fue mal. |

### Sin interfaz, por lotes y sin supervisión

| Opción | Significado |
|---|---|
| `-q, --batch` (también `--quiet`) | Modo guion: órdenes desde stdin o `-C` sin indicador ni eco; sin cabecera, línea de instrucción, lista de visualización ni resumen de registros al final de la ejecución. stdout contiene solo lo que imprimen `print`, `console`, ...; los acuses, los motivos de parada y los diagnósticos van a stderr. Las líneas en blanco se omiten; el fin de la entrada termina la ejecución; el primer `expect` fallido, o un `until` que agota su tiempo, la termina con estado 1; una orden que no se puede analizar la termina con 4. |
| `--expect-reach ETIQUETA[,ETIQUETA...]` | Etiquetas (de los archivos de depuración) a las que debe llegar la ejecución. En la primera: `EXPECT REACHED <etiqueta> frame=N ip=0x........` en stdout, se detiene y sale con 0. Si la ejecución termina de otro modo (límite de cuadros, HLT, error de CPU, `quit`): `EXPECT MISSED <etiquetas>` y sale con 1 (también con `-x`). Una etiqueta desconocida es un error de arranque (sale con 1). |
| `-x, --exit-status` | El estado de salida indica cómo terminó el programa (ver *Estado de salida*). |
| `--dump=N[@ADDR]` o `ADDR:N` | Tras la ejecución imprime N palabras de RAM (con signo, una por línea) desde ADDR: hex, o una etiqueta (probada también como `global_NOMBRE`); por defecto el inicio de la RAM. |
| `--console` | Imprime la pantalla de texto con la fuente de la BIOS (64x18, u 80x24 en v32c++) cuando termina la ejecución. |

### Entrada

| Opción | Significado |
|---|---|
| `--input=GUION` | Mantiene pulsados botones del mando por **cuadro del cartucho** (0 = el primer cuadro que ejecuta el cartucho): `BOTÓN[+BOTÓN]@PRIMERO-ÚLTIMO,...` (`@CUADRO` para uno solo). Botones `LEFT RIGHT UP DOWN START A B X Y L R` (`SELECT` = X). `@ARCHIVO` lee la línea `V32_INPUT=` de ARCHIVO (un informe `.trace.txt`). Por defecto: `$V32_INPUT`. |
| `--input-port=N` | Puerto de mando que maneja `--input` (por defecto 0); queda conectado. |
| `--input2=GUION` | Lo mismo para el mando 2 (puerto 1); `@ARCHIVO` lee la línea `V32_INPUT2=`. Por defecto: `$V32_INPUT2`. Con `--input-port=1` es un error. |
| `--v32kbd[=PUERTO]` | Emula un teclado v32io en el puerto de mando PUERTO (0-3, por defecto 0). El programa debe leerlo en cada cuadro. |
| `--kbd-input=ORIGEN` | Teclas del anfitrión: `stdin`, `fd:N`, o una ruta de FIFO/tubería/archivo. Implica `--v32kbd`. |
| `--kbd-sync` | Entrega el siguiente evento de tecla solo después de que el programa haya leído el mando desde el anterior (por defecto: el evento nuevo reemplaza a uno sin leer, como hace el teclado real). |
| `--v32mouse[=PUERTO]` | Emula un ratón v32io en el puerto de mando PUERTO (0-3, por defecto 1); se maneja con la orden `mouse`. |
| `--tty` | Modo interactivo a pantalla completa: la consola de texto en este terminal, las teclas a un v32kbd (puerto 0 salvo `--v32kbd=N`), implica `-r`. Ctrl-] = indicador del depurador, `continue` vuelve. Necesita un terminal en stdin y stdout. |

El mando, el teclado y el ratón también se pueden manejar desde el indicador (`gamepad`, `kbd`, `mouse`). README «El teclado v32io»,
«El ratón v32io», «Mando con guion (--input)».

### Tarjeta de memoria y v32dev

`-M ARCHIVO` es la tarjeta. Un buzón v32dev vive en las últimas 16384 palabras de ella; el lado del anfitrión es o bien un proceso
externo (`v32dev.py serve`) o el propio v32sim:

| Opción | Significado |
|---|---|
| `--v32dev-doorbell[=MS]` | Bit 1 de la bandera de cabecera, contadores de respuesta por canal (palabras de cabecera 10..13); un almacenamiento en el `c2h_head` de una parte del cartucho hace sonar el timbre: vacía, da al anfitrión MS ms (por defecto 2; 0 = sin espera) para responder, importa la respuesta y sigue. Desactivado por defecto. |
| `--v32dev-host=N=SPEC` | Atiende el canal N (0-3) de forma nativa. Repetible. Necesita `-M ARCHIVO` con una tarjeta v32dev escribible (o con los últimos 64 KiB a cero: se formatea). SPEC: `storage:ARCHIVO[,bytes][,ro][,sector=N][,create=S]` o `serial:stdio\|echo[,hup=N]\|tcp=PORT[,in=ARCHIVO][,out=ARCHIVO][,hangup]` (`tcp=PORT`: solo 127.0.0.1, un cliente a la vez; las opciones de `storage:` se leen de derecha a izquierda, así que ARCHIVO puede contener comas). Gana a `V32DEV_CHN` en su canal. |
| `--v32dev-host-force` | Formatea la tarjeta sobre otros datos en sus últimos 64 KiB. |

### Salida, capturas de pantalla y consola de texto

| Opción | Significado |
|---|---|
| `--screenshot-at=CUADROS` | Escribe la pantalla (PNG) tras estos cuadros del cartucho: `N`, `A-B`, `A-B/PASO`, separados por comas. El cuadro 0 es el primer cuadro que termina con el contador de programa en el cartucho, es decir, después de que la BIOS ha cedido el control (el logotipo de la BIOS estándar dura unos 250 cuadros, que `--max-frames` también cuenta). Los cuadros que la ejecución no llegó a alcanzar se notifican por stderr al salir. |
| `--screenshot-file=PAT` | Nombre de archivo para ellas; `%d` = el cuadro (estilo printf, p. ej. `%05d`). Por defecto `v32sim-shot-%05d.png`. |
| `--screengrid[=SPEC]` | Cuadrícula sobre las capturas: `DX[xDY]`, `color=C`, `alpha=N`, `labels`/`nolabels`, separados por comas. Por defecto 40x40. |
| `--highlight=T:R[:COLOR]` | Perfila la región R de la textura T (`bios:R` para la textura de la BIOS); separados por comas. |
| `--noextra` | Sin cuadrícula ni perfiles. |
| `--console-color=MODO` | Colores de la consola de texto: `none`, `16`, `256`, `true`, `auto`. Por defecto: `auto` con `--tty`, texto sin formato en los demás casos. |
| `--cursor-color=COLOR` | Cambia el color del bloque del cursor (carácter 20 de la BIOS): un nombre, `#RRGGBB` u `off`. |
| `-v, --verbose` | Más salida (a stderr). |
| `-D, --debug` | Salida de depuración del simulador (desarrollo; a stderr). |

### Perfilado, estadísticas y trazas

| Opción | Significado |
|---|---|
| `--frame-stats=ARCHIVO` (`--stats`) | Una fila CSV por cuadro (ciclos, % de CPU, píxeles de GPU, % de GPU, y luego los contadores de GPU/SPU/bus); resumen (media, p50, p95, p99, máximo, total, regiones más ocupadas) a stderr al salir. |
| `--frame-stats-skip=N` (`--stats-skip`) | Deja fuera del resumen los primeros N cuadros (el logotipo de la BIOS, unos 250). |
| `--frame-stats-top=N` (`--stats-top`) | Lista las N regiones a las que se cargaron más píxeles de GPU (por defecto 8; 0 = ninguna). |
| `--stats-format=csv\|json` | `json`: JSON Lines con nombres con puntos estables y una línea de resumen al final. |
| `--profile-out=ARCHIVO` | Al final de la ejecución escribe cuántas veces se ejecutó cada dirección de instrucción (no necesita indicador ni `-p`); un archivo de depuración añade una columna de etiqueta. |
| `--profile-format=text\|bin` | `bin`: cabecera `V32PROF1`, luego pares (dirección, cuenta) de 32 bits little-endian. Por defecto text. |
| `--profile-top=N` | Imprime a stderr al final las N direcciones (o funciones) más calientes. |
| `--profile-by=address\|function` | Con `function`, `--profile-top` y el `--profile-out` de texto suman cada función (la etiqueta más cercana). Por defecto address. |
| `--profile-event=TIPO` | Qué se cuenta: `instr` (por defecto), `gpu-draw`, `gpu-portwrite`, `spu-write`, `cycles`. |
| `--heap[=PROG.asm]` | Cartuchos v32lua: al final de cada cuadro imprime `HEAP F<cuadro> ptr=0x.. used=N peak=N` (con `--gc arena` también `floor= pin= depth= stack=`) y `HEAP PEAK` / `HEAP END` al salir. Lee `HEAP_POINTER`, `HEAP_START` y `ARENA_*` del `.asm` del cartucho (junto a él) o de PROG.asm. README «Vigilancia del montón». |
| `--time-breakdown` | Al salir, en qué se va por cuadro el tiempo de reloj de pared del propio v32sim (cpu/gpu/spu/salida). |
| `--frame-log=ARCHIVO` | Con ella, una fila por cuadro de ese desglose. |
| `--gpu-trace=ARCHIVO`, `--spu-trace=ARCHIVO` | Registra las escrituras en puertos de la GPU / SPU por cuadro (binario, [TRACE-FORMAT.md](TRACE-FORMAT.md)). |
| `--trace-dump=ARCHIVO` | Imprime un archivo de traza como texto y termina. |
| `--v32trace=PREFIJO` | Registrador V32TRACE ([V32TRACE-SPEC.md](V32TRACE-SPEC.md) 2.1): lee `<cart>.v32trace.meta` y las etiquetas, escribe `PREFIJO.traps.txt`, `.blocks.txt`, `.trace.txt`. El cartucho se reinicia tras una trampa. |
| `--v32trace-meta=ARCHIVO`, `--v32trace-labels=ARCHIVO` | Nombran esos archivos cuando no están junto al cartucho. |
| `--v32trace-norestart` | Permanece en la trampa. |

## Entorno

| Variable | Se usa para |
|---|---|
| `V32_INPUT`, `V32_INPUT2` | Guiones de mando que se usan cuando no se dan `--input` / `--input2` (se avisa con una nota en stderr). |
| `V32DEV_DISABLE` | Cualquier valor salvo `0`: una tarjeta es una tarjeta ordinaria, sin buzón v32dev; con `--v32dev-host` o `V32DEV_SERVER` un aviso, y el anfitrión no arranca. |
| `V32DEV_SERVER` | Definida (y no `0`): el servidor v32dev integrado, como en el parche del DesktopEmulator del proyecto v32io: la tarjeta no necesita formato, el buzón vive solo en la memoria y nunca llega al archivo de la tarjeta, y los canales vienen de `V32DEV_CH0`..`V32DEV_CH3`. Sin definir o `0`: esas variables no se miran. |
| `V32DEV_CH0` ... `V32DEV_CH3` | Con `V32DEV_SERVER`: lo que atiende el canal N, una SPEC como para `--v32dev-host` (`storage:ARCHIVO[,bytes][,ro][,sector=N][,create=S]`, `serial:echo[,hup=N]`, `serial:stdio`, `serial:tcp=PORT`). Vacía o sin definir: nada. Una incorrecta se informa y su canal queda fuera. |
| `NO_COLOR`, `COLORTERM`, `TERM` | Lo que elige `--console-color=auto` (y `--tty`): ninguno si `NO_COLOR` está definida y no vacía; color verdadero para `truecolor`/`24bit`; 256 para `*256color`; 16 para otros terminales salvo `dumb`. |
| `HOME`, `INPUTRC`, `TERM` | Los lee GNU readline para el indicador (edición de línea, historial solo en memoria). |

Los scripts auxiliares de `tools/` leen además `V32SIM` (ruta del binario), `V32SIM_FLAGS` (opciones adicionales), `V32BIOS`, `V32TC`
(directorio de la cadena de herramientas) y `V32SIM_HOME` (para las herramientas del oráculo); ver [TOOLS.md](TOOLS.md).

## Archivos

Leídos:

| Archivo | |
|---|---|
| BIOS | `-B ARCHIVO`, si no `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32`. |
| Cartucho | `CARTFILE.v32`. |
| `NOMBRE.vbin.debug`, `NOMBRE.asm.debug` | Etiquetas y tabla de líneas (ensamblador, compilador) del cartucho y de la BIOS, junto a él, en `obj/`, `../obj/`, `--debug-dir`. |
| `NOMBRE.asm.sym`, `NOMBRE.vbin.sym`, `NOMBRE.asm` | Símbolos de datos (v32cc `-g`; v32as `-g`; las líneas `%define global_NOMBRE`). |
| `NOMBRE.vbin.vars` | Variables locales para `locals` (v32as `-g`, a partir de v32cc / v32lua `-g`). |
| `NOMBRE.v32trace.meta`, etiquetas | Archivos del traductor para `--v32trace`. |
| `-C ARCHIVO`, archivos de guion, `--input=@ARCHIVO`, `--kbd-input=RUTA` | Según lo indicado. |

Escritos:

| Archivo | |
|---|---|
| Tarjeta de memoria `-M ARCHIVO` | Se actualiza al final de cada cuadro (las tarjetas de solo lectura avisan y no se guardan). |
| `v32sim-shot-NNNNN.png` | `--screenshot-at` (`--screenshot-file` lo renombra). |
| `v32sim.AAAAMMDD-HHMMSS.png`, `.sav` | Nombres por defecto de las órdenes `screenshot` y `save`. |
| `--frame-stats`, `--profile-out`, `--frame-log`, `--gpu-trace`, `--spu-trace` | Según lo indicado. |
| `PREFIJO.traps.txt`, `PREFIJO.blocks.txt`, `PREFIJO.trace.txt` | `--v32trace`. |
| `--v32dev-host` `storage:ARCHIVO`, `out=ARCHIVO` | Imagen de disco y salida serie de un dispositivo nativo. |

## Estado de salida

| Estado | Significado |
|---|---|
| 0 | Fin normal: el programa ejecutó HLT (salvo `--halt-status`), o `quit`, o fin de la entrada. |
| 1 | Falló una comprobación: falló un `expect`, un `until` agotó su tiempo o `--expect-reach` no llegó a su etiqueta (`EXPECT MISSED`; prevalece sobre 2 y 3). Problemas de arranque que salen con 1: una opción o un valor de opción incorrectos (también `-L` con un modo desconocido), un archivo de `-C` que no se puede abrir, `--tty` sin terminal, un `--v32dev-host` incorrecto. |
| 2 | Se alcanzó el límite de cuadros (solo con `-x`). También es el estado cuando no se puede abrir `/dev/null` al arrancar. |
| 3 | Error de sistema (solo con `-x`; la ejecución se detiene en lugar de entrar en el manejador de errores de la BIOS). |
| 4 | `--batch` no pudo analizar una orden (o falló una orden `dev`). |
| 5 | `--dump` no pudo imprimir lo pedido (especificación incorrecta, símbolo desconocido, memoria ilegible), cuando nada más fijó un estado. |
| 11 | Arranque: no se puede abrir un archivo de BIOS, cartucho o tarjeta de memoria. |
| 12 | Arranque: no se puede leer ese archivo (truncado, posición incorrecta). |
| 21 | Arranque: ese archivo no es un archivo V32 válido. |
| 4, 5, 7, 13, 14, 15 | Fallos internos (memoria agotada, un modo de acceso a IOPort no válido), con un mensaje `[ERROR]` en stderr; no deberían ocurrir. 4 y 5 son también los estados anteriores. |
| N | `quit N` prevalece sobre todo lo anterior; `--halt-status=N` fija el estado de un HLT. |

## Véase también

[README](../README.es.md) (órdenes del depurador, depuración a nivel de código fuente, tarjeta de memoria y v32dev, teclado y ratón v32io, consola
de texto, ejecución sin supervisión, guiones), [TOOLS.md](TOOLS.md), [TESTING.md](TESTING.md), [PROFILING.md](PROFILING.md),
[PERFORMANCE.md](PERFORMANCE.md), [TRACE-FORMAT.md](TRACE-FORMAT.md), [V32TRACE-SPEC.md](V32TRACE-SPEC.md). Página de manual:
`man v32sim` (`man/v32sim.1`).
