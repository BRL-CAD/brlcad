#include <stdlib.h>
#include <stdio.h>
#include "dom2dox.h"

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

    if (argc != 3) {
	fprintf(stderr, "Usage: %s input output", argv[0]);
	exit(1);
    }

    inputFile = fopen(argv[1], "r");
    outputFile = fopen(argv[2], "w");

    scanner = perplexFileScanner(inputFile);
    perplexSetExtra(scanner, &appData);
    appData.outfile = outputFile;
    appData.doc_comment = 0;
    appData.tag_text = 0;
    appData.example_text = 0;
    appData.return_text = 0;
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
