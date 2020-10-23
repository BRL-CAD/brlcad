#ifndef PERPLEX_H
#define PERPLEX_H
/*              P E R P L E X _ T E M P L A T E . C
 * BRL-CAD
 *
 * Copyright (c) 1990-2011 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * Copyright (c) 1990 The Regents of the University of California.
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
/*  Parts of this file are based on sources from the flex project.
 *
 *  This code is derived from software contributed to Berkeley by
 *  Vern Paxson.
 *
 *  The United States Government has rights in this work pursuant
 *  to contract no. DE-AC03-76SF00098 between the United States
 *  Department of Energy and the University of California.
 */
/** @file perplex_template.c
 *
 * template for generated scanners
 *
 */
#include <stdio.h>
#include <stdlib.h>

#define YYEOF -1

struct Buf {
    void   *elts;	/* elements. */
    int     nelts;	/* number of elements. */
    size_t  elt_size;	/* in bytes. */
    int     nmax;	/* max capacity of elements. */
};

/* scanner data */
typedef struct perplex {
    void *extra;        /* application data */
    FILE *inFile;
    struct Buf *buffer;
    char *tokenText;
    char *tokenStart;
    char *cursor;
    char *marker;
    char *null;
    int atEOI;
    int condition;
} *perplex_t;

perplex_t perplexFileScanner(FILE *input);
perplex_t perplexStringScanner(char *firstChar, size_t numChars);
void perplexFree(perplex_t scanner);

void perplexUnput(perplex_t scanner, char c);
void perplexSetExtra(perplex_t scanner, void *extra);
void* perplexGetExtra(perplex_t scanner);

#ifndef PERPLEX_LEXER
#define PERPLEX_LEXER yylex
#endif

#define PERPLEX_PUBLIC_LEXER  PERPLEX_LEXER(perplex_t scanner)
#define PERPLEX_PRIVATE_LEXER PERPLEX_LEXER_private(perplex_t scanner)

#ifndef PERPLEX_ON_ENTER
#define PERPLEX_ON_ENTER /* do nothing */
#endif

int PERPLEX_PUBLIC_LEXER;

#endif
