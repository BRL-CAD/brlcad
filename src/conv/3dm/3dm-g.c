/*                         3 D M - G . C
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


static const struct gcv_filter *
find_filter(enum gcv_filter_type filter_type, bu_mime_model_t mime_type, const char *data, struct gcv_context *context)
{
    const struct gcv_filter * const *entry;
    const struct bu_ptbl * const filters = gcv_list_filters(context);

    for (BU_PTBL_FOR(entry, (const struct gcv_filter * const *), filters)) {
	bu_mime_model_t emt = (*entry)->mime_type;
	if ((*entry)->filter_type != filter_type) continue;
	if ( (emt != BU_MIME_MODEL_AUTO) && (emt == mime_type)) return *entry;
	if ( (emt == BU_MIME_MODEL_AUTO) && ((*entry)->data_supported && data && (*(*entry)->data_supported)(data)) ) return *entry;
    }
    return NULL;
}


int
main(int argc, char **argv)
{
    const char * const usage =
	"Usage: 3dm-g [-r] [-v] [-h] -o output_file.g input_file.3dm\n";

    const struct gcv_filter *out_filter;
    const struct gcv_filter *in_filter;

    struct gcv_context context;
    struct gcv_opts gcv_options;
    const char *output_path = NULL;
    const char *input_path;
    int c;

    bu_setprogname(argv[0]);
    gcv_opts_default(&gcv_options);

    while ((c = bu_getopt(argc, argv, "o:rvh?")) != -1) {
	switch (c) {
	    case 'o':
		output_path = bu_optarg;
		break;

	    case 'r':
		gcv_options.randomize_colors = 1;
		break;

	    case 'v':
		gcv_options.verbosity_level = 1;
		break;

	    default:
		bu_log("%s", usage);
		return 1;
	}
    }

    if (bu_optind != argc - 1 || !output_path) {
	bu_log("%s", usage);
	return 1;
    }

    input_path = argv[bu_optind];

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
