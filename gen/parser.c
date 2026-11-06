/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 0

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* First part of user prologue.  */
#line 1 "src/parser.y"

/*
 * parser.y: bison grammar for v32sim prompt / command-file lines.
 *
 * The grammar only *recognizes* a command and records its pieces in a cmd_t; it
 * performs no simulator side effects (see command.c: cmd_execute()). One call to
 * yyparse() consumes one line.
 *
 * Summary of the language (case-insensitive; every command word may be abbreviated,
 * see cmd_lookup() in command.c):
 *
 *   break [ADDR|LABEL]            unbreak INDEX             backtrace
 *   continue | next | step | ignore | quit | profile | inventory | version
 *   print[/F]   OPERAND           display[/F] [OPERAND [LABEL]]
 *   label [ADDR NAME]             unlabel INDEX             undisplay INDEX
 *   set [NAME = VALUE]            replace IP:0xA IR:0xB IV:0xC
 *   watch [REG OP VALUE [LABEL]]  watchlist                unwatch INDEX|LABEL
 *   load PAGE:PATH                unload PAGE
 *   gamepad [N] [COMMAND]         help [COMMAND]            screenshot [FILE]
 *   save [FILE]                   restore FILE
 *
 *   run N | wait N                until "TEXT" [timeout N] [rows N]
 *   expect [/d] LOCATION OP VALUE   expect console "TEXT" [rows N]     quit [N]
 *   x[/F] LOCATION [COUNT]
 *
 *   OPERAND  := BASE | '[' BASE ']'                   ('[ ]' dereferences)
 *             | SYMREF | '[' SYMREF ']' | '[[' SYMREF ']]' | '&' SYMREF
 *   BASE     := REGISTER | reg | IOPORT_SYMBOL | HEXNUM | HEXNUM '-' HEXNUM
 *   SYMREF   := NAME | NAME '+' NUMBER | NAME '-' NUMBER      (a label or a C global)
 *   F        := b B d D f o s u x X
 */
#include "defines.h"
#include "command.h"

#define  CMD  (cmd_current ())

static void  set_operand    (uint8_t kind, uint8_t reg, uint32_t lo, uint32_t hi);
static int   operand_number (tok_t *t, uint32_t *value);
static int   operand_iosym  (tok_t *t);
static void  set_fmt        (char ch);
static int   set_lhs_word   (tok_t *t);
static int   break_all      (tok_t *t);
static int   loc_number     (tok_t *t, wloc_t *loc);
static int   loc_iosym      (tok_t *t, wloc_t *loc);
static int   sym_resolve    (tok_t *t, int sign, tok_t *offset, symv_t *out);
static void  set_operand_sym (symv_t *sym, uint8_t deref);
static int   console_form   (tok_t *word, tok_t *text);
static int   print_form     (tok_t *word);
static int   add_option     (tok_t *word, tok_t *value);
static int   check_signed   (void);

#line 123 "obj/parser.c"

# ifndef YY_CAST
#  ifdef __cplusplus
#   define YY_CAST(Type, Val) static_cast<Type> (Val)
#   define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type> (Val)
#  else
#   define YY_CAST(Type, Val) ((Type) (Val))
#   define YY_REINTERPRET_CAST(Type, Val) ((Type) (Val))
#  endif
# endif
# ifndef YY_NULLPTR
#  if defined __cplusplus
#   if 201103L <= __cplusplus
#    define YY_NULLPTR nullptr
#   else
#    define YY_NULLPTR 0
#   endif
#  else
#   define YY_NULLPTR ((void*)0)
#  endif
# endif

#include "parser.h"
/* Symbol kind.  */
enum yysymbol_kind_t
{
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                      /* "end of file"  */
  YYSYMBOL_YYerror = 1,                    /* error  */
  YYSYMBOL_YYUNDEF = 2,                    /* "invalid token"  */
  YYSYMBOL_T_NUM = 3,                      /* T_NUM  */
  YYSYMBOL_T_REG = 4,                      /* T_REG  */
  YYSYMBOL_T_IOSYM = 5,                    /* T_IOSYM  */
  YYSYMBOL_T_WORD = 6,                     /* T_WORD  */
  YYSYMBOL_T_PATH = 7,                     /* T_PATH  */
  YYSYMBOL_T_FILELINE = 8,                 /* T_FILELINE  */
  YYSYMBOL_T_FMT = 9,                      /* T_FMT  */
  YYSYMBOL_T_REGS = 10,                    /* T_REGS  */
  YYSYMBOL_T_EQEQ = 11,                    /* T_EQEQ  */
  YYSYMBOL_T_NE = 12,                      /* T_NE  */
  YYSYMBOL_T_LE = 13,                      /* T_LE  */
  YYSYMBOL_T_GE = 14,                      /* T_GE  */
  YYSYMBOL_T_BACKTRACE = 15,               /* T_BACKTRACE  */
  YYSYMBOL_T_BREAK = 16,                   /* T_BREAK  */
  YYSYMBOL_T_CONTINUE = 17,                /* T_CONTINUE  */
  YYSYMBOL_T_DISPLAY = 18,                 /* T_DISPLAY  */
  YYSYMBOL_T_GAMEPAD = 19,                 /* T_GAMEPAD  */
  YYSYMBOL_T_HELP = 20,                    /* T_HELP  */
  YYSYMBOL_T_IGNORE = 21,                  /* T_IGNORE  */
  YYSYMBOL_T_INVENTORY = 22,               /* T_INVENTORY  */
  YYSYMBOL_T_LABEL = 23,                   /* T_LABEL  */
  YYSYMBOL_T_LOAD = 24,                    /* T_LOAD  */
  YYSYMBOL_T_NEXT = 25,                    /* T_NEXT  */
  YYSYMBOL_T_PRINT = 26,                   /* T_PRINT  */
  YYSYMBOL_T_PROFILE = 27,                 /* T_PROFILE  */
  YYSYMBOL_T_QUIT = 28,                    /* T_QUIT  */
  YYSYMBOL_T_REPLACE = 29,                 /* T_REPLACE  */
  YYSYMBOL_T_SCREENGRID = 30,              /* T_SCREENGRID  */
  YYSYMBOL_T_NOEXTRA = 31,                 /* T_NOEXTRA  */
  YYSYMBOL_T_HIGHLIGHT = 32,               /* T_HIGHLIGHT  */
  YYSYMBOL_T_ASM = 33,                     /* T_ASM  */
  YYSYMBOL_T_RESTORE = 34,                 /* T_RESTORE  */
  YYSYMBOL_T_SAVE = 35,                    /* T_SAVE  */
  YYSYMBOL_T_SCREENSHOT = 36,              /* T_SCREENSHOT  */
  YYSYMBOL_T_SET = 37,                     /* T_SET  */
  YYSYMBOL_T_STEP = 38,                    /* T_STEP  */
  YYSYMBOL_T_UNBREAK = 39,                 /* T_UNBREAK  */
  YYSYMBOL_T_UNDISPLAY = 40,               /* T_UNDISPLAY  */
  YYSYMBOL_T_UNLABEL = 41,                 /* T_UNLABEL  */
  YYSYMBOL_T_UNLOAD = 42,                  /* T_UNLOAD  */
  YYSYMBOL_T_UNWATCH = 43,                 /* T_UNWATCH  */
  YYSYMBOL_T_VERSION = 44,                 /* T_VERSION  */
  YYSYMBOL_T_WATCH = 45,                   /* T_WATCH  */
  YYSYMBOL_T_WATCHLIST = 46,               /* T_WATCHLIST  */
  YYSYMBOL_T_KBD = 47,                     /* T_KBD  */
  YYSYMBOL_T_MOUSE = 48,                   /* T_MOUSE  */
  YYSYMBOL_T_MAILBOX = 49,                 /* T_MAILBOX  */
  YYSYMBOL_T_CONSOLE = 50,                 /* T_CONSOLE  */
  YYSYMBOL_T_RUN = 51,                     /* T_RUN  */
  YYSYMBOL_T_UNTIL = 52,                   /* T_UNTIL  */
  YYSYMBOL_T_EXPECT = 53,                  /* T_EXPECT  */
  YYSYMBOL_T_EXAMINE = 54,                 /* T_EXAMINE  */
  YYSYMBOL_T_SSTEP = 55,                   /* T_SSTEP  */
  YYSYMBOL_T_SNEXT = 56,                   /* T_SNEXT  */
  YYSYMBOL_T_FINISH = 57,                  /* T_FINISH  */
  YYSYMBOL_T_TBREAK = 58,                  /* T_TBREAK  */
  YYSYMBOL_T_LIST = 59,                    /* T_LIST  */
  YYSYMBOL_T_FRAME = 60,                   /* T_FRAME  */
  YYSYMBOL_T_UP = 61,                      /* T_UP  */
  YYSYMBOL_T_DOWN = 62,                    /* T_DOWN  */
  YYSYMBOL_T_LOCALS = 63,                  /* T_LOCALS  */
  YYSYMBOL_T_MARK = 64,                    /* T_MARK  */
  YYSYMBOL_T_BADCMD = 65,                  /* T_BADCMD  */
  YYSYMBOL_T_BAD = 66,                     /* T_BAD  */
  YYSYMBOL_67_ = 67,                       /* '-'  */
  YYSYMBOL_68_ = 68,                       /* '['  */
  YYSYMBOL_69_ = 69,                       /* ']'  */
  YYSYMBOL_70_ = 70,                       /* '&'  */
  YYSYMBOL_71_ = 71,                       /* '+'  */
  YYSYMBOL_72_ = 72,                       /* '='  */
  YYSYMBOL_73_ = 73,                       /* ':'  */
  YYSYMBOL_74_ = 74,                       /* '<'  */
  YYSYMBOL_75_ = 75,                       /* '>'  */
  YYSYMBOL_YYACCEPT = 76,                  /* $accept  */
  YYSYMBOL_line = 77,                      /* line  */
  YYSYMBOL_command = 78,                   /* command  */
  YYSYMBOL_name = 79,                      /* name  */
  YYSYMBOL_break_cmd = 80,                 /* break_cmd  */
  YYSYMBOL_list_cmd = 81,                  /* list_cmd  */
  YYSYMBOL_locals_cmd = 82,                /* locals_cmd  */
  YYSYMBOL_label_cmd = 83,                 /* label_cmd  */
  YYSYMBOL_format = 84,                    /* format  */
  YYSYMBOL_print_cmd = 85,                 /* print_cmd  */
  YYSYMBOL_display_cmd = 86,               /* display_cmd  */
  YYSYMBOL_operand = 87,                   /* operand  */
  YYSYMBOL_symref = 88,                    /* symref  */
  YYSYMBOL_base = 89,                      /* base  */
  YYSYMBOL_set_cmd = 90,                   /* set_cmd  */
  YYSYMBOL_set_lhs = 91,                   /* set_lhs  */
  YYSYMBOL_set_rhs = 92,                   /* set_rhs  */
  YYSYMBOL_replace_cmd = 93,               /* replace_cmd  */
  YYSYMBOL_repl_list = 94,                 /* repl_list  */
  YYSYMBOL_repl_item = 95,                 /* repl_item  */
  YYSYMBOL_watch_cmd = 96,                 /* watch_cmd  */
  YYSYMBOL_watch_target = 97,              /* watch_target  */
  YYSYMBOL_watch_rhs = 98,                 /* watch_rhs  */
  YYSYMBOL_relop = 99,                     /* relop  */
  YYSYMBOL_run_cmd = 100,                  /* run_cmd  */
  YYSYMBOL_until_cmd = 101,                /* until_cmd  */
  YYSYMBOL_until_opts = 102,               /* until_opts  */
  YYSYMBOL_expect_cmd = 103,               /* expect_cmd  */
  YYSYMBOL_expect_opts = 104,              /* expect_opts  */
  YYSYMBOL_examine_cmd = 105,              /* examine_cmd  */
  YYSYMBOL_examine_loc = 106,              /* examine_loc  */
  YYSYMBOL_gamepad_cmd = 107,              /* gamepad_cmd  */
  YYSYMBOL_gp_word = 108,                  /* gp_word  */
  YYSYMBOL_help_cmd = 109,                 /* help_cmd  */
  YYSYMBOL_load_cmd = 110,                 /* load_cmd  */
  YYSYMBOL_undo_cmd = 111                  /* undo_cmd  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;




#ifdef short
# undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
# include <limits.h> /* INFRINGES ON USER NAME SPACE */
# if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#  define YY_STDINT_H
# endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
# undef UINT_LEAST8_MAX
# undef UINT_LEAST16_MAX
# define UINT_LEAST8_MAX 255
# define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H \
       && UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
# if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#  define YYPTRDIFF_T __PTRDIFF_TYPE__
#  define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
# elif defined PTRDIFF_MAX
#  ifndef ptrdiff_t
#   include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  endif
#  define YYPTRDIFF_T ptrdiff_t
#  define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
# else
#  define YYPTRDIFF_T long
#  define YYPTRDIFF_MAXIMUM LONG_MAX
# endif
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned
# endif
#endif

#define YYSIZE_MAXIMUM                                  \
  YY_CAST (YYPTRDIFF_T,                                 \
           (YYPTRDIFF_MAXIMUM < YY_CAST (YYSIZE_T, -1)  \
            ? YYPTRDIFF_MAXIMUM                         \
            : YY_CAST (YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST (YYPTRDIFF_T, sizeof (X))


/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif


#ifndef YY_ATTRIBUTE_PURE
# if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define YY_ATTRIBUTE_PURE
# endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#  define YY_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define YY_ATTRIBUTE_UNUSED
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YY_USE(E) ((void) (E))
#else
# define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && ! defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
# if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")
# else
#  define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                           \
    _Pragma ("GCC diagnostic push")                                     \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")              \
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# endif
# define YY_IGNORE_MAYBE_UNINITIALIZED_END      \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && ! defined __ICC && 6 <= __GNUC__
# define YY_IGNORE_USELESS_CAST_BEGIN                          \
    _Pragma ("GCC diagnostic push")                            \
    _Pragma ("GCC diagnostic ignored \"-Wuseless-cast\"")
# define YY_IGNORE_USELESS_CAST_END            \
    _Pragma ("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_BEGIN
# define YY_IGNORE_USELESS_CAST_END
#endif


#define YY_ASSERT(E) ((void) (0 && (E)))

#if !defined yyoverflow

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* !defined yyoverflow */

#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (YYSIZEOF (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (YYSIZEOF (yy_state_t) + YYSIZEOF (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYPTRDIFF_T yynewbytes;                                         \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * YYSIZEOF (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / YYSIZEOF (*yyptr);                        \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, YY_CAST (YYSIZE_T, (Count)) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYPTRDIFF_T yyi;                      \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  138
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   268

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  76
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  36
/* YYNRULES -- Number of rules.  */
#define YYNRULES  178
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  242

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK   321


/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                                \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                     \
   ? YY_CAST (yysymbol_kind_t, yytranslate[YYX])        \
   : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    70,     2,
       2,     2,     2,    71,     2,    67,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    73,     2,
      74,    72,    75,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    68,     2,    69,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    53,    54,
      55,    56,    57,    58,    59,    60,    61,    62,    63,    64,
      65,    66
};

#if YYDEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] =
{
       0,    81,    81,    82,    86,    87,    88,    89,    90,    91,
      92,    93,    94,    95,    96,    97,    98,    99,   100,   101,
     102,   103,   104,   105,   106,   107,   108,   109,   110,   111,
     112,   113,   114,   119,   120,   121,   122,   123,   124,   125,
     126,   127,   128,   129,   130,   131,   132,   133,   134,   135,
     136,   137,   138,   139,   140,   141,   142,   143,   144,   145,
     146,   147,   148,   149,   150,   162,   163,   164,   170,   171,
     173,   174,   176,   179,   181,   183,   184,   191,   192,   193,
     195,   201,   202,   208,   209,   216,   217,   221,   225,   226,
     227,   231,   232,   233,   234,   235,   236,   247,   248,   249,
     253,   254,   255,   256,   262,   271,   272,   276,   277,   280,
     284,   288,   289,   290,   291,   297,   301,   302,   306,   327,
     328,   333,   343,   344,   345,   346,   347,   353,   354,   355,
     356,   357,   358,   359,   360,   361,   362,   363,   367,   368,
     369,   370,   371,   372,   373,   379,   380,   390,   391,   392,
     394,   399,   400,   406,   410,   415,   422,   426,   435,   436,
     442,   443,   450,   451,   452,   460,   461,   462,   463,   467,
     473,   474,   480,   481,   487,   488,   489,   490,   491
};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST (yysymbol_kind_t, yystos[State])

#if YYDEBUG || 0
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char *yysymbol_name (yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "\"end of file\"", "error", "\"invalid token\"", "T_NUM", "T_REG",
  "T_IOSYM", "T_WORD", "T_PATH", "T_FILELINE", "T_FMT", "T_REGS", "T_EQEQ",
  "T_NE", "T_LE", "T_GE", "T_BACKTRACE", "T_BREAK", "T_CONTINUE",
  "T_DISPLAY", "T_GAMEPAD", "T_HELP", "T_IGNORE", "T_INVENTORY", "T_LABEL",
  "T_LOAD", "T_NEXT", "T_PRINT", "T_PROFILE", "T_QUIT", "T_REPLACE",
  "T_SCREENGRID", "T_NOEXTRA", "T_HIGHLIGHT", "T_ASM", "T_RESTORE",
  "T_SAVE", "T_SCREENSHOT", "T_SET", "T_STEP", "T_UNBREAK", "T_UNDISPLAY",
  "T_UNLABEL", "T_UNLOAD", "T_UNWATCH", "T_VERSION", "T_WATCH",
  "T_WATCHLIST", "T_KBD", "T_MOUSE", "T_MAILBOX", "T_CONSOLE", "T_RUN",
  "T_UNTIL", "T_EXPECT", "T_EXAMINE", "T_SSTEP", "T_SNEXT", "T_FINISH",
  "T_TBREAK", "T_LIST", "T_FRAME", "T_UP", "T_DOWN", "T_LOCALS", "T_MARK",
  "T_BADCMD", "T_BAD", "'-'", "'['", "']'", "'&'", "'+'", "'='", "':'",
  "'<'", "'>'", "$accept", "line", "command", "name", "break_cmd",
  "list_cmd", "locals_cmd", "label_cmd", "format", "print_cmd",
  "display_cmd", "operand", "symref", "base", "set_cmd", "set_lhs",
  "set_rhs", "replace_cmd", "repl_list", "repl_item", "watch_cmd",
  "watch_target", "watch_rhs", "relop", "run_cmd", "until_cmd",
  "until_opts", "expect_cmd", "expect_opts", "examine_cmd", "examine_loc",
  "gamepad_cmd", "gp_word", "help_cmd", "load_cmd", "undo_cmd", YY_NULLPTR
};

static const char *
yysymbol_name (yysymbol_kind_t yysymbol)
{
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-135)

#define yypact_value_is_default(Yyn) \
  ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-159)

#define yytable_value_is_error(Yyn) \
  0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] =
{
     203,  -135,   105,  -135,    15,   121,   165,  -135,  -135,     2,
      10,  -135,    31,  -135,    53,    59,    50,    57,    62,    64,
      66,    70,    71,   149,  -135,    63,    77,    78,    79,   153,
    -135,    26,  -135,    81,    83,    88,    87,   113,   127,    46,
      31,  -135,  -135,  -135,   134,   143,   114,   117,   131,   165,
     129,  -135,   150,  -135,  -135,  -135,  -135,  -135,  -135,  -135,
    -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,
    -135,  -135,  -135,  -135,  -135,  -135,   137,  -135,  -135,    28,
     165,  -135,  -135,  -135,   165,   100,    28,  -135,   101,    59,
    -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,
    -135,  -135,    94,  -135,  -135,  -135,  -135,  -135,  -135,    96,
    -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,
    -135,     8,    96,   169,  -135,    93,     4,  -135,  -135,  -135,
    -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,  -135,    36,
     109,  -135,  -135,   -65,  -135,    55,   169,   165,  -135,  -135,
    -135,  -135,   170,  -135,   175,  -135,    22,    93,   173,     3,
      96,   177,   178,    93,    93,   115,  -135,  -135,  -135,  -135,
    -135,  -135,  -135,    16,  -135,   169,  -135,   179,  -135,  -135,
     180,   169,   116,   118,  -135,  -135,  -135,  -135,  -135,  -135,
     183,   185,  -135,    16,   186,   184,    93,  -135,  -135,    16,
      16,  -135,  -135,  -135,  -135,   188,   157,   169,   189,  -135,
    -135,   124,  -135,  -135,   125,  -135,  -135,  -135,  -135,   165,
    -135,   192,    16,  -135,  -135,  -135,   128,   130,   132,   133,
    -135,  -135,  -135,   135,  -135,  -135,  -135,  -135,  -135,  -135,
    -135,  -135
};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       2,     4,    68,     5,    85,   165,   170,    21,    25,    83,
       0,     6,    85,    24,    22,     0,    33,    35,    37,     0,
      32,    28,    39,   105,     7,     0,     0,     0,     0,     0,
      26,    85,    27,    53,    55,    59,    57,   146,   147,     0,
      85,     8,     9,    10,     0,    77,    11,    13,    15,    81,
      19,    64,     0,     3,    41,    17,    18,    42,    43,    44,
      45,    46,    47,    48,    49,    50,    51,    52,    61,    62,
      63,    69,    66,    67,    65,    71,     0,    70,    86,     0,
     166,   169,   167,   171,     0,     0,     0,    23,     0,   115,
     116,    34,    36,    38,    29,    31,    30,    40,   109,   107,
     108,   110,     0,   174,   175,   176,   173,   177,   178,     0,
      54,    56,    60,    58,   145,   148,   149,   151,   124,   122,
     123,    97,     0,     0,   125,     0,     0,    74,    76,    75,
      78,    79,    80,    12,    14,    16,    82,    20,     1,     0,
     103,   100,   102,    97,   101,     0,     0,    89,    93,    91,
     168,    84,     0,    87,     0,   117,     0,     0,   150,    97,
       0,     0,     0,     0,     0,     0,   139,   140,   143,   144,
     138,   141,   142,     0,   164,     0,   162,   160,    73,    72,
       0,     0,     0,     0,    96,    90,   172,   118,   111,   114,
       0,     0,   106,     0,     0,   153,     0,    99,    98,     0,
       0,   126,   127,   130,   131,     0,     0,     0,     0,   136,
     156,     0,   161,   104,     0,    94,    92,   112,   113,   120,
     152,     0,     0,   154,   157,   128,     0,     0,     0,     0,
     137,   129,   163,     0,   121,   159,   155,   132,   134,   133,
     135,    95
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -135,  -135,  -135,    -2,  -135,  -135,  -135,  -135,     6,  -135,
    -135,   110,   -78,    58,  -135,  -135,  -135,  -135,  -135,   111,
    -135,   -68,   -81,  -134,  -135,  -135,  -135,  -135,  -135,  -135,
    -135,  -135,   126,  -135,  -135,  -135
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] =
{
       0,    52,    53,    81,    54,    55,    56,    57,    79,    58,
      59,   147,   124,   149,    60,   102,   192,    61,    89,    90,
      62,   125,   210,   173,    63,    64,   158,    65,   195,    66,
     177,    67,    82,    68,    69,    70
};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      77,   148,   161,  -158,    83,    84,   162,   174,   148,  -158,
     143,   118,   119,   120,   159,   -88,    85,   160,    86,   202,
     203,   204,   143,   193,    78,   188,  -119,   108,   189,   199,
     200,   140,   141,   142,   143,    78,   117,   109,   144,   178,
      78,   157,   129,   132,   179,   165,   126,   136,   176,   118,
     119,   120,   121,   163,   164,   122,    87,    91,   140,   141,
     142,   143,   222,    88,    92,   144,   103,   182,   184,    93,
     161,    94,   175,    95,   162,   161,   123,    96,    97,   162,
     104,   105,   151,   205,   206,   106,   207,   208,   110,   190,
     111,   112,   196,   191,   113,   209,   145,   211,   146,   118,
     119,   120,   143,   214,   166,   167,   168,   169,    71,    72,
      73,    74,   219,    75,   123,   209,   114,   133,   223,   224,
     134,   209,   209,   181,    80,    72,    73,    74,   229,   230,
     115,    72,    73,    74,   135,   116,   137,   127,    72,    73,
      74,   236,   128,   139,   209,   185,   130,    72,    73,    74,
     138,   131,    98,    99,   100,   101,   107,    72,    73,    74,
     226,   227,   228,   143,   123,   170,   156,   171,   172,    72,
      73,    74,    76,   152,   154,   143,   180,   186,   187,   194,
     197,   198,   212,   213,   201,   215,   217,   216,   218,   220,
     221,   225,   231,   232,   233,   235,   153,   237,     0,   238,
     155,   239,   240,   183,   241,     0,   150,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,   234,     1,     2,
       3,     4,     5,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,    27,    28,    29,    30,    31,    32,
      33,    34,    35,    36,    37,    38,    39,    40,    41,    42,
      43,    44,    45,    46,    47,    48,    49,    50,    51
};

static const yytype_int16 yycheck[] =
{
       2,    79,    67,     0,     6,     3,    71,     3,    86,     6,
       6,     3,     4,     5,     6,     0,     6,     9,    12,     3,
       4,     5,     6,   157,     9,     3,     0,    29,     6,   163,
     164,     3,     4,     5,     6,     9,    38,    31,    10,     3,
       9,   109,    44,    45,     8,   123,    40,    49,   126,     3,
       4,     5,     6,   121,   122,     9,     3,     7,     3,     4,
       5,     6,   196,     4,     7,    10,     3,   145,   146,     7,
      67,     7,    68,     7,    71,    67,    68,     7,     7,    71,
       3,     3,    84,    67,    68,     6,    70,    71,     7,    67,
       7,     3,   160,    71,     7,   173,    68,   175,    70,     3,
       4,     5,     6,   181,    11,    12,    13,    14,     3,     4,
       5,     6,   193,     8,    68,   193,     3,     3,   199,   200,
       3,   199,   200,    68,     3,     4,     5,     6,   206,   207,
       3,     4,     5,     6,     3,     8,     7,     3,     4,     5,
       6,   222,     8,     6,   222,   147,     3,     4,     5,     6,
       0,     8,     3,     4,     5,     6,     3,     4,     5,     6,
       3,     4,     5,     6,    68,    72,    72,    74,    75,     4,
       5,     6,    67,    73,    73,     6,    67,     7,     3,     6,
       3,     3,     3,     3,    69,    69,     3,    69,     3,     3,
       6,     3,     3,    69,    69,     3,    86,    69,    -1,    69,
      89,    69,    69,   145,    69,    -1,    80,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,   219,    15,    16,
      17,    18,    19,    20,    21,    22,    23,    24,    25,    26,
      27,    28,    29,    30,    31,    32,    33,    34,    35,    36,
      37,    38,    39,    40,    41,    42,    43,    44,    45,    46,
      47,    48,    49,    50,    51,    52,    53,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    63,    64,    65
};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] =
{
       0,    15,    16,    17,    18,    19,    20,    21,    22,    23,
      24,    25,    26,    27,    28,    29,    30,    31,    32,    33,
      34,    35,    36,    37,    38,    39,    40,    41,    42,    43,
      44,    45,    46,    47,    48,    49,    50,    51,    52,    53,
      54,    55,    56,    57,    58,    59,    60,    61,    62,    63,
      64,    65,    77,    78,    80,    81,    82,    83,    85,    86,
      90,    93,    96,   100,   101,   103,   105,   107,   109,   110,
     111,     3,     4,     5,     6,     8,    67,    79,     9,    84,
       3,    79,   108,    79,     3,     6,    84,     3,     4,    94,
      95,     7,     7,     7,     7,     7,     7,     7,     3,     4,
       5,     6,    91,     3,     3,     3,     6,     3,    79,    84,
       7,     7,     3,     7,     3,     3,     8,    79,     3,     4,
       5,     6,     9,    68,    88,    97,    84,     3,     8,    79,
       3,     8,    79,     3,     3,     3,    79,     7,     0,     6,
       3,     4,     5,     6,    10,    68,    70,    87,    88,    89,
     108,    79,    73,    87,    73,    95,    72,    97,   102,     6,
       9,    67,    71,    97,    97,    88,    11,    12,    13,    14,
      72,    74,    75,    99,     3,    68,    88,   106,     3,     8,
      67,    68,    88,    89,    88,    79,     7,     3,     3,     6,
      67,    71,    92,    99,     6,   104,    97,     3,     3,    99,
      99,    69,     3,     4,     5,    67,    68,    70,    71,    88,
      98,    88,     3,     3,    88,    69,    69,     3,     3,    98,
       3,     6,    99,    98,    98,     3,     3,     4,     5,    88,
      88,     3,    69,    69,    79,     3,    98,    69,    69,    69,
      69,    69
};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] =
{
       0,    76,    77,    77,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    78,    78,    78,    78,    78,
      78,    78,    78,    78,    78,    79,    79,    79,    80,    80,
      80,    80,    80,    80,    80,    80,    80,    81,    81,    81,
      81,    82,    82,    83,    83,    84,    84,    85,    86,    86,
      86,    87,    87,    87,    87,    87,    87,    88,    88,    88,
      89,    89,    89,    89,    89,    90,    90,    91,    91,    91,
      91,    92,    92,    92,    92,    93,    94,    94,    95,    96,
      96,    96,    97,    97,    97,    97,    97,    98,    98,    98,
      98,    98,    98,    98,    98,    98,    98,    98,    99,    99,
      99,    99,    99,    99,    99,   100,   100,   101,   101,   101,
     101,   102,   102,   103,   103,   103,   103,   103,   104,   104,
     105,   105,   106,   106,   106,   107,   107,   107,   107,   108,
     109,   109,   110,   110,   111,   111,   111,   111,   111
};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr2[] =
{
       0,     2,     0,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     2,     1,     2,     1,     2,     1,     1,     1,
       2,     1,     1,     2,     1,     1,     1,     1,     1,     2,
       2,     2,     1,     1,     2,     1,     2,     1,     2,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     2,     1,     2,     1,     2,     1,
       2,     1,     1,     1,     1,     1,     1,     1,     1,     2,
       2,     2,     4,     4,     2,     2,     2,     1,     2,     2,
       2,     1,     2,     1,     3,     0,     1,     3,     1,     3,
       4,     1,     3,     1,     3,     5,     2,     1,     3,     3,
       1,     1,     1,     1,     3,     1,     4,     1,     1,     1,
       1,     1,     2,     2,     1,     2,     1,     2,     3,     1,
       5,     6,     1,     1,     1,     1,     3,     1,     2,     2,
       1,     1,     3,     3,     3,     3,     1,     2,     1,     1,
       1,     1,     1,     1,     1,     2,     1,     1,     2,     2,
       3,     0,     3,     4,     5,     6,     4,     5,     0,     3,
       3,     4,     1,     3,     1,     1,     2,     2,     3,     1,
       1,     2,     4,     2,     2,     2,     2,     2,     2
};


enum { YYENOMEM = -2 };

#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab
#define YYNOMEM         goto yyexhaustedlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                    \
  do                                                              \
    if (yychar == YYEMPTY)                                        \
      {                                                           \
        yychar = (Token);                                         \
        yylval = (Value);                                         \
        YYPOPSTACK (yylen);                                       \
        yystate = *yyssp;                                         \
        goto yybackup;                                            \
      }                                                           \
    else                                                          \
      {                                                           \
        yyerror (YY_("syntax error: cannot back up")); \
        YYERROR;                                                  \
      }                                                           \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use YYerror or YYUNDEF. */
#define YYERRCODE YYUNDEF


/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)




# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Kind, Value); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void
yy_symbol_value_print (FILE *yyo,
                       yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  FILE *yyoutput = yyo;
  YY_USE (yyoutput);
  if (!yyvaluep)
    return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void
yy_symbol_print (FILE *yyo,
                 yysymbol_kind_t yykind, YYSTYPE const * const yyvaluep)
{
  YYFPRINTF (yyo, "%s %s (",
             yykind < YYNTOKENS ? "token" : "nterm", yysymbol_name (yykind));

  yy_symbol_value_print (yyo, yykind, yyvaluep);
  YYFPRINTF (yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yy_state_t *yybottom, yy_state_t *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yy_state_t *yyssp, YYSTYPE *yyvsp,
                 int yyrule)
{
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %d):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       YY_ACCESSING_SYMBOL (+yyssp[yyi + 1 - yynrhs]),
                       &yyvsp[(yyi + 1) - (yynrhs)]);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args) ((void) 0)
# define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif






/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg,
            yysymbol_kind_t yykind, YYSTYPE *yyvaluep)
{
  YY_USE (yyvaluep);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE (yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}


/* Lookahead token kind.  */
int yychar;

/* The semantic value of the lookahead symbol.  */
YYSTYPE yylval;
/* Number of syntax errors so far.  */
int yynerrs;




/*----------.
| yyparse.  |
`----------*/

int
yyparse (void)
{
    yy_state_fast_t yystate = 0;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus = 0;

    /* Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* Their size.  */
    YYPTRDIFF_T yystacksize = YYINITDEPTH;

    /* The state stack: array, bottom, top.  */
    yy_state_t yyssa[YYINITDEPTH];
    yy_state_t *yyss = yyssa;
    yy_state_t *yyssp = yyss;

    /* The semantic value stack: array, bottom, top.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs = yyvsa;
    YYSTYPE *yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yychar = YYEMPTY; /* Cause a token to be read.  */

  goto yysetstate;


/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;


/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF ((stderr, "Entering state %d\n", yystate));
  YY_ASSERT (0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST (yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT (yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYPTRDIFF_T yysize = yyssp - yyss + 1;

# if defined yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        yy_state_t *yyss1 = yyss;
        YYSTYPE *yyvs1 = yyvs;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * YYSIZEOF (*yyssp),
                    &yyvs1, yysize * YYSIZEOF (*yyvsp),
                    &yystacksize);
        yyss = yyss1;
        yyvs = yyvs1;
      }
# else /* defined YYSTACK_RELOCATE */
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        YYNOMEM;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yy_state_t *yyss1 = yyss;
        union yyalloc *yyptr =
          YY_CAST (union yyalloc *,
                   YYSTACK_ALLOC (YY_CAST (YYSIZE_T, YYSTACK_BYTES (yystacksize))));
        if (! yyptr)
          YYNOMEM;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YY_IGNORE_USELESS_CAST_BEGIN
      YYDPRINTF ((stderr, "Stack size increased to %ld\n",
                  YY_CAST (long, yystacksize)));
      YY_IGNORE_USELESS_CAST_END

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */


  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;


/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token\n"));
      yychar = yylex ();
    }

  if (yychar <= YYEOF)
    {
      yychar = YYEOF;
      yytoken = YYSYMBOL_YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else if (yychar == YYerror)
    {
      /* The scanner already issued an error message, process directly
         to error recovery.  But do not keep the error token as
         lookahead, it is too special and may lead us to an endless
         loop in error recovery. */
      yychar = YYUNDEF;
      yytoken = YYSYMBOL_YYerror;
      goto yyerrlab1;
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = YYEMPTY;
  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
  case 4: /* command: T_BACKTRACE  */
#line 86 "src/parser.y"
                                        { CMD -> id = CMD_BACKTRACE; }
#line 1412 "obj/parser.c"
    break;

  case 5: /* command: T_CONTINUE  */
#line 87 "src/parser.y"
                                        { CMD -> id = CMD_CONTINUE;  }
#line 1418 "obj/parser.c"
    break;

  case 6: /* command: T_NEXT  */
#line 88 "src/parser.y"
                                        { CMD -> id = CMD_NEXT;      }
#line 1424 "obj/parser.c"
    break;

  case 7: /* command: T_STEP  */
#line 89 "src/parser.y"
                                        { CMD -> id = CMD_STEP;      }
#line 1430 "obj/parser.c"
    break;

  case 8: /* command: T_SSTEP  */
#line 90 "src/parser.y"
                                        { CMD -> id = CMD_SSTEP;     }
#line 1436 "obj/parser.c"
    break;

  case 9: /* command: T_SNEXT  */
#line 91 "src/parser.y"
                                        { CMD -> id = CMD_SNEXT;     }
#line 1442 "obj/parser.c"
    break;

  case 10: /* command: T_FINISH  */
#line 92 "src/parser.y"
                                        { CMD -> id = CMD_FINISH;    }
#line 1448 "obj/parser.c"
    break;

  case 11: /* command: T_FRAME  */
#line 93 "src/parser.y"
                                        { CMD -> id = CMD_FRAME;     }
#line 1454 "obj/parser.c"
    break;

  case 12: /* command: T_FRAME T_NUM  */
#line 94 "src/parser.y"
                                        { CMD -> id = CMD_FRAME;     CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 1460 "obj/parser.c"
    break;

  case 13: /* command: T_UP  */
#line 95 "src/parser.y"
                                        { CMD -> id = CMD_UP;        }
#line 1466 "obj/parser.c"
    break;

  case 14: /* command: T_UP T_NUM  */
#line 96 "src/parser.y"
                                        { CMD -> id = CMD_UP;        CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 1472 "obj/parser.c"
    break;

  case 15: /* command: T_DOWN  */
#line 97 "src/parser.y"
                                        { CMD -> id = CMD_DOWN;      }
#line 1478 "obj/parser.c"
    break;

  case 16: /* command: T_DOWN T_NUM  */
#line 98 "src/parser.y"
                                        { CMD -> id = CMD_DOWN;      CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 1484 "obj/parser.c"
    break;

  case 19: /* command: T_MARK  */
#line 101 "src/parser.y"
                                        { CMD -> id = CMD_MARK; }
#line 1490 "obj/parser.c"
    break;

  case 20: /* command: T_MARK T_PATH  */
#line 102 "src/parser.y"
                                        { CMD -> id = CMD_MARK; CMD -> path = (yyvsp[0].t).text; }
#line 1496 "obj/parser.c"
    break;

  case 21: /* command: T_IGNORE  */
#line 103 "src/parser.y"
                                        { CMD -> id = CMD_IGNORE;    }
#line 1502 "obj/parser.c"
    break;

  case 22: /* command: T_QUIT  */
#line 104 "src/parser.y"
                                        { CMD -> id = CMD_QUIT;      }
#line 1508 "obj/parser.c"
    break;

  case 23: /* command: T_QUIT T_NUM  */
#line 105 "src/parser.y"
                                        { CMD -> id = CMD_QUIT;      CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 1514 "obj/parser.c"
    break;

  case 24: /* command: T_PROFILE  */
#line 106 "src/parser.y"
                                        { CMD -> id = CMD_PROFILE;   }
#line 1520 "obj/parser.c"
    break;

  case 25: /* command: T_INVENTORY  */
#line 107 "src/parser.y"
                                        { CMD -> id = CMD_INVENTORY; }
#line 1526 "obj/parser.c"
    break;

  case 26: /* command: T_VERSION  */
#line 108 "src/parser.y"
                                        { CMD -> id = CMD_VERSION;   }
#line 1532 "obj/parser.c"
    break;

  case 27: /* command: T_WATCHLIST  */
#line 109 "src/parser.y"
                                        { CMD -> id = CMD_WATCHLIST; }
#line 1538 "obj/parser.c"
    break;

  case 28: /* command: T_SAVE  */
#line 110 "src/parser.y"
                                        { CMD -> id = CMD_SAVE; }
#line 1544 "obj/parser.c"
    break;

  case 29: /* command: T_ASM T_PATH  */
#line 111 "src/parser.y"
                                        { CMD -> id = CMD_ASM;     CMD -> path = (yyvsp[0].t).text; }
#line 1550 "obj/parser.c"
    break;

  case 30: /* command: T_SAVE T_PATH  */
#line 112 "src/parser.y"
                                        { CMD -> id = CMD_SAVE;    CMD -> path = (yyvsp[0].t).text; }
#line 1556 "obj/parser.c"
    break;

  case 31: /* command: T_RESTORE T_PATH  */
#line 113 "src/parser.y"
                                        { CMD -> id = CMD_RESTORE; CMD -> path = (yyvsp[0].t).text; }
#line 1562 "obj/parser.c"
    break;

  case 32: /* command: T_RESTORE  */
#line 115 "src/parser.y"
        {
            cmd_seterr ("restore: a file name is required");
            YYERROR;
        }
#line 1571 "obj/parser.c"
    break;

  case 33: /* command: T_SCREENGRID  */
#line 119 "src/parser.y"
                                        { CMD -> id = CMD_SCREENGRID; }
#line 1577 "obj/parser.c"
    break;

  case 34: /* command: T_SCREENGRID T_PATH  */
#line 120 "src/parser.y"
                                        { CMD -> id = CMD_SCREENGRID; CMD -> path = (yyvsp[0].t).text; }
#line 1583 "obj/parser.c"
    break;

  case 35: /* command: T_NOEXTRA  */
#line 121 "src/parser.y"
                                        { CMD -> id = CMD_NOEXTRA; }
#line 1589 "obj/parser.c"
    break;

  case 36: /* command: T_NOEXTRA T_PATH  */
#line 122 "src/parser.y"
                                        { CMD -> id = CMD_NOEXTRA; CMD -> path = (yyvsp[0].t).text; }
#line 1595 "obj/parser.c"
    break;

  case 37: /* command: T_HIGHLIGHT  */
#line 123 "src/parser.y"
                                        { CMD -> id = CMD_HIGHLIGHT; }
#line 1601 "obj/parser.c"
    break;

  case 38: /* command: T_HIGHLIGHT T_PATH  */
#line 124 "src/parser.y"
                                        { CMD -> id = CMD_HIGHLIGHT; CMD -> path = (yyvsp[0].t).text; }
#line 1607 "obj/parser.c"
    break;

  case 39: /* command: T_SCREENSHOT  */
#line 125 "src/parser.y"
                                        { CMD -> id = CMD_SCREENSHOT; }
#line 1613 "obj/parser.c"
    break;

  case 40: /* command: T_SCREENSHOT T_PATH  */
#line 126 "src/parser.y"
                                        { CMD -> id = CMD_SCREENSHOT; CMD -> path = (yyvsp[0].t).text; }
#line 1619 "obj/parser.c"
    break;

  case 53: /* command: T_KBD  */
#line 139 "src/parser.y"
                                        { CMD -> id = CMD_KBD; }
#line 1625 "obj/parser.c"
    break;

  case 54: /* command: T_KBD T_PATH  */
#line 140 "src/parser.y"
                                        { CMD -> id = CMD_KBD; CMD -> path = (yyvsp[0].t).text; }
#line 1631 "obj/parser.c"
    break;

  case 55: /* command: T_MOUSE  */
#line 141 "src/parser.y"
                                        { CMD -> id = CMD_MOUSE; }
#line 1637 "obj/parser.c"
    break;

  case 56: /* command: T_MOUSE T_PATH  */
#line 142 "src/parser.y"
                                        { CMD -> id = CMD_MOUSE; CMD -> path = (yyvsp[0].t).text; }
#line 1643 "obj/parser.c"
    break;

  case 57: /* command: T_CONSOLE  */
#line 143 "src/parser.y"
                                        { CMD -> id = CMD_CONSOLE; }
#line 1649 "obj/parser.c"
    break;

  case 58: /* command: T_CONSOLE T_PATH  */
#line 144 "src/parser.y"
                                        { CMD -> id = CMD_CONSOLE; CMD -> path = (yyvsp[0].t).text; }
#line 1655 "obj/parser.c"
    break;

  case 59: /* command: T_MAILBOX  */
#line 145 "src/parser.y"
                                        { CMD -> id = CMD_MAILBOX; }
#line 1661 "obj/parser.c"
    break;

  case 60: /* command: T_MAILBOX T_NUM  */
#line 146 "src/parser.y"
                                        { CMD -> id = CMD_MAILBOX; CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 1667 "obj/parser.c"
    break;

  case 64: /* command: T_BADCMD  */
#line 151 "src/parser.y"
        {
            cmd_seterr ("unknown command '%s' (try 'help')", CMD -> badword);
            YYERROR;
        }
#line 1676 "obj/parser.c"
    break;

  case 65: /* name: T_WORD  */
#line 162 "src/parser.y"
                                        { (yyval.t) = (yyvsp[0].t); }
#line 1682 "obj/parser.c"
    break;

  case 66: /* name: T_REG  */
#line 163 "src/parser.y"
                                        { (yyval.t) = (yyvsp[0].t); }
#line 1688 "obj/parser.c"
    break;

  case 67: /* name: T_IOSYM  */
#line 164 "src/parser.y"
                                        { (yyval.t) = (yyvsp[0].t); }
#line 1694 "obj/parser.c"
    break;

  case 68: /* break_cmd: T_BREAK  */
#line 170 "src/parser.y"
                                        { CMD -> id = CMD_BREAK; }
#line 1700 "obj/parser.c"
    break;

  case 69: /* break_cmd: T_BREAK T_NUM  */
#line 171 "src/parser.y"
                                        { CMD -> id = CMD_BREAK; CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u;
                                          CMD -> num_dec = ((yyvsp[0].t).kind == TOK_DEC); }
#line 1707 "obj/parser.c"
    break;

  case 70: /* break_cmd: T_BREAK name  */
#line 173 "src/parser.y"
                                        { CMD -> id = CMD_BREAK; CMD -> name = (yyvsp[0].t).text; }
#line 1713 "obj/parser.c"
    break;

  case 71: /* break_cmd: T_BREAK T_FILELINE  */
#line 174 "src/parser.y"
                                        { CMD -> id = CMD_BREAK; CMD -> file = (yyvsp[0].t).text; CMD -> line = (yyvsp[0].t).u;
                                          CMD -> has_line = TRUE; }
#line 1720 "obj/parser.c"
    break;

  case 72: /* break_cmd: T_BREAK '-' T_WORD T_FILELINE  */
#line 176 "src/parser.y"
                                        { if (break_all (&(yyvsp[-1].t))) YYERROR;
                                          CMD -> id = CMD_BREAK; CMD -> file = (yyvsp[0].t).text; CMD -> line = (yyvsp[0].t).u;
                                          CMD -> has_line = TRUE; }
#line 1728 "obj/parser.c"
    break;

  case 73: /* break_cmd: T_BREAK '-' T_WORD T_NUM  */
#line 179 "src/parser.y"
                                        { if (break_all (&(yyvsp[-1].t))) YYERROR;
                                          CMD -> id = CMD_BREAK; CMD -> line = (yyvsp[0].t).u; CMD -> has_line = TRUE; }
#line 1735 "obj/parser.c"
    break;

  case 74: /* break_cmd: T_TBREAK T_NUM  */
#line 181 "src/parser.y"
                                        { CMD -> id = CMD_TBREAK; CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u;
                                          CMD -> num_dec = ((yyvsp[0].t).kind == TOK_DEC); }
#line 1742 "obj/parser.c"
    break;

  case 75: /* break_cmd: T_TBREAK name  */
#line 183 "src/parser.y"
                                        { CMD -> id = CMD_TBREAK; CMD -> name = (yyvsp[0].t).text; }
#line 1748 "obj/parser.c"
    break;

  case 76: /* break_cmd: T_TBREAK T_FILELINE  */
#line 184 "src/parser.y"
                                        { CMD -> id = CMD_TBREAK; CMD -> file = (yyvsp[0].t).text; CMD -> line = (yyvsp[0].t).u;
                                          CMD -> has_line = TRUE; }
#line 1755 "obj/parser.c"
    break;

  case 77: /* list_cmd: T_LIST  */
#line 191 "src/parser.y"
                                        { CMD -> id = CMD_LIST; }
#line 1761 "obj/parser.c"
    break;

  case 78: /* list_cmd: T_LIST T_NUM  */
#line 192 "src/parser.y"
                                        { CMD -> id = CMD_LIST; CMD -> line = (yyvsp[0].t).u; CMD -> has_line = TRUE; }
#line 1767 "obj/parser.c"
    break;

  case 79: /* list_cmd: T_LIST T_FILELINE  */
#line 193 "src/parser.y"
                                        { CMD -> id = CMD_LIST; CMD -> file = (yyvsp[0].t).text; CMD -> line = (yyvsp[0].t).u;
                                          CMD -> has_line = TRUE; }
#line 1774 "obj/parser.c"
    break;

  case 80: /* list_cmd: T_LIST name  */
#line 195 "src/parser.y"
                                        { CMD -> id = CMD_LIST; CMD -> name = (yyvsp[0].t).text; }
#line 1780 "obj/parser.c"
    break;

  case 81: /* locals_cmd: T_LOCALS  */
#line 201 "src/parser.y"
                                        { CMD -> id = CMD_LOCALS; }
#line 1786 "obj/parser.c"
    break;

  case 82: /* locals_cmd: T_LOCALS name  */
#line 202 "src/parser.y"
                                        { CMD -> id = CMD_LOCALS; CMD -> name = (yyvsp[0].t).text; }
#line 1792 "obj/parser.c"
    break;

  case 83: /* label_cmd: T_LABEL  */
#line 208 "src/parser.y"
                                        { CMD -> id = CMD_LABEL; }
#line 1798 "obj/parser.c"
    break;

  case 84: /* label_cmd: T_LABEL T_NUM name  */
#line 209 "src/parser.y"
                                        { CMD -> id = CMD_LABEL; CMD -> has_num = TRUE;
                                          CMD -> num = (yyvsp[-1].t).u; CMD -> name = (yyvsp[0].t).text; }
#line 1805 "obj/parser.c"
    break;

  case 86: /* format: T_FMT  */
#line 217 "src/parser.y"
                                        { set_fmt ((yyvsp[0].ch)); if (CMD -> errmsg[0]) YYERROR; }
#line 1811 "obj/parser.c"
    break;

  case 87: /* print_cmd: T_PRINT format operand  */
#line 221 "src/parser.y"
                                        { CMD -> id = CMD_PRINT; }
#line 1817 "obj/parser.c"
    break;

  case 88: /* display_cmd: T_DISPLAY  */
#line 225 "src/parser.y"
                                        { CMD -> id = CMD_DISPLAY; }
#line 1823 "obj/parser.c"
    break;

  case 89: /* display_cmd: T_DISPLAY format operand  */
#line 226 "src/parser.y"
                                        { CMD -> id = CMD_DISPLAY; }
#line 1829 "obj/parser.c"
    break;

  case 90: /* display_cmd: T_DISPLAY format operand name  */
#line 227 "src/parser.y"
                                        { CMD -> id = CMD_DISPLAY; CMD -> name = (yyvsp[0].t).text; }
#line 1835 "obj/parser.c"
    break;

  case 91: /* operand: base  */
#line 231 "src/parser.y"
                                        { CMD -> opd.deref = FALSE; }
#line 1841 "obj/parser.c"
    break;

  case 92: /* operand: '[' base ']'  */
#line 232 "src/parser.y"
                                        { CMD -> opd.deref = TRUE;  }
#line 1847 "obj/parser.c"
    break;

  case 93: /* operand: symref  */
#line 233 "src/parser.y"
                                        { set_operand_sym (&(yyvsp[0].s), FALSE); }
#line 1853 "obj/parser.c"
    break;

  case 94: /* operand: '[' symref ']'  */
#line 234 "src/parser.y"
                                        { set_operand_sym (&(yyvsp[-1].s), FALSE); }
#line 1859 "obj/parser.c"
    break;

  case 95: /* operand: '[' '[' symref ']' ']'  */
#line 235 "src/parser.y"
                                        { set_operand_sym (&(yyvsp[-2].s), TRUE);  }
#line 1865 "obj/parser.c"
    break;

  case 96: /* operand: '&' symref  */
#line 236 "src/parser.y"
                                        { set_operand (OPD_VALUE, 0, (yyvsp[0].s).addr, 0);
                                          CMD -> opd.name = (yyvsp[0].s).text; }
#line 1872 "obj/parser.c"
    break;

  case 97: /* symref: T_WORD  */
#line 247 "src/parser.y"
                                        { if (sym_resolve (&(yyvsp[0].t), 0, NULL, &(yyval.s))) YYERROR; }
#line 1878 "obj/parser.c"
    break;

  case 98: /* symref: T_WORD '+' T_NUM  */
#line 248 "src/parser.y"
                                        { if (sym_resolve (&(yyvsp[-2].t), +1, &(yyvsp[0].t), &(yyval.s))) YYERROR; }
#line 1884 "obj/parser.c"
    break;

  case 99: /* symref: T_WORD '-' T_NUM  */
#line 249 "src/parser.y"
                                        { if (sym_resolve (&(yyvsp[-2].t), -1, &(yyvsp[0].t), &(yyval.s))) YYERROR; }
#line 1890 "obj/parser.c"
    break;

  case 100: /* base: T_REG  */
#line 253 "src/parser.y"
                                        { set_operand (OPD_REG, (uint8_t) (yyvsp[0].t).u, 0, 0); }
#line 1896 "obj/parser.c"
    break;

  case 101: /* base: T_REGS  */
#line 254 "src/parser.y"
                                        { set_operand (OPD_REGS, 0, 0, 0); }
#line 1902 "obj/parser.c"
    break;

  case 102: /* base: T_IOSYM  */
#line 255 "src/parser.y"
                                        { if (operand_iosym (&(yyvsp[0].t))) YYERROR; }
#line 1908 "obj/parser.c"
    break;

  case 103: /* base: T_NUM  */
#line 256 "src/parser.y"
                                        { uint32_t v = 0;
                                          if (operand_number (&(yyvsp[0].t), &v)) YYERROR;
                                          if ((yyvsp[0].t).digits <= 3)
                                              set_operand (OPD_IOPORT, 0, v, 0);
                                          else
                                              set_operand (OPD_MEM,    0, v, 0); }
#line 1919 "obj/parser.c"
    break;

  case 104: /* base: T_NUM '-' T_NUM  */
#line 262 "src/parser.y"
                                        { uint32_t lo = 0, hi = 0;
                                          if (operand_number (&(yyvsp[-2].t), &lo)) YYERROR;
                                          if (operand_number (&(yyvsp[0].t), &hi)) YYERROR;
                                          set_operand (OPD_RANGE, 0, lo, hi); }
#line 1928 "obj/parser.c"
    break;

  case 105: /* set_cmd: T_SET  */
#line 271 "src/parser.y"
                                        { CMD -> id = CMD_SET; }
#line 1934 "obj/parser.c"
    break;

  case 106: /* set_cmd: T_SET set_lhs '=' set_rhs  */
#line 272 "src/parser.y"
                                        { CMD -> id = CMD_SET; }
#line 1940 "obj/parser.c"
    break;

  case 107: /* set_lhs: T_REG  */
#line 276 "src/parser.y"
                                        { CMD -> set_kind = SETK_REG; CMD -> set_reg = (uint8_t) (yyvsp[0].t).u; }
#line 1946 "obj/parser.c"
    break;

  case 108: /* set_lhs: T_IOSYM  */
#line 277 "src/parser.y"
                                        { int16_t p = ioports_num ((uint8_t *) (yyvsp[0].t).text);
                                          if (p < 0) { cmd_seterr ("unknown IOPort '%s'", (yyvsp[0].t).text); YYERROR; }
                                          CMD -> set_kind = SETK_IOPORT; CMD -> set_addr = (uint32_t) p; }
#line 1954 "obj/parser.c"
    break;

  case 109: /* set_lhs: T_NUM  */
#line 280 "src/parser.y"
                                        { uint32_t v = 0;
                                          if (operand_number (&(yyvsp[0].t), &v)) YYERROR;
                                          CMD -> set_kind = ((yyvsp[0].t).digits <= 3) ? SETK_IOPORT : SETK_MEM;
                                          CMD -> set_addr = v; }
#line 1963 "obj/parser.c"
    break;

  case 110: /* set_lhs: T_WORD  */
#line 284 "src/parser.y"
                                        { if (set_lhs_word (&(yyvsp[0].t))) YYERROR; }
#line 1969 "obj/parser.c"
    break;

  case 111: /* set_rhs: T_NUM  */
#line 288 "src/parser.y"
                                        { CMD -> set_isnum = TRUE; CMD -> set_val = (yyvsp[0].t); }
#line 1975 "obj/parser.c"
    break;

  case 112: /* set_rhs: '-' T_NUM  */
#line 289 "src/parser.y"
                                        { CMD -> set_isnum = TRUE; CMD -> set_val = (yyvsp[0].t); CMD -> set_neg = TRUE; }
#line 1981 "obj/parser.c"
    break;

  case 113: /* set_rhs: '+' T_NUM  */
#line 290 "src/parser.y"
                                        { CMD -> set_isnum = TRUE; CMD -> set_val = (yyvsp[0].t); }
#line 1987 "obj/parser.c"
    break;

  case 114: /* set_rhs: T_WORD  */
#line 291 "src/parser.y"
                                        { CMD -> word = (yyvsp[0].t).text; }
#line 1993 "obj/parser.c"
    break;

  case 115: /* replace_cmd: T_REPLACE repl_list  */
#line 297 "src/parser.y"
                                        { CMD -> id = CMD_REPLACE; }
#line 1999 "obj/parser.c"
    break;

  case 118: /* repl_item: T_REG ':' T_NUM  */
#line 307 "src/parser.y"
        {
            if (((yyvsp[-2].t).u != IP) && ((yyvsp[-2].t).u != IR) && ((yyvsp[-2].t).u != IV))
            {
                cmd_seterr ("replace: only IP, IR and IV can be replaced (got '%s')", (yyvsp[-2].t).text);
                YYERROR;
            }
            if (CMD -> rep_count >= CMD_MAX_REPLACE)
            {
                cmd_seterr ("replace: at most %d items (IP, IR, IV)", CMD_MAX_REPLACE);
                YYERROR;
            }
            CMD -> rep[CMD -> rep_count].which = (uint8_t) (yyvsp[-2].t).u;
            CMD -> rep[CMD -> rep_count].value = (yyvsp[0].t).u;
            CMD -> rep_count = CMD -> rep_count + 1;
        }
#line 2019 "obj/parser.c"
    break;

  case 119: /* watch_cmd: T_WATCH  */
#line 327 "src/parser.y"
                                        { CMD -> id = CMD_WATCHLIST; }
#line 2025 "obj/parser.c"
    break;

  case 120: /* watch_cmd: T_WATCH format watch_target relop watch_rhs  */
#line 329 "src/parser.y"
        {
            CMD -> id = CMD_WATCH; CMD -> watch_op = (uint8_t) (yyvsp[-1].op);
            if (check_signed ()) YYERROR;
        }
#line 2034 "obj/parser.c"
    break;

  case 121: /* watch_cmd: T_WATCH format watch_target relop watch_rhs name  */
#line 334 "src/parser.y"
        {
            CMD -> id = CMD_WATCH; CMD -> watch_op = (uint8_t) (yyvsp[-2].op);
            CMD -> name = (yyvsp[0].t).text;
            if (check_signed ()) YYERROR;
        }
#line 2044 "obj/parser.c"
    break;

  case 122: /* watch_target: T_REG  */
#line 343 "src/parser.y"
                                        { CMD -> watch_target.kind = WLOC_REG;    CMD -> watch_target.id = (yyvsp[0].t).u; }
#line 2050 "obj/parser.c"
    break;

  case 123: /* watch_target: T_IOSYM  */
#line 344 "src/parser.y"
                                        { if (loc_iosym (&(yyvsp[0].t), &CMD -> watch_target)) YYERROR; }
#line 2056 "obj/parser.c"
    break;

  case 124: /* watch_target: T_NUM  */
#line 345 "src/parser.y"
                                        { if (loc_number (&(yyvsp[0].t), &CMD -> watch_target)) YYERROR; }
#line 2062 "obj/parser.c"
    break;

  case 125: /* watch_target: symref  */
#line 346 "src/parser.y"
                                        { CMD -> watch_target.kind = WLOC_MEM;    CMD -> watch_target.id = (yyvsp[0].s).addr; }
#line 2068 "obj/parser.c"
    break;

  case 126: /* watch_target: '[' symref ']'  */
#line 347 "src/parser.y"
                                        { CMD -> watch_target.kind = WLOC_MEM;    CMD -> watch_target.id = (yyvsp[-1].s).addr; }
#line 2074 "obj/parser.c"
    break;

  case 127: /* watch_rhs: T_NUM  */
#line 353 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_IMM; CMD -> watch_rhs.id = (yyvsp[0].t).u; }
#line 2080 "obj/parser.c"
    break;

  case 128: /* watch_rhs: '-' T_NUM  */
#line 354 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_IMM; CMD -> watch_rhs.id = (uint32_t) (0 - (yyvsp[0].t).u); }
#line 2086 "obj/parser.c"
    break;

  case 129: /* watch_rhs: '+' T_NUM  */
#line 355 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_IMM; CMD -> watch_rhs.id = (yyvsp[0].t).u; }
#line 2092 "obj/parser.c"
    break;

  case 130: /* watch_rhs: T_REG  */
#line 356 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_REG; CMD -> watch_rhs.id = (yyvsp[0].t).u; }
#line 2098 "obj/parser.c"
    break;

  case 131: /* watch_rhs: T_IOSYM  */
#line 357 "src/parser.y"
                                        { if (loc_iosym (&(yyvsp[0].t), &CMD -> watch_rhs)) YYERROR; }
#line 2104 "obj/parser.c"
    break;

  case 132: /* watch_rhs: '[' T_NUM ']'  */
#line 358 "src/parser.y"
                                        { if (loc_number (&(yyvsp[-1].t), &CMD -> watch_rhs)) YYERROR; }
#line 2110 "obj/parser.c"
    break;

  case 133: /* watch_rhs: '[' T_IOSYM ']'  */
#line 359 "src/parser.y"
                                        { if (loc_iosym (&(yyvsp[-1].t), &CMD -> watch_rhs)) YYERROR; }
#line 2116 "obj/parser.c"
    break;

  case 134: /* watch_rhs: '[' T_REG ']'  */
#line 360 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_REG; CMD -> watch_rhs.id = (yyvsp[-1].t).u; }
#line 2122 "obj/parser.c"
    break;

  case 135: /* watch_rhs: '[' symref ']'  */
#line 361 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_MEM; CMD -> watch_rhs.id = (yyvsp[-1].s).addr; }
#line 2128 "obj/parser.c"
    break;

  case 136: /* watch_rhs: symref  */
#line 362 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_IMM; CMD -> watch_rhs.id = (yyvsp[0].s).addr; }
#line 2134 "obj/parser.c"
    break;

  case 137: /* watch_rhs: '&' symref  */
#line 363 "src/parser.y"
                                        { CMD -> watch_rhs.kind = WLOC_IMM; CMD -> watch_rhs.id = (yyvsp[0].s).addr; }
#line 2140 "obj/parser.c"
    break;

  case 138: /* relop: '='  */
#line 367 "src/parser.y"
                                        { (yyval.op) = WATCH_OP_EQUAL;         }
#line 2146 "obj/parser.c"
    break;

  case 139: /* relop: T_EQEQ  */
#line 368 "src/parser.y"
                                        { (yyval.op) = WATCH_OP_EQUAL;         }
#line 2152 "obj/parser.c"
    break;

  case 140: /* relop: T_NE  */
#line 369 "src/parser.y"
                                        { (yyval.op) = WATCH_OP_NOT_EQUAL;     }
#line 2158 "obj/parser.c"
    break;

  case 141: /* relop: '<'  */
#line 370 "src/parser.y"
                                        { (yyval.op) = WATCH_OP_LESS;          }
#line 2164 "obj/parser.c"
    break;

  case 142: /* relop: '>'  */
#line 371 "src/parser.y"
                                        { (yyval.op) = WATCH_OP_GREATER;       }
#line 2170 "obj/parser.c"
    break;

  case 143: /* relop: T_LE  */
#line 372 "src/parser.y"
                                        { (yyval.op) = WATCH_OP_LESS_EQUAL;    }
#line 2176 "obj/parser.c"
    break;

  case 144: /* relop: T_GE  */
#line 373 "src/parser.y"
                                        { (yyval.op) = WATCH_OP_GREATER_EQUAL; }
#line 2182 "obj/parser.c"
    break;

  case 145: /* run_cmd: T_RUN T_NUM  */
#line 379 "src/parser.y"
                                        { CMD -> id = CMD_RUN; CMD -> has_count = TRUE; CMD -> count = (yyvsp[0].t).u; }
#line 2188 "obj/parser.c"
    break;

  case 146: /* run_cmd: T_RUN  */
#line 381 "src/parser.y"
        {
            cmd_seterr ("run: the number of frames is required (run N)");
            YYERROR;
        }
#line 2197 "obj/parser.c"
    break;

  case 147: /* until_cmd: T_UNTIL  */
#line 390 "src/parser.y"
                                        { CMD -> id = CMD_UNTILSRC; }
#line 2203 "obj/parser.c"
    break;

  case 148: /* until_cmd: T_UNTIL T_NUM  */
#line 391 "src/parser.y"
                                        { CMD -> id = CMD_UNTILSRC; CMD -> line = (yyvsp[0].t).u; CMD -> has_line = TRUE; }
#line 2209 "obj/parser.c"
    break;

  case 149: /* until_cmd: T_UNTIL T_FILELINE  */
#line 392 "src/parser.y"
                                        { CMD -> id = CMD_UNTILSRC; CMD -> file = (yyvsp[0].t).text; CMD -> line = (yyvsp[0].t).u;
                                          CMD -> has_line = TRUE; }
#line 2216 "obj/parser.c"
    break;

  case 150: /* until_cmd: T_UNTIL name until_opts  */
#line 394 "src/parser.y"
                                        { CMD -> id = CMD_UNTIL; CMD -> text = (yyvsp[-1].t).text;
                                          if ((yyvsp[-1].t).text[0] == '\0') { cmd_seterr ("until: the text is empty"); YYERROR; } }
#line 2223 "obj/parser.c"
    break;

  case 152: /* until_opts: until_opts T_WORD T_NUM  */
#line 400 "src/parser.y"
                                        { if (add_option (&(yyvsp[-1].t), &(yyvsp[0].t))) YYERROR; }
#line 2229 "obj/parser.c"
    break;

  case 153: /* expect_cmd: T_EXPECT T_WORD T_WORD expect_opts  */
#line 407 "src/parser.y"
        {
            if (console_form (&(yyvsp[-2].t), &(yyvsp[-1].t))) YYERROR;
        }
#line 2237 "obj/parser.c"
    break;

  case 154: /* expect_cmd: T_EXPECT T_WORD watch_target relop watch_rhs  */
#line 411 "src/parser.y"
        {
            if (print_form (&(yyvsp[-3].t))) YYERROR;
            CMD -> id = CMD_EXPECT; CMD -> expect_kind = EXPECT_VALUE; CMD -> watch_op = (uint8_t) (yyvsp[-1].op);
        }
#line 2246 "obj/parser.c"
    break;

  case 155: /* expect_cmd: T_EXPECT T_WORD T_FMT watch_target relop watch_rhs  */
#line 416 "src/parser.y"
        {
            if (print_form (&(yyvsp[-4].t))) YYERROR;
            set_fmt ((yyvsp[-3].ch)); if (CMD -> errmsg[0]) YYERROR;
            CMD -> id = CMD_EXPECT; CMD -> expect_kind = EXPECT_VALUE; CMD -> watch_op = (uint8_t) (yyvsp[-1].op);
            if (check_signed ()) YYERROR;
        }
#line 2257 "obj/parser.c"
    break;

  case 156: /* expect_cmd: T_EXPECT watch_target relop watch_rhs  */
#line 423 "src/parser.y"
        {
            CMD -> id = CMD_EXPECT; CMD -> expect_kind = EXPECT_VALUE; CMD -> watch_op = (uint8_t) (yyvsp[-1].op);
        }
#line 2265 "obj/parser.c"
    break;

  case 157: /* expect_cmd: T_EXPECT T_FMT watch_target relop watch_rhs  */
#line 427 "src/parser.y"
        {
            set_fmt ((yyvsp[-3].ch)); if (CMD -> errmsg[0]) YYERROR;
            CMD -> id = CMD_EXPECT; CMD -> expect_kind = EXPECT_VALUE; CMD -> watch_op = (uint8_t) (yyvsp[-1].op);
            if (check_signed ()) YYERROR;
        }
#line 2275 "obj/parser.c"
    break;

  case 159: /* expect_opts: expect_opts T_WORD T_NUM  */
#line 436 "src/parser.y"
                                        { if (add_option (&(yyvsp[-1].t), &(yyvsp[0].t))) YYERROR; }
#line 2281 "obj/parser.c"
    break;

  case 160: /* examine_cmd: T_EXAMINE format examine_loc  */
#line 442 "src/parser.y"
                                        { CMD -> id = CMD_EXAMINE; }
#line 2287 "obj/parser.c"
    break;

  case 161: /* examine_cmd: T_EXAMINE format examine_loc T_NUM  */
#line 444 "src/parser.y"
        {
            CMD -> id = CMD_EXAMINE; CMD -> has_count = TRUE; CMD -> count = (yyvsp[0].t).u;
        }
#line 2295 "obj/parser.c"
    break;

  case 162: /* examine_loc: symref  */
#line 450 "src/parser.y"
                                        { set_operand (OPD_MEM, 0, (yyvsp[0].s).addr, 0); CMD -> opd.name = (yyvsp[0].s).text; CMD -> x_words = (yyvsp[0].s).words; }
#line 2301 "obj/parser.c"
    break;

  case 163: /* examine_loc: '[' symref ']'  */
#line 451 "src/parser.y"
                                        { set_operand (OPD_MEM, 0, (yyvsp[-1].s).addr, 0); CMD -> opd.name = (yyvsp[-1].s).text; CMD -> x_words = (yyvsp[-1].s).words; }
#line 2307 "obj/parser.c"
    break;

  case 164: /* examine_loc: T_NUM  */
#line 452 "src/parser.y"
                                        { uint32_t v = 0;
                                          if (operand_number (&(yyvsp[0].t), &v)) YYERROR;
                                          set_operand (OPD_MEM, 0, v, 0); CMD -> x_words = 1; }
#line 2315 "obj/parser.c"
    break;

  case 165: /* gamepad_cmd: T_GAMEPAD  */
#line 460 "src/parser.y"
                                        { CMD -> id = CMD_GAMEPAD; }
#line 2321 "obj/parser.c"
    break;

  case 166: /* gamepad_cmd: T_GAMEPAD T_NUM  */
#line 461 "src/parser.y"
                                        { CMD -> id = CMD_GAMEPAD; CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 2327 "obj/parser.c"
    break;

  case 167: /* gamepad_cmd: T_GAMEPAD gp_word  */
#line 462 "src/parser.y"
                                        { CMD -> id = CMD_GAMEPAD; }
#line 2333 "obj/parser.c"
    break;

  case 168: /* gamepad_cmd: T_GAMEPAD T_NUM gp_word  */
#line 463 "src/parser.y"
                                        { CMD -> id = CMD_GAMEPAD; CMD -> has_num = TRUE; CMD -> num = (yyvsp[-1].t).u; }
#line 2339 "obj/parser.c"
    break;

  case 169: /* gp_word: name  */
#line 467 "src/parser.y"
                                        { CMD -> word = (yyvsp[0].t).text; }
#line 2345 "obj/parser.c"
    break;

  case 170: /* help_cmd: T_HELP  */
#line 473 "src/parser.y"
                                        { CMD -> id = CMD_HELP; }
#line 2351 "obj/parser.c"
    break;

  case 171: /* help_cmd: T_HELP name  */
#line 474 "src/parser.y"
                                        { CMD -> id = CMD_HELP; CMD -> name = (yyvsp[0].t).text; }
#line 2357 "obj/parser.c"
    break;

  case 172: /* load_cmd: T_LOAD T_WORD ':' T_PATH  */
#line 480 "src/parser.y"
                                        { CMD -> id = CMD_LOAD;   CMD -> name = (yyvsp[-2].t).text; CMD -> path = (yyvsp[0].t).text; }
#line 2363 "obj/parser.c"
    break;

  case 173: /* load_cmd: T_UNLOAD T_WORD  */
#line 481 "src/parser.y"
                                        { CMD -> id = CMD_UNLOAD; CMD -> name = (yyvsp[0].t).text; }
#line 2369 "obj/parser.c"
    break;

  case 174: /* undo_cmd: T_UNBREAK T_NUM  */
#line 487 "src/parser.y"
                                        { CMD -> id = CMD_UNBREAK;   CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 2375 "obj/parser.c"
    break;

  case 175: /* undo_cmd: T_UNDISPLAY T_NUM  */
#line 488 "src/parser.y"
                                        { CMD -> id = CMD_UNDISPLAY; CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 2381 "obj/parser.c"
    break;

  case 176: /* undo_cmd: T_UNLABEL T_NUM  */
#line 489 "src/parser.y"
                                        { CMD -> id = CMD_UNLABEL;   CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 2387 "obj/parser.c"
    break;

  case 177: /* undo_cmd: T_UNWATCH T_NUM  */
#line 490 "src/parser.y"
                                        { CMD -> id = CMD_UNWATCH;   CMD -> has_num = TRUE; CMD -> num = (yyvsp[0].t).u; }
#line 2393 "obj/parser.c"
    break;

  case 178: /* undo_cmd: T_UNWATCH name  */
#line 491 "src/parser.y"
                                        { CMD -> id = CMD_UNWATCH;   CMD -> name = (yyvsp[0].t).text; }
#line 2399 "obj/parser.c"
    break;


#line 2403 "obj/parser.c"

      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", YY_CAST (yysymbol_kind_t, yyr1[yyn]), &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
               ? yytable[yyi]
               : yydefgoto[yylhs]);
  }

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE (yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
      yyerror (YY_("syntax error"));
    }

  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0)
    YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYSYMBOL_YYerror;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  YY_ACCESSING_SYMBOL (yystate), yyvsp);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", YY_ACCESSING_SYMBOL (yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;


/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;


/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror (YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;


/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  YY_ACCESSING_SYMBOL (+*yyssp), yyvsp);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif

  return yyresult;
}

#line 494 "src/parser.y"


/* ------------------------------------------------------------- helpers */

static void  set_operand (uint8_t kind, uint8_t reg, uint32_t lo, uint32_t hi)
{
    CMD -> opd.kind  = kind;
    CMD -> opd.reg   = reg;
    CMD -> opd.lo    = lo;
    CMD -> opd.hi    = hi;
}

/*
 * operand_number(): print/display/set operands are written in hex (0x...)
 */
static int  operand_number (tok_t *t, uint32_t *value)
{
    if (t -> kind != TOK_HEX)
    {
        cmd_seterr ("'%s': addresses and IOPort numbers must be hexadecimal (0x...)", t -> text);
        return (1);
    }
    *value = t -> u;
    return (0);
}

static int  operand_iosym (tok_t *t)
{
    int16_t   port   = ioports_num ((uint8_t *) t -> text);
    uint32_t  addr   = 0;
    uint32_t  words  = 0;

    if (port < 0)
    {
        if (sym_lookup (t -> text, &addr, &words))      // a C global that happens to be named like a port group
        {
            set_operand (OPD_MEM, 0, addr, 0);
            CMD -> opd.name  = t -> text;
            return (0);
        }
        cmd_seterr ("unknown IOPort '%s'", t -> text);
        return (1);
    }
    set_operand (OPD_IOPORT, 0, (uint32_t) port, 0);
    return (0);
}

/*
 * loc_number()/loc_iosym(): watchpoint locations. As with print, a hexadecimal number
 * of up to 3 digits is an IOPort and a longer one a memory address.
 */
static int  loc_number (tok_t *t, wloc_t *loc)
{
    if (t -> kind != TOK_HEX)
    {
        cmd_seterr ("'%s': addresses and IOPort numbers must be hexadecimal (0x...)", t -> text);
        return (1);
    }
    loc -> kind  = (t -> digits <= 3) ? WLOC_IOPORT : WLOC_MEM;
    loc -> id    = t -> u;
    return (0);
}

static int  loc_iosym (tok_t *t, wloc_t *loc)
{
    int16_t   port   = ioports_num ((uint8_t *) t -> text);
    uint32_t  addr   = 0;
    uint32_t  words  = 0;

    if (port < 0)
    {
        if (sym_lookup (t -> text, &addr, &words))      // a C global that happens to be named like a port group
        {
            loc -> kind  = WLOC_MEM;
            loc -> id    = addr;
            return (0);
        }
        cmd_seterr ("unknown IOPort '%s'", t -> text);
        return (1);
    }
    loc -> kind  = WLOC_IOPORT;
    loc -> id    = (uint32_t) port;
    return (0);
}

/*
 * set_fmt(): /b /B /d /D /f /o /s /u /x /X
 */
static void  set_fmt (char ch)
{
    switch (ch)
    {
        case 'b':  CMD -> fmt = FORMAT_BINARY;    break;
        case 'B':  CMD -> fmt = FORMAT_BOOLEAN;   break;
        case 'd':  CMD -> fmt = FORMAT_SIGNED;    break;
        case 'D':  CMD -> fmt = FORMAT_DECODE;    break;
        case 'f':
        case 'F':  CMD -> fmt = FORMAT_FLOAT;     break;
        case 'o':
        case 'O':  CMD -> fmt = FORMAT_OCTAL;     break;
        case 's':
        case 'S':  CMD -> fmt = FORMAT_STRING;    break;
        case 'u':
        case 'U':  CMD -> fmt = FORMAT_UNSIGNED;  break;
        case 'x':  CMD -> fmt = FORMAT_LOWERHEX;  break;
        case 'X':  CMD -> fmt = FORMAT_HEX;       break;
        default:
            cmd_seterr ("unknown format '/%c' (valid: b B d D f o s u x X)", ch);
            break;
    }
}

/*
 * set_lhs_word(): the left side of 'set NAME = VALUE' is a simulator setting
 */
static int  break_all (tok_t *t)
{
    if (0 != strcasecmp (t -> text, "a"))
    {
        cmd_seterr ("break: unknown option '-%s' (only -a: every address of the line)", t -> text);
        return (1);
    }
    CMD -> all  = TRUE;
    return (0);
}

static int  set_lhs_word (tok_t *t)
{
    static const struct { const char *name; uint8_t opt; } options[] =
    {
        { "color",    OPT_COLOR    }, { "colors",   OPT_COLOR    },
        { "deref",    OPT_DEREF    }, { "derefaddr", OPT_DEREF   },
        { "errorchk", OPT_ERRORCHK }, { "errorcheck", OPT_ERRORCHK },
        { "error",    OPT_ERRORCHK },
        { "debug",    OPT_DEBUG    },
        { "profile",  OPT_PROFILE  },
        { "verbose",  OPT_VERBOSE  },
        { "source",   OPT_SOURCE   },
        { "mode",     OPT_MODE     },
        { NULL,       OPT_NONE     }
    };
    int  index  = 0;

    for (index = 0; options[index].name != NULL; index = index + 1)
    {
        if (0 == strcasecmp (options[index].name, t -> text))
        {
            CMD -> set_kind = SETK_OPTION;
            CMD -> set_opt  = options[index].opt;
            CMD -> name     = t -> text;
            return (0);
        }
    }

    cmd_seterr ("set: unknown setting '%s' (color deref errorchk debug profile verbose)", t -> text);
    return (1);
}

/*
 * sym_resolve(): a name (with an optional '+'/'-' offset) -> the address it stands for.
 * The offset is a number of words, written in decimal or hexadecimal.
 */
static int  sym_resolve (tok_t *t, int sign, tok_t *offset, symv_t *out)
{
    uint32_t  addr   = 0;
    uint32_t  words  = 0;
    char      text[160];

    if (!locals_lookup (t -> text, &addr, &words) &&     // a local of the selected frame first
        !sym_lookup (t -> text, &addr, &words))
    {
        cmd_seterr ("unknown symbol '%s' (a label of the debug files, or a C global from NAME.asm.sym; "
                    "'label 0xADDR NAME' defines one by hand)", t -> text);
        return (1);
    }
    out -> text   = t -> text;
    out -> words  = words;
    if (offset != NULL)
    {
        if (offset -> kind == TOK_FLT)
        {
            cmd_seterr ("'%s': an offset must be a whole number", offset -> text);
            return (1);
        }
        addr        = (sign > 0) ? (addr + offset -> u) : (addr - offset -> u);
        words       = (words > offset -> u) ? (words - offset -> u) : 1;
        out -> words  = (sign > 0) ? words : 1;
        snprintf (text, sizeof (text), "%s%c%s", t -> text, (sign > 0) ? '+' : '-', offset -> text);
        out -> text = cmd_strdup (text, strlen (text));
    }
    out -> addr = addr;
    return (0);
}

static void  set_operand_sym (symv_t *sym, uint8_t deref)
{
    set_operand (OPD_MEM, 0, sym -> addr, 0);
    CMD -> opd.deref  = deref;
    CMD -> opd.name   = sym -> text;
}

/*
 * expect console "TEXT" -- the first word must say so
 */
static int  console_form (tok_t *word, tok_t *text)
{
    if (0 != strcasecmp (word -> text, "console"))
    {
        cmd_seterr ("expect: use 'expect LOCATION OP VALUE' or 'expect console \"TEXT\"' (got '%s')", word -> text);
        return (1);
    }
    if (text -> text[0] == '\0')
    {
        cmd_seterr ("expect console: the text is empty");
        return (1);
    }
    CMD -> id           = CMD_EXPECT;
    CMD -> expect_kind  = EXPECT_CONSOLE;
    CMD -> text         = text -> text;
    return (0);
}

/*
 * expect print LOCATION OP VALUE -- 'print' is optional noise, any other word is a mistake
 */
static int  print_form (tok_t *word)
{
    if (0 != strcasecmp (word -> text, "print"))
    {
        cmd_seterr ("expect: use 'expect LOCATION OP VALUE' or 'expect console \"TEXT\"' (got '%s')", word -> text);
        return (1);
    }
    return (0);
}

/*
 * until/expect console options: timeout N (frames), rows N (only the last N rows count)
 */
static int  add_option (tok_t *word, tok_t *value)
{
    if (value -> kind == TOK_FLT)
    {
        cmd_seterr ("'%s': a whole number is required", value -> text);
        return (1);
    }
    if (0 == strcasecmp (word -> text, "rows"))
    {
        CMD -> rows     = value -> u;
        return (0);
    }
    if (0 == strcasecmp (word -> text, "timeout"))
    {
        CMD -> timeout  = value -> u;
        return (0);
    }
    cmd_seterr ("unknown option '%s' (timeout N, rows N)", word -> text);
    return (1);
}

/*
 * watch/d and expect/d compare as signed numbers; /u is the default; other formats make no sense
 */
static int  check_signed (void)
{
    switch (CMD -> fmt)
    {
        case FORMAT_DEFAULT:
        case FORMAT_UNSIGNED:
            CMD -> watch_signed = FALSE;
            return (0);

        case FORMAT_SIGNED:
            CMD -> watch_signed = TRUE;
            return (0);
    }
    cmd_seterr ("only /d (signed) and /u (unsigned) can be used when comparing");
    return (1);
}
