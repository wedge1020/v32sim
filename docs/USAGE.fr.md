[Version anglaise / in English](USAGE.md) | [Version espagnole / en español](USAGE.es.md)

# v32sim : référence de la ligne de commande

v32sim exécute une cartouche Vircon32 dans ses propres modèles de CPU, de GPU et de SPU, et vous donne soit une invite de
débogage pas à pas, soit une exécution sans surveillance suivie d'un rapport. Cette page couvre la ligne de commande,
l'environnement, les fichiers et le code de sortie. Le langage de commandes du débogueur (`step`, `break`, `print`, `watch`,
`kbd`, ...) est décrit dans le [README](../README.fr.md) (« Le Débogueur », « Commandes », « Écriture de Scripts ») ; `help` à l'invite
liste toutes les commandes et `help COMMANDE` en explique une. Quel outil répond à quelle question : [TOOLS.md](TOOLS.md).
Recettes de test : [TESTING.md](TESTING.md). Profilage : [PROFILING.md](PROFILING.md).

## Synopsis

```
v32sim [OPTION]... [CARTFILE.v32]
v32sim --version | --help
```

`CARTFILE.v32` est la cartouche. Donnez `-` (ou rien) pour n'avoir aucune cartouche : le BIOS s'exécute seul. Les options peuvent
venir avant ou après la cartouche. Les options longues qui prennent un argument s'écrivent `--nom=VALEUR` (ou `--nom VALEUR`) ;
`[=X]` ci-dessous signifie que la valeur est facultative et doit être attachée avec `=`. Les nombres donnés comme OFFSET, OPCODE
ou ADDR sont hexadécimaux (`0x` facultatif).

## Démarrage rapide

Déboguer une cartouche (l'invite apparaît à la première instruction ; `help` liste les commandes) :

```
v32sim -B StandardBios.v32 cart.v32
v32sim -B StandardBios.v32 -b main cart.v32      # point d'arrêt à l'étiquette main, puis `continue`
v32sim -B StandardBios.v32 --source -L src cart.v32   # affiche la ligne source C / Lua à chaque arrêt
```

Les fichiers de débogage (étiquettes, lignes source, symboles) sont trouvés automatiquement à côté de la cartouche ; produisez-les
avec `v32as -g` et `v32cc -g` / `v32lua -g`.

Exécuter sans interface, vérifier le code de sortie, afficher quelques mots de RAM (`-r` exécution, `-q` silencieux, `-x` code de
sortie significatif) :

```
v32sim -B StandardBios.v32 -r -q -x --fast --max-frames=600 --dump=4@counter cart.v32 </dev/null; echo $?
```

Recettes :

```
# capture d'écran après les images 60 et 120 de la cartouche (v32sim-shot-00060.png, ...)
v32sim -B B.v32 -r -q --fast --max-frames=400 --screenshot-at=60,120 cart.v32 </dev/null

# jouer un script de manette (A maintenu aux images 40-44 de la cartouche, Start à l'image 100), puis faire une capture d'écran
v32sim -B B.v32 -r -q --fast --max-frames=450 --input='A@40-44,START@100' --screenshot-at=150 cart.v32 </dev/null

# statistiques d'images : une ligne CSV par image, résumé (moyenne, p95, maximum, régions) sur stderr
v32sim -B B.v32 -r -q --fast --max-frames=1800 --frame-stats=load.csv --frame-stats-skip=250 cart.v32 </dev/null

# les fonctions les plus sollicitées
v32sim -B B.v32 -r -q --fast --max-frames=1800 --profile-by=function --profile-top=20 cart.v32 </dev/null

# un script de commandes du débogueur lu dans un fichier, sur stdout seulement ce qu'impriment `print`/`console`
v32sim -B B.v32 --batch --fast -x --v32kbd dos.v32 < session.cmd

# l'écran texte du programme à sa fin ; exécutions reproductibles
v32sim -B B.v32 -r -q --fast -x --max-frames=900 --console --fixed-clock --seed=1 --fixed-cycles cart.v32 </dev/null

# un programme texte sur votre terminal (Ctrl-] revient à l'invite)
v32sim --tty -B B.v32 v32nix.v32

# une session V32TRACE : rapports de trappes/événements PREFIX.traps.txt, .blocks.txt, .trace.txt
v32sim -B B.v32 -r -q --fast --max-frames=3000 --v32trace=out/run cart.v32 </dev/null
```

Sans `--batch`, un script envoyé sur stdin est répété à l'écran avec l'invite ; donnez `-q` quand un programme lit la sortie. Une
exécution `-r` avec des commandes sur stdin ou `-C` exécute ces commandes une fois au départ.

## Options

Les options marquées (forme courte) en ont une : `-B`, etc. « Par défaut » est ce qui s'applique quand l'option est absente.

### Chargement et BIOS

| Option | Signification |
|---|---|
| `-B, --biosfile=FILE` | Fichier V32 du BIOS. Par défaut : `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32` (l'installation du DesktopEmulator). |
| `-M, --memcfile=FILE` | Fichier de carte mémoire (`V32-MEMC` + 262144 mots). Les modifications sont réécrites à la fin de chaque image. Voir *Carte mémoire* ci-dessous. |
| `-E, --entry-point=OFFSET` | Commence l'exécution à OFFSET (hexadécimal) au lieu du début du BIOS. |
| `-S, --bios-start` | Démarre au début du code du BIOS (le comportement par défaut ; conservé pour la compatibilité, et `-E` l'emporte). |
| `CARTFILE.v32` ou `-` | La cartouche ; `-` = aucune. |
| `-V, --version` | Affiche `v32sim VERSION` et quitte. |
| `-h, --help` | Affiche le texte d'utilisation et quitte (code 0). |

### Informations de débogage et source

| Option | Signification |
|---|---|
| `--cart-asm-debug=FILE`, `--cart-c-debug=FILE` | Étiquettes / table des lignes de la cartouche, lues dans FILE (`.vbin.debug` de l'assembleur ; `.asm.debug` du compilateur). |
| `--bios-asm-debug=FILE`, `--bios-c-debug=FILE` | La même chose pour le BIOS. |
| `--debug-dir=DIR` | Cherche aussi les fichiers de débogage dans DIR, essayé en premier. Sans ces options, les fichiers de débogage sont cherchés à côté du fichier V32, dans `obj/` et `../obj/` de celui-ci, dans `./obj` et dans `.`. |
| `--symbol-file=FILE` | Symboles de données (`NOM.asm.sym` de v32cc). Les `NOM.asm.sym` / `NOM.vbin.sym` à côté de la cartouche sont lus automatiquement. |
| `-n, --no-debug` | Ne traite aucun fichier de débogage (étiquettes, lignes, symboles). |
| `--source` | Affiche la ligne source de chaque arrêt (assembleur ; C ou Lua avec `-L`). |
| `-L, --langmode=MODE` | `C`, `lua` ou `src` : le langage source de la superposition. `C` et `src` activent aussi `--source` ; `lua` affiche les valeurs Lua dans les vidages mémoire. `step`/`next` avancent alors par ligne source. Toute autre valeur est une erreur (message sur stderr, code de sortie 1). |
| `-d, --deref-addr` | Affiche l'adresse d'un déréférencement. |
| `-c, --colors` | Sortie en couleurs à l'invite. |

### Points d'arrêt, point d'entrée et surveillance

| Option | Signification |
|---|---|
| `-b, --break=OFFSET\|LABEL\|FILE:LINE` | Pose un point d'arrêt (répétable), comme le fait la commande `break` une fois les fichiers de débogage chargés. Un nom qui est une étiquette (ou une fonction) connue désigne cette étiquette même s'il est aussi un nombre hexadécimal (`add`, `face`, `dead` ; écrivez `0xdead` pour l'adresse) ; sinon la valeur est un OFFSET hexadécimal, avec ou sans `0x`. `FILE:LINE` demande des informations de ligne. Une valeur qui n'est rien de tout cela est signalée sur stderr. |
| `-w, --watch-for=OPCODE` | Exécute (`-r`) jusqu'à ce que ce mot d'instruction de 32 bits soit exécuté. |
| `-C, --command-file=FILE` | Exécute les commandes du débogueur contenues dans FILE. Elles sont lues par l'invite après la lecture de la première instruction (donc `step`, `next`, `continue` agissent) ; avec `-r`, elles s'exécutent une fois au départ. |
| `-e, --errorcheck` | Vérification des erreurs à l'exécution. |
| `-p, --profile` | Profilage des instructions ; `profile` affiche le rapport. |

### Contrôle de l'exécution, vitesse et turbo

| Option | Signification |
|---|---|
| `-r, --run` | Ne démarre pas en mode pas à pas : exécute jusqu'à un point d'arrêt, un point de surveillance, HLT ou la limite d'images. |
| `--max-frames=N` | S'arrête après N images, comptées depuis la mise sous tension (le logo du BIOS, environ 250 images, compris ; code de sortie 2 avec `-x`). |
| `--fast` | Ne bride pas à la vitesse du temps réel. |
| `--frame-us=N` | Microsecondes d'horloge murale par image. Par défaut 16666 ; 0 = `--fast`. |
| `--turbo[=report]` | `--fast`, plus l'avance rapide des boucles d'attente active et l'abandon d'une file de pixels GPU pleine au lieu de la peindre (les captures d'écran peuvent être incomplètes). `=report` affiche sur stderr à la sortie ce qui a été sauté. README « Vitesse et Mode Turbo ». Désactivé dès que quelque chose surveille les instructions une à une. |
| `--gpu-eager` | Peint chaque commande GPU immédiatement (mêmes images ; pour comparer et chronométrer). |
| `--fixed-clock[=Y,D,S]` | `TIM_CurrentDate/Time` démarrent à l'année Y, au jour D (0 = 1er janvier), à S secondes (par défaut `2026,0,0`) au lieu de l'horloge de l'hôte. |
| `--seed=N` | Initialise `RNG_CurrentValue` avec N au démarrage. |
| `--fixed-cycles` | Les lectures de `TIM_CycleCounter` renvoient 0. |
| `--warn-halt` | Signale sur stderr quand le programme exécute HLT (l'émulateur s'arrête alors). |
| `--halt-status=N` | Code de sortie N (1-255) quand l'exécution se termine sur HLT et que rien d'autre n'a mal tourné. |

### Sans interface, par lot et sans surveillance

| Option | Signification |
|---|---|
| `-q, --batch` (aussi `--quiet`) | Mode script : commandes lues sur stdin ou `-C` sans invite ni répétition ; ni bannière, ni ligne d'instruction, ni liste d'affichage, ni résumé des registres en fin d'exécution. stdout ne contient que ce qu'impriment `print`, `console`, ... ; les acquittements, les raisons d'arrêt et les diagnostics vont sur stderr. Les lignes vides sont ignorées ; la fin de l'entrée termine l'exécution ; le premier `expect` en échec, ou un `until` qui expire, la termine avec le code 1 ; une commande qui ne peut pas être analysée la termine avec 4. |
| `-x, --exit-status` | Le code de sortie indique comment le programme s'est terminé (voir *Code de sortie*). |
| `--dump=N[@ADDR]` ou `ADDR:N` | Après l'exécution, affiche N mots de RAM (signés, un par ligne) à partir de ADDR : hexadécimal, ou une étiquette (essayée aussi sous la forme `global_NOM`) ; par défaut le début de la RAM. |
| `--console` | Affiche l'écran texte en police du BIOS (64x18, ou 80x24 pour v32c++) quand l'exécution se termine. |

### Entrée

| Option | Signification |
|---|---|
| `--input=SCRIPT` | Maintient des boutons de manette par **image de cartouche** (0 = première image que la cartouche exécute) : `BOUTON[+BOUTON]@PREMIÈRE-DERNIÈRE,...` (`@IMAGE` pour une seule). Boutons `LEFT RIGHT UP DOWN START A B X Y L R` (`SELECT` = X). `@FICHIER` lit la ligne `V32_INPUT=` de FICHIER (un rapport `.trace.txt`). Par défaut : `$V32_INPUT`. |
| `--input-port=N` | Port de manette que pilote `--input` (0 par défaut) ; il est connecté. |
| `--input2=SCRIPT` | La même chose pour la manette 2 (port 1) ; `@FICHIER` lit la ligne `V32_INPUT2=`. Par défaut : `$V32_INPUT2`. Avec `--input-port=1`, c'est une erreur. |
| `--v32kbd[=PORT]` | Émule un clavier v32io sur le port de manette PORT (0-3, 0 par défaut). Le programme doit le lire à chaque image. |
| `--kbd-input=SOURCE` | Touches de l'hôte : `stdin`, `fd:N`, ou un chemin de FIFO/tube/fichier. Implique `--v32kbd`. |
| `--kbd-sync` | Ne remet l'événement de touche suivant qu'une fois que le programme a lu la manette depuis le précédent (par défaut, le nouvel événement remplace un événement non lu, comme le fait le vrai clavier). |
| `--v32mouse[=PORT]` | Émule une souris v32io sur le port de manette PORT (0-3, 1 par défaut) ; pilotez-la avec la commande `mouse`. |
| `--tty` | Mode interactif plein écran : la console texte sur ce terminal, les touches vers un v32kbd (port 0 sauf `--v32kbd=N`), implique `-r`. Ctrl-] = invite du débogueur, `continue` revient. Exige un terminal sur stdin et stdout. |

La manette, le clavier et la souris peuvent aussi être pilotés depuis l'invite (`gamepad`, `kbd`, `mouse`). README « Le Clavier
v32io », « La Souris v32io », « Manette Scriptée (--input) ».

### Carte mémoire et v32dev

`-M FICHIER` est la carte. Une boîte aux lettres v32dev se trouve dans les 16384 derniers mots de celle-ci ; le côté hôte est soit
un processus externe (`v32dev.py serve`), soit v32sim lui-même :

| Option | Signification |
|---|---|
| `--v32dev-doorbell[=MS]` | Bit 1 de l'indicateur d'en-tête, compteurs de réponses par canal (mots d'en-tête 10..13) ; une écriture dans le `c2h_head` d'une partie de cartouche sonne : vide les tampons, laisse à l'hôte MS ms (2 par défaut ; 0 = pas d'attente) pour répondre, importe la réponse, continue. Désactivé par défaut. |
| `--v32dev-host=N=SPEC` | Sert le canal N (0-3) nativement. Répétable. Exige `-M FICHIER` désignant une carte v32dev accessible en écriture (ou dont les 64 derniers Kio sont tous à zéro : elle est alors formatée). SPEC : `storage:FICHIER[,bytes][,ro][,sector=N][,create=S]` ou `serial:stdio\|echo[,hup=N]\|tcp=PORT[,in=FICHIER][,out=FICHIER][,hangup]` (`tcp=PORT` : 127.0.0.1 seulement, un client à la fois ; les options de `storage:` sont lues de droite à gauche, donc FICHIER peut contenir des virgules). L'emporte sur `V32DEV_CHN` pour son canal. |
| `--v32dev-host-force` | Formate la carte par-dessus d'autres données dans ses 64 derniers Kio. |

### Sortie, captures d'écran et console texte

| Option | Signification |
|---|---|
| `--screenshot-at=FRAMES` | Écrit l'écran (PNG) après ces images de cartouche : `N`, `A-B`, `A-B/PAS`, séparés par des virgules. L'image 0 est la première image qui se termine avec le compteur de programme dans la cartouche, c'est-à-dire une fois que le BIOS a passé la main (le logo du BIOS standard dure environ 250 images, que `--max-frames` compte aussi). Les images que l'exécution n'a jamais atteintes sont signalées sur stderr à la sortie. |
| `--screenshot-file=PAT` | Nom de fichier de celles-ci ; `%d` = l'image (style printf, par ex. `%05d`). Par défaut `v32sim-shot-%05d.png`. |
| `--screengrid[=SPEC]` | Grille sur les captures d'écran : `DX[xDY]`, `color=C`, `alpha=N`, `labels`/`nolabels`, séparés par des virgules. Par défaut 40x40. |
| `--highlight=T:R[:COLOR]` | Entoure la région R de la texture T (`bios:R` pour la texture du BIOS) ; séparés par des virgules. |
| `--noextra` | Ni grille ni contours. |
| `--console-color=MODE` | Couleurs de la console texte : `none`, `16`, `256`, `true`, `auto`. Par défaut : `auto` avec `--tty`, texte brut sinon. |
| `--cursor-color=COLOR` | Change la couleur du bloc curseur (caractère 20 du BIOS) : un nom, `#RRGGBB` ou `off`. |
| `-v, --verbose` | Plus de sortie (sur stderr). |
| `-D, --debug` | Sortie de débogage du simulateur (développement ; sur stderr). |

### Profilage, statistiques et traces

| Option | Signification |
|---|---|
| `--frame-stats=FILE` (`--stats`) | Une ligne CSV par image (cycles, % CPU, pixels GPU, % GPU, puis les compteurs GPU/SPU/bus) ; résumé (moyenne, p50, p95, p99, maximum, total, régions les plus chargées) sur stderr à la sortie. |
| `--frame-stats-skip=N` (`--stats-skip`) | Laisse les N premières images (le logo du BIOS, environ 250) hors du résumé. |
| `--frame-stats-top=N` (`--stats-top`) | Liste les N régions auxquelles ont été imputés le plus de pixels GPU (8 par défaut ; 0 = aucune). |
| `--stats-format=csv\|json` | `json` : JSON Lines avec des noms pointés stables et une ligne de résumé en dernier. |
| `--profile-out=FILE` | À la fin de l'exécution, écrit combien de fois chaque adresse d'instruction a été exécutée (n'exige ni invite ni `-p`) ; un fichier de débogage ajoute une colonne d'étiquettes. |
| `--profile-format=text\|bin` | `bin` : en-tête `V32PROF1`, puis des paires (adresse, compteur) de 32 bits petit-boutiste. Par défaut text. |
| `--profile-top=N` | Affiche sur stderr à la fin les N adresses (ou fonctions) les plus sollicitées. |
| `--profile-by=address\|function` | Avec `function`, `--profile-top` et le `--profile-out` texte additionnent chaque fonction (étiquette la plus proche). Par défaut address. |
| `--profile-event=KIND` | Ce qui est compté : `instr` (par défaut), `gpu-draw`, `gpu-portwrite`, `spu-write`, `cycles`. |
| `--time-breakdown` | À la sortie, où va le temps d'horloge murale propre à v32sim, par image (cpu/gpu/spu/sortie). |
| `--frame-log=FILE` | Avec lui, une ligne par image de cette ventilation. |
| `--gpu-trace=FILE`, `--spu-trace=FILE` | Enregistre les écritures de ports GPU / SPU par image (binaire, [TRACE-FORMAT.md](TRACE-FORMAT.md)). |
| `--trace-dump=FILE` | Affiche un fichier de trace en texte et quitte. |
| `--v32trace=PREFIX` | Enregistreur V32TRACE ([V32TRACE-SPEC.md](V32TRACE-SPEC.md) 2.1) : lit `<cart>.v32trace.meta` et les étiquettes, écrit `PREFIX.traps.txt`, `.blocks.txt`, `.trace.txt`. La cartouche redémarre après une trappe. |
| `--v32trace-meta=FILE`, `--v32trace-labels=FILE` | Désignent ces fichiers quand ils ne sont pas à côté de la cartouche. |
| `--v32trace-norestart` | Reste dans la trappe. |

## Environnement

| Variable | Usage |
|---|---|
| `V32_INPUT`, `V32_INPUT2` | Scripts de manette utilisés quand `--input` / `--input2` ne sont pas donnés (une note va sur stderr). |
| `V32DEV_DISABLE` | Toute valeur sauf `0` : une carte est une carte ordinaire, sans boîte aux lettres v32dev ; avec `--v32dev-host` ou `V32DEV_SERVER`, un avertissement, l'hôte ne démarre pas. |
| `V32DEV_SERVER` | Définie (et pas `0`) : le serveur v32dev intégré, comme dans le patch du DesktopEmulator du projet v32io : la carte n'a pas besoin d'être formatée, la boîte aux lettres vit en mémoire seulement et n'arrive jamais dans le fichier de la carte, et les canaux viennent de `V32DEV_CH0`..`V32DEV_CH3`. Non définie ou `0` : ces variables ne sont pas regardées. |
| `V32DEV_CH0` ... `V32DEV_CH3` | Avec `V32DEV_SERVER` : ce que sert le canal N, une SPEC comme pour `--v32dev-host` (`storage:FICHIER[,bytes][,ro][,sector=N][,create=S]`, `serial:echo[,hup=N]`, `serial:stdio`, `serial:tcp=PORT`). Vide ou non définie : rien. Une mauvaise est signalée et son canal laissé de côté. |
| `NO_COLOR`, `COLORTERM`, `TERM` | Ce que choisit `--console-color=auto` (et `--tty`) : aucune couleur si `NO_COLOR` est défini et non vide ; couleurs vraies pour `truecolor`/`24bit` ; 256 pour `*256color` ; 16 pour les autres terminaux sauf `dumb`. |
| `HOME`, `INPUTRC`, `TERM` | Lues par GNU readline pour l'invite (édition de ligne, historique en mémoire seulement). |

Les scripts utilitaires de `tools/` lisent aussi `V32SIM` (chemin du binaire), `V32SIM_FLAGS` (options supplémentaires), `V32BIOS`,
`V32TC` (répertoire de la chaîne d'outils) et `V32SIM_HOME` (pour les outils oracle) ; voir [TOOLS.md](TOOLS.md).

## Fichiers

Lus :

| Fichier | |
|---|---|
| BIOS | `-B FICHIER`, sinon `/usr/local/Vircon32/Emulator/Bios/StandardBios.v32`. |
| Cartouche | `CARTFILE.v32`. |
| `NOM.vbin.debug`, `NOM.asm.debug` | Étiquettes et table des lignes (assembleur, compilateur) de la cartouche et du BIOS, à côté de celle-ci, dans `obj/`, `../obj/`, `--debug-dir`. |
| `NOM.asm.sym`, `NOM.vbin.sym`, `NOM.asm` | Symboles de données (v32cc `-g` ; v32as `-g` ; les lignes `%define global_NOM`). |
| `NOM.vbin.vars` | Variables locales pour `locals` (v32as `-g`, issu de v32cc / v32lua `-g`). |
| `NOM.v32trace.meta`, étiquettes | Fichiers du traducteur pour `--v32trace`. |
| `-C FICHIER`, fichiers de script, `--input=@FICHIER`, `--kbd-input=CHEMIN` | Comme nommés. |

Écrits :

| Fichier | |
|---|---|
| Carte mémoire `-M FICHIER` | Mise à jour à la fin de chaque image (les cartes en lecture seule avertissent et ne sont pas sauvegardées). |
| `v32sim-shot-NNNNN.png` | `--screenshot-at` (`--screenshot-file` renomme). |
| `v32sim.YYYYMMDD-HHMMSS.png`, `.sav` | Noms par défaut des commandes `screenshot` et `save`. |
| `--frame-stats`, `--profile-out`, `--frame-log`, `--gpu-trace`, `--spu-trace` | Comme nommés. |
| `PREFIX.traps.txt`, `PREFIX.blocks.txt`, `PREFIX.trace.txt` | `--v32trace`. |
| `--v32dev-host` `storage:FICHIER`, `out=FICHIER` | Image disque et sortie série d'un périphérique natif. |

## Code de sortie

| Code | Signification |
|---|---|
| 0 | Fin normale : le programme a exécuté HLT (sauf avec `--halt-status`), ou `quit`, ou fin de l'entrée. |
| 1 | Une vérification a échoué : `expect` a échoué ou `until` a expiré. Problèmes de démarrage qui sortent avec 1 : une mauvaise option ou valeur d'option (`-L` avec un mode inconnu aussi), un fichier `-C` qui ne peut pas être ouvert, `--tty` sans terminal, un `--v32dev-host` erroné. |
| 2 | Limite d'images atteinte (`-x` seulement). C'est aussi le code quand `/dev/null` ne peut pas être ouvert au démarrage. |
| 3 | Erreur système (`-x` seulement ; l'exécution s'arrête au lieu d'entrer dans le gestionnaire d'erreurs du BIOS). |
| 4 | `--batch` n'a pas pu analyser une commande (ou une commande `dev` a échoué). |
| 5 | `--dump` n'a pas pu afficher ce qui était demandé (spécification invalide, symbole inconnu, mémoire illisible), quand rien d'autre n'avait fixé de code. |
| 11 | Démarrage : un fichier de BIOS, de cartouche ou de carte mémoire ne peut pas être ouvert. |
| 12 | Démarrage : un tel fichier ne peut pas être lu (tronqué, mauvaise position). |
| 21 | Démarrage : un tel fichier n'est pas un fichier V32 valide. |
| 4, 5, 7, 13, 14, 15 | Défaillances internes (mémoire insuffisante, mode d'accès invalide à un IOPort), avec un message `[ERROR]` sur stderr ; elles ne devraient pas arriver. 4 et 5 sont aussi les codes ci-dessus. |
| N | `quit N` l'emporte sur tout ce qui précède ; `--halt-status=N` fixe le code d'un HLT. |

## Voir aussi

[README](../README.fr.md) (commandes du débogueur, débogage au niveau du source, carte mémoire et v32dev, clavier et souris v32io,
console texte, exécution sans surveillance, scripts), [TOOLS.md](TOOLS.md), [TESTING.md](TESTING.md), [PROFILING.md](PROFILING.md),
[PERFORMANCE.md](PERFORMANCE.md), [TRACE-FORMAT.md](TRACE-FORMAT.md), [V32TRACE-SPEC.md](V32TRACE-SPEC.md). Page de manuel :
`man v32sim` (`man/v32sim.1`).
