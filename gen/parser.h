/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison interface for Yacc-like parsers in C

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_YY_OBJ_PARSER_H_INCLUDED
# define YY_YY_OBJ_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token kinds.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    YYEMPTY = -2,
    YYEOF = 0,                     /* "end of file"  */
    YYerror = 256,                 /* error  */
    YYUNDEF = 257,                 /* "invalid token"  */
    T_NUM = 258,                   /* T_NUM  */
    T_REG = 259,                   /* T_REG  */
    T_IOSYM = 260,                 /* T_IOSYM  */
    T_WORD = 261,                  /* T_WORD  */
    T_PATH = 262,                  /* T_PATH  */
    T_FILELINE = 263,              /* T_FILELINE  */
    T_FMT = 264,                   /* T_FMT  */
    T_REGS = 265,                  /* T_REGS  */
    T_EQEQ = 266,                  /* T_EQEQ  */
    T_NE = 267,                    /* T_NE  */
    T_LE = 268,                    /* T_LE  */
    T_GE = 269,                    /* T_GE  */
    T_BACKTRACE = 270,             /* T_BACKTRACE  */
    T_BREAK = 271,                 /* T_BREAK  */
    T_CONTINUE = 272,              /* T_CONTINUE  */
    T_DISPLAY = 273,               /* T_DISPLAY  */
    T_GAMEPAD = 274,               /* T_GAMEPAD  */
    T_HELP = 275,                  /* T_HELP  */
    T_IGNORE = 276,                /* T_IGNORE  */
    T_INVENTORY = 277,             /* T_INVENTORY  */
    T_LABEL = 278,                 /* T_LABEL  */
    T_LOAD = 279,                  /* T_LOAD  */
    T_NEXT = 280,                  /* T_NEXT  */
    T_PRINT = 281,                 /* T_PRINT  */
    T_PROFILE = 282,               /* T_PROFILE  */
    T_QUIT = 283,                  /* T_QUIT  */
    T_REPLACE = 284,               /* T_REPLACE  */
    T_ASM = 285,                   /* T_ASM  */
    T_RESTORE = 286,               /* T_RESTORE  */
    T_SAVE = 287,                  /* T_SAVE  */
    T_SCREENSHOT = 288,            /* T_SCREENSHOT  */
    T_SET = 289,                   /* T_SET  */
    T_STEP = 290,                  /* T_STEP  */
    T_UNBREAK = 291,               /* T_UNBREAK  */
    T_UNDISPLAY = 292,             /* T_UNDISPLAY  */
    T_UNLABEL = 293,               /* T_UNLABEL  */
    T_UNLOAD = 294,                /* T_UNLOAD  */
    T_UNWATCH = 295,               /* T_UNWATCH  */
    T_VERSION = 296,               /* T_VERSION  */
    T_WATCH = 297,                 /* T_WATCH  */
    T_WATCHLIST = 298,             /* T_WATCHLIST  */
    T_KBD = 299,                   /* T_KBD  */
    T_MAILBOX = 300,               /* T_MAILBOX  */
    T_CONSOLE = 301,               /* T_CONSOLE  */
    T_RUN = 302,                   /* T_RUN  */
    T_UNTIL = 303,                 /* T_UNTIL  */
    T_EXPECT = 304,                /* T_EXPECT  */
    T_EXAMINE = 305,               /* T_EXAMINE  */
    T_SSTEP = 306,                 /* T_SSTEP  */
    T_SNEXT = 307,                 /* T_SNEXT  */
    T_FINISH = 308,                /* T_FINISH  */
    T_TBREAK = 309,                /* T_TBREAK  */
    T_LIST = 310,                  /* T_LIST  */
    T_FRAME = 311,                 /* T_FRAME  */
    T_UP = 312,                    /* T_UP  */
    T_DOWN = 313,                  /* T_DOWN  */
    T_LOCALS = 314,                /* T_LOCALS  */
    T_MARK = 315,                  /* T_MARK  */
    T_BADCMD = 316,                /* T_BADCMD  */
    T_BAD = 317                    /* T_BAD  */
  };
  typedef enum yytokentype yytoken_kind_t;
#endif

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
union YYSTYPE
{
#line 54 "src/parser.y"

    tok_t   t;
    char    ch;
    int     op;
    symv_t  s;

#line 133 "obj/parser.h"

};
typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif


extern YYSTYPE yylval;


int yyparse (void);


#endif /* !YY_YY_OBJ_PARSER_H_INCLUDED  */
