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
- recognized token text available in yytext buffer
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

    while ((tokenID = yylex(scanner)) != YYEOF) {
	switch(tokenID) {
	case TOKEN_DEFINITIONS:
	    while ((c = fgetc(templateFile)) != EOF) {
		printf("%c", c);
	    }
	    printf("\n");
	    fclose(templateFile);
	    break;
	case TOKEN_RULES:
	    printf("*/\n}\n");
	}
    }

    perplexFree(scanner);
    fclose(inFile);

    return 0;
}
