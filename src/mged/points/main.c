#include <stdio.h>

#include "./count.h"
#include "./points_parse.h"

extern FILE *yyin;
extern int yyparse (void);


#if 0
int main(int argc, char *argv[])
#else

#  include "tcl.h"
ClientData *cdata;
Tcl_Interp *twerp;

int parse_point_file(ClientData clientData, Tcl_Interp *interp, int argc, char *argv[])
#endif
{
    long int datapoints;

    if (argc > 0) {
	printf("Reading from %s\n", argv[0]);
	yyin = fopen(argv[0], "r");
    } else {
	printf("Reading from stdin\n");
	yyin = stdin;
    }

    cdata = &clientData;
    twerp = interp;

    while (!feof(yyin)) {
	yyparse();
    }

    datapoints = 
	count_get_token(PLATE) +
	count_get_token(ARB) +
	count_get_token(SYMMETRY) +
	count_get_token(POINTS) +
	count_get_token(CYLINDER) +
	count_get_token(PIPE);

    printf("\nSUMMARY:\n\n");
    printf("PLATE lines: %ld\n", count_get_token(PLATE));
    printf("ARB lines: %ld\n", count_get_token(ARB));
    printf("SYMMETRY lines: %ld\n", count_get_token(SYMMETRY));
    printf("POINTS lines: %ld\n", count_get_token(POINTS));
    printf("CYLINDER lines: %ld\n", count_get_token(CYLINDER));
    printf("PIPE lines: %ld\n", count_get_token(PIPE));

    printf("\n\tData points: %ld\n\tComments: %ld\n\tLines: %ld\n\tWords: %ld\n\tBytes: %ld\n\n", datapoints, count_get_token(COMMENT), get_lines(), get_words(), get_bytes());

    return 0;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
