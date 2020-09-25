#include <stdio.h>
#include <stdlib.h>
#include "dplot_reader.h"
#include "bu.h"

HIDDEN void *
dplot_malloc(size_t s) {
    return bu_malloc(s, "dplot_malloc");
}

HIDDEN void
dplot_free(void *p) {
    bu_free(p, "dplot_free");
}

int
main(int argc, char *argv[])
{
    int i, token_id;
    FILE *input_file;
    perplex_t scanner;
    void *parser;
    struct ssx *curr;
    struct dplot_data data;

    if (argc != 2) {
	bu_exit(1, "usage: %s input\n", argv[0]);
    }

    /* initialize scanner */
    input_file = fopen(argv[1], "r");
    if (!input_file) {
	bu_exit(2, "error: couldn't open \"%s\" for reading.\n", argv[1]);
    }
    scanner = perplexFileScanner(input_file);

    data.ssx_count = 0;
    BU_LIST_INIT(&data.ssx_list);
    perplexSetExtra(scanner, (void *)&data);

    /* initialize parser */
    parser = ParseAlloc(dplot_malloc);

    /* parse */
    while ((token_id = yylex(scanner)) != YYEOF) {
	Parse(parser, token_id, data.token_data, &data);
    }
    Parse(parser, 0, data.token_data, &data);

    /* move list data to array */
    if (data.ssx_count > 0) {
	data.ssx = (struct ssx *)bu_malloc(
		sizeof(struct ssx) * data.ssx_count, "ssx array");

	i = data.ssx_count - 1;
	while (BU_LIST_WHILE(curr, ssx, &data.ssx_list)) {
	    data.ssx[i--] = *curr;
	    BU_LIST_DEQUEUE(&curr->l);
	    BU_PUT(curr, struct ssx);
	}
    }

    /* print data */
    bu_log("brep1: %d surfaces\n", data.brep1_surface_count);
    bu_log("brep2: %d surfaces\n", data.brep2_surface_count);

    for (i = 0; i < data.ssx_count; ++i) {
	bu_log("%d curve events between surface 1-%d and 2-%d\n",
		data.ssx[i].final_curve_events, data.ssx[i].brep1_surface,
		data.ssx[i].brep2_surface);
	bu_log("\t%d brep1 isocurves intersected brep2\n",
		data.ssx[i].intersecting_brep1_isocurves);
	bu_log("\t%d brep2 isocurves intersected brep1\n",
		data.ssx[i].intersecting_isocurves -
		data.ssx[i].intersecting_brep1_isocurves);
    }

    /* clean up */
    if (data.ssx) {
	bu_free(data.ssx, "ssx array");
    }
    ParseFree(parser, dplot_free);
    perplexFree(scanner);
    fclose(input_file);

    return 0;
}
