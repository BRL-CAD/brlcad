/*                   P O I N T S _ P A R S E . Y
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
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
/*                   P O I N T S _ P A R S E . Y
 * BRL-CAD
 *
 * Copyright (C) 2004-2005 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this file; see the file named COPYING for more
 * information.
 *
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
%token PLATE ARB SYMMETRY POINTS CYLINDER PIPE

%start file
%%

/* labels */

number:
      INT
    | FLT

eol:
    NL
    | comment_line

point:
    number COMMA number COMMA number

/* lines */

comment_line : COMMENT NL {
    printf("COMMENT IGNORED\n");
}

plate_line: INT COMMA point COMMA PLATE eol {
    point_line_t *plt = &yylval;
    process_point(plt);
    INITIALIZE_POINT_LINE_T(yylval);
}
arb_line: INT COMMA point COMMA ARB eol {
    point_line_t *plt = &yylval;
    process_point(plt);
    INITIALIZE_POINT_LINE_T(yylval);
}
symmetry_line: INT COMMA point COMMA SYMMETRY eol {
    point_line_t *plt = &yylval;
    process_point(plt);
    INITIALIZE_POINT_LINE_T(yylval);
}
points_line: INT COMMA point COMMA POINTS eol {
    point_line_t *plt = &yylval;
    process_point(plt);
    INITIALIZE_POINT_LINE_T(yylval);
}
cylinder_line: INT COMMA point COMMA CYLINDER eol {
    point_line_t *plt = &yylval;
    process_point(plt);
    INITIALIZE_POINT_LINE_T(yylval);
}
pipe_line: INT COMMA point COMMA PIPE eol {
    point_line_t *plt = &yylval;
    process_point(plt);
    INITIALIZE_POINT_LINE_T(yylval);
}


/* blocks */

/*
plate_block: 
      plate_line
    | plate_block plate_line

arb_block: 
      arb_line
    | arb_block arb_line

symmetry_block: 
      symmetry_line
    | symmetry_block symmetry_line

points_block: 
      points_line
    | points_block points_line

cylinder_block: 
      cylinder_line
    | cylinder_block cylinder_line

pipe_block: 
      pipe_line
    | pipe_block pipe_line
*/

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
    | pipe_line
   
file: 
    | file file_top_level_element

    /*
    {
	printf("WARNING: No input given.  There's nothing to do.\n\n");
    }
    */

%%
int yyerror(char *msg)
{
    if (get_column() == 0) {
	printf("\nERROR: Unexpected end of line reached on line %ld, column %ld  (file offset %ld)\n", get_lines(), strlen(previous_linebuffer)+1, get_bytes());
	printf("%s\n%*s\n", previous_linebuffer, (int)strlen(previous_linebuffer)+1, "^");
	fprintf(stderr, "ERROR: Unexpected end of line reached on line %ld, column %ld  (file offset %ld)\n", get_lines(), strlen(previous_linebuffer)+1, get_bytes());
    } else {
	printf("\nERROR: Unexpected input on line %ld, column %ld  (file offset %ld)\n", get_lines()+1, get_column()-1, get_bytes());
	printf("%s\n%*s\n", linebuffer, (int)get_column()-1, "^");
	fprintf(stderr, "ERROR: Unexpected input on line %ld, column %ld  (file offset %ld)\n", get_lines()+1, get_column()-1, get_bytes());
    }
    /* printf("ERROR:\n%s\n%*s (line %ld, column %ld)\n%s\n", lastline, column, "^", line, column, msg); */
    exit(1);
}

int yywrap()
{
    point_line_t plt;
    INITIALIZE_POINT_LINE_T(plt);
    plt.code = 0;
    process_point(&plt);
    
    return(1);
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
