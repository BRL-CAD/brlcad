/*                       P E R P L E X . C
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
/** @file perplex.c
 *
 * perplex scanner-generator
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "perplex.h"
#if 0
Desired Features:
- string and file inputs w/ autobuffering
- autogenerate scanner function
- indicate end-of-input by returning -1
- if rule actions don't end with a return,
  ignore the recognized token and continue parsing
- character classes (might implement using auto-included named patterns)
- include C code (re2c passes through by default) 
- error mechanism ? (reentrancy?)
- start conditions
- unquoted literal characters
- echo unmatched text?
- yylineno? (reentrancy?)

Possible Options (borrowed from flex usage message):
-?
-h,  --help		produce this help message
     --header-file=FILE	create a C header file in addition to the scanner
-L,  --noline		suppress #line directives in scanner
-o,  --outfile=FILE	specify output filename (stdout by default?)
-P,  --prefix=STRING	use STRING as prefix instead of "yy"
-V,  --version		report perplex version
     --yylineno		track line count in yylineno
#endif

int main(int argc, char *argv[])
{
    perplex_t scanner;
    FILE *inFile, *templateFile;
    char c;
    int tokenID;
    void *parser;
    appData_t *appData;

    if (argc != 2) {
	printf("Usage: perplex input.l\n");
	return 0;
    }

    if ((templateFile = fopen("template.c", "r")) == NULL) {
	fprintf(stderr, "Error: couldn't open template.c for reading\n");
	exit(1);
    }

    if ((inFile = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "Error: couldn't open \"%s\" for reading\n", argv[1]);
	exit(1);
    }

    scanner = perplexFileScanner(inFile);
    parser = ParseAlloc(malloc);

    scanner->appData = malloc(sizeof(appData_t));
    appData = scanner->appData;

    appData->in = inFile;
    appData->template = templateFile;

    while ((tokenID = yylex(scanner)) != YYEOF) {
	Parse(parser, tokenID, appData->tokenData, appData);
    }
    Parse(parser, 0, appData->tokenData, appData);

    free(appData);
    ParseFree(parser, free);
    perplexFree(scanner);
    fclose(inFile);

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
