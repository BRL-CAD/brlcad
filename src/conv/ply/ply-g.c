/*                         P L Y - G . C
 * BRL-CAD
 *
 * Copyright (c) 2004-2022 United States Government as represented by
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
 *
 */
/** @file conv/ply/ply-g.c
 *
 * Program to convert the PLY format to the BRL-CAD format using GCV execution
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/app.h"
#include "bu/getopt.h"
#include "gcv/api.h"

static char* usage="Usage:\n\tply-g [-s scale_factor] [-d] [-v] input_file.ply output_file.g";

int
main(int argc, char *argv[])
{
    const struct gcv_filter *in_filter;
    const struct gcv_filter *out_filter;
    struct gcv_context context;
    struct gcv_opts gcv_options;
    const char *output_path = NULL;
    const char *input_path;
    int c;
    char* scale_factor = "1000.0";      // scale factor gets passed as a string, no sense converting

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);

    while ((c = bu_getopt(argc, argv, "s:dv")) != -1) {
	switch (c) {
	    case 's':
		scale_factor = bu_optarg;
		break;
	    case 'd':
		gcv_options.bu_debug_flag = 1;
		break;
	    case 'v':
		gcv_options.verbosity_level = 1;
		break;
	    default:
		bu_log("%s\n", usage);
		return 1;
	}
    }

    /* per usage, we should have only two argv left:
     * input_file.ply output_file.g
     */
    if (bu_optind != argc - 2) {
	bu_log("%s\n", usage);
	return 1;
    }
    input_path = argv[bu_optind];
    output_path = argv[bu_optind + 1];

    /* since ply files are typically in meters, we want to pass in the scale factor
     * to gcv_exec with argc and argv as "-s scale_factor"
     */
    const char* unique_options_av[2];
    unique_options_av[0] = "-s";
    unique_options_av[1] = scale_factor;

    /* setup to call gcv */
    gcv_context_init(&context);

    /* ensure filters exist */
    in_filter = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_PLY, input_path, &context);
    out_filter = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, output_path, &context);

    if (!out_filter || !in_filter) {
	gcv_context_destroy(&context);
        bu_log("ERROR: could not find the conversion filters\n");
        return 1;
    }

    /* do the conversion */
    if (!gcv_execute(&context, in_filter, &gcv_options, 2, unique_options_av, input_path)) {
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
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
