# v32sim

[Version anglaise / in English](README.md) | [Version espagnole / en español](README.es.md)

**Référence de la ligne de commande :** [docs/USAGE.fr.md](docs/USAGE.fr.md) (chaque option, variable d'environnement et code de sortie), ou `man v32sim`.

**v32sim** est un débogueur et simulateur en mode texte pour la console de
fantaisie Vircon32. Son but est de rendre la machine observable : exécuter
le CPU pas à pas, instruction par instruction, inspecter et modifier les
registres, la mémoire et les IOPorts, poser des points d'arrêt et des points
de surveillance, profiler l'exécution, et piloter la machine à partir de
fichiers de commandes pour des tests automatisés.

Ce n'est pas un lecteur : il n'y a ni fenêtre ni sortie audio. Il modélise
le CPU, le système de mémoire, les IOPorts, un GPU logiciel (de sorte que
`screenshot` puisse écrire l'écran dans un PNG) et un SPU (l'état des canaux
et des sons, mais rien n'est joué) assez fidèlement pour étudier, déboguer et
bidouiller des logiciels Vircon32 — et pour automatiser le test des
cartouches.

## État Actuel

- Les 64 instructions du CPU sont implémentées et vérifiées en exécutant les
  mêmes cartouches sur v32sim et sur la logique de console d'origine
  (`tests/run_gaps.sh`, `run_fuzz.sh`, `run_romcheck.sh`, `run_clock.sh`, `run_memcard.sh` ; voir
  [docs/V32SIMGAPS.md](docs/V32SIMGAPS.md)) : le timer et le RNG, la pile, le
  bus mémoire, le moment des erreurs, un mot par cycle pour `MOVS`/`SETS`/`CMPS`,
  l'écrêtage des ports du GPU et la validation de la ROM correspondent à la
  console. Seuls diffèrent le comportement indéfini côté hôte (`INT_MIN / -1`, et les
  décalages >= 32 sur les hôtes autres que x86-64) et le rendu au niveau du pixel
- Les registres (R0-R15 avec leurs alias, plus les registres système IP/IR/IV)
  et les quatre pages mémoire (RAM, BIOS, CART, MEMCARD) se comportent comme le
  matériel
- Les erreurs système (accès invalide à la mémoire ou à un port, division par
  zéro, etc.) enregistrent l'état de la machine dans R0-R3, ramènent SP/BP au
  dernier mot de la RAM, et redirigent l'exécution vers le gestionnaire
  d'erreurs du BIOS à `0x10000000`
- Groupes d'IOPorts : TIM (avec les années bissextiles), RNG, INP (manettes),
  CAR, MEM, GPU, SPU
- **GPU** : un moteur de rendu logiciel avec un tampon d'image de 640x360 :
  effacement, dessin, zoom, rotation, rotozoom, mélange (alpha/addition/
  soustraction), couleur de multiplication, et le budget de pixels par image
  (`GPU_RemainingPixels`) ; `screenshot` l'écrit dans un PNG
- **SPU** : 16 canaux dont la sélection du son, le volume, la vitesse, la
  boucle et la position avancent à chaque image, et les données VSND du BIOS et
  de la cartouche sont chargées ; il n'y a pas de sortie audio
- **Carte mémoire** : l'en-tête `V32-MEMC` et la taille sont validés ; les
  modifications arrivent dans le fichier à la fin de chaque image (un
  simulateur tué perd donc au plus une image), comme le fait le DesktopEmulator.
  Une carte formatée pour **v32dev** garde une boîte aux lettres active, de
  sorte que les programmes qui utilisent la bibliothèque v32dev (stockage,
  série, ...) parlent à un vrai processus hôte `v32dev.py serve` (voir *Carte
  Mémoire et v32dev*)
- **Clavier v32io** : un `v32kbd` sur n'importe quel port de manette, alimenté
  depuis l'invite (`kbd type`, `kbd key`) ou depuis l'hôte (un terminal, un
  tube, un FIFO ou un fichier), voir *Le Clavier v32io*
- **Souris v32io** : une `v32mouse` sur un autre port de manette, pilotée depuis
  l'invite (`mouse move`, `mouse click`), voir *La Souris v32io*
- **Points de surveillance** sur les registres, les IOPorts et les mots
  mémoire, comparés à un nombre ou à un autre registre/IOPort/mot mémoire ;
  déclenchés sur front
- **Sauvegarde / restauration** de l'état complet de la machine
- **Assembleur à la volée** : `asm` assemble une instruction et la patche en
  mémoire, ou l'exécute une fois
- **Débogage au niveau source** (V32DBG) : `step`/`next` (dans les modes source),
  `finish` et `until` avancent par ligne source, `break`/`tbreak FILE:LINE`,
  `list`, et `bt` avec noms de fonctions et lignes, pour Lua, C et assembleur ;
  voir [Débogage au Niveau Source](#débogage-au-niveau-source)
- **Superposition de la source** : la ligne source assembleur, C ou Lua de l'IP
  courant est affichée à l'invite
- Interface de commandes construite sur **flex/bison** (`lexer.l`, `parser.y`),
  et une seconde paire flex/bison pour l'assembleur (`asm_lexer.l`,
  `asm_parser.y`) : les commandes peuvent être abrégées, `#` commence un
  commentaire, et les erreurs de syntaxe signalent le jeton fautif et la colonne
- `ignore` saute l'instruction courante ; un gestionnaire de
  **SIGSEGV/SIGBUS/SIGFPE/SIGABRT** affiche l'état du simulateur avant de mourir
- **Mode Lua** expérimental (`--langmode=lua`) : les mots mémoire sont examinés
  à la recherche de marqueurs de valeurs Lua (chaînes, tables, fonctions,
  nil/true/false) lors de l'affichage avec `/B` ou `/s`
- **Scripts** (`--batch`) : symboles (étiquettes et globales C) partout où un
  nombre est accepté, `run N`, `until "TEXT"`, `expect`, `quit N`, codes de
  sortie, `console FILE` et `console --attrs`, `watch/d` signé, et profils par
  fonction ; voir [Écriture de Scripts](#écriture-de-scripts)
- **Fichiers de débogage** (étiquettes et numéros de ligne) chargés
  automatiquement (voir [Fichiers de Débogage](#fichiers-de-débogage)) ; `-n`
  le désactive

- **Dépôt central d'outils** : les scripts, les outils de la console d'origine
  (« oracle ») et les bancs de test que d'autres projets Vircon32 utilisent pour
  tester, déboguer, profiler et simuler leurs programmes vivent aussi ici : voir
  [docs/TOOLS.md](docs/TOOLS.md), [docs/TESTING.md](docs/TESTING.md),
  [docs/PROFILING.md](docs/PROFILING.md), et pour un agent qui travaille dans un
  autre projet [docs/AGENTS-SNIPPET.md](docs/AGENTS-SNIPPET.md) ; `AGENTS.md`
  est destiné au travail sur v32sim lui-même
- **Variables locales** (`locals`, `info locals` ; section 4 de V32DBG) : les
  variables du cadre sélectionné avec leurs types et leurs valeurs, d'après le
  `NAME.vbin.vars` que `v32as -g` écrit pour la sortie de `v32cc -g` et de
  `v32lua -g`
- **Manette scriptée par image de cartouche** (`--input`, la syntaxe
  `V32_INPUT` des enregistreurs V32TRACE)
- **Statistiques d'images** (`--frame-stats=FILE`) : la charge CPU et GPU de
  chaque image en CSV, avec un résumé moyenne / p95 / maximum

## Table des Matières

- [Organisation du Projet](#organisation-du-projet)
- [Prérequis](#prérequis)
- [Compilation](#compilation)
- [Installation](#installation)
- [Utilisation](#utilisation)
- [Plan de la Mémoire](#plan-de-la-mémoire)
- [Le Débogueur](#le-débogueur)
- [Commandes](#commandes)
- [Débogage au Niveau Source](#débogage-au-niveau-source)
- [Carte Mémoire et v32dev](#carte-mémoire-et-v32dev)
- [La Souris v32io](#la-souris-v32io)
- [Le Clavier v32io](#le-clavier-v32io)
- [La Console Texte](#la-console-texte)
- [Exécution Sans Surveillance](#exécution-sans-surveillance)
- [Vitesse et Mode Turbo](#vitesse-et-mode-turbo)
- [Écriture de Scripts](#écriture-de-scripts)
- [Tests](#tests)
- [IOPORTS](#ioports)
- [Limitations Connues et Fonctionnalités Prévues](#limitations-connues-et-fonctionnalités-prévues)

## Organisation du Projet

```
.
├── Makefile              définitions de compilation
├── README.md             ce fichier (README.es.md, README.fr.md : traductions espagnole et française)
├── LICENSE
├── TODO.md
├── src/                  toutes les sources C
│   ├── lexer.l           scanner flex des lignes de l'invite / des fichiers de commandes
│   ├── parser.y          grammaire bison ; remplit une commande analysée (cmd_t)
│   ├── command.c         table des commandes, abréviations, et tous les exécuteurs
│   ├── asm_lexer.l       scanner flex de l'assembleur à la volée (préfixe `asm`)
│   ├── asm_parser.y      grammaire bison de l'assembleur à la volée (préfixe `asm`)
│   ├── assembly.c        table des mnémoniques, symboles, encodage des instructions
│   ├── gpu.c             GPU logiciel, tampon d'image, capture d'écran PNG
│   ├── spu.c             modèle des canaux / sons du SPU, chargement VSND
│   ├── savestate.c       sauvegarde / restauration de l'état de la machine
│   ├── source.c          superposition de la ligne source assembleur / C / Lua
│   ├── srcdebug.c        débogage au niveau source : table des lignes, pas / finish / until par ligne,
│                         cadres et bt, break FILE:LINE, list
│   ├── watchpoint.c      points de surveillance de registres / IOPorts / mémoire
│   ├── args.c help.c     options de la ligne de commande, le texte d'utilisation et le `help` de la session
│   └── input.c           boucle de l'invite, readline, lecture des fichiers de commandes, fichiers de débogage
├── inc/                  tous les en-têtes (command.h, version.h, ...)
├── obj/                  objets, fichiers de dépendances, et les
│                         lexer.c / parser.c / parser.h générés et leurs équivalents asm_*
├── bin/                  le binaire v32sim compilé (généré)
├── bios/                 fichiers de données du BIOS
├── gen/                  sortie de flex/bison, versionnée (`make gen`), pour les compilations sans flex/bison
├── tools/                scripts et outils oracle pour tester d'autres projets (docs/TOOLS.md)
│   └── oracle/           outils C++ sur la ConsoleLogic d'origine : v32orun (refrun v32run vrun v32prof v32peek v32shot l'enveloppent) sysprof
├── tests/                les suites de tests (voir Tests ci-dessous), sources de cartouches, cas différentiels (tests/gaps)
├── docs/                 USAGE.md (référence de la ligne de commande, avec les versions espagnole et française USAGE.es.md / USAGE.fr.md),
│                         TOOLS, TESTING, PROFILING, PERFORMANCE, les spécifications et documents d'état ;
│                         agents/ (une fiche par outil), proposals/ (demandes ouvertes aux autres projets)
├── man/                  pages de manuel : v32sim.1 (v32sim.es.1, v32sim.fr.1)
├── AGENTS.md CLAUDE.md   notes pour les agents (CLAUDE.md importe AGENTS.md)
└── cart/                 fichiers de données de cartouches
```

## Prérequis

- Un compilateur C (GCC ou Clang)
- **GNU readline** — édition de la ligne de l'invite, touches fléchées et
  historique des commandes (sous macOS la fonction d'historique fait partie de
  readline ; sous Linux `libhistory` est une bibliothèque séparée — le Makefile
  gère les deux)
- **flex** et **bison** (toute version raisonnablement récente ; la grammaire
  évite les directives propres à une version, donc le bison 2.3 / flex 2.5.35
  fournis avec macOS fonctionnent)
- libm (standard sous Linux et macOS)

Les expressions régulières POSIX ne sont plus utilisées, et la sortie PNG n'a
aucune dépendance de bibliothèque (l'écrivain de captures d'écran est intégré).

## Compilation

```
make            # compilation par défaut -> bin/v32sim
make LTO=       # sans optimisation à l'édition de liens (par défaut : -O2 -flto quand le compilateur l'a ; OPT=-O3 etc.)
make debug      # compilation -DDEBUG -g -O0   -> bin/v32sim
make asan       # compilation AddressSanitizer + UBSan, pour traquer les erreurs mémoire
make gen        # rafraîchit gen/ (la sortie de flex/bison) après modification d'un fichier .l ou .y
make version    # inscrit la version de inc/version.h dans TODO.md et la ligne .TH des pages de manuel (VERSION=X, ou V=X, écrit d'abord X dans inc/version.h)
make test       # tests/run_kbd_sync_tests.sh, run_cli_tests.sh, run_v32dev_server_tests.sh : n'ont besoin d'aucune chaîne d'outils
make check TC=DIR [V32IO=DIR] [SRCMARK=FILE]   # chaque suite à partir d'un répertoire de chaîne d'outils (tools/v32sim-check.sh)
make test-harness                              # tests/run_harness_tests.sh (V32TC, V32BIOS, ORACLE dans l'environnement)
make oracle COMPUTERSOFTWARE=DIR               # compile les outils de la logique de console d'origine (tools/oracle/build.sh)
make archive    # ../v32sim-project-YYYYMMDD.zip (la date de VERSION) de l'arborescence nettoyée, avec .github et .gitignore
make dist       # v32sim-<version>-<os>-<arch>.zip pour <Program Files>/Vircon32/v32tools : v32sim, docs/v32sim/, tools/v32sim/ (lancez make avant)
make clean      # supprime le contenu de obj/ (y compris l'analyseur généré) et bin/v32sim
```

Sans flex et bison, la sortie versionnée de `gen/` est utilisée (comme le fait `tools/build-nogen.sh`).
N'ajoutez pas `-ffast-math` ni `-march=native` : le GPU et les instructions flottantes doivent donner le même dernier bit sur chaque hôte.

Les fichiers objets et de dépendances atterrissent dans `obj/` ; le binaire
atterrit dans `bin/v32sim`. Les dépendances des en-têtes sont suivies :
modifier un en-tête de `inc/` recompile chaque source qui l'inclut. Les
fichiers objets **ne sont pas** recompilés quand les options changent —
lancez `make clean` quand vous passez entre les compilations par défaut,
`debug` et `asan`.

## Installation

```
make install      # le binaire dans ~/bin/bin.<arch>/ (ou ~/bin/, créé s'il manque) ; l'arborescence tools/ dans ~/.local/lib/v32sim/tools ;
                  # la page de manuel (compressée avec gzip) dans ~/.local/share/man/man1
make sysinstall   # le binaire dans /usr/local/bin/ ; l'arborescence tools/ dans /usr/local/Vircon32/v32tools/v32sim/tools ;
                  # la page de manuel (compressée avec gzip) dans /usr/local/share/man/man1
make uninstall    # et make sysuninstall
```

`sysinstall` prend `PREFIX` (par défaut `/usr/local`), `VROOT` (par défaut `$(PREFIX)/Vircon32/v32tools`), `MANDIR` (par défaut
`$(PREFIX)/share/man/man1`) et `DESTDIR` ; `install` prend `USRLIB` (par défaut `~/.local/lib/v32sim`) et `USRMAN` (par défaut
`~/.local/share/man/man1`). `DEST_LANG=en|es|fr` installe la page de cette langue (`man/v32sim.es.1` ...) sous le nom `v32sim.1`, de sorte que `man v32sim`
l'affiche ; non défini ou `all`, il installe la page anglaise. Les outils de `tools/` cherchent `v32sim` à côté d'eux (`../bin/v32sim`, dans une copie de travail),
puis dans `$PATH` ; les autres projets les atteignent par `$V32SIM_HOME/tools`. Les deux cibles suppriment ce qu'une version plus ancienne avait installé dans
`share/v32sim`.

Les zips de version (`make dist`) se décompressent dans `<Program Files>/Vircon32/v32tools` (ou `/usr/local/Vircon32/v32tools`) : `v32sim` à la racine, le README, le manuel d'utilisation et les pages de manuel en anglais, espagnol et français dans `docs/v32sim/`, les scripts auxiliaires dans `tools/v32sim/` (ils trouvent le binaire via `$V32SIM` ou `$PATH`). Windows (x86_64) est une compilation facultative et expérimentale : elle est faite dans l'environnement POSIX de MSYS2 et son zip contient les DLL d'exécution de MSYS à côté de `v32sim.exe` ; elle n'est publiée que si cette compilation a réussi. Le reste de `docs/` (documents de conception, d'état et de propositions) n'existe qu'en anglais.

## Utilisation

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
     --heap[=PROG.asm]     v32lua carts: at each frame end print HEAP F<frame> ptr=0x.. used=N peak=N [free=N blocks=N] (with --gc arena also
                           floor= pin= depth= stack=) and HEAP PEAK / HEAP END at exit; the %define symbols are read from the
                           cart's .asm (found beside it) or from PROG.asm
     --expect-reach LABELS exit 0 with EXPECT REACHED <label> frame=N ip=0x.. when the CPU reaches one of the labels
                           (comma separated, from the debug files); otherwise EXPECT MISSED <labels> and exit status 1
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
                           (always: 1 a check failed (expect, until, --expect-reach) or a bad option, 4 --batch could not parse a command, 5 --dump failed,
                           N 'quit N'; at startup 11 a file cannot be opened, 12 not read, 21 not a V32 file)
 -h, --help                display this information

FILE   is any path plus the filename desired
OFFSET is the full 32-bit/4-byte memory address (hex)
OPCODE is the full 32-bit/4-byte instruction hex
```

Ceci est la sortie de `--help` ; [docs/USAGE.fr.md](docs/USAGE.fr.md) (et `man v32sim`) décrivent chaque option, les variables
d'environnement, les fichiers et les codes de sortie. Les options qui prennent un argument l'acceptent sous la forme `--opt=VALEUR` ou `--opt VALEUR` ; les
facultatives (`--v32kbd`, `--v32mouse`, `--turbo`, `--fixed-clock`, `--screengrid`, `--v32dev-doorbell`) seulement sous la forme `--opt=VALEUR`.
`--quiet` est un autre nom de `--batch`, et `--stats`, `--stats-skip` et `--stats-top` ceux des options `--frame-stats`. `-w OPCODE` implique `-r`,
`--tty` implique `-r`, et `--kbd-input` implique `--v32kbd`. Un CARTFILE valant `-` signifie aucune cartouche : le BIOS s'exécute seul.
`-b OFFSET|LABEL|FILE:LINE` accepte ce qu'accepte la commande `break` et s'exécute une fois les fichiers de débogage chargés : un nom qui est
une étiquette (ou une fonction) connue désigne cette étiquette même s'il est aussi un nombre hexadécimal (`add`, `face`, `dead` ; écrivez
`0xdead` pour l'adresse), sinon c'est un OFFSET hexadécimal, avec ou sans `0x` ; une valeur qui n'est rien de tout cela est signalée sur
stderr. `-L MODE` doit être `C`, `lua` ou `src` ; toute autre valeur est une erreur (message sur stderr, code de sortie 1). `-S` démarre au
début du code du BIOS, ce qui est le comportement par défaut (`-E` démarre ailleurs).
`--screenshot-at` compte les images de la cartouche : l'image 0 est la première image qui se termine avec le compteur de programme dans la
cartouche, c'est-à-dire une fois que le BIOS a passé la main (le logo du BIOS standard dure environ 250 images) ; `--max-frames` compte toutes
les images depuis la mise sous tension, logo compris, donnez-lui donc assez d'images (`--max-frames=400` pour l'image 120 de la cartouche).
Les images que l'exécution n'a jamais atteintes sont signalées sur stderr à la fin (`[WARNING] --screenshot-at: ...`).

`v32sim --version` affiche `v32sim ` et la version (`YYYYMMDD`, `-dev` sur une compilation de développement). La version est la
définition `VERSION` de `inc/version.h` ; elle apparaît dans la bannière, le texte d'utilisation,
la commande `version` de l'invite, et `make version`. Elle se remplace à la compilation avec
`-DVERSION=\"...\"`.

Exemples :

```
v32sim cart.v32                     # démarre dans le débogueur
v32sim -r cart.v32                  # exécute simplement la cartouche
v32sim -b main -C setup.cmds cart.v32
v32sim -L lua -r cart.v32           # affichage des valeurs Lua
v32sim -q --fast cart.v32 < t.cmd   # exécution par script, voir Écriture de Scripts
```

Par défaut, le simulateur charge `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32`
(l'emplacement d'installation standard de ComputerSoftware pour Vircon32) ; remplacez-le avec `--biosfile`.
L'exécution commence à la première instruction du BIOS (`0x10000004`) sauf si `--entry-point` en dispose autrement.

## Plan de la Mémoire

| Page | Première adresse | Notes |
|------|------------------|-------|
| RAM  | `0x00000000` | 16 Mo / 4 M mots, lecture-écriture |
| BIOS | `0x10000000` | point d'entrée à `0x10000004`, gestionnaire d'erreurs à `0x10000000` |
| CART | `0x20000000` | ROM du programme de la cartouche |
| MEMC | `0x30000000` | carte mémoire |

## Le Débogueur

Quand il n'est pas lancé avec `--run`, le simulateur s'arrête à la première
instruction et présente l'invite `v32sim>`. À partir de là vous pouvez
avancer pas à pas (`step`), passer par-dessus les sous-routines (`next`),
exécuter librement (`continue`), et faire en sorte que l'exécution s'arrête à
nouveau sur des points d'arrêt, des points de surveillance ou un opcode
surveillé.

- **Ctrl-C (SIGINT)** fait sortir du mode d'exécution et revenir à l'invite
- **EOF sur stdin** (par ex. Ctrl-D, ou un tube d'entrée fermé) quitte
  proprement — pratique pour piloter v32sim depuis des scripts
- Lors d'une **erreur système**, l'état de la machine est stocké dans R0-R3,
  SP/BP sont effacés, l'exécution est redirigée vers le gestionnaire d'erreurs
  du BIOS à `0x10000000`, et le simulateur s'arrête à l'invite pour vous laisser
  inspecter la faute

### Fichiers de Commandes

`--command-file=FILE` (ou `-C`) fournit au simulateur un fichier texte brut
avec **une commande d'invite valide par ligne** (n'importe quelle commande :
points d'arrêt, mise en place de la liste d'affichage, `continue`, etc.). Les
lignes vides et les lignes (ou fins de ligne) commençant par `#` sont des
commentaires. C'est la manière principale d'automatiser des sessions ;
combinez-la avec `--run` et un stdin/stdout redirigés pour des tests par
script.

## Commandes

Les commandes ne sont pas sensibles à la casse et peuvent être abrégées en un préfixe d'une longueur minimale que
`src/command.c` fixe pour chacune (`s` = step, `n` = next, `c` = continue,
`b` = break, `p` = print, `x` = x, `se` = set, `bt` = backtrace, `pro` = profile, `in` = inventory,
`lo` = load, ...). Une ligne vide répète le dernier step/next/continue. Les lettres seules sont `b` break, `c` continue,
`d` display, `g` gamepad, `h` help, `i` ignore, `l` label, `n` next, `p` print, `q` quit, `r` replace, `s` step, `w` watch,
`x` x et `?` help. Quand plusieurs commandes commencent par les mêmes lettres, la première de la table de `src/command.c` l'emporte : `r` est
donc `replace`, pas `run` (`ru` est `run`) ; en cas de doute, tapez le nom entier dans les scripts.

| Commande   | Description |
| ---------- | ----------- |
| `break [0xMEM\|LABEL\|FILE:LINE\|LINE]` | pose un point d'arrêt d'exécution (adresse, étiquette, fonction ou ligne source ; sans argument : les liste) |
| `tbreak ...` | pareil, supprimé quand il est atteint (sans argument : liste les points d'arrêt, comme `break`) |
| `unbreak #` | supprime un point d'arrêt par son indice |
| `continue` | reprend l'exécution jusqu'au prochain déclencheur |
| `step` | exécute l'instruction courante, s'arrête à la suivante ; dans les modes source (`set mode = src\|C\|lua`) exécute jusqu'à la ligne source suivante, en entrant dans les appels |
| `next` | passe par-dessus les sous-routines ; dans les modes source exécute jusqu'à la ligne source suivante, les appels s'exécutant jusqu'à leur retour |
| `finish` | exécute jusqu'au retour de la fonction du cadre sélectionné |
| `until [FILE:LINE\|LINE]` | exécute jusqu'à une ligne ultérieure de la fonction (sort d'une boucle), ou jusqu'à cette ligne |
| `list [FILE:LINE\|LINE\|FUNCTION]` | affiche la source autour de la ligne courante (ou de celle-ci) |
| `frame [N]` / `up [N]` / `down [N]` | sélectionne un cadre de pile |
| `print/fmt XYZ` | affichage ponctuel de XYZ (registre, mémoire, IOPort, symbole) |
| `x/fmt SYMBOL\|0xADDR [COUNT]` | affiche COUNT mots à partir d'un symbole (par défaut : toute la variable C) |
| `display/fmt XYZ [LABEL]` | ajoute un élément à la liste d'affichage montré à chaque arrêt |
| `undisplay #` | supprime un élément de la liste d'affichage par son indice |
| `label [0xMEM_ADDR LABEL]` | liste ou définit une étiquette pour un décalage mémoire |
| `unlabel #` | supprime une étiquette par son indice |
| `watch [LOC OP VALUE [LABEL]]` | pose un point de surveillance sur un registre, un IOPort ou un mot mémoire (sans argument : les liste) |
| `watchlist` | liste les points de surveillance actifs |
| `run N` / `wait N` | continue pendant N images, puis s'arrête |
| `until "TEXT" [timeout N] [rows N]` | continue jusqu'à ce que la console texte affiche TEXT |
| `expect [print] LOC OP VALUE` / `expect console "TEXT"` | vérifie une valeur ou l'écran ; un échec met le code de sortie à 1 |
| `console [FILE]` / `console --attrs[=fg\|bg\|all] [FILE]` | affiche ou écrit l'écran texte (avec les grilles de couleurs) |
| `unwatch #\|LABEL` | supprime un point de surveillance |
| `backtrace` (`bt`) | liste les appels de sous-routines, le plus récent d'abord ; avec les informations de ligne Lua/C, les cadres BP sous la forme `#0 f (file:line)` |
| `locals [NAME]` / `info locals [NAME]` | variables locales du cadre sélectionné avec types et valeurs (voir *Variables Locales*) |
| `profile` | affiche le rapport de profilage (nécessite `-p`) |
| `inventory` | vue d'ensemble des ressources du système |
| `gamepad [# [COMMAND]]` | affiche/sélectionne/simule l'état d'une manette (`gamepad 1` sélectionne la manette 1) |
| `kbd [attach [PORT]\|detach\|type TEXT\|key KEY [MODE]\|input SOURCE\|off\|clear\|sync [on\|off]]` | le clavier v32kbd (voir *Le Clavier v32io*) |
| `mouse [attach [PORT]\|detach\|move DX DY\|click\|down\|up [BUTTON]\|wait N\|clear]` | la souris v32mouse (voir *La Souris v32io*) |
| `mailbox [CHANNEL]` | la boîte aux lettres v32dev de la carte mémoire : activité, anneaux, descripteurs, données en attente |
| `dev [info\|format [force] [N=TYPE ...]\|attach N SPEC\|detach N\|swap N SPEC]` | l'hôte v32dev natif (voir *Carte Mémoire et v32dev*) |
| `mark [NOTE]` | avec `--v32trace` : dépose un marqueur dans le rapport de session |
| `load cart:path` / `load memc:path` | charge un composant à l'exécution |
| `unload bios\|cart\|memc` | décharge une page mémoire |
| `replace [IP:0xADDR] [IR:0x...] [IV:0x...]` | patche l'instruction (et l'immédiat) à IP/ADDR en mémoire |
| `asm [@0xADDR\|-x] INSTRUCTION` | assemble une instruction : la patche (à IP ou ADDR), ou l'exécute une fois avec `-x` |
| `screenshot [--plain] [FILE]` | écrit l'écran du GPU dans un fichier PNG |
| `stats [show\|regions [N]\|reset\|on\|off\|save FILE [json]\|expect NAME[.AGG] OP VALUE]` | compteurs GPU / SPU / bus (dessins, abandonnés, changements de région, mots MOVS...) par image et par région ; `expect stats NAME.AGG OP VALUE` |
| `highlight [TEX:REGION [COLOR]]` | entoure une région dans les captures d'écran (`clear`, `on`, `off` ; aussi appelée `boundbox`) |
| `screengrid [on\|off] [DX [DY]] [color C] [alpha N] [labels on\|off]` | pose une grille sur les captures d'écran |
| `noextra [on\|off]` | captures d'écran sans grille ni contours (`screenshot --plain` pour une seule) |
| `save [FILE]` / `restore FILE` | écrit / restaure l'état complet de la machine |
| `set [NAME=VALUE]` | définit des réglages, des registres, la mémoire ou des IOPorts (sans argument : liste les réglages) |
| `ignore` | saute l'instruction courante (elle n'est pas exécutée et les compteurs n'avancent pas) |
| `version` | affiche la version du simulateur |
| `help [COMMAND]` / `?` | aide sur les commandes |
| `quit [N]` | quitte le simulateur (avec le code de sortie N) |

### Suffixes de Formatage

`print` et `display` acceptent un suffixe de format. Les opérandes numériques sont en hexadécimal : jusqu'à
3 chiffres, c'est un IOPort, au-delà une adresse mémoire (`ram:0x75` est le mot 0x75 de la RAM ; `print/x 0x205`,
`print GPU_ClearColor`, `print R3`, `print reg`, `print [SP]`) :

- `/X` hexadécimal en majuscules (par défaut), `/x` hexadécimal en minuscules
- `/u` décimal non signé, `/d` décimal signé, `/o` octal, `/b` binaire
- `/f` virgule flottante
- `/B` booléen (TRUE/FALSE)
- `/D` décode la valeur comme une instruction
- `/s` chaîne (un mot de 32 bits par caractère ; en mode Lua, déballe les
  chaînes marquées)

### Points de Surveillance

`watch  LOCATION  OP  VALUE  [LABEL]` s'arrête quand la comparaison *devient*
vraie. `LOCATION` est un registre (R0-R15, BP, SP, CR, SR, DR, IP, IR, IV), un
IOPort (un symbole, ou de l'hexadécimal de 3 chiffres au plus), ou un mot mémoire (hexadécimal de plus de
3 chiffres) ; `OP` est l'un de `=`, `==`, `!=`, `<`, `>`, `<=`, `>=` ; `VALUE` est un
nombre (hexadécimal, binaire, octal, décimal), un registre, un symbole d'IOPort, ou le
contenu de `[0xMEM]`, `[IOPORT]` ou `[REG]` ; l'étiquette LABEL est facultative :

```
watch SP <= 0x20000000 stack_underflow
watch 0x00001000 != 0
watch TIM_FrameCounter = 60
watch R1 > [0x00000010]
```

Les points de surveillance sont déclenchés sur front : une condition déjà vraie quand il est posé,
ou qui reste vraie, ne s'arrête pas à nouveau tant qu'elle n'a pas été fausse entre-temps.
Ils sont vérifiés après chaque instruction et chaque fois que l'état de la machine change
(donc une valeur qui change et revient en arrière au sein d'une même instruction est vue).
Supprimez avec `unwatch #` ou `unwatch LABEL` ; `watchlist` (ou `watch` seul)
les liste. Les comparaisons sont non signées sur 32 bits.

### Assemblage à la Volée

`asm` assemble une instruction avec la syntaxe de l'assembleur Vircon32 :

```
asm mov R1, 0x10                       # patche l'instruction à l'IP courant
asm @0x20000100 out GPU_Command, GPUCommand_ClearScreen
asm -x iadd R0, -1                     # exécute une fois, mémoire intacte
```

Les opérandes sont des registres (`R0`-`R15`, `CR`, `SR`, `DR`, `BP`, `SP`), des nombres
(décimal, `0xHEX`, `0bBINAIRE`, `0OCTAL`, flottants), des étiquettes issues des fichiers de
débogage chargés, des noms d'IOPorts, et les constantes `GPUCommand_*`, `GPUBlendingMode_*`,
`SPUCommand_*` et `SPUChannelState_*`. Un entier décimal donné à une instruction
flottante est converti (`fadd R4, 1` ajoute 1.0) ; l'hexadécimal, le binaire et l'octal
sont des motifs de bits bruts. Un patch ne peut pas être plus long que l'instruction qu'il
remplace ; un patch plus court est complété par `MOV R0, R0` (un no-op). La sortie assemblée
de chaque instruction du BIOS a été vérifiée comme identique à son code machine mot pour mot.

### Captures d'Écran

`screenshot [FILE]` écrit le tampon d'image 640x360 du GPU dans un PNG (par défaut
`v32sim.DATESTAMP.png`). Le moteur de rendu est un modèle logiciel du GPU de la console
(échantillonnage au plus proche voisin, même budget de pixels et même mélange), donc une capture d'écran
montre ce que la cartouche a dessiné jusqu'ici pendant cette image.

`highlight TEXTURE:REGION [COLOR]` (aussi écrit `boundbox`) marque une région (`bios:REGION` pour la texture du BIOS ; COLOR est un nom ou RRGGBB, rouge par défaut) : chaque
quad que le programme en dessine, mis à l'échelle ou pivoté compris, reçoit un contour d'un pixel juste à l'extérieur dans les captures d'écran. Les contours
sont peints sur le PNG seulement, jamais sur l'écran que voit le programme ; ils couvrent les quads dessinés pendant cette image ou la dernière (un clear_screen
oublie les précédents), et un dessin que le GPU a abandonné faute de pixels n'en a pas. `highlight` liste, `highlight clear [TEX:REGION]`
oublie, `highlight off|on` les cache et les montre. Seuls les dessins faits après la commande sont entourés : laissez s'exécuter une image avant la capture d'écran.

`screengrid [on|off] [DX [DY]] [color C] [alpha PERCENT]` mélange une grille claire aux captures d'écran, pour juger où les choses ont été dessinées : verte à
55 % tous les 40 x 40 pixels par défaut (16 x 9 cellules carrées ; 8, 10, 20 et 40 divisent également 640 et 360 ; `screengrid 32 30` donne 20 x 12
cellules). Les contours de `highlight` sont dessinés par-dessus. `noextra [on|off]` laisse la grille et les contours hors de chaque capture d'écran tout
en conservant leurs réglages, et `screenshot --plain [FILE]` le fait pour une seule. La grille porte les coordonnées en pixels sur ses lignes (`screengrid labels off`
les désactive) ; des lignes plus proches que la taille d'une étiquette les partagent. Rien de tout cela ne touche l'écran que voit le programme.

### Sauvegarde et Restauration

`save [FILE]` écrit l'état de la machine (registres, IOPorts, RAM, carte mémoire,
régions et tampon d'image du GPU, SPU, timers, manettes ; par défaut
`v32sim.DATESTAMP.sav`), et `restore FILE` le recharge. Le fichier est
en petit-boutiste avec les suites de zéros compressées, et enregistre des sommes de contrôle du BIOS et de la
cartouche : il n'est restauré que dans une machine qui a les mêmes. Les points d'arrêt,
points de surveillance, la liste d'affichage, les étiquettes et les données de profilage font partie de la
session de débogage et ne sont pas sauvegardés.

### Liste d'Affichage

La liste d'affichage est rendue à nouveau chaque fois que le simulateur s'arrête.
Les éléments peuvent être des registres (déréférençables avec `[...]`), des adresses mémoire ou des plages, ou
des IOPorts (numériques comme `0x205` ou symboliques comme `GPU_SelectedTexture`). Si
aucune étiquette n'est donnée pour un élément IOPort, son nom symbolique est utilisé.

### Étiquettes

`label 0x10000040 main` associe un nom à un décalage ; les étiquettes peuvent ensuite
servir pour les points d'arrêt (`-b main` ou `break main`) et comme opérandes de `asm`.
Les étiquettes sont aussi chargées automatiquement à partir des fichiers de débogage de l'assembleur / du C (voir ci-dessous).

### Fichiers de Débogage

À côté de chaque fichier V32, v32sim cherche ses fichiers de débogage : `NAME.vbin.debug` (celui de l'assembleur : `v32as -g`, ou le
`assemble -g` d'origine) et `NAME.asm.debug` (le fichier annexe du compilateur C), puis `NAME.vbin.vars`, `NAME.vbin.sym` / `NAME.asm.sym` et `NAME.asm`
(voir *Variables Locales* et *Symboles*). Chacun est cherché dans cet ordre : le répertoire donné par `--debug-dir` (s'il y en a un), le répertoire du fichier V32,
son `obj/`, son `../obj/`, le `obj/` du répertoire courant, et le répertoire courant. `--bios-asm-debug` et consorts nomment un fichier
explicitement. `-n` désactive tout cela. Les fichiers de débogage contiennent une entrée `OFFSET,FILE,LINE[,LABEL]`
par instruction.

Avec `--source` (ou `set source=true`) la ligne source de l'instruction courante
est affichée à chaque arrêt : les lignes assembleur par défaut, les lignes C ou Lua
avec `-L src` (n'importe quel langage ; `-L C` et `-L lua` sont des alias qui conservent aussi
leurs anciens effets supplémentaires) ou `set mode = src`. L'indication du langage (`ASM`, `C`, `LUA`)
vient de l'extension du fichier. Les noms de fichiers dans les fichiers de débogage sont relatifs au répertoire
dans lequel le programme a été compilé, donc ils sont essayés tels quels et relativement aux
répertoires où les fichiers de débogage et V32 ont été trouvés (et à leurs parents).

### Set

```
set color=true      set deref=true       set debug=true
set verbose=true    set errorchk=true    set profile=true
set source=true     set mode=C           # superposition de la source, et quelle source (asm, src, C ou lua)
set R4=0x4004       set IP=0x10000040    # IP est un saut
set 0x00224466=0x71
set GPU_SelectedTexture=-1
```

Les booléens acceptent true/false, on/off, yes/no, 1/0 (`color` aussi sous la forme `colors`, `deref` sous la forme `derefaddr`, `errorchk` sous la forme `errorcheck` ou `error`).
`set` sans argument liste les réglages courants. Les cibles mémoire et IOPort sont validées.

### Chargement/Déchargement

`load  cart:path` et `load memc:path` échangent des composants à l'exécution
(principalement pour changer de carte mémoire). `unload bios|cart|memc` libère la
page. `load` vérifie d'abord que le fichier peut être ouvert, et `unload` refuse une page
qui n'est pas chargée. `load bios:path` remplace le BIOS (ses étiquettes sont
rechargées et l'instruction lue est rafraîchie).

## Débogage au Niveau Source

Les fichiers de débogage portent le fichier source et la ligne de chaque instruction (la convention est [docs/V32DBG-SPEC.md](docs/V32DBG-SPEC.md)) :
`v32as -g` écrit `ADDRESS,FILE,LINE[,FUNCTION]` d'après les marqueurs `;@src FILE:LINE` d'un
programme Lua (`v32lua -g`) ou C, et le fichier annexe du compilateur C est rapproché des adresses
comme avant. v32sim garde une table de lignes triée à partir de ce qu'il trouve et bâtit dessus les commandes
au niveau des lignes. `set mode = asm` (le défaut) garde `step` et `next` comme des pas d'instruction. Dans les
modes source (`set mode = src|C|lua`, ou `-L src|C|lua`) les mêmes `step`, `next`, `s`, `n` et la
répétition par ligne vide avancent par **ligne source** à la place ; sans information de ligne chargée, ils restent des pas
d'instruction.

| Commande | Action |
| -------- | ------ |
| `step` (modes source) | exécute jusqu'à ce que la ligne source change (un autre fichier, une autre ligne ou fonction, ou un saut en arrière au début d'une ligne : un nouveau tour de boucle), en entrant **dans** les appels |
| `next` (modes source) | pareil, mais un `CALL` s'exécute jusqu'à son retour (le pointeur de pile doit être revenu où il était : sûr pour la récursion ; aucun `RET` n'a besoin d'être vu) |
| `finish` | exécute jusqu'au retour du cadre sélectionné ; fonctionne aussi à la première instruction d'une fonction (voir *Cadres de Pile*) |
| `until` | comme `next`, mais ne s'arrête que sur une ligne **ultérieure** de cette fonction (ou à son retour) : sort d'une boucle |
| `until FILE:LINE` | exécute jusqu'à cette ligne, ou jusqu'au retour de la fonction courante |
| `break FILE:LINE`, `break LINE` | la plus basse adresse de la ligne ; `break -a` toutes. Une ligne sans code passe à la ligne suivante qui en a, et le dit |
| `tbreak ...` | à usage unique |
| `list`, `list LINE`, `list FILE:LINE`, `list FUNCTION` | dix lignes de source, `=>` marquant la ligne courante ; `list` à nouveau continue |
| `bt`, `frame N`, `up`, `down` | les cadres, le cadre sélectionné (utilisé par `list` et `finish`) |

### La pile de débogage : C, C++ et Lua, optimisé ou non

v32sim est l'extrémité d'un pipeline de débogage partagé par tous les outils de langage Vircon32 :

```
source ->  v32cc -g | v32c++ -g --compiler=v32cc | v32lua -g   ->  [v32opt -O2/-O3]  ->  v32as -g  ->  v32sim
```

`tools/v32mkcart.sh [-O LEVEL] SRC.c|.cpp|.lua|.asm` exécute la bonne chaîne (définissez `V32TC` sur le répertoire
de la chaîne d'outils) et laisse `NAME.vbin.debug`, `.vbin.vars` et `.asm.sym` à côté de la cartouche. Les numéros de ligne
suivent le fichier que vous avez écrit (pour les macros C/C++, la ligne de l'*utilisation*, pas de l'en-tête). Après `v32opt`,
une variable gardée dans un registre est suivie par ses enregistrements `;@home` ; une variable dont le registre
a disparu affiche `<optimized out>`. `tests/run_stack_tests.sh` vérifie tout cela sur un programme écrit dans trois
langages à -O0/-O2/-O3 (fait partie de `make check`). État par outil : [docs/V32DBG-STATUS.md](docs/V32DBG-STATUS.md).

`break LINE` est un nombre **décimal** inférieur à `0x10000000` quand les informations de ligne sont chargées ;
les adresses s'écrivent en hexadécimal (`break 0x20000012`), comme avant. `break FUNCTION` pose le
point d'arrêt sur la première instruction de la fonction (pas sur chaque ligne, bien que toutes les entrées de l'assembleur
portent le nom de la fonction). FILE peut être un nom de base (`bench.lua`) ou n'importe quelle fin de
chemin.

Chaque arrêt affiche une ligne, `[step] bench.lua:4 in add:   local s = a + b` (sur stderr en
mode `--batch`, comme les autres raisons d'arrêt ; `bt`, `list` et `frame` affichent sur stdout).

### Cadres de Pile

v32cc et v32lua utilisent tous deux des cadres `PUSH BP; MOV BP,SP`, donc après le prologue l'adresse
de retour est à `[BP+1]` et le BP de l'appelant à `[BP]`. Le cadre de la fonction en cours est
reconnu à partir de ses instructions, il est donc aussi correct à sa première instruction (rien
n'est encore empilé), après `PUSH BP`, et à `POP BP; RET`. Le code sans source (voir ci-dessous) est
supposé sans cadre et entré par un `CALL` : l'adresse de retour est au sommet de la pile, si
le mot qui s'y trouve est l'adresse après un `CALL`. Le parcours s'arrête à une adresse qui n'est pas du code,
ou à un BP illisible ou non croissant.

`backtrace` utilise ces cadres quand les données de débogage concernent Lua ou C (un fichier qui n'est pas
`.asm`) ; pour les programmes en assembleur il continue de lister les cibles des `CALL`, comme il l'a toujours fait.
Le code compilé avec `-O2` et `omit-frame-pointers` n'a pas de chaîne : `next` et `until` n'en
ont pas besoin (ils utilisent l'instruction `CALL` et SP), `finish` et `bt` si.

### Variables Locales

`locals [NAME]` (aussi `info locals`) liste les variables visibles dans le cadre sélectionné (`frame`, `up`, `down`) avec
leurs types et leurs valeurs ; `print NAME`, `x NAME`, `display`, `watch` et `expect` trouvent une variable locale de ce cadre avant une globale du
même nom (son adresse est figée quand la commande est donnée). L'information est le fichier `NAME.vbin.vars`, que `v32as -g` écrit
à partir des lignes `;@var` / `;@scope` de `v32cc -g` et `v32lua -g`, trouvé comme les autres fichiers de débogage :

```
v32sim> locals
a = 3    (int, BP+2)
arr = {3, 4, 7, 12}    (int[4], BP-4)
p = {0x00000003, 0x00000005}    (struct:pt:2, BP-8)
q = 0x003FFFE2    (ptr, BP-9)
i = 99    (int, BP-10)
```

Types : `int uint float bool ptr`, tableaux `T[N]` (16 éléments affichés), `struct:`/`union:` (membres par nom, `p = {x = 3, y = 5}`, quand le compilateur les écrit dans `type=` ; sinon en mots bruts), `lua` (valeurs
NaN-boxées : nombres, chaînes, nil/true/false, tables, fonctions ; une variable locale `boxed`, capturée par une fermeture, est lue à travers sa
boîte). Une variable locale que l'optimiseur a déplacée dans un registre (`loc=R5`) n'est affichée que dans le cadre 0 ; `-`/`optimized-out` l'indiquent. De deux
variables de même nom, celle du bloc le plus interne l'emporte. Une variable n'est affichée que dans la fonction où le fichier de débogage place
l'instruction, parce que v32as termine les plages d'une fonction C à la fin du programme quand v32cc n'écrit pas de `;@fn` (sans
`--emit-hints`).

### Code Sans Source

Une entrée dont le fichier est `-` (ou dont la ligne est 0), par ex. `0x20000060,-,0`, indique que les
instructions de cette adresse jusqu'à l'entrée suivante n'ont pas de source : une bibliothèque d'exécution. C'est
ce que signifie `;@src -`. `step` traverse un tel code sans s'arrêter, `bt` affiche
`?? [0x...]` et `finish` en revient. `v32as -g` écrit une telle entrée par plage `;@src -` ;
un fichier de débogage qui n'en a pas (un `v32as` plus ancien) fait que la dernière ligne avant la bibliothèque d'exécution s'étend jusqu'à l'entrée suivante.

## Carte Mémoire et v32dev

Un fichier de carte mémoire est `V32-MEMC` suivi de 262144 mots petit-boutistes. Le
simulateur le garde synchronisé avec la mémoire de la carte comme le fait le DesktopEmulator :
chaque écriture marque sa page de 256 mots, et les pages modifiées sont écrites à la
fin de chaque image.

**v32dev** (le transport hôte-périphérique du projet v32io) utilise les 16384 derniers
mots de la carte comme boîte aux lettres à quatre canaux. Le côté hôte est un processus
séparé (`v32dev.py serve CARD --ch1 storage:disk.img`) qui lit et écrit
le *fichier* de la carte pendant que le programme s'exécute. Pour que cela fonctionne, le simulateur
joue le rôle de l'émulateur :

  * l'**en-tête** (indicateur d'activité et battement de cœur, mots 8 et 9 de la région)
    est tenu en mémoire seulement et jamais écrit dans le fichier
  * la **zone propre à l'hôte** est relue depuis le fichier au début de chaque
    image, et jamais écrite ; le programme ne peut pas écraser ce que l'hôte a écrit
  * parmi les parties **propres à la cartouche**, les 16 mots de contrôle de chaque canal (les indices
    d'anneau) sont écrits après tout le reste, de sorte qu'un hôte qui voit un nouvel indice
    voit aussi les données qu'il annonce
  * une carte sans en-tête valide (ou avec `V32DEV_DISABLE` défini à autre chose que `0`) est une carte
    ordinaire ; rien de tout cela ne s'applique

`mailbox [CHANNEL]` montre l'état complet : activité, battements de cœur, indices d'anneau,
descripteurs et données en attente. `restore` laisse intacts l'en-tête et la zone
hôte d'une boîte aux lettres active.

La suite de tests de cartouches réelles v32dev du projet v32io (`dev_rom_test.py`)
passe sans modification avec v32sim : voir *Tests*.

**`--v32dev-doorbell[=MS]`** (désactivé par défaut ; P-E2 de v32nix, interface convenue avec v32nix le 2026-11-06) : activé, le mot 8 de l'en-tête
(indicateurs de l'émulateur, mémoire seulement) reçoit le bit 1 = « sonnette prise en charge » (bit 0 comme avant), et les mots 10..13 de l'en-tête (`0x3003C000 + 10 + c`) comptent, par
canal, les messages hôte-vers-cartouche rendus visibles : +1 chaque fois qu'un import de la partie hôte trouve `h2c_head` avancé. Une écriture du programme qui
modifie le `c2h_head` d'une partie de cartouche (`0x3003E020 + c*0x7F8 + 1`) est la sonnette : les pages modifiées sont écrites (données, puis mots de validation), un hôte
vivant dispose de jusqu'à MS millisecondes d'horloge murale (2 par défaut ; 0 = pas d'attente) pour répondre, la partie hôte du canal est relue, et le CPU
continue sans qu'aucun cycle soit facturé. Un hôte qui répond plus tard est importé à la frontière d'image, où le compteur avance alors : le code doit fonctionner
avec la réponse arrivée tôt ou à la frontière. L'attente est en horloge murale, donc une exécution avec un hôte externe qui répond n'est pas répétable au cycle près :
laissez l'option désactivée dans les tests différentiels (désactivée, l'en-tête, le mot d'indicateurs et le timing sont identiques au bit près à avant).
`mailbox` montre les anneaux, les attentes et les réponses précoces. Test : `tests/examples/bell.c` avec `tests/doorbell_host.py` (section du banc de test
« --v32dev-doorbell », nécessite `V32IO=` le projet v32io pour `v32dev.py format`). Le P-E1 de v32nix (interruption de timer) n'est pas construit : v32nix
l'a décliné (la préemption D30/D37 fonctionne sans ; consigné dans `docs/PROPOSALS-STATUS.md`).

**`--v32dev-host=N=SPEC`** (répétable ; sur demande ; VS-19) : un **hôte v32dev natif dans le simulateur**, pour les périphériques de stockage et série, avec le
comportement de `tools/v32dev.py` du projet v32io (la référence ; l'hôte Python et le patch du DesktopEmulator restent dans v32io, ceci est la
troisième implémentation). Il travaille directement sur la mémoire de la carte : pas d'aller-retour par le fichier de la carte, pas de second processus, pas d'horloge murale. Le canal N (0..3) est servi
dès la première image ; la carte (`-M FILE`) doit être une carte v32dev accessible en écriture, ou une dont les 64 derniers Kio sont tous à zéro (elle est alors formatée, comme le fait
`v32dev.py format`, l'en-tête étant écrit en dernier) ; d'autres données à cet endroit le font refuser, sauf si `--v32dev-host-force` est donné. Avec `V32DEV_DISABLE` défini
(à autre chose que `0`) la carte reste une carte ordinaire et l'hôte natif n'est pas démarré, avec un avertissement. SPEC, même syntaxe que `v32dev.py serve` (les options de
`storage:` sont lues de droite à gauche, comme dans le serveur du DesktopEmulator, de sorte qu'un nom de fichier peut contenir des virgules) :

  * `storage:FILE[,bytes][,ro][,sector=N][,create=SECTORS]` un fichier comme périphérique bloc (mode mot : 4 octets par mot, secteurs de 128 mots ; `bytes` : un octet
    par mot, secteurs de 512 mots ; opérations READ/WRITE/FLUSH, statut OK/EINVAL/ERANGE/EROFS/EIO, descripteur `[sector_words, sectors, flags, max_sectors]`)
  * `serial:stdio` sortie du programme vers la sortie standard (8 bits de poids faible) ; entrée depuis l'entrée standard en exécution sans invite (`-r` ; fin de l'entrée = raccrochage)
  * `serial:tcp=PORT` un port TCP de cet ordinateur seulement (127.0.0.1) : un client à la fois (un nouveau remplace l'ancien), la sortie tant que personne n'est connecté est abandonnée, et le programme voit un raccrochage quand le client s'en va, jusqu'à la connexion du suivant (il n'accepte pas d'autre option que `out=FILE`)
  * `serial:echo[,hup=N]` ce que le programme écrit lui revient ; raccroche après N mots
  * extensions de v32sim pour le série : `in=FILE` (fourni au départ et à chaque réouverture du canal), `out=FILE` (sortie du programme, 8 bits de poids faible), `hangup` (raccroche quand
    les données de `in=` ont été livrées) ; elles se combinent avec les autres

L'hôte s'exécute à chaque frontière d'image (battement de cœur une fois par image, puis chaque canal jusqu'à ce qu'il ne reste plus de travail). Avec `--v32dev-doorbell` un anneau est
traité **tout de suite** (T = 0, pas de vidage, pas d'attente, pas d'horloge murale), donc une exécution avec un hôte natif est répétable (les tests comparent deux cartes octet pour
octet). Le numéro de session est un compteur, pas l'heure. `mailbox [N]` montre les compteurs de l'hôte natif par canal. **`save` / `restore` conservent l'hôte natif** : le fichier d'état se termine par les
files, les compteurs et le numéro de session de l'hôte, et une restauration remet toute la région de la boîte aux lettres, côté hôte compris, de sorte qu'une exécution coupée en deux (save, nouveau
processus, restore, suite) est identique à l'exécution entière, pour le stockage et pour le série, avec et sans la sonnette (vérifié à chaque image de la
conversation). Un fichier d'état sans ce bloc, ou sauvegardé avec d'autres sortes de périphériques natifs, se restaure comme avant (le côté hôte présent est conservé, un
message à moitié reçu est abandonné, avec un avertissement dans le second cas). L'image disque est un fichier extérieur à l'état : restaurez avec l'image telle qu'elle était (un nombre de secteurs
différent est signalé). **Commande `dev`** (`help dev`) : `dev [info]` (les lignes de `v32dev.py info`), `dev format [force]` (le
`v32dev.py format` intégré : la carte devient une boîte aux lettres tout de suite, avec les périphériques configurés inscrits), `dev attach N SPEC`, `dev detach N`, `dev swap N SPEC` à l'exécution
(le programme voit l'hôte disparaître, puis une nouvelle session quand il revient, comme un support changé) ; en `--batch` une commande `dev` qui échoue termine l'exécution avec le
code 4. Test : `tests/native_host_test.py` (lancé par `tests/run_tests.sh`) exécute les cartouches réelles de v32io (`storage_test`, `serial_test`) contre lui et contre `v32dev.py serve` : même rapport,
même image.

**`V32DEV_SERVER` et `V32DEV_CH0`..`V32DEV_CH3`** (le serveur v32dev intégré du DesktopEmulator du projet v32io, `emulator/v32dev-server` : les mêmes
variables avec le même sens, de sorte qu'une même ligne de commande et un même environnement servent les deux émulateurs) : avec `V32DEV_SERVER` définie à autre chose que `0`, v32sim sert la
carte chargée avec les périphériques de `V32DEV_CH0` à `V32DEV_CH3` (une SPEC chacune, comme ci-dessus ; une variable vide ou non définie laisse son canal non servi, une mauvaise
est signalée et laissée de côté). Non définies ou à `0`, les variables ne sont pas regardées. La carte **n'a pas besoin d'être formatée** et **aucune boîte aux lettres n'arrive jamais dans le fichier de la carte** : si les
64 derniers Kio ne contiennent rien, l'en-tête y est placé en mémoire seulement (le programme voit une boîte aux lettres active), toute la région est exclue de chaque sauvegarde, et un programme
qui plante ne laisse rien derrière lui. Une carte qui contient des données dans ces 64 Kio est laissée telle quelle et n'est pas servie (un avertissement le dit) ; `V32DEV_DISABLE` garde la carte
ordinaire ; une carte formatée par `v32dev.py format` fonctionne aussi (le serveur prend la place de l'hôte, et la zone hôte du fichier n'est pas lue). Une carte absente n'est pas une
erreur : il n'y a rien à servir. `--v32dev-host=N=SPEC` continue de fonctionner et l'emporte sur `V32DEV_CHN` pour son canal ; avec `V32DEV_SERVER` active, il garde lui aussi la
boîte aux lettres hors du fichier, et sans elle (ou avec `V32DEV_SERVER=0`) il formate le fichier de la carte comme avant. En mode serveur rien n'est fatal (un périphérique qui ne peut pas être
ouvert, un port pris : le canal reste non servi, avec un message), `dev format` n'a rien à faire, et `dev attach|detach|swap` changent les canaux pendant l'exécution.
Non repris de l'émulateur, car v32sim n'a ni menu ni fichier de réglages : les entrées de menu (mode octet, lecture seule, nouvelle image), le fichier de réglages, le
port TCP par défaut `3200 + N` (donnez `serial:tcp=PORT`), et `serial:echo` par son nom seulement. **`dev format [force] [N=TYPE ...]`** accepte aussi les types de canal de `v32dev.py format --chN`
(`none storage block serial char pipe tcp udp netplay socket`, ou un nombre), et `dev info` nomme chaque type comme le fait `v32dev.py info`. Test : `tests/run_v32dev_server_tests.sh`
(sans chaîne d'outils ; 84 vérifications) et, avec le projet v32io, ses scénarios intégrés de `dev_rom_test.py` via `tests/v32sim_runner.sh`.


## La Souris v32io

La souris, `v32mouse`, est faite de deux compteurs de 12 positions (X sur Left/Right/X/Y, Y sur Up/Down/L/R : un trit et une paire codée en Gray, de sorte qu'un seul
contrôle change à chaque pas) et de trois boutons (Start = milieu, A = gauche, B = droit). Un programme lit les compteurs une fois par image, donc le
périphérique envoie au plus 5 pas par axe et par image ; un déplacement plus long prend plusieurs images. v32sim en émule une sur n'importe quel port sauf celui du clavier :

    v32sim --v32mouse=1 -B bios.v32 cart.v32            (puis pilotez-la depuis l'invite ou un script --batch)

`mouse attach [PORT]` (1 par défaut), `mouse detach`, `mouse move DX DY`, `mouse click|down|up [left|right|middle]` (un clic est un appui, deux
images, un relâchement), `mouse wait N` (images), `mouse clear`, `mouse` (état : compteurs, boutons, file). Les actions attendent dans une file de 4096 entrées et
sont livrées une par image, dans l'ordre, avant l'exécution de l'image. Restaurer un état sauvegardé oublie les actions en file et reprend les compteurs à partir
de la manette restaurée. Les `tests/vectors/mouse_*.vec` (du projet v32io) sont vérifiés image par image par
`tests/run_mouse_vectors.py`, que `run_harness_tests.sh` exécute.

## Le Clavier v32io

Les périphériques v32io ressemblent à une manette pour le programme. Le clavier, `v32kbd`, utilise
Left/Right comme strobe, Up/Down pour appui/relâchement, et Start A B X Y L R comme code de touche
sur 7 bits, un événement de touche par image. v32sim en émule un sur n'importe quel port :

    v32sim --v32kbd=1 --kbd-input=stdin -B bios.v32 cart.v32
    v32sim --kbd-input=/tmp/keys cart.v32            (un FIFO, un tube ou un fichier)

ou depuis l'invite : `kbd attach 1`, `kbd type "ls -l\n"`, `kbd key ctrl+c`,
`kbd key f1 down`, `kbd input FILE`, `kbd sync on|off`, `kbd` (état), `kbd clear`.

Le texte devient des événements de touche comme les produit un clavier US (Maj est maintenue
sur les caractères consécutifs qui en ont besoin) ; les caractères UTF-8 que la police du BIOS
possède (accents, traits de tableau...) sont envoyés comme des *caractères saisis* v32io. Un terminal
utilisé comme entrée est mis en mode brut pendant l'exécution du programme : les flèches, Suppr,
F1-F12, les combinaisons Ctrl et Alt sont traduites, et **Ctrl-]** revient à l'invite du
débogueur (qui récupère le terminal). Le texte qui arrive plus vite
qu'un événement par image attend, rien n'est perdu.

**Ce que voit le programme (et quand il perd une touche).** Comme sur l'adaptateur réel et dans le DesktopEmulator, un événement de touche est
livré par image, *au début de l'image, que le programme regarde la manette ou non*. Les contrôles ne gardent que le
dernier événement : le strobe (Left/Right) bascule à chaque événement, donc un programme qui n'a pas lu la manette entre deux événements ne
voit jamais le premier, et avec un nombre pair d'événements entre-temps il ne voit aucun changement. Taper `cat` dans un programme qui
est occupé pendant quelques images (un aller-retour v32dev, un long calcul, un programme qui interroge moins d'une fois par image) donne
`at`, `t` ou rien : chaque touche est un événement d'appui et un événement de relâchement, deux images. v32sim ne met pas en file pour le programme de
lui-même, parce que la console réelle non plus. Un invité doit lire son v32kbd **à chaque image** (le `v32kbd_probe()` de la
bibliothèque le fait), y compris pendant qu'il attend autre chose, ou bien le script ne doit pas taper avant que le programme soit prêt (`until` son
invite). Deux aides :

  * `kbd` affiche `replaced before the program read them: N` (événements livrés alors que le précédent n'avait pas été lu : ceux-là
    sont perdus), de sorte qu'un test puisse distinguer une touche perdue d'une faute de frappe. Il ne fait qu'observer.
  * `--kbd-sync` / `kbd sync on|off` (désactivé par défaut) : le simulateur retient chaque événement jusqu'à ce que le programme ait exécuté un `IN` sur
    un contrôle de la manette du clavier depuis le précédent, de sorte que rien n'est remplacé ; un programme qui lit à chaque image obtient le
    même événement par image qu'avant. Un programme qui ne lit jamais n'obtient plus d'événements (ils attendent dans la file ; `kbd` montre les
    images retenues). L'exécution n'est plus ce que ferait le clavier réel tant que le programme est occupé, donc servez-vous-en pour savoir si
    une touche perdue en est la cause, ou dans des scripts, pas dans des exécutions différentielles sensibles au timing.

`tests/kbd_probe.py` écrit la cartouche sonde (occupée pendant N images, puis journalise les événements qu'elle voit) et
`tests/run_kbd_sync_tests.sh` vérifie tout cela sans chaîne d'outils.

## La Console Texte

Les caractères dessinés avec la texture du BIOS (texture -1, région = code du caractère) sont
aussi conservés sous forme de texte. Deux dispositions sont reconnues :

  * la **police du BIOS** telle que `print_at()` la dispose : cellules de 10x20 pixels, 64x18
  * le **terminal v32c++** (`libc/v32term.h`) : 80x24 cellules de 8x15 pixels, dessinées
    légèrement mises à l'échelle à partir de régions que la bibliothèque définit sur la texture du BIOS ; le dessin
    du « papier » d'une cellule l'efface et le dessin du glyphe qui suit fixe son caractère

La disposition dessinée en dernier est celle qui est affichée. `console` l'affiche (UTF-8 ; les caractères de traits de tableau
du BIOS deviennent de l'Unicode), `--console` l'affiche à la fin de l'exécution, et
ClearScreen la vide.

**Couleurs.** La couleur de multiplication du GPU de chaque dessin est conservée : la couleur du glyphe est le
premier plan de la cellule et, pour le terminal 80x24, son dessin de « papier » est l'arrière-plan
(donc la vidéo inverse fonctionne). Elles sont affichées avec de simples séquences de couleur ANSI, sans bibliothèque
curses, réduites au niveau que vous choisissez : `none`, `16`, `256` ou
`true` (24 bits), couleur la plus proche par distance RVB. `--console-color=MODE` ou
`console color MODE` le règle ; `auto` suit `NO_COLOR`, `COLORTERM` et `TERM`.
Non défini, `--tty` utilise auto et le vidage de `console` est du texte brut (il peut donc aller dans un
fichier). Le texte blanc et le fond noir sont les valeurs par défaut du terminal lui-même.

**Curseur.** Un programme dessine lui-même son curseur, d'ordinaire le bloc plein (caractère
20 du BIOS). `--cursor-color=COLOR` / `console cursor COLOR [CODE...]` recolore
ces cellules (un nom, `#RRGGBB`, ou `off`) ; d'autres codes de caractères, comme un
contour de boîte creuse, peuvent être listés après la couleur.

### Mode Interactif

    v32sim --tty -B bios.v32 v32nix.v32            clavier sur la manette 0
    v32sim --tty --v32kbd=1 -B bios.v32 cart.v32   clavier sur la manette 1

`--tty` exécute le programme avec la console texte sur votre terminal (écran
alternatif, repeint quand le texte change) et vos touches envoyées à un
v32kbd émulé, de sorte qu'un programme texte tel que v32nix ou V32DOS peut être utilisé directement là. Il
a besoin d'un terminal sur stdin et stdout. **Ctrl-]** part vers l'invite du débogueur
(le terminal est rendu ; le dernier écran reste dans l'historique de défilement sous forme de texte) ;
`continue` retourne au programme. Ctrl-C et les autres touches de contrôle vont au
programme, pas à v32sim. Le programme doit lire un v32kbd sur ce port de manette
(bibliothèque `v32kbd_init` / `v32io`) ; un programme qui lit le clavier à l'écran de la
source de touches par défaut de v32term ne verra pas vos touches.

## Exécution Sans Surveillance

  * `--fast` / `--frame-us=N` : ne pas dormir pour tenir 60 images par seconde (ou
    attendre N microsecondes par image). `--max-frames=N` s'arrête après N images, comptées depuis la mise sous tension (le logo du BIOS, environ 250 images, compris).
  * `-x` / `--exit-status` : le code de sortie dit comment l'exécution s'est terminée : 0 le
    programme s'est arrêté, 2 la limite d'images a été atteinte, 3 une erreur système (l'exécution
    s'arrête là au lieu d'entrer dans le gestionnaire d'erreurs du BIOS).
  * Les commandes `-C FILE` sont lues par l'invite (après la lecture de la première instruction),
    donc `step`, `next` et `continue` agissent ; avec `-r`, elles s'exécutent une fois au départ.
  * `--dump=N[@ADDR|@LABEL]` (ou `ADDR:N`) : à la fin de l'exécution, affiche `N` mots de mémoire comme
    entiers signés, un par ligne, avant la bannière finale des registres. `ADDR` est
    en hexadécimal (par défaut : le début de la RAM) ; une étiquette est cherchée telle quelle puis comme
    `global_LABEL`, donc une globale v32cc `counter` peut être vidée par
    `--dump=1@counter` quand son fichier de débogage est chargé. Avec `-r -x --fast
    --max-frames=N` c'est un mode sans interface « exécuter jusqu'à HLT et vider la RAM », comme
    `vrun -d N` (v32orun --compat=vrun, utilisé par v32cc).
  * `--fixed-clock[=YEAR,DAY,SECONDS]` et `--seed=N` rendent les exécutions reproductibles : les
    ports de date/heure partent d'une valeur fixe au lieu de l'horloge de l'hôte, et le
    RNG est initialisé au démarrage. Deux exécutions de la même cartouche donnent alors des vidages de
    RAM identiques (un programme qui s'initialise à partir de `TIM_CycleCounter` diffère encore
    entre des compilations de vitesses différentes, puisque le nombre de cycles diffère).

### Statistiques d'Images

`--frame-stats=FILE` écrit une ligne CSV par image, `frame,cycles,cpu_pct,gpu_pixels,gpu_pct,end` (cycles par rapport au budget de
250 000, pixels GPU par rapport à la capacité de 9 écrans ; `end` vaut `w` pour une image terminée par `WAIT`, `b` quand le budget est épuisé), et
affiche un résumé (moyenne, 95e centile, maximum, images à 100 %) sur stderr à la fin de l'exécution. `--frame-stats-skip=N`
laisse les N premières images (le logo du BIOS en prend environ 250) hors du résumé. Après ces colonnes viennent les compteurs GPU / SPU
(`draws,dropped,clears,tex_switches,region_switches,regions,spu_cmds,spu_plays`, puis `scaled_draws,rotated_draws,region_defs,color_changes,
blend_changes,spu_assigns,spu_params,movs_words,sets_words,gamepad_reads,instructions,memcard_accesses,spu_playing`), et le résumé les liste (moyenne, p50, p95, p99,
maximum, total) ainsi que les régions auxquelles le plus de pixels sont imputés. `--stats-format=json` écrit du JSON Lines à la place (un objet par image avec les noms
stables `gpu.draws`, `cpu.cycles`, ..., et une ligne `{"type":"summary"...}` en dernier) ; `--stats`, `--stats-skip`, `--stats-top` sont les mêmes
options que celles de `--frame-stats`. Dans une session, `stats` affiche les compteurs, `expect stats gpu.draws.p95 <= 600` (ou
`stats expect ...`) en vérifie un (`.last .mean .min .max .p50 .p95 .p99 .total`), `stats save FILE [json]` écrit chaque image. Il ne fait que lire l'état ; les nombres sont égaux à
ceux de `v32prof`, image pour image. Voir [docs/PROFILING.md](docs/PROFILING.md).

### Surveillance du tas et expect-reach

`--heap[=PROG.asm]` est pour les cartouches construites par `v32lua` : il surveille le tas de l'extérieur. À la fin de chaque image (à partir de la première où la
cartouche a tourné) il affiche sur stdout

    HEAP F263 ptr=0x00000389 used=780 peak=790 free=56 blocks=3
    HEAP F263 ptr=0x0000010E used=122 peak=122 free=0 blocks=0 floor=0x00000101 pin=0x00000000 depth=2 stack=0x000000F4,0x00000101

`F` est l'image qui vient de se terminer (`TIM_FrameCounter` ; les images du BIOS comptent), `ptr` le pointeur de tas (le mot 0 de la RAM, `HEAP_POINTER`),
`used` vaut `ptr - HEAP_START` en mots et `peak` le plus grand `used` jusque-là. Le pic est exact : le pointeur est lu après chaque instruction, pas seulement à la
fin des images. Pour une cartouche construite avec `v32lua --gc arena`, la ligne se poursuit avec `ARENA_FLOOR`, `ARENA_PIN_TOP`, `ARENA_DEPTH` et les premières
`depth` entrées de `ARENA_STACK` (`arena.mark()` / `arena.release()` se voient comme `depth` qui monte et descend). À la fin de l'exécution il affiche
`HEAP PEAK used=N ptr=0x.. frame=F` (où le pic a été atteint) et
`HEAP END ptr=0x.. used=N peak=N[ free=N blocks=N] frame=F[ floor= pin= depth= stack=]` ; `stack=-` si aucune arène n'est ouverte. `used` est le sommet du tas et
compte les blocs des listes libres ; `free=` (mots) et `blocks=` apparaissent quand la cartouche a été construite par un `v32lua` qui rend avec `__mfree` les
blocs de table devenus trop petits (`FREE_MASK` et `FREE_BINS`, 22 listes par taille, dans le `.asm`) : `free` est la part de `used` réutilisable, donc
`used - free` est la mémoire vivante. Les listes sont parcourues en lecture seule avec un plafond de pas (`free=? blocks=?` si l'une est corrompue) ; un `v32lua` plus ancien n'affiche pas ces champs.
`v32orun --heap` affiche le même format ; les numéros d'image diffèrent (ceux de v32sim sont ceux de la console, images du BIOS comprises ; le mode bare de v32orun compte depuis 0).
Les noms viennent des lignes `%define NAME 0xADDR` du `NAME.asm` généré, cherché à côté de la cartouche comme les fichiers de débogage
(`obj/`, `--debug-dir`) ou donné par `--heap=PROG.asm` ; ce qui manque dans ce fichier est cherché parmi les symboles de données (`--symbol-file`,
`.asm.sym`), et `HEAP_POINTER` vaut par défaut le mot 0 de la RAM. Sans `HEAP_START` (pas de `.asm`, ou pas un de `v32lua`) un avertissement sur stderr le dit une fois et
les lignes ne montrent que `ptr`. Il ne fait que lire la RAM : une exécution avec et sans l'option donne les mêmes résultats.

`--expect-reach ÉTIQUETTE[,ÉTIQUETTE...]` dit à quel code l'exécution doit arriver. Les étiquettes sont celles des fichiers de débogage (celles que `break` et `-b`
utilisent ; `NAME.vbin.debug` de `v32as -g` ou de l'`assemble -g program` standard) ; une étiquette inconnue est une erreur de démarrage (code de sortie 1, comme une mauvaise option). Quand le
CPU s'apprête à exécuter l'adresse de l'une d'elles, v32sim affiche `EXPECT REACHED <étiquette> frame=N ip=0x........` sur stdout, s'arrête et sort avec 0.
Si l'exécution se termine autrement (la limite d'images de `--max-frames`, `HLT`, une erreur CPU, `quit`, la fin de l'entrée) il affiche
`EXPECT MISSED <étiquette>,<étiquette>` et sort avec 1 (aussi avec `-x` : une attente échouée l'emporte sur le code 2 de la limite d'images et le 3 d'une erreur système ;
seul `quit N` la remplace). Cela marche pas à pas, dans les scripts `--batch` et avec `-r --fast --max-frames=N` ; c'est ainsi qu'un test dit
« ce programme doit atteindre `__oom_handler` » (ou `main_loop`, ou `game_over`).

### Profil d'Adresses

`--profile-out=FILE` écrit, à la fin de l'exécution, combien de fois l'instruction à chaque
adresse a été exécutée. Il n'a besoin ni de l'invite, ni de `-p`, ni de fichiers de débogage, et fonctionne avec
`-r -x --max-frames=N` (une exécution arrêtée par la limite d'images l'écrit aussi) comme avec une
exécution qui atteint `HLT` :

```
# v32sim address profile v1
# cycles 403 frames 0
0x00000004 100
0x00000006 100
```

  * Une ligne par adresse exécutée (BIOS compris), triée par adresse ; l'adresse est celle
    du premier mot de l'instruction. `# cycles` est le nombre d'instructions exécutées
    (un `WAIT` compte une fois), donc les compteurs s'additionnent à lui. Avec un fichier de débogage asm de
    cartouche chargé, une troisième colonne nomme l'étiquette qui couvre l'adresse.
  * `--profile-format=bin` écrit la ligne `V32PROF1`, puis des paires
    `address, count` de 32 bits petit-boutistes.
  * `--profile-top=N` affiche les N adresses les plus sollicitées (avec les étiquettes, quand elles sont connues) sur stderr.
  * `--profile-by=function` additionne les compteurs sur les adresses de chaque fonction (l'
    étiquette qui les couvre dans les fichiers de débogage chargés ; les adresses du BIOS sans étiquette
    forment un seul groupe) : `--profile-top=N` liste alors les N fonctions les plus sollicitées avec leur
    part et le nombre d'adresses, et le `--profile-out` texte a une ligne par
    fonction (`# v32sim function profile v1`, puis `0xFIRST COUNT NADDR NAME`). Chargez
    la carte C (`-L C`, ou le `.vbin.debug` fait par `v32as --map`) pour obtenir des noms C ;
    sans étiquettes, la sortie le dit. Le format binaire est inchangé.
  * `--fixed-cycles` fait que les lectures de `TIM_CycleCounter` par un programme renvoient 0, de sorte que le code qui
    s'initialise à partir de lui se comporte de la même façon d'une compilation à l'autre ; avec `--fixed-clock` et `--seed` le
    profil est reproductible, octet pour octet.

### V32TRACE (trappes et événements d'une cartouche traduite)

`--v32trace=PREFIX` fait de l'exécution un enregistreur V32TRACE (spécification version 2.1, [docs/V32TRACE-SPEC.md](docs/V32TRACE-SPEC.md)). Un traducteur
(NEStoV32, GBtoV32, ...) écrit `<cart>.v32trace.meta` et `<cart>.labels.txt` à côté de la cartouche ; v32sim les lit (ou
`--v32trace-meta=FILE`, `--v32trace-labels=FILE`), surveille les routines de trappe et les événements qu'ils nomment, et écrit `PREFIX.traps.txt` (une ligne
TRAP/EVENT par trappe ou événement distinct, avec l'état capturé du traducteur, les derniers blocs invités entrés, et à partir de la 2.1
`segstart=` et `input=` : l'historique de la manette 1 depuis le dernier redémarrage sous forme de script `V32_INPUT`) et `PREFIX.blocks.txt` (les blocs
invités qui ont été exécutés, avec leur nombre d'instructions) chaque fois qu'un nouveau est rencontré et à la fin de l'exécution. L'outil de comblement des lacunes d'un traducteur lit
ces fichiers, de sorte qu'une boucle compiler-exécuter-corriger n'a pas besoin de fenêtre d'émulateur (le `make test-trace` de GBtoV32 s'exécute contre ce simulateur : tout passe).

Après une trappe la cartouche est redémarrée 60 images plus tard (registres et pile réinitialisés, saut au début de la cartouche ou à l'étiquette `resume` du fichier meta ;
la RAM, le GPU et le SPU sont conservés) de sorte qu'une exécution puisse rencontrer plusieurs lacunes ; la même trappe trois fois de suite l'arrête.
`--v32trace-norestart` garde la cartouche dans la trappe. Un fichier meta fait pour une autre compilation de la cartouche (`words` ou `fnv1a`
diffèrent) est ignoré avec une note sur stderr.

v32sim écrit aussi le rapport lisible de l'enregistreur, `PREFIX.trace.txt` (spécification 4.3), dans la disposition de l'enregistreur du DesktopEmulator : `[events]`
(nouveaux coups, redémarrages, abandon, erreurs CPU), `[input]` (la manette 1 de la session sous forme d'une seule ligne `V32_INPUT=`, avec les images de redémarrage),
`[perf]` (moyennes de charge CPU, séries d'images abandonnées, images les plus coûteuses, une chronologie), `[host]` (quads GPU et vidages de dessins par
image, le modèle de coût hôte de l'enregistreur, et chaque `series NAME ADDR` que nomme le fichier meta), `[hot spots, whole session]` (les principales
étiquettes et blocs invités) et `[markers]`. La commande `mark [NOTE]` pose un marqueur (le marqueur de touche F de l'enregistreur) à l'image courante,
avec les captures simples du fichier meta dans sa note ; le rapport répartit alors ses nombres selon les segments entre marqueurs. Le
`trace2input.py --session` de GBtoV32 le lit, et `--input=@PREFIX.trace.txt` rejoue sa ligne `V32_INPUT=`. Vérifié ligne à ligne contre
l'enregistreur réel (`tools/oracle/v32trace-ref`, plus bas) sur la ROM de lacunes de GBtoV32 dans trois scénarios (entrée avec deux marqueurs, une
trappe répétée jusqu'à l'abandon, `--v32trace-norestart`) : `traps.txt`, `blocks.txt` et `trace.txt` sont égaux, au commentaire de rejeu près et à
l'ordre des points chauds de compteurs égaux (`run_harness_tests.sh`). `input2=` (manette 2) est écrit après `input=` sur chaque ligne, et `V32_INPUT2=` dans le rapport, seulement quand la seconde manette a tenu un bouton
(de sorte qu'une trace à un joueur reste égale à celle de l'enregistreur) ; `--input2=SCRIPT` / `--input2=@PREFIX.trace.txt` la rejoue.

Pour revenir à un coup, donnez son `input=` à `--input` (ci-dessous) : la manette est rejouée par image de cartouche et la même trappe
revient à la même `cartframe`. Vérifié contre la logique de console d'origine sur la ROM de lacunes de GBtoV32 : avec A maintenu à partir de l'image 40 de la cartouche, la
routine de trappe démarre à l'image 41 de la cartouche dans les deux.

### Manette Scriptée (--input)

`--input=SCRIPT` maintient des boutons de manette par **image de cartouche** (l'image 0 est la première image que la cartouche exécute, après le logo
du BIOS) : `BUTTON[+BUTTON...]@FIRST-LAST[,...]` (ou `@FRAME`), boutons `LEFT RIGHT UP DOWN START A B X Y L R` (`SELECT` est X, le
nom des enregistreurs). C'est la syntaxe `V32_INPUT` des enregistreurs V32TRACE et des profileurs des traducteurs ; `V32_INPUT` dans
l'environnement est utilisé quand `--input` n'est pas donné. `--input=@FILE` prend le script dans la ligne `V32_INPUT=` de FILE (un
rapport `.trace.txt`). `--input-port=N` choisit la manette (0 par défaut) ; elle est connectée. `--input2=SCRIPT` fait de même pour la manette 2 (port 1),
avec `V32_INPUT2` dans l'environnement (et `@FILE` lisant la ligne `V32_INPUT2=`). Les changements se font
entre les images, comme les fait la commande `gamepad` : un bouton maintenu à partir de l'image F se lit 1 à l'image F.

## Vitesse et Mode Turbo

La vitesse d'exécution de v32sim compte pour les tests (un démarrage et une compilation de v32nix, ce sont des centaines de millions d'instructions). Ce
qui le rend rapide, dans l'ordre où cela a été trouvé (chiffres dans `docs/PERFORMANCE.md`) :

  * **Aucun travail pour les messages que personne ne lit.** Le texte de `--debug` / `--verbose` était formaté pour chaque instruction et
    jeté dans `/dev/null` (jusqu'à 20261122) ; `DPRINTF` (inc/globals.h) teste désormais d'abord le flux. Les vérifications mémoire ne construisent leurs textes
    d'erreur que lorsqu'il y a une erreur.
  * **Chemins courts pour le cas courant** : les lectures et écritures RAM/ROM, la lecture d'instruction, les lectures de ports et le compteur de cycles
    sautent les vérifications génériques ; la boucle principale n'appelle pas le code des points de surveillance, du pas à pas source ou du profil d'adresses sauf si l'un d'eux est armé.
  * **Les nœuds de backtrace** (CALL/RET) viennent d'une liste libre et leurs étiquettes sont cherchées quand `backtrace` est demandé.
  * **Peinture différée.** Le tampon d'image n'est lu que par une capture d'écran ou un état sauvegardé, donc un dessin ou un effacement est mis en file
    et peint quand les pixels sont nécessaires (ou quand 16384 sont en file). Un effacement opaque abandonne tout ce qui était en file avant lui,
    de sorte qu'un programme qui efface à chaque image ne peint que ce qu'une capture d'écran demande. La peinture est exactement ce que donnerait
    une peinture immédiate (`--gpu-eager` la fait, pour comparer) ; elle est aussi devenue plus rapide (les dessins alignés sur les axes trouvent leurs
    colonnes et lignes source une seule fois, les texels transparents sont sautés, les produits de couleur par canal viennent d'une table).
  * **Options de compilation** : `-O2` et `-flto` par défaut ; `-ffp-contract=off` garde des résultats flottants identiques sur les hôtes à multiplication-addition fusionnée.

Tout cela est actif dans le mode par défaut et ne change aucun résultat (les suites différentielles contre la logique de console d'origine, les
captures d'écran et les statistiques sont identiques à l'octet près à la compilation du 20261109, avant ces changements ; la seule différence visible est que `backtrace`
résout une étiquette quand il l'affiche, pas quand l'appel a eu lieu).

`--turbo[=report]` est pour les exécutions où personne ne regarde les pixels ni l'horloge. Il implique `--fast` (ne dort jamais) et ajoute deux
choses qui peuvent différer d'une exécution normale :

  * **Avance rapide des boucles d'attente active.** Un programme qui interroge (typiquement `TIM_FrameCounter`, ou un bouton) jusqu'à ce que l'image change brûle
    les 250000 cycles de l'image un tour à la fois. Quand on voit une boucle se répéter exactement (à deux branchements pris vers la même
    adresse, les 16 registres sont égaux, chaque mot de RAM écrit entre-temps a retrouvé son ancienne valeur - un CALL et un RET laissent
    la pile comme ils l'ont trouvée - et aucun port n'a été écrit, aucun port volatil (`TIM_CycleCounter`, `RNG_CurrentValue`) lu,
    aucune mémoire de carte écrite, aucune image terminée), des tours entiers sont sautés en une étape : `TIM_CycleCounter`, le nombre d'instructions et
    les compteurs par image d'instructions / lectures de manette / lectures de carte avancent comme ils l'auraient fait, et les derniers tours s'exécutent toujours pour
    de vrai, de sorte que l'image se termine à la même instruction avec les mêmes registres et la même mémoire. **Aucun résultat ne change** (vérifié sur la
    RAM, les registres, le code de sortie et `--frame-stats`) ; ce qu'on ne voit pas, ce sont les instructions individuelles, donc le saut est désactivé
    tant que quelque chose les observerait : points d'arrêt, points de surveillance, `-w`, pas à pas et pas à pas source, `-p` et `--profile-out/-top`,
    V32TRACE, `-v` / `-D`. Une boucle qui compte ou stocke quelque chose de nouveau à chaque tour n'est jamais sautée.
  * **Une file GPU pleine est abandonnée, pas peinte.** Si 16384 dessins sont en file sans capture d'écran ni sauvegarde entre-temps (un programme
    qui n'efface jamais), les plus anciens sont abandonnés. Rien de ce que le programme peut observer ne change (la comptabilité des dessins, la console texte,
    les compteurs et traces du GPU utilisent les commandes, pas les pixels), mais une `screenshot` ou `save` ultérieure ne montre que ce qui a été mis
    en file depuis, et le dit (`warning: --turbo skipped N draws`). Les programmes qui effacent leur écran, et toute exécution
    dont on fait une capture d'écran au moins tous les 16384 dessins, sont exacts.

`--turbo=report` affiche sur stderr à la sortie ce qui a été sauté. Turbo ne change jamais l'horloge de la machine : le compteur de cycles, le
compteur d'images, la date et l'heure avancent comme dans une exécution normale.

## Écriture de Scripts

Tout ce dont un script a besoin pour piloter une exécution sans interface et dire ce qui s'est mal passé. Un script
est l'entrée standard du simulateur (ou `-C FILE`).

### Mode par Lot

`--batch` (`-q`, `--quiet`) fait que stdout contient exactement ce que le script a demandé :

  * pas d'invite `v32sim>` et pas d'écho des commandes, pas de ligne d'instruction `[ADDR] HH HH HH HH OPC`
    après un arrêt, pas de bannière de registres à la fin, pas de bannière de fin d'exécution
    (`regs`, `print` et `console` affichent toujours, sur stdout) ;
  * les accusés de réception (`[watch] ...`, `[console] wrote ...`, la raison pour laquelle une exécution s'est arrêtée) et tous les
    diagnostics vont sur stderr, de sorte que le stdout d'un script n'a jamais besoin d'être filtré ;
  * une ligne vide est sautée et la fin de l'entrée termine l'exécution (pas de boucle sans fin) ;
  * le premier `expect` échoué, ou un `until` qui expire, termine l'exécution.

Code de sortie (toujours ; `-x` ajoute les codes de limite d'images et d'erreur système) : 0 succès ;
1 un `expect` a échoué, un `until` a expiré ou une étiquette de `--expect-reach` n'a pas été atteinte ; 2 la limite d'images a été atteinte (`-x`) ;
3 une erreur système (`-x`) ; 4 une ligne de commande n'a pas pu être analysée (mode par lot) ; 5 `--dump` n'a pas pu afficher ce qui
était demandé (spécification erronée, symbole inconnu, mémoire illisible ; seulement quand rien d'autre n'a fixé un code) ; `quit N`
les remplace tous. Une exécution qui se termine parce que le programme a exécuté `HLT` donne un simple 0 (une fin propre) sauf si
`--halt-status=N` en dispose autrement ; `--warn-halt` affiche `[HLT] ...` sur stderr quand cela arrive. Une exécution qui ne peut pas démarrer sort plus tôt
avec 1 (une mauvaise option ou valeur d'option, un fichier `-C` qui ne peut pas être ouvert, `--tty` sans terminal, un `--v32dev-host` erroné), 11 (un fichier de BIOS, de cartouche ou de carte ne peut pas être ouvert), 12 (il ne peut pas être lu) ou 21 (ce n'est pas un fichier V32 valide).
Les défaillances internes (mémoire insuffisante, accès invalide à un IOPort) affichent `[ERROR]` sur stderr et sortent avec 4, 5, 7, 13, 14 ou 15 (4 et 5 sont aussi les codes ci-dessus ;
2 est aussi le code quand `/dev/null` ne peut pas être ouvert) ; elles ne devraient pas arriver. Sans `--batch` rien ne change dans la sortie interactive
(les scripts qui filtrent l'invite et les lignes d'instruction continuent de fonctionner).

### Symboles

Un nom est accepté partout où un nombre l'est : `print`, `display`, `watch` (cible et côté
droit), `expect`, `x`, `break` et `--dump`. Un symbole est cherché comme symbole de données,
puis comme étiquette des fichiers de débogage chargés, chacun tel quel puis comme `global_NAME`
(le nom que l'assembleur donne à une globale C), et enfin sans tenir compte de la casse.

  * Les symboles de données viennent de `NAME.asm.sym` (écrit par `v32cc -g`, avec la taille de chaque
    variable) et de `NAME.vbin.sym` (`v32as -g` ; lu en second, de sorte que ses adresses l'emportent), ou, quand
    aucun n'existe, des lignes `%define global_NAME ADDRESS` de `NAME.asm`, trouvé à côté
    du fichier V32 ou comme les fichiers de débogage (voir [Fichiers de Débogage](#fichiers-de-débogage)).
    `--symbol-file=FILE` nomme explicitement un fichier `.asm.sym`. `-n` les désactive.
  * `print counter` affiche le mot à l'adresse que `counter` représente, avec le
    nom dans l'en-tête : `counter(0x00000002): 0x00000096` ; `[counter]` est identique ;
    `[[ptr]]` suit un pointeur ; `&counter` est l'adresse elle-même ; `buf+2` et `buf-1`
    ajoutent une constante (une constante `+`/`-` est acceptée).
  * `x/d buf` liste les mots d'une variable (le tableau entier par défaut),
    `x/D work 10` dix instructions d'une fonction.
  * `watch [counter] == 6`, `watch/d [neg] < 0` et la liste des points de surveillance nomment la
    variable (`counter`, `buf+2`) au lieu d'une adresse.

### Attente et Vérification

  * `run N` (alias `wait N`) : continue pendant N images. Un point d'arrêt ou un point de surveillance qui
    se déclenche d'abord l'arrête plus tôt ; un `continue` ensuite n'est pas limité.
  * `until "TEXT" [timeout N] [rows N]` : continue jusqu'à ce que la console texte affiche TEXT
    (n'importe où dans une ligne ; `^` et `$` ancrent au début et à la fin d'une ligne), en ne regardant
    que les N dernières lignes quand `rows` est donné. TEXT est littéral : `.`, `*`, `[`, `(`, `\`
    se représentent eux-mêmes, et `^` / `$` ne comptent que comme premier / dernier caractère. Avec le
    préfixe `regex:` c'est une expression régulière étendue POSIX, essayée sur chaque ligne (donc
    `until "regex:^(ok|done)$"`) ; une expression invalide est signalée une fois et ne correspond jamais.
    `expect console` prend les mêmes motifs. L'écran est vérifié à la fin de chaque
    image et doit avoir changé depuis la commande (donc ce qui s'y trouve déjà ne
    met pas fin à l'attente). Délai par défaut : 3600 images.
  * `expect LOC OP VALUE` (comme `watch`, évalué maintenant ; `expect print R0 == 5` fonctionne
    aussi) et `expect console "TEXT" [rows N]`. Une vérification réussie est silencieuse en mode par lot.
  * `watch/d` (ou `watch/D`) compare comme des nombres signés sur 32 bits : `watch/d [neg] < 0`.
    La liste le marque `[signed]`. `watch` simple reste non signé.

### Console vers un Fichier

`console FILE` écrit l'écran texte dans FILE. `console --attrs[=fg|bg|all] [FILE]`
ajoute, après le texte, des grilles `-- fg --` et `-- bg --` avec les mêmes lignes et colonnes
que le texte ; chaque cellule est le chiffre hexadécimal de la couleur ANSI la plus proche (0 noir, 1 rouge,
2 vert, 3 jaune, 4 bleu, 5 magenta, 6 cyan, 7 gris clair, 8-F versions vives, F
blanc), `.` pour un blanc dans la grille du premier plan. Un test peut vérifier qu'un mot est rouge.
`screenshot FILE` écrit les pixels en PNG.

### Exemple

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

## Tests

`tests/run_tests.sh TOOLCHAIN V32IO_PROJECT` compile les cartouches de test avec
les DevTools Vircon32 et exécute : la suite v32dev via
`tests/v32sim_runner.sh` (un sosie de `dev_rom_runner` : 98 vérifications de périphériques de stockage
et série, plantage et reprise de l'hôte, cartes ordinaires et non formatées, et les scénarios du serveur v32dev : `V32DEV_SERVER`, `serial:stdio`, `serial:tcp`), les
vérifications v32kbd (commandes, entrée de l'hôte avec séquences d'échappement, caractères saisis,
ordre des événements), l'aller-retour de désassemblage des immédiats flottants, `load bios:`, et
la persistance de la carte quand le simulateur est tué par `kill -9`.

`tests/run_batch_tests.sh [TOOLCHAIN]` compile `tests/batch/prog.c` avec `v32cc`, `v32as`
et `packrom` (depuis TOOLCHAIN ou le PATH, avec `StandardBios.v32` ou `$V32BIOS`) et
vérifie les fonctionnalités de script : symboles, `x`, `run`, `until`, `expect`, `quit N`, les codes
de sortie, `watch` signé, `console FILE`/`--attrs`, `--profile-by=function`, et que l'
invite interactive est inchangée.

`tests/run_src_tests.sh [V32AS_DIR]` vérifie les commandes de niveau source (54 vérifications) sur
`tests/src/bench.asm`, de l'assembleur écrit comme `v32lua -g` l'écrit, avec `v32as` seulement : la
cartouche est faite par `tests/mkcart.py`, et le BIOS est `tests/gaps/bios.asm`.

`tests/run_kbd_sync_tests.sh` (`make test` ; 22 vérifications, sans chaîne d'outils : livraison v32kbd avec un programme occupé, `--kbd-sync`, `--warn-halt`,
`--halt-status`, `regex:`) et `tests/run_cli_tests.sh` (aussi `make test` ; 144 vérifications, sans chaîne d'outils : la ligne de
commande, les textes d'aide et les codes de sortie) et `tests/run_v32dev_server_tests.sh` (aussi `make test` ; 84 vérifications, sans chaîne d'outils ni
v32io : `V32DEV_SERVER`/`V32DEV_CHn`, `serial:tcp`, l'analyse de SPEC, les types de `dev format`).

`tests/run_turbo_tests.sh TOOLCHAIN` (`compile`, `assemble`, `packrom` d'origine et `StandardBios.v32` seulement ; 45 vérifications) couvre `--turbo`, `--turbo=report`
et `--gpu-eager` : résultats égaux avec et sans turbo sur une cartouche qui interroge, rien de sauté quand la boucle écrit, captures
d'écran identiques différé / immédiat / turbo, et l'avertissement de file abandonnée.

`tests/run_harness_tests.sh` (`V32TC`, `V32BIOS`, éventuellement `ORACLE` et `V32SRCMARK`) vérifie les scripts de `tools/` et leur
accord avec la logique de console d'origine : réussite/échec de l'auto-vérification sur les deux moteurs, symboles d'un `.asm` du compilateur d'origine,
`--frame-stats` contre `v32prof`, rejeu de manette contre `v32peek`, `bt` de niveau source sur une cartouche compilée par le compilateur d'origine,
sessions d'écran de référence, `v32sim-peek.sh`, `v32sim-addrprof.py`, `locals` sur des cartouches C et Lua, `--input`, et l'exécuteur de
corpus (98 vérifications en tout avec `ORACLE` et `V32IO` définis ; 2 de plus demandent `V32SRCMARK` et une arborescence GBtoV32). `make oracle COMPUTERSOFTWARE=DIR` compile
les outils oracle.

`tests/run_gaps.sh TOOLS`, `run_romcheck.sh TOOLS`, `run_fuzz.sh TOOLS`, `run_clock.sh TOOLS` (l'horloge de démarrage dans plusieurs fuseaux horaires autour des changements d'heure) et `run_memcard.sh TOOLS` (cartes mémoire et boîtes aux lettres v32dev) sont les suites différentielles contre la logique de console d'origine
(TOOLS : le répertoire de sortie de `tools/oracle/build.sh` ; voir `docs/V32SIMGAPS.md`), et `tests/run_stack_tests.sh TOOLCHAIN` vérifie la pile
de débogage (C, C++ et Lua, chacun à -O0, -O2 et -O3). `make check TC=DIR` exécute chaque suite à partir d'un répertoire de chaîne d'outils et affiche une ligne
de synthèse ; une suite dont les entrées manquent est signalée comme SKIP, jamais comme une réussite. Les recettes sont dans [docs/TESTING.md](docs/TESTING.md).

## IOPORTS

Tous les IOPorts existent et suivent l'implémentation du DesktopEmulator :

- **TIM** : date/heure (les années bissextiles sont gérées ; l'horloge de démarrage est l'heure locale de l'hôte, comme sur la console, y compris lors des changements d'heure), compteurs d'images/de cycles
- **RNG, INP, CAR, MEM** : se comportent comme attendu
- **GPU** : `GPU_ClearColor`, `GPU_SelectedTexture`, `GPU_SelectedRegion`,
  `GPU_Command` (ClearScreen, DrawRegion, DrawRegionZoomed, DrawRegionRotated,
  DrawRegionRotozoomed), `GPU_MultiplyColor`, `GPU_ActiveBlending`,
  `GPU_DrawingPointX/Y`, `GPU_DrawingScaleX/Y`, `GPU_DrawingAngle`,
  `GPU_RegionMinX/MinY/MaxX/MaxY`, `GPU_RegionHotspotX/Y`, et
  `GPU_RemainingPixels` effectuent leur rendu dans le tampon d'image
- **SPU** : `SPU_GlobalVolume`, `SPU_SelectedSound`, `SPU_SoundLength`
  (lecture seule), `SPU_SoundPlayWithLoop`, `SPU_SoundLoopStart/End`,
  `SPU_SelectedChannel`, `SPU_ChannelState` (lecture seule),
  `SPU_ChannelAssignedSound`, `SPU_ChannelVolume`, `SPU_ChannelSpeed`,
  `SPU_ChannelLoopEnabled`, `SPU_ChannelPosition`, `SPU_Command` ; les canaux
  avancent de 735 échantillons par image, mais rien n'est mixé ni joué

Toutes les textures et tous les sons présents dans un fichier V32 sont chargés ; `inventory` les
liste. Depuis l'invite, les ports peuvent être manipulés avec `set` et inspectés avec
`print`/`display`.

## Limitations Connues et Fonctionnalités Prévues

- **SPU** : pas de sortie audio (pas de capture WAV non plus)
- **Rendu** : la rastérisation par le moteur de rendu logiciel des quads pivotés et mis à l'échelle n'a pas été comparée à la sortie OpenGL
  (`docs/V32SIMGAPS.md` G14) ; `INT_MIN / -1` est indéfini côté hôte et diffère de la console (G12) ; les décalages de 32 ou plus sont aussi indéfinis côté hôte mais concordent avec la console sur x86-64
- **Console texte** : seuls les dessins de la texture du BIOS sont capturés, comme des caractères ; l'alpha, le clignotement et la couleur d'effacement de l'invité sont ignorés, et
  le texte dessiné avec la police propre d'un programme n'est pas capturé. `until` et `expect console` comparent du texte, pas des couleurs
- **Symboles** : seules les variables ayant une adresse numérique (globales v32cc et v32lua) sont des symboles de données ; les variables locales à une fonction et
  les membres de struct ne sont pas des symboles (les variables locales ont leur propre table, `locals` ; un membre n'est nommé que dans le `type=` de sa variable),
  et une entrée `.asm.sym` sans taille compte pour un mot
- Les modes d'affichage Lua et C (`-L`) sont expérimentaux. Les variables locales (`locals`, section 4 de V32DBG) montrent des scalaires, des tableaux, des struct
  par nom de membre, les globales avec leur `type=`, des valeurs Lua, les globales Lua et les upvalues des fermetures ; un membre de struct ne peut pas être nommé
  dans une expression (`print p.x` ; `x p` montre les mots), un membre qui est lui-même un agrégat est montré en mots bruts, les tables Lua imbriquées ne montrent
  que leur adresse, et `display NAME` garde l'adresse qu'avait le nom au lieu de suivre le cadre
- `--heap` ne connaît que le tas des cartouches `v32lua` (il lui faut `HEAP_START`, tiré du `.asm` de la cartouche ou d'un fichier de symboles ; les trames du BIOS ne sont pas affichées), et `--expect-reach` prend des étiquettes de code, pas des adresses ni `FICHIER:LIGNE`
- Le lecteur de fichiers de débogage est écrit à la main (il pourrait passer sur flex)
- Le démarrage par défaut se fait au BIOS (`0x10000004`), pas à la cartouche

Voir TODO.md pour les points ouverts.
