/*                   P O I N T S _ S C A N . L
 * BRL-CAD
 *
 * Copyright (c) 2005-2011 United States Government as represented by
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
/** @file points_scan.l
 *
 * Scan tokens out of a comma-separated value file
 *
 */
%{
/*                   P O I N T S _ S C A N . L
 * BRL-CAD
 *
 * Copyright (c) 2005-2011 United States Government as represented by
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
/** @file points_scan.c
 *
 * This lexer grammar is to tokenize comma-separated value point
 * files.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#include "vmath.h"
#include "bu.h"

/* increase token limits of at&t and mk2 lex */
#undef YYLMAX
#define YYLMAX 4096

/* increase token limits of pclex (2x) */
#undef F_BUFSIZE
#define F_BUFSIZE 2048

#include "./count.h"
#include "./process.h"

extern int yyerror(char *msg);
extern YYSTYPE yylval;

/* used to set the initial state */
static int first_token = 1;
char previous_linebuffer[YYLMAX+1] = {0};
char linebuffer[YYLMAX+1] = {0};

/* flex is being stupid.  need to check fwrite() return value to quell warning. */
#ifndef ECHO
#  define ECHO  if (fwrite( yytext, yyleng, 1, yyout ) != 1) perror("fwrite")
#endif


%}

%s LINE

LETTER		[a-zA-Z_]
PUNCT		[<>:|]
INTDIGIT	[0-9]
FLOATTYPE	(f|F|l|L)
DIGITTYPE	(u|U|l|L)*
SPACE		[ \t\v\f]+
EOL		[\n\r]+
NONPRINT	[^[:print:]]

EXPONENT	[Ee][+-]?{INTDIGIT}+

FLOATA	([+-])?{INTDIGIT}*"."{INTDIGIT}+({EXPONENT})?{FLOATTYPE}?
FLOATB	([+-])?{INTDIGIT}+"."{INTDIGIT}*({EXPONENT})?{FLOATTYPE}?

INT	\-?[0-9]+{INTDIGIT}*{DIGITTYPE}?

FLOAT	({FLOATA}|{FLOATB})

COMMENT	#([^\n])*

COMMA	[,]

%%

%{
    /* if we haven't gotten a token yet, get ready to read a block tag */
    if (first_token) {
	BEGIN(LINE);
	first_token = 0;
    }

    /* BEGIN application-specific lexer declarations */
%}

<LINE>"PLATE" {
    BEGIN(INITIAL);
    process_type(&yylval, "PLATE", PLATE);
    TABULATE(PLATE);
}
<LINE>"STRIPPLATE" {
    BEGIN(INITIAL);
    process_type(&yylval, "PLATE", PLATE);
    TABULATE(PLATE);
}
<LINE>"ARB" {
    BEGIN(INITIAL);
    process_type(&yylval, "ARB", ARB);
    TABULATE(PLATE);
}
<LINE>"SYMMETRY" {
    BEGIN(INITIAL);
    process_type(&yylval, "POINTS", SYMMETRY);
    TABULATE(SYMMETRY);
}
<LINE>"POINTS" {
    BEGIN(INITIAL);
    process_type(&yylval, "POINTS", POINTS);
    TABULATE(POINTS);
}
<LINE>"GROUND" {
    BEGIN(INITIAL);
    process_type(&yylval, "POINTS", POINTS);
    TABULATE(POINTS);
}
<LINE>"CYLINDER" {
    BEGIN(INITIAL);
    process_type(&yylval, "CYLINDER", CYLINDER);
    TABULATE(CYLINDER);
}
<LINE>"CYL" {
    BEGIN(INITIAL);
    process_type(&yylval, "CYL", CYL);
    TABULATE(CYL);
}
<LINE>"PIPE" {
    BEGIN(INITIAL);
    process_type(&yylval, "PIPE", PIPE);
    TABULATE(PIPE);
}
<LINE>"SPHERE" {
    BEGIN(INITIAL);
    process_type(&yylval, "SPHERE", SPHERE);
    TABULATE(SPHERE);
}

%{
    /* END application-specific lexer declarations */
%}

{INT} {
    tabulate(INT, yytext);
    /* printf("found int: %s (int %ld)\n", yytext, atol(yytext)); */
    if (yylval.index > 0 || yylval.count > 0) {
	process_value(&yylval, atof(yytext));
    } else {
	yylval.index = atol(yytext);
    }
    return INT;
}

{FLOAT} {
    tabulate(FLT, yytext);
    /* printf("found a float: %s (flt %lf)\n", yytext, atof(yytext)); */
    process_value(&yylval, atof(yytext));
    return FLT;
}

{COMMENT} {
    tabulate(COMMENT, yytext);
    /* printf("found a comment: %s\n", yytext); */
    /*    yylval = strdup(yytext);*/
    return COMMENT;
}

{COMMA} {
    return COMMA;
}

{SPACE} {
    tabulate(0, yytext);
}

{EOL}.* {
    int i=0;

    BEGIN(LINE);

    /* save the next line for error reporting */
    bu_strlcpy(previous_linebuffer, linebuffer, YYLMAX);
    bu_strlcpy(linebuffer, yytext+1, YYLMAX);

    while (i < yyleng && (yytext[i] == '\n' || yytext[i] == '\r')) {
	if (yytext[i] == '\n') {
	    tabulate(NL, "\n");
	}
	i++;
    }
    yyless(i);

    /* collapse them all to one */
    return NL;
}

{NONPRINT} {
    printf("WARNING: Ignoring non-printable character (0x%x) on line %ld  (file offset %ld)\n", yytext[0], get_lines()+1, get_bytes());
}

. {
    /* bad input?  */
    printf("TOKENIZE ERROR: Unexpected character ('%c' 0x%x) on line %ld, column %ld  (file offset %ld)\n", yytext[0], yytext[0], get_lines()+1, get_column(), get_bytes());
    yyerror(yytext);
    unput(' '); /* unreached, quell yyunput unused warning instead of using YY_NO_UNPUT */
    bu_exit(1, NULL);
}
%%


/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
