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

/* helper to copy next line of in into buf
 * returns length of copied line
 */
HIDDEN int
scannerGetNextLine(struct bu_vls *buf, FILE *in)
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

/* If scanner's input buffer is exhausted, it is refilled.
 * returns new length of input buffer
 */
int
scannerLoadInput(scanner_t *scanner)
{
    int lineLen;
    char *lineEnd;
    struct bu_vls *line;

    line = scanner->currLine;
    lineLen = bu_vls_strlen(line);
    lineEnd = bu_vls_addr(line) + lineLen;

    if (scanner->tokenStart >= lineEnd) {
	lineLen = scannerGetNextLine(line, scanner->in);
	scanner->tokenStart = bu_vls_addr(line);
    }

    return lineLen;
}

/* copy scanner's internal string for current token to tokenString */
char*
scannerCopyCurrTokenText(struct bu_vls *tokenString, scanner_t *scanner)
{
    int tokenLen = *scanner->cursor_p - scanner->tokenStart;

    bu_vls_strncpy(tokenString, scanner->tokenStart, tokenLen);

    return bu_vls_addr(tokenString);
}

/* set scanner's start condition to sc */
void
scannerSetStartCondition(scanner_t *scanner, const CONDTYPE sc)
{
    scanner->startCondition = sc;
}

/* get scanner's current start condition */
CONDTYPE
scannerGetStartCondition(scanner_t *scanner)
{
    return scanner->startCondition;
}

/* initialize scanner that reads from file in */
void
scannerInit(scanner_t *scanner, FILE *in)
{
    scannerSetStartCondition(scanner, INITIAL);

    BU_GET(scanner->tokenText, struct bu_vls);
    bu_vls_init(scanner->tokenText);

    scanner->in = in;

    BU_GET(scanner->currLine, struct bu_vls);
    bu_vls_init(scanner->currLine);

    scannerGetNextLine(scanner->currLine, scanner->in);

    scanner->tokenStart = bu_vls_addr(scanner->currLine);
}

/* free all resources associated with scanner */
void
scannerFree(scanner_t *scanner)
{
    bu_vls_vlsfree(scanner->currLine);
    bu_vls_vlsfree(scanner->tokenText);
    scanner->currLine = NULL;
    scanner->tokenText = NULL;

    scanner->tokenStart = NULL;
    scanner->marker = NULL;
    scanner->cursor_p = NULL;

    bu_free(scanner, "scannerFree scanner_t");
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
