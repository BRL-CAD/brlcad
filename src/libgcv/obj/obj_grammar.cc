/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse obj_parser_parse
#define yylex   obj_parser_lex
#define yyerror obj_parser_error
#define yylval  obj_parser_lval
#define yychar  obj_parser_char
#define yydebug obj_parser_debug
#define yynerrs obj_parser_nerrs


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     FLOAT = 258,
     INTEGER = 259,
     V_REFERENCE = 260,
     TV_REFERENCE = 261,
     NV_REFERENCE = 262,
     TNV_REFERENCE = 263,
     ID = 264,
     VERTEX = 265,
     T_VERTEX = 266,
     N_VERTEX = 267,
     POINT = 268,
     LINE = 269,
     FACE = 270,
     GROUP = 271,
     SMOOTH = 272,
     OBJECT = 273,
     USEMTL = 274,
     MTLLIB = 275,
     USEMAP = 276,
     MAPLIB = 277,
     BEVEL = 278,
     C_INTERP = 279,
     D_INTERP = 280,
     LOD = 281,
     SHADOW_OBJ = 282,
     TRACE_OBJ = 283,
     ON = 284,
     OFF = 285
   };
#endif
/* Tokens.  */
#define FLOAT 258
#define INTEGER 259
#define V_REFERENCE 260
#define TV_REFERENCE 261
#define NV_REFERENCE 262
#define TNV_REFERENCE 263
#define ID 264
#define VERTEX 265
#define T_VERTEX 266
#define N_VERTEX 267
#define POINT 268
#define LINE 269
#define FACE 270
#define GROUP 271
#define SMOOTH 272
#define OBJECT 273
#define USEMTL 274
#define MTLLIB 275
#define USEMAP 276
#define MAPLIB 277
#define BEVEL 278
#define C_INTERP 279
#define D_INTERP 280
#define LOD 281
#define SHADOW_OBJ 282
#define TRACE_OBJ 283
#define ON 284
#define OFF 285




/* Copy the first part of user declarations.  */
#line 1 "obj_grammar.yy"

  /* 
  * Copyright (c) 1995-2010 United States Government as represented by
  * the U.S. Army Research Laboratory.
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Lesser General Public License
  * version 2.1 as published by the Free Software Foundation.
  *
  * This library is distributed in the hope that it will be useful, but
  * WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Lesser General Public License for more details.
  *
  * You should have received a copy of the GNU Lesser General Public
  * License along with this file; see the file named COPYING for more
  * information.
  */

  #include "gcv/obj_parser.h"
  #include "obj_parser_state.h"
  #include "obj_grammar.h"
  #include "obj_rules.h"

  #include <sys/types.h>

  #include <stdlib.h>
  #include <stdio.h>
  #include <stddef.h>
  #include <string.h>
  
  #include <vector>
  #include <set>
  #include <string>
  #include <cstddef>
  #include <sys/types.h>

  /**
   *  Use namespaces here to avoid multiple symbol name clashes
   */
  void obj_parser_error(yyscan_t scanner, const char *s);

  namespace arl {
    namespace obj_parser {
      namespace detail {
        static const char vertex_ref_err[] = "Invalid vertex reference";
        static const char texture_ref_err[] = "Invalid texture reference";
        static const char normal_ref_err[] = "Invalid normal reference";
        static const char lod_range_err[] = "LOD value must be between 0 and 100";
        static const char smooth_range_err[] = "negative smoothing group id";
        static const char face_length_err[] = "Faces must contain at least 3 references";
        static const char line_length_err[] = "Lines must contain at least 2 references";
      
        typedef parser_extra::contents_type contents_type;
        typedef parser_extra::parser_state_type parser_state_type;
        //typedef contents_type::group_type group_type;
      
        /**
         *  convenience routines
         */
        inline static parser_state_type & get_state(yyscan_t scanner) {
          return static_cast<parser_extra*>(obj_parser_get_extra(scanner))->parser_state;
        }
        
        inline static parser_extra::parser_type & get_parser(yyscan_t scanner) {
          return *(static_cast<parser_extra*>(obj_parser_get_extra(scanner))->parser);
        }

        inline static contents_type & get_contents(yyscan_t scanner) {
          return *(static_cast<parser_extra*>(obj_parser_get_extra(scanner))->contents);
        }

        inline static parser_extra & get_extra(yyscan_t scanner) {
          return *static_cast<parser_extra*>(obj_parser_get_extra(scanner));
        }
        
        /*
        inline static group_type & current_group(yyscan_t scanner) {
          return detail::get_contents(scanner).group_set
            [detail::get_state(scanner).current_group];
        }

        inline static contents_type::polygonal_attributes_type &
        current_polyattributes(yyscan_t scanner)
        {
          return detail::get_contents(scanner).polyattribute_set
            [detail::get_state(scanner).current_polygonal_attributes];
        }
        */
        
        inline static size_t real_index(int val, std::size_t nvert) {
          return (val<0?nvert-size_t(std::abs(val+1)):size_t(val));
        }
        
        template<typename charT>
        inline static bool index_check(int raw, std::size_t index,
          size_t vertices, const charT *log, yyscan_t scanner)
        {
          if(!index || index > vertices) {
            std::stringstream err;
            err << "index '" << raw << "': " << log;
            std::string str = err.str();
            obj_parser_error(scanner,str.c_str());
            return true;
          }
          return false;
        }
      }
    }
  }

  using namespace arl::obj_parser;
  
  /**
   *  Error reporting function as required by yacc
   */
  void obj_parser_error(yyscan_t scanner, const char *s)
  {
    detail::verbose_output_formatter(detail::get_state(scanner),s);
  }



/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 129 "obj_grammar.yy"
{
  float real;
  int integer;
  int reference[3];
  bool toggle;
  size_t index;
}
/* Line 193 of yacc.c.  */
#line 295 "obj_grammar.cc"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 308 "obj_grammar.cc"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

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
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
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
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  82
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   99

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  32
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  32
/* YYNRULES -- Number of rules.  */
#define YYNRULES  78
/* YYNRULES -- Number of states.  */
#define YYNSTATES  105

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   285

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      31,     2,     2,     2,     2,     2,     2,     2,     2,     2,
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
      25,    26,    27,    28,    29,    30
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint8 yyprhs[] =
{
       0,     0,     3,     5,     8,    11,    15,    17,    19,    21,
      23,    25,    27,    29,    31,    33,    35,    37,    39,    41,
      43,    45,    47,    49,    51,    53,    55,    57,    62,    68,
      71,    75,    80,    85,    87,    89,    92,    95,    97,    99,
     102,   105,   107,   110,   112,   114,   117,   120,   122,   125,
     127,   130,   132,   135,   138,   141,   144,   147,   150,   153,
     156,   159,   162,   165,   168,   171,   174,   177,   180,   183,
     186,   189,   192,   195,   198,   201,   203,   206,   208
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      33,     0,    -1,    31,    -1,    34,    31,    -1,    33,    31,
      -1,    33,    34,    31,    -1,    36,    -1,    37,    -1,    38,
      -1,    46,    -1,    47,    -1,    48,    -1,    49,    -1,    50,
      -1,    51,    -1,    52,    -1,    53,    -1,    54,    -1,    55,
      -1,    56,    -1,    57,    -1,    58,    -1,    59,    -1,    60,
      -1,    61,    -1,     3,    -1,     4,    -1,    10,    35,    35,
      35,    -1,    10,    35,    35,    35,    35,    -1,    11,    35,
      -1,    11,    35,    35,    -1,    11,    35,    35,    35,    -1,
      12,    35,    35,    35,    -1,     4,    -1,     5,    -1,    39,
       4,    -1,    39,     5,    -1,     4,    -1,     5,    -1,    40,
       4,    -1,    40,     5,    -1,     6,    -1,    41,     6,    -1,
       4,    -1,     5,    -1,    42,     4,    -1,    42,     5,    -1,
       6,    -1,    43,     6,    -1,     7,    -1,    44,     7,    -1,
       8,    -1,    45,     8,    -1,    13,    39,    -1,    14,    40,
      -1,    14,    41,    -1,    15,    42,    -1,    15,    43,    -1,
      15,    44,    -1,    15,    45,    -1,    16,    62,    -1,    17,
       4,    -1,    17,    30,    -1,    18,     9,    -1,    19,     9,
      -1,    20,    62,    -1,    21,     9,    -1,    21,    30,    -1,
      22,    62,    -1,    27,     9,    -1,    28,     9,    -1,    23,
      63,    -1,    24,    63,    -1,    25,    63,    -1,    26,     4,
      -1,     9,    -1,    62,     9,    -1,    29,    -1,    30,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   178,   178,   179,   180,   181,   184,   185,   186,   187,
     188,   189,   190,   191,   192,   193,   194,   195,   196,   197,
     198,   199,   200,   201,   202,   205,   206,   210,   215,   223,
     228,   233,   241,   249,   261,   272,   283,   297,   309,   320,
     331,   345,   364,   386,   398,   409,   420,   434,   453,   475,
     494,   516,   538,   563,   579,   598,   619,   637,   655,   673,
     693,   699,   714,   723,   729,   735,   741,   745,   752,   758,
     764,   770,   779,   788,   797,   813,   819,   827,   831
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "FLOAT", "INTEGER", "V_REFERENCE",
  "TV_REFERENCE", "NV_REFERENCE", "TNV_REFERENCE", "ID", "VERTEX",
  "T_VERTEX", "N_VERTEX", "POINT", "LINE", "FACE", "GROUP", "SMOOTH",
  "OBJECT", "USEMTL", "MTLLIB", "USEMAP", "MAPLIB", "BEVEL", "C_INTERP",
  "D_INTERP", "LOD", "SHADOW_OBJ", "TRACE_OBJ", "ON", "OFF", "'\\n'",
  "$accept", "statement_list", "statement", "coord", "vertex", "t_vertex",
  "n_vertex", "p_v_reference_list", "l_v_reference_list",
  "l_tv_reference_list", "f_v_reference_list", "f_tv_reference_list",
  "f_nv_reference_list", "f_tnv_reference_list", "point", "line", "face",
  "group", "smooth", "object", "usemtl", "mtllib", "usemap", "maplib",
  "shadow_obj", "trace_obj", "bevel", "c_interp", "d_interp", "lod",
  "id_list", "toggle", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,    10
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    32,    33,    33,    33,    33,    34,    34,    34,    34,
      34,    34,    34,    34,    34,    34,    34,    34,    34,    34,
      34,    34,    34,    34,    34,    35,    35,    36,    36,    37,
      37,    37,    38,    39,    39,    39,    39,    40,    40,    40,
      40,    41,    41,    42,    42,    42,    42,    43,    43,    44,
      44,    45,    45,    46,    47,    47,    48,    48,    48,    48,
      49,    50,    50,    51,    52,    53,    54,    54,    55,    56,
      57,    58,    59,    60,    61,    62,    62,    63,    63
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     2,     3,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     1,     1,     1,
       1,     1,     1,     1,     1,     1,     1,     4,     5,     2,
       3,     4,     4,     1,     1,     2,     2,     1,     1,     2,
       2,     1,     2,     1,     1,     2,     2,     1,     2,     1,
       2,     1,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     1,     2,     1,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       2,     0,     0,     6,     7,     8,     9,    10,    11,    12,
      13,    14,    15,    16,    17,    18,    19,    20,    21,    22,
      23,    24,    25,    26,     0,    29,     0,    33,    34,    53,
      37,    38,    41,    54,    55,    43,    44,    47,    49,    51,
      56,    57,    58,    59,    75,    60,    61,    62,    63,    64,
      65,    66,    67,    68,    77,    78,    71,    72,    73,    74,
      69,    70,     1,     4,     0,     3,     0,    30,     0,    35,
      36,    39,    40,    42,    45,    46,    48,    50,    52,    76,
       5,    27,    31,    32,    28
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,    21,    22,    44,    23,    24,    25,    49,    53,    54,
      60,    61,    62,    63,    26,    27,    28,    29,    30,    31,
      32,    33,    34,    35,    36,    37,    38,    39,    40,    41,
      65,    76
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -8
static const yytype_int8 yypact[] =
{
      35,    34,    34,    34,    36,    30,     3,    -6,     1,    23,
      67,    -6,    58,    -6,    39,    39,    39,    73,    69,    70,
      -8,     2,     8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,
      -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,
      -8,    -8,    -8,    -8,    34,    34,    34,    -8,    -8,    60,
      -8,    -8,    -8,    66,    74,    -8,    -8,    -8,    -8,    -8,
      68,    75,    76,    79,    -8,    80,    -8,    -8,    -8,    -8,
      80,    -8,    -8,    80,    -8,    -8,    -8,    -8,    -8,    -8,
      -8,    -8,    -8,    -8,    51,    -8,    34,    34,    34,    -8,
      -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,
      -8,    34,    -8,    -8,    -8
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
      -8,    -8,    71,    -2,    -8,    -8,    -8,    -8,    -8,    -8,
      -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,
      -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,    -8,
      -7,    59
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint8 yytable[] =
{
      45,    46,    82,    64,    70,    66,    73,    55,    56,    57,
      58,    59,     1,     2,     3,     4,     5,     6,     7,     8,
       9,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    67,    68,    83,    50,    51,    52,    42,    43,    85,
      47,    48,    86,    87,    88,     1,     2,     3,     4,     5,
       6,     7,     8,     9,    10,    11,    12,    13,    14,    15,
      16,    17,    18,    19,    89,    90,    20,    71,    74,    75,
      91,    92,    94,    95,    77,    78,    69,    79,    80,    81,
      93,    96,   100,    97,   101,   102,   103,    98,    72,    99,
       0,     0,    84,     0,     0,     0,     0,     0,     0,   104
};

static const yytype_int8 yycheck[] =
{
       2,     3,     0,     9,    11,     4,    13,     4,     5,     6,
       7,     8,    10,    11,    12,    13,    14,    15,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    30,     9,    31,     4,     5,     6,     3,     4,    31,
       4,     5,    44,    45,    46,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,     4,     5,    31,     9,    29,    30,
       4,     5,     4,     5,    15,    16,     9,     4,     9,     9,
       6,     6,    31,     7,    86,    87,    88,     8,    30,     9,
      -1,    -1,    21,    -1,    -1,    -1,    -1,    -1,    -1,   101
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    10,    11,    12,    13,    14,    15,    16,    17,    18,
      19,    20,    21,    22,    23,    24,    25,    26,    27,    28,
      31,    33,    34,    36,    37,    38,    46,    47,    48,    49,
      50,    51,    52,    53,    54,    55,    56,    57,    58,    59,
      60,    61,     3,     4,    35,    35,    35,     4,     5,    39,
       4,     5,     6,    40,    41,     4,     5,     6,     7,     8,
      42,    43,    44,    45,     9,    62,     4,    30,     9,     9,
      62,     9,    30,    62,    29,    30,    63,    63,    63,     4,
       9,     9,     0,    31,    34,    31,    35,    35,    35,     4,
       5,     4,     5,     6,     4,     5,     6,     7,     8,     9,
      31,    35,    35,    35,    35
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (scanner, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval, scanner)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, scanner); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t scanner)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    yyscan_t scanner;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (scanner);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, yyscan_t scanner)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, scanner)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    yyscan_t scanner;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, yyscan_t scanner)
#else
static void
yy_reduce_print (yyvsp, yyrule, scanner)
    YYSTYPE *yyvsp;
    int yyrule;
    yyscan_t scanner;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , scanner);
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, scanner); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
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



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, yyscan_t scanner)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, scanner)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    yyscan_t scanner;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (scanner);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (yyscan_t scanner);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */






/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (yyscan_t scanner)
#else
int
yyparse (scanner)
    yyscan_t scanner;
#endif
#endif
{
  /* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;

  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
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
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

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
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 25:
#line 205 "obj_grammar.yy"
    { (yyval.real) = float((yyvsp[(1) - (1)].real)); }
    break;

  case 26:
#line 206 "obj_grammar.yy"
    { (yyval.real) = float((yyvsp[(1) - (1)].integer)); }
    break;

  case 27:
#line 211 "obj_grammar.yy"
    {
      detail::obj_contents::gvertex_t vertex = {{(yyvsp[(2) - (4)].real),(yyvsp[(3) - (4)].real),(yyvsp[(4) - (4)].real),1}};
      detail::get_contents(scanner).gvertices_list.push_back(vertex);
    }
    break;

  case 28:
#line 216 "obj_grammar.yy"
    {
      detail::obj_contents::gvertex_t vertex = {{(yyvsp[(2) - (5)].real),(yyvsp[(3) - (5)].real),(yyvsp[(4) - (5)].real),(yyvsp[(5) - (5)].real)}};
      detail::get_contents(scanner).gvertices_list.push_back(vertex);
    }
    break;

  case 29:
#line 224 "obj_grammar.yy"
    {
      detail::obj_contents::tvertex_t vertex = {{(yyvsp[(2) - (2)].real),0,0}};
      detail::get_contents(scanner).tvertices_list.push_back(vertex);
    }
    break;

  case 30:
#line 229 "obj_grammar.yy"
    {
      detail::obj_contents::tvertex_t vertex = {{(yyvsp[(2) - (3)].real),(yyvsp[(3) - (3)].real),0}};
      detail::get_contents(scanner).tvertices_list.push_back(vertex);
    }
    break;

  case 31:
#line 234 "obj_grammar.yy"
    {
      detail::obj_contents::tvertex_t vertex = {{(yyvsp[(2) - (4)].real),(yyvsp[(3) - (4)].real),(yyvsp[(4) - (4)].real)}};
      detail::get_contents(scanner).tvertices_list.push_back(vertex);
    }
    break;

  case 32:
#line 242 "obj_grammar.yy"
    {
      detail::obj_contents::nvertex_t vertex = {{(yyvsp[(2) - (4)].real),(yyvsp[(3) - (4)].real),(yyvsp[(4) - (4)].real)}};
      detail::get_contents(scanner).nvertices_list.push_back(vertex);
    }
    break;

  case 33:
#line 250 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(1) - (1)].integer),num_gvertices);

      if(detail::index_check((yyvsp[(1) - (1)].integer),gindex,num_gvertices,detail::vertex_ref_err,scanner)) {
        YYERROR;
      }
      
      (yyval.index) = detail::get_contents(scanner).point_v_indexlist.size();
      detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
    }
    break;

  case 34:
#line 262 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(1) - (1)].reference)[0],num_gvertices);

      if(detail::index_check((yyvsp[(1) - (1)].reference)[0],gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;
      
      (yyval.index) = detail::get_contents(scanner).point_v_indexlist.size();
      detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
    }
    break;

  case 35:
#line 273 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(2) - (2)].integer),num_gvertices);

      if(detail::index_check((yyvsp[(2) - (2)].integer),gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;

      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
    }
    break;

  case 36:
#line 284 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(2) - (2)].reference)[0],num_gvertices);

      if(detail::index_check((yyvsp[(2) - (2)].reference)[0],gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;
      
      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).point_v_indexlist.push_back(gindex);
    }
    break;

  case 37:
#line 298 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(1) - (1)].integer),num_gvertices);

      if(detail::index_check((yyvsp[(1) - (1)].integer),gindex,num_gvertices,detail::vertex_ref_err,scanner)) {
        YYERROR;
      }
      
      (yyval.index) = detail::get_contents(scanner).line_v_indexlist.size();
      detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
    }
    break;

  case 38:
#line 310 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(1) - (1)].reference)[0],num_gvertices);

      if(detail::index_check((yyvsp[(1) - (1)].reference)[0],gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;
      
      (yyval.index) = detail::get_contents(scanner).line_v_indexlist.size();
      detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
    }
    break;

  case 39:
#line 321 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(2) - (2)].integer),num_gvertices);

      if(detail::index_check((yyvsp[(2) - (2)].integer),gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;

      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
    }
    break;

  case 40:
#line 332 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(2) - (2)].reference)[0],num_gvertices);

      if(detail::index_check((yyvsp[(2) - (2)].reference)[0],gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;
      
      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).line_v_indexlist.push_back(gindex);
    }
    break;

  case 41:
#line 346 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

      detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index((yyvsp[(1) - (1)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(1) - (1)].reference)[1],num_tvertices)
      }};

      if(detail::index_check((yyvsp[(1) - (1)].reference)[0],tv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(1) - (1)].reference)[1],tv_index.v[1],num_tvertices,detail::texture_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = detail::get_contents(scanner).line_tv_indexlist.size();
      detail::get_contents(scanner).line_tv_indexlist.push_back(tv_index);
    }
    break;

  case 42:
#line 365 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

      detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index((yyvsp[(2) - (2)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(2) - (2)].reference)[1],num_tvertices)
      }};

      if(detail::index_check((yyvsp[(2) - (2)].reference)[0],tv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(2) - (2)].reference)[1],tv_index.v[1],num_tvertices,detail::texture_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).line_tv_indexlist.push_back(tv_index);
    }
    break;

  case 43:
#line 387 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(1) - (1)].integer),num_gvertices);

      if(detail::index_check((yyvsp[(1) - (1)].integer),gindex,num_gvertices,detail::vertex_ref_err,scanner)) {
        YYERROR;
      }
      
      (yyval.index) = detail::get_contents(scanner).pologonal_v_indexlist.size();
      detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
    }
    break;

  case 44:
#line 399 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(1) - (1)].reference)[0],num_gvertices);

      if(detail::index_check((yyvsp[(1) - (1)].reference)[0],gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;
      
      (yyval.index) = detail::get_contents(scanner).pologonal_v_indexlist.size();
      detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
    }
    break;

  case 45:
#line 410 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(2) - (2)].integer),num_gvertices);

      if(detail::index_check((yyvsp[(2) - (2)].integer),gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;

      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
    }
    break;

  case 46:
#line 421 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      detail::contents_type::polygonal_v_index_type gindex = detail::real_index((yyvsp[(2) - (2)].reference)[0],num_gvertices);

      if(detail::index_check((yyvsp[(2) - (2)].reference)[0],gindex,num_gvertices,detail::vertex_ref_err,scanner))
        YYERROR;
      
      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).pologonal_v_indexlist.push_back(gindex);
    }
    break;

  case 47:
#line 435 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

      detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index((yyvsp[(1) - (1)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(1) - (1)].reference)[1],num_tvertices)
      }};

      if(detail::index_check((yyvsp[(1) - (1)].reference)[0],tv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(1) - (1)].reference)[1],tv_index.v[1],num_tvertices,detail::texture_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = detail::get_contents(scanner).pologonal_tv_indexlist.size();
      detail::get_contents(scanner).pologonal_tv_indexlist.push_back(tv_index);
    }
    break;

  case 48:
#line 454 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();

      detail::contents_type::polygonal_tv_index_type tv_index = {{
        detail::real_index((yyvsp[(2) - (2)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(2) - (2)].reference)[1],num_tvertices)
      }};

      if(detail::index_check((yyvsp[(2) - (2)].reference)[0],tv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(2) - (2)].reference)[1],tv_index.v[1],num_tvertices,detail::texture_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).pologonal_tv_indexlist.push_back(tv_index);
    }
    break;

  case 49:
#line 476 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

      detail::contents_type::polygonal_nv_index_type nv_index = {{
        detail::real_index((yyvsp[(1) - (1)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(1) - (1)].reference)[2],num_nvertices)
      }};

      if(detail::index_check((yyvsp[(1) - (1)].reference)[0],nv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(1) - (1)].reference)[2],nv_index.v[1],num_nvertices,detail::normal_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = detail::get_contents(scanner).pologonal_nv_indexlist.size();
      detail::get_contents(scanner).pologonal_nv_indexlist.push_back(nv_index);
    }
    break;

  case 50:
#line 495 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

      detail::contents_type::polygonal_nv_index_type nv_index = {{
        detail::real_index((yyvsp[(2) - (2)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(2) - (2)].reference)[2],num_nvertices)
      }};

      if(detail::index_check((yyvsp[(2) - (2)].reference)[0],nv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(2) - (2)].reference)[2],nv_index.v[1],num_nvertices,detail::normal_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).pologonal_nv_indexlist.push_back(nv_index);
    }
    break;

  case 51:
#line 517 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();
      std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

      detail::contents_type::polygonal_tnv_index_type tnv_index = {{
        detail::real_index((yyvsp[(1) - (1)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(1) - (1)].reference)[1],num_tvertices),
        detail::real_index((yyvsp[(1) - (1)].reference)[2],num_nvertices)
      }};

      if(detail::index_check((yyvsp[(1) - (1)].reference)[0],tnv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(1) - (1)].reference)[1],tnv_index.v[1],num_tvertices,detail::texture_ref_err,scanner) ||
        detail::index_check((yyvsp[(1) - (1)].reference)[2],tnv_index.v[2],num_nvertices,detail::normal_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = detail::get_contents(scanner).pologonal_tnv_indexlist.size();
      detail::get_contents(scanner).pologonal_tnv_indexlist.push_back(tnv_index);
    }
    break;

  case 52:
#line 539 "obj_grammar.yy"
    {
      std::size_t num_gvertices = detail::get_contents(scanner).gvertices_list.size();
      std::size_t num_tvertices = detail::get_contents(scanner).tvertices_list.size();
      std::size_t num_nvertices = detail::get_contents(scanner).nvertices_list.size();

      detail::contents_type::polygonal_tnv_index_type tnv_index = {{
        detail::real_index((yyvsp[(2) - (2)].reference)[0],num_gvertices),
        detail::real_index((yyvsp[(2) - (2)].reference)[1],num_tvertices),
        detail::real_index((yyvsp[(2) - (2)].reference)[2],num_nvertices)
      }};

      if(detail::index_check((yyvsp[(2) - (2)].reference)[0],tnv_index.v[0],num_gvertices,detail::vertex_ref_err,scanner) ||
        detail::index_check((yyvsp[(2) - (2)].reference)[1],tnv_index.v[1],num_tvertices,detail::texture_ref_err,scanner) ||
        detail::index_check((yyvsp[(2) - (2)].reference)[2],tnv_index.v[2],num_nvertices,detail::normal_ref_err,scanner))
      {
        YYERROR;
      }
      
      (yyval.index) = (yyvsp[(1) - (2)].index);
      detail::get_contents(scanner).pologonal_tnv_indexlist.push_back(tnv_index);
    }
    break;

  case 53:
#line 564 "obj_grammar.yy"
    {
      if(detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
      detail::get_contents(scanner).point_v_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

      detail::get_contents(scanner).point_v_loclist.
        resize(detail::get_contents(scanner).point_v_loclist.size()+1);
      detail::get_contents(scanner).point_v_loclist.back().first = (yyvsp[(2) - (2)].index);
      detail::get_contents(scanner).point_v_loclist.back().second =
        detail::get_contents(scanner).point_v_indexlist.size()-(yyvsp[(2) - (2)].index);
    }
    break;

  case 54:
#line 580 "obj_grammar.yy"
    {
      if(detail::get_contents(scanner).line_v_indexlist.size()-(yyvsp[(2) - (2)].index) < 2) {
        obj_parser_error(scanner,detail::line_length_err);
        YYERROR;
      }
      
      if(detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
      detail::get_contents(scanner).line_v_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

      detail::get_contents(scanner).line_v_loclist.
        resize(detail::get_contents(scanner).line_v_loclist.size()+1);
      detail::get_contents(scanner).line_v_loclist.back().first = (yyvsp[(2) - (2)].index);
      detail::get_contents(scanner).line_v_loclist.back().second =
        detail::get_contents(scanner).line_v_indexlist.size()-(yyvsp[(2) - (2)].index);
    }
    break;

  case 55:
#line 599 "obj_grammar.yy"
    {
      if(detail::get_contents(scanner).line_tv_indexlist.size()-(yyvsp[(2) - (2)].index) < 3) {
        obj_parser_error(scanner,detail::line_length_err);
        YYERROR;
      }
      
      if(detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
      detail::get_contents(scanner).line_tv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

      detail::get_contents(scanner).line_tv_loclist.
        resize(detail::get_contents(scanner).line_tv_loclist.size()+1);
      detail::get_contents(scanner).line_tv_loclist.back().first = (yyvsp[(2) - (2)].index);
      detail::get_contents(scanner).line_tv_loclist.back().second =
        detail::get_contents(scanner).line_tv_indexlist.size()-(yyvsp[(2) - (2)].index);
    }
    break;

  case 56:
#line 620 "obj_grammar.yy"
    {
      if(detail::get_contents(scanner).pologonal_v_indexlist.size()-(yyvsp[(2) - (2)].index) < 3) {
        obj_parser_error(scanner,detail::face_length_err);
        YYERROR;
      }
      
      if(detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
      detail::get_contents(scanner).polygonal_v_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

      detail::get_contents(scanner).polygonal_v_loclist.
        resize(detail::get_contents(scanner).polygonal_v_loclist.size()+1);
      detail::get_contents(scanner).polygonal_v_loclist.back().first = (yyvsp[(2) - (2)].index);
      detail::get_contents(scanner).polygonal_v_loclist.back().second =
        detail::get_contents(scanner).pologonal_v_indexlist.size()-(yyvsp[(2) - (2)].index);
    }
    break;

  case 57:
#line 638 "obj_grammar.yy"
    {
      if(detail::get_contents(scanner).pologonal_tv_indexlist.size()-(yyvsp[(2) - (2)].index) < 3) {
        obj_parser_error(scanner,detail::face_length_err);
        YYERROR;
      }
      
      if(detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
      detail::get_contents(scanner).polygonal_tv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

      detail::get_contents(scanner).polygonal_tv_loclist.
        resize(detail::get_contents(scanner).polygonal_tv_loclist.size()+1);
      detail::get_contents(scanner).polygonal_tv_loclist.back().first = (yyvsp[(2) - (2)].index);
      detail::get_contents(scanner).polygonal_tv_loclist.back().second =
        detail::get_contents(scanner).pologonal_tv_indexlist.size()-(yyvsp[(2) - (2)].index);
    }
    break;

  case 58:
#line 656 "obj_grammar.yy"
    {
      if(detail::get_contents(scanner).pologonal_nv_indexlist.size()-(yyvsp[(2) - (2)].index) < 3) {
        obj_parser_error(scanner,detail::face_length_err);
        YYERROR;
      }
      
      if(detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
      detail::get_contents(scanner).polygonal_nv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

      detail::get_contents(scanner).polygonal_nv_loclist.
        resize(detail::get_contents(scanner).polygonal_nv_loclist.size()+1);
      detail::get_contents(scanner).polygonal_nv_loclist.back().first = (yyvsp[(2) - (2)].index);
      detail::get_contents(scanner).polygonal_nv_loclist.back().second =
        detail::get_contents(scanner).pologonal_nv_indexlist.size()-(yyvsp[(2) - (2)].index);
    }
    break;

  case 59:
#line 674 "obj_grammar.yy"
    {
      if(detail::get_contents(scanner).pologonal_tnv_indexlist.size()-(yyvsp[(2) - (2)].index) < 3) {
        obj_parser_error(scanner,detail::face_length_err);
        YYERROR;
      }
      
      if(detail::get_state(scanner).polyattributes_dirty)
        detail::set_working_polygattributes(detail::get_extra(scanner));
      detail::get_contents(scanner).polygonal_tnv_attr_list.
        push_back(detail::get_state(scanner).current_polyattributes);

      detail::get_contents(scanner).polygonal_tnv_loclist.
        resize(detail::get_contents(scanner).polygonal_tnv_loclist.size()+1);
      detail::get_contents(scanner).polygonal_tnv_loclist.back().first = (yyvsp[(2) - (2)].index);
      detail::get_contents(scanner).polygonal_tnv_loclist.back().second =
        detail::get_contents(scanner).pologonal_tnv_indexlist.size()-(yyvsp[(2) - (2)].index);
    }
    break;

  case 60:
#line 694 "obj_grammar.yy"
    {
      detail::set_working_groupset(detail::get_extra(scanner));
    }
    break;

  case 61:
#line 700 "obj_grammar.yy"
    {
      if((yyvsp[(2) - (2)].integer) < 0) {
        obj_parser_error(scanner,detail::smooth_range_err);
        YYERROR;
      }

      if(detail::get_state(scanner).working_polyattributes.smooth_group != 
        static_cast<unsigned int>((yyvsp[(2) - (2)].integer)))
      {
        detail::get_state(scanner).working_polyattributes.smooth_group = 
          static_cast<unsigned int>((yyvsp[(2) - (2)].integer));
        detail::get_state(scanner).polyattributes_dirty = true;
      }
    }
    break;

  case 62:
#line 715 "obj_grammar.yy"
    {
      if(detail::get_state(scanner).working_polyattributes.smooth_group != 0) {
        detail::get_state(scanner).working_polyattributes.smooth_group = 0;
        detail::get_state(scanner).polyattributes_dirty = true;
      }
    }
    break;

  case 63:
#line 724 "obj_grammar.yy"
    {
      detail::set_working_object(detail::get_extra(scanner));
    }
    break;

  case 64:
#line 730 "obj_grammar.yy"
    {
      detail::set_working_material(detail::get_extra(scanner));
    }
    break;

  case 65:
#line 736 "obj_grammar.yy"
    {
      detail::set_working_materiallib(detail::get_extra(scanner));
    }
    break;

  case 66:
#line 742 "obj_grammar.yy"
    {
      detail::set_working_texmap(detail::get_extra(scanner));
    }
    break;

  case 67:
#line 746 "obj_grammar.yy"
    {
      detail::get_state(scanner).working_string.clear();
      detail::set_working_texmap(detail::get_extra(scanner));
    }
    break;

  case 68:
#line 753 "obj_grammar.yy"
    {
      detail::set_working_texmaplib(detail::get_extra(scanner));
    }
    break;

  case 69:
#line 759 "obj_grammar.yy"
    {
      detail::set_working_shadow_obj(detail::get_extra(scanner));
    }
    break;

  case 70:
#line 765 "obj_grammar.yy"
    {
      detail::set_working_trace_obj(detail::get_extra(scanner));
    }
    break;

  case 71:
#line 771 "obj_grammar.yy"
    {
      if(detail::get_state(scanner).working_polyattributes.bevel != (yyvsp[(2) - (2)].toggle)) {
        detail::get_state(scanner).working_polyattributes.bevel = (yyvsp[(2) - (2)].toggle);
        detail::get_state(scanner).polyattributes_dirty = true;
      }
    }
    break;

  case 72:
#line 780 "obj_grammar.yy"
    {
      if(detail::get_state(scanner).working_polyattributes.c_interp != (yyvsp[(2) - (2)].toggle)) {
        detail::get_state(scanner).working_polyattributes.c_interp = (yyvsp[(2) - (2)].toggle);
        detail::get_state(scanner).polyattributes_dirty = true;
      }
    }
    break;

  case 73:
#line 789 "obj_grammar.yy"
    {
      if(detail::get_state(scanner).working_polyattributes.d_interp != (yyvsp[(2) - (2)].toggle)) {
        detail::get_state(scanner).working_polyattributes.d_interp = (yyvsp[(2) - (2)].toggle);
        detail::get_state(scanner).polyattributes_dirty = true;
      }
    }
    break;

  case 74:
#line 798 "obj_grammar.yy"
    {
      if(!((yyvsp[(2) - (2)].integer) >= 0 && (yyvsp[(2) - (2)].integer) <= 100)) {
        obj_parser_error(scanner,detail::lod_range_err);
        YYERROR;
      }
      
      unsigned char tmp = (yyvsp[(2) - (2)].integer);
      
      if(detail::get_state(scanner).working_polyattributes.lod != tmp) {
        detail::get_state(scanner).working_polyattributes.lod = tmp;
        detail::get_state(scanner).polyattributes_dirty = true;
      }
    }
    break;

  case 75:
#line 814 "obj_grammar.yy"
    {
      detail::get_state(scanner).working_stringset.
        insert(detail::get_state(scanner).working_string);
      detail::get_state(scanner).working_string.clear();
    }
    break;

  case 76:
#line 820 "obj_grammar.yy"
    {
      detail::get_state(scanner).working_stringset.
        insert(detail::get_state(scanner).working_string);
      detail::get_state(scanner).working_string.clear();
    }
    break;

  case 77:
#line 828 "obj_grammar.yy"
    {
      (yyval.toggle) = true;
    }
    break;

  case 78:
#line 832 "obj_grammar.yy"
    {
      (yyval.toggle) = false;
    }
    break;


/* Line 1267 of yacc.c.  */
#line 2335 "obj_grammar.cc"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (scanner, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (scanner, yymsg);
	  }
	else
	  {
	    yyerror (scanner, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
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
		      yytoken, &yylval, scanner);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
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
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
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
		  yystos[yystate], yyvsp, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, scanner);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}


#line 837 "obj_grammar.yy"




