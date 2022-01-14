/*                       C S G . C
 * BRL-CAD
 *
 * Copyright (c) 2013-2022 United States Government as represented by
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

#include "common.h"
#include <stdlib.h>
#include <stdio.h>
#include "csg.h"


static void *
parse_alloc(size_t size)
{
    return bu_malloc(size, "parse_alloc");
}


int
main(int argc, char *argv[])
{
    FILE *inputFile, *outputFile;
    void *parser;
    int tokenID;
    perplex_t scanner;
    token_t *tokenData;
    app_data_t appData;

    bu_setprogname(argv[0]);

    if (argc != 3) {
	fprintf(stderr, "Usage: %s input output", argv[0]);
	exit(1);
    }

    inputFile = fopen(argv[1], "r");
    outputFile = fopen(argv[2], "w");

    scanner = perplexFileScanner(inputFile);
    perplexSetExtra(scanner, &appData);
    appData.outfile = outputFile;
    appData.example_text = 0;
    bu_vls_init(&appData.description);
    bu_vls_init(&appData.tags);

    parser = ParseAlloc(parse_alloc);

    BU_GET(tokenData, token_t);
    bu_vls_init(&tokenData->value);
    appData.tokenData = tokenData;

    while ((tokenID = yylex(scanner)) != YYEOF) {
	Parse(parser, tokenID, tokenData, &appData);
	BU_GET(tokenData, token_t);
	bu_vls_init(&tokenData->value);
	appData.tokenData = tokenData;
    }
    Parse(parser, 0, tokenData, &appData);

    ParseFree(parser, free);
    perplexFree(scanner);
    fclose(inputFile);
    fclose(outputFile);

    return 0;
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
