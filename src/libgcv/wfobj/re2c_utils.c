/*                    R E 2 C _ U T I L S . C
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
/** @file re2c_utils.c
 *
 * Utilities to simplify usage of re2c.
 *
 */

#include "re2c_utils.h"
#include "bu.h"

bu_string *newString()
{
    struct bu_vls *string = NULL;

    BU_GETSTRUCT(string, bu_vls);

    bu_vls_init(string);

    return string;
}

int getNextLine(bu_string *buf, FILE *in)
{
    int len;

    bu_vls_trunc(buf, 0);

    len = bu_vls_gets(buf, in);

    if (len < 0) {
	return len;
    }

    bu_vls_strcat(buf, "\n");

    return len + 1;
}

int loadInput(scanner_t *scanner)
{
    bu_string *line = scanner->currLine;
    int len = bu_vls_strlen(line);
    char *lineEnd = line->vls_str + len;

    if (scanner->tokenStart >= lineEnd) {
	len = getNextLine(line, scanner->in);
	scanner->tokenStart = line->vls_str;
    }

    return len;
}

char *copyCurrTokenText(bu_string *tokenString, scanner_t *scanner)
{
    int tokenLen = *scanner->cursor_p - scanner->tokenStart;

    bu_vls_strncpy(tokenString, scanner->tokenStart, tokenLen);

    return tokenString->vls_str;
}

void setStartCondition(scanner_t *scanner, const CONDTYPE sc)
{
    scanner->startCondition = sc;
}

CONDTYPE getStartCondition(scanner_t *scanner)
{
    return scanner->startCondition;
}

void initScanner(scanner_t *scanner, FILE *in)
{
    setStartCondition(scanner, INITIAL);

    scanner->tokenText = newString();

    scanner->in = in;

    scanner->currLine = newString();
    getNextLine(scanner->currLine, scanner->in);

    scanner->tokenStart = scanner->currLine->vls_str;
}

void freeScanner(scanner_t *scanner)
{
    bu_vls_vlsfree(scanner->currLine);
    bu_vls_vlsfree(scanner->tokenText);
    scanner->currLine = NULL;
    scanner->tokenText = NULL;

    scanner->tokenStart = NULL;
    scanner->marker = NULL;
    scanner->cursor_p = NULL;

    bu_free(scanner, "freeScanner scanner_t");
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
