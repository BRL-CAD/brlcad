/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton interface for Bison's Yacc-like parsers in C

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
/* Line 1529 of yacc.c.  */
#line 117 "obj_grammar.h"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



