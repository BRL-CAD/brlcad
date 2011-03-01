/*                  P O I N T S _ P A R S E . Y
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file points_parse.y
 *
 * This parser grammar file is used to parse point data files
 * in the form of three numbers followed by an optional label with
 * one point per line.  This is the format output, for example, by
 * the ArcSecond Vulcan scanner.
 *
 * Author -
 *   Christopher Sean Morrison
 */

%{
/*                  P O I N T S _ P A R S E . Y
 * BRL-CAD
 *
 * Copyright (c) 2004-2011 United States Government as represented by
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
/** @file points_parse.c
 *
 * This parser grammar file is used to parse point data files
 * in the form of three numbers followed by an optional label with
 * one point per line.  This is the format output, for example, by
 * the ArcSecond Vulcan scanner.
 *
 * Author -
 *   Christopher Sean Morrison
 */

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./count.h"
#include "./process.h"

#ifndef YYDEBUG
#  define YYDEBUG 0
#endif
#ifndef YYMAXDEPTH
#  define YYMAXDEPTH 1 /* >0 to quell size_t always true warning */
#endif
#ifndef YYENABLE_NLS
#  define YYENABLE_NLS 0
#endif
#ifndef YYLTYPE_IS_TRIVIAL
#  define YYLTYPE_IS_TRIVIAL 0
#endif


extern FILE *yyin;
extern int yylex();
int yyerror(char *msg);
int yydebug=1;
extern char previous_linebuffer[];
extern char linebuffer[];


%}

/* structure */
%token COMMA NL

/* preserve comments */
%token COMMENT

/* values */
%token INT FLT

/* geometry */
%token PLATE ARB SYMMETRY POINTS CYL CYLINDER PIPE SPHERE

%start file
%%

/* labels */

number:
    INT
  | FLT
    ;

eol:
    NL
  | comment_line
    ;

point: number COMMA number COMMA number
    ;

/* lines */

comment_line : COMMENT NL
    {
	printf("COMMENT IGNORED\n");
    };

plate_line: INT COMMA point COMMA PLATE eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };

arb_line: INT COMMA point COMMA ARB eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };

symmetry_line: INT COMMA point COMMA SYMMETRY eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };

points_line: INT COMMA point COMMA POINTS eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };

cylinder_line: INT COMMA point COMMA CYLINDER eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };

cyl_line: INT COMMA point COMMA CYL eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };

pipe_line: INT COMMA point COMMA PIPE eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };

sphere_line: INT COMMA point COMMA SPHERE eol
    {
	point_line_t *plt = &yylval;
	process_point(plt);
	INITIALIZE_POINT_LINE_T(yylval);
    };


/* top-level document blocks */

file_top_level_element: NL
    {
	/* printf("Skipped empty line\n"); */
    }
  | plate_line
  | arb_line
  | symmetry_line
  | points_line
  | cylinder_line
  | cyl_line
  | pipe_line
  | sphere_line
  ;

file:
  | file file_top_level_element
  ;
  /*
    {
    printf("WARNING: No input given.  There's nothing to do.\n\n");
    }
  */

%%
int
yyerror(char *msg)
{
    if (get_column() == 0) {
	printf("\nERROR: Unexpected end of line reached on line %ld, column %ld  (file offset %ld)\n", get_lines(), (long int)strlen(previous_linebuffer)+1, get_bytes());
	printf("%s\n%*s\n", previous_linebuffer, (int)strlen(previous_linebuffer)+1, "^");
	fprintf(stderr, "ERROR: Unexpected end of line reached on line %ld, column %ld  (file offset %ld)\n", get_lines(), (long int)strlen(previous_linebuffer)+1, get_bytes());
    } else {
	printf("\nERROR: Unexpected input on line %ld, column %ld  (file offset %ld)\n", get_lines()+1, get_column()-1, get_bytes());
	printf("%s\n%*s\n", linebuffer, (int)get_column()-1, "^");
	fprintf(stderr, "ERROR: Unexpected input on line %ld, column %ld  (file offset %ld)\n", get_lines()+1, get_column()-1, get_bytes());
    }

    if (msg) {
	printf("\n%s\n", msg);
    }

    bu_exit(1, NULL);

    return 0;
}

int
yywrap()
{
    point_line_t plt;
    INITIALIZE_POINT_LINE_T(plt);
    plt.code = 0;
    process_point(&plt);

    return 1;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
