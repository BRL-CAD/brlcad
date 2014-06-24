#include <stdio.h>
#include <stdlib.h>
#include "main.h"

int
main(int argc, char *argv[])
{
    int token_id;
    FILE *input_file;
    perplex_t scanner;
    void *parser;
    app_data_t data;

    if (argc != 2) {
	fprintf(stderr, "usage: %s input\n", argv[0]);
	exit(1);
    }

    /* init scanner */
    input_file = fopen(argv[1], "r");
    if (!input_file) {
	fprintf(stderr, "error: couldn't open \"%s\" for reading.\n", argv[1]);
	exit(2);
    }
    scanner = perplexFileScanner(input_file);

    perplexSetExtra(scanner, (void *)&data);

    /* init parser */
    parser = ParseAlloc(malloc());

    /* parse */
    while ((token_id = yylex(scanner)) != YYEOF) {
	Parse(parser, token_id, data.token_data, &data);
    }
    Parse(parser, 0, data.token_data, &data);

    /* this is where you'd do somthing with the app data */

    /* cleanup */
    ParseFree(parser, free);
    perplexFree(scanner);
    fclose(input_file);

    return 0;
}
