/*                         3 D M - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2023 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */
/** @file 3dm-g.cpp
 *
 * Conversion of Rhino models (.3dm files) into BRL-CAD databases.
 *
 */


#include "common.h"

#include "bu/app.h"
#include "bu/exit.h"
#include "bu/getopt.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "gcv/api.h"


int
main(int argc, char **argv)
{
    const char * const usage =
	"Usage: 3dm-g [-r] [-v] [-o output_file.g] [-h] input_file.3dm output_file.g\n";

    const struct gcv_filter *out_filter;
    const struct gcv_filter *in_filter;

    struct gcv_context context;
    struct gcv_opts gcv_options;
    const char *output_path = NULL;
    const char *input_path;
    int print_help = 0;

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);

    struct bu_opt_desc d[6];
    BU_OPT(d[0], "r", "",          "", NULL,              &gcv_options.randomize_colors, "Randomize object colors");
    BU_OPT(d[1], "v", "verbosity", "", &bu_opt_incr_long, &gcv_options.verbosity_level,  "Increase verbosity level");
    BU_OPT(d[2], "o", "output",    "", &bu_opt_str,       &output_path,                  "Set output filename");
    BU_OPT(d[3], "h", "help",      "", NULL,              &print_help,                   "Print help and exit");
    BU_OPT(d[4], "?", "",          "", NULL,              &print_help,                   "");
    BU_OPT_NULL(d[5]);

    /* parse standard options */
    argc--; argv++;
    int opt_ret = bu_opt_parse(NULL, argc, (const char**)argv, d);

    /* requested help or bad usage */
    if (print_help || (opt_ret == 1 && !output_path) || (opt_ret == 2 && output_path) || opt_ret < 1 || opt_ret > 2) {
	char* help = bu_opt_describe(d, NULL);
	bu_log("%s\nOptions:\n%s", usage, help);
	if (help) bu_free(help, "help str");
	return 1;
    }

    input_path = argv[0];
    if (!output_path)
	output_path = argv[1];

    gcv_context_init(&context);

    in_filter = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_VND_RHINO, input_path, &context);
    out_filter = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, NULL, &context);

    if (!out_filter) {
	gcv_context_destroy(&context);
	bu_bomb("could not find the BRL-CAD writer filter");
    }

    if (!in_filter) {
	bu_log("could not find the Rhino reader filter");
	gcv_context_destroy(&context);
	return 1;
    }


    if (!gcv_execute(&context, in_filter, &gcv_options, 0, NULL, input_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to load input file");
    }

    gcv_options.debug_mode = 0;

    if (!gcv_execute(&context, out_filter, &gcv_options, 0, NULL, output_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to export to output file");
    }

    gcv_context_destroy(&context);

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
