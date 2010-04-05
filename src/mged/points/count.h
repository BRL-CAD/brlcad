/*                         C O U N T . H
 * BRL-CAD
 *
 * Copyright (c) 2005-2010 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 *
 */
/** @file count.h
 *
 * Basic counting structure and functions.
 *
 * Author -
 *   Christopher Sean Morrison
 */

#ifndef __COUNT_H__
#define __COUNT_H__


typedef struct token {
    struct token *next;
    long int id;
    long int count;
} token_t;

typedef struct counter {
    long int column;
    long int bytes;
    long int words;
    long int lines;
    token_t token;
} counter_t;

#define INIT_TOKEN_T(t) { \
    (t).next = NULL; \
    (t).id = 0; \
    (t).count = 0; \
}

#define INIT_COUNTER_T(c) { \
    (c).column = 0; \
    (c).bytes = 0; \
    (c).words = 0; \
    (c).lines = 0; \
    INIT_TOKEN_T((c).token); \
}

#define TABULATE(x) {\
    tabulate(x, yytext);\
    yylval.type = # x;\
    return x;\
}

/** count up what's just been parsed */
void tabulate(long int id, const char *text);

/** release resources used counting */
void free_tabulation();


/** get current column number */
long int get_column();

/** get bytes parsed */
long int get_bytes();

/** get words/tokens parsed */
long int get_words();

/** get lines parsed */
long int get_lines();

/** lookup a particular token's count */
long int count_get_token(long int id);


#endif  /* __COUNT_H__ */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
