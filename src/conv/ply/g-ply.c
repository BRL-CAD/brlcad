/*                  G - P L Y . C
 * BRL-CAD
 *
 * Copyright (c) 2003-2022 United States Government as represented by
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
/** @file conv/ply/g-ply.c
 *
 * Program to convert a BRL-CAD model (in a .g file) to the Stanford
 * PLY format by calling on the NMG booleans. Based on g-stl.c.
 *
 */

#include "common.h"

#include <stdlib.h>

#include "bu/app.h"
#include "bu/getopt.h"
#include "gcv/api.h"

    static char usage[] = "\
Usage: %s [-v][-xX lvl][-a abs_tess_tol (default: 0.0)][-r rel_tess_tol (default: 0.01)]\n\
  [-n norm_tess_tol (default: 0.0)][-t type (asc: ascii), (le: little endian), (be: big endian)]\n\
  [-s separate file per object][-D dist_calc_tol (default: 0.0005)] -o output_file_name brlcad_db.g object(s)\n";

int
main(int argc, char **argv)
{
    const struct gcv_filter *in_filter;
    const struct gcv_filter *out_filter;
    struct gcv_context context;
    struct gcv_opts gcv_options;
    const char *output_path = NULL;
    const char *input_path = NULL;
    int separate = 0;
    char* type = "de";
    int c;

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);

    /* Get command line arguments. */
    while ((c = bu_getopt(argc, argv, "r:a:n:o:t:svx:D:X:h?")) != -1) {
	switch (c) {
	    case 'r':		/* Relative tolerance. */
		gcv_options.tessellation_tolerance.rel = atof(bu_optarg);
		break;
	    case 'a':		/* Absolute tolerance. */
		gcv_options.tessellation_tolerance.abs = atof(bu_optarg);
		gcv_options.tessellation_tolerance.rel = 0.0;
		break;
	    case 'n':		/* Surface normal tolerance. */
		gcv_options.tessellation_tolerance.norm = atof(bu_optarg);
		gcv_options.tessellation_tolerance.rel = 0.0;
		break;
	    case 'o':		/* Output file name. */
		output_path = bu_optarg;
		break;
	    case 't':
                type = bu_optarg;
		break;
	    case 's':		/* separate objects into individual ply files */
                separate = 1;
		break;
	    case 'v':
                gcv_options.verbosity_level = 1;
		break;
	    case 'x':
		sscanf(bu_optarg, "%x", (unsigned int *)&gcv_options.rt_debug_flag);
		break;
	    case 'D':
		gcv_options.calculational_tolerance.dist = atof(bu_optarg);
		gcv_options.calculational_tolerance.dist_sq = gcv_options.calculational_tolerance.dist * gcv_options.calculational_tolerance.dist;
		break;
	    case 'X':
		sscanf(bu_optarg, "%x", (unsigned int *)&gcv_options.nmg_debug_flag);
		break;
	    default:
		bu_exit(1, usage, argv[0]);
	}
    }

    /* handle options unique to ply conversion - these are passed into
     * the writer gcv_execute as argc and argv 
     * NOTE: we need two options for "-t type" and potentially
     * another for separate if user specifed
     */
    int unique_options_ac = 2 + separate;
    const char* unique_options_av[4] = {NULL, NULL, NULL, NULL};
    
    unique_options_av[0] = "-t";
    unique_options_av[1] = type;
    if (separate)
        unique_options_av[2] = "-s";

    /* per usage, we should have atleast two argv left:
     * input_file.g object(s)
     * and -o should have been supplied
     */
    if (bu_optind >= argc - 1 || !output_path)
	bu_exit(1, usage, argv[0]);

    input_path = argv[bu_optind];
    gcv_options.num_objects = argc - bu_optind - 1;     // objects left after flags and input_file name
    gcv_options.object_names = (const char* const*)argv + bu_optind + 1;    // offset after flags and input_file name

    /* setup to call gcv */
    gcv_context_init(&context);

    /* ensure filters exist */
    in_filter = find_filter(GCV_FILTER_READ, BU_MIME_MODEL_VND_BRLCAD_PLUS_BINARY, input_path, &context);
    out_filter = find_filter(GCV_FILTER_WRITE, BU_MIME_MODEL_PLY, output_path, &context);

    if (!out_filter || !in_filter) {
	gcv_context_destroy(&context);
        bu_log("ERROR: could not find the conversion filters\n");
        return 1;
    }

    /* do the conversion */
    if (!gcv_execute(&context, in_filter, &gcv_options, 0, NULL, input_path)) {
	gcv_context_destroy(&context);
	bu_exit(1, "failed to load input file");
    }

    gcv_options.debug_mode = 0;

    if (!gcv_execute(&context, out_filter, &gcv_options, unique_options_ac, unique_options_av, output_path)) {
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
