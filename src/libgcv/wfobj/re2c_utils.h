/*                    R E 2 C _ U T I L S . H
 * BRL-CAD
 *
 * Copyright (c) 2011 United States Government as represented by
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
/** @file re2c_utils.h
 *
 * Utilities to simplify usage of re2c.
 *
 */

/* Utilities to simplify usage of re2c by providing support for:
 *  - automatic input buffer management
 *        - Just specify a FILE* and the scanner will read from the input file
 *          as needed and save its place in the input between calls to the
 *          scan function.
 *
 *  - end-of-input token
 *        - The YYEOF symbol is defined, and is returned by the scan function 
 *          when the end of the input file is reached.
 *
 *  - ignoring tokens
 *        - Use IGNORE_TOKEN in an action body to ignore the recognized token
 *          and continue scanning.
 *
 *  - (f)lex-like yytext string
 *        - char *yytext will exist as a local buffer that holds the text of the
 *          current token.
 *
 *  - (f)lex-like BEGIN statement
 *        - You can use BEGIN(condition) to set the current start condition.
 *
 * There are a few requirements to get this support:
 *  - You need to include this header in the re2c input file and link to the
 *    corresponding object file.
 *
 *  - Disable the re2c buffering interface in the re2c comment block:
 *        re2c:yyfill:enable = 0;
 *
 *  - Your scanning function needs to return int (the token id). To return the
 *    token id, you must use RETURN(id) rather than simply return id.
 *
 *  - A scanner_t* that you have initialized with scannerInit(scanner_t*, FILE*)
 *    (and that you can deconstruct with scannerFree(scanner_t*)) must be
 *    available inside your scan function.
 *
 *  - You need to wrap the re2c comment block between the macros
 *    BEGIN_RE2C(scanner, cursor) and END_RE2C, where scanner is the name of
 *    a scanner_t*, and cursor is the name of the char* given to re2c via:
 *        re2c:define:YYCURSOR = cursor;
 *
 *  - To use the (f)lex-like BEGIN(condition) statement you need to define the
 *    CONDTYPE symbol before including this header:
 *        enum YYCONDTYPE {
 *            INITIAL,
 *            ...other states...
 *        };
 *        #define CONDTYPE enum YYCONDTYPE
 *
 *    In the re2c comment block set:
 *        re2c:define:condenumprefix = "";
 *        re2c:define:YYSETCONDITION = BEGIN;
 */

#ifndef RE2C_UTILS_H
#define RE2C_UTILS_H

#include "common.h"
#include "bu.h"

__BEGIN_DECLS

/* end of file token - since parser generators start defining token ids at 0,
 * -1 is the least likely value to be used by an existing token
 */
#define YYEOF -1

#ifndef CONDTYPE
enum YYCONDTYPE { INITIAL };
#define CONDTYPE enum YYCONDTYPE
#endif

typedef struct {
    void *extra; /* user data - user must free */
    FILE *in;    /* user must close */
    struct bu_vls *currLine;
    struct bu_vls *tokenText;
    char *tokenStart;
    char *marker;
    char **cursor_p;
    CONDTYPE startCondition;
} scanner_t;

/* HELPERS */
/* condition support */
void
scannerSetStartCondition(scanner_t *scanner, const CONDTYPE sc);

CONDTYPE
scannerGetStartCondition(scanner_t *scanner);

/* text processing support */
#define YYMARKER _re2c_scanner->marker

int
scannerLoadInput(scanner_t *scanner);
#define UPDATE_START _re2c_scanner->tokenStart = *_re2c_scanner->cursor_p;

char*
scannerCopyCurrTokenText(struct bu_vls *tokenString, scanner_t *scanner);
#define FREE_TOKEN_TEXT bu_vls_vlsfree(_re2c_token_string);


/* USER FUNCTIONS */
void
scannerInit(scanner_t *scanner, FILE *in);

void
scannerFree(scanner_t *scanner);

/* USER MACROS */
/* support for (f)lex token string */
#define yytext (scannerCopyCurrTokenText(_re2c_token_string, _re2c_scanner))

/* support for (f)lex "start conditions" */
#define BEGIN(sc) scannerSetStartCondition(_re2c_scanner, sc)
#define YYGETCONDITION() scannerGetStartCondition(_re2c_scanner)

/* use inside action to return tokenID to caller */
#define RETURN(tokenID) \
    FREE_TOKEN_TEXT \
    UPDATE_START; \
    return tokenID;

/* use inside action to skip curr token and continue scanning */
#define IGNORE_TOKEN \
    UPDATE_START; \
    continue;

#define BEGIN_RE2C(scanner, yycursor) \
    /* define names used by helper macros */ \
    scanner_t *_re2c_scanner = scanner; \
    struct bu_vls *_re2c_token_string; \
    char *yycursor; \
\
    if (scannerLoadInput(_re2c_scanner) < 0) { \
	return YYEOF; \
    } \
\
    yycursor = _re2c_scanner->tokenStart; \
    _re2c_scanner->cursor_p = &yycursor; \
\
    BU_GET(_re2c_token_string, struct bu_vls); \
    bu_vls_init(_re2c_token_string); \
\
    /* create implicit label */ \
    while (1) {

#define END_RE2C } /* while (1) */

__END_DECLS

#endif /* RE2C_UTILS_H */

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
