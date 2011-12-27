/*                       P E R P L E X . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided
 * with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote
 * products derived from this software without specific prior written
 * permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/** @file perplex.h
 *
 * definitions common to the perplex main and scanner sources
 *
 */

#ifndef PERPLEX_H
#define PERPLEX_H

#include <stdio.h>
#include "parser.h"
#include "token_type.h"

/* tokens are typically ints defined starting from 0,
 * thus -1 is least likely to conflict with another token
 */
#define YYEOF -1

/* support for start conditions */
typedef enum YYCONDTYPE {
    initial,
    definitions,
    rules,
    code,
    squote_string,
    dquote_string,
    bracket_string,
    comment,
    line_comment,
    condition_list
} condition_t;

struct Buf {
    void   *elts;	/* elements. */
    int     nelts;	/* number of elements. */
    size_t  elt_size;	/* in bytes. */
    int     nmax;	/* max capacity of elements. */
};

typedef struct appData_t {
    FILE *in;
    FILE *out;
    FILE *header;
    FILE *scanner_template;
    YYSTYPE tokenData;
    char *conditions;
    int usingConditions;
} appData_t;

/* scanner data */
typedef struct perplex {
    FILE *inFile;
    int atEOI;
    int braceCount;
    int scopeBraceCount;
    int conditionScope;
    int inAction;
    int inDefinition;
    char *cursor;
    char *marker;
    char *null;
    char *tokenStart;
    struct Buf *buffer;
    char *tokenText;
    appData_t *appData;
    condition_t condition;
} *perplex_t;

#ifdef __cplusplus
extern "C" {
#endif
int yylex(perplex_t scanner);

perplex_t perplexFileScanner(FILE *input);
void perplexFree(perplex_t scanner);

void *ParseAlloc(void *(*mallocProc)(size_t));
void ParseFree(void *parser, void (*freeProc)(void*));
void Parse(void *parser, int tokenID, YYSTYPE tokenData, appData_t *appData);
void ParseTrace(FILE *fp, char *s);
#ifdef __cplusplus
}
#endif

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
