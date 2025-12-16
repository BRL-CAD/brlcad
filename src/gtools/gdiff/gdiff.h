/*                     G D I F F 2 . H
 * BRL-CAD
 *
 * Copyright (c) 2014-2025 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this file; see the file named COPYING for more
 * information.
 */

#include "common.h"

#include <string.h>

#include "bu/getopt.h"
#include "raytrace.h"
#include "rt/db_diff.h"

#ifndef _GDIFF2_H
#define _GDIFF2_H

/*******************************************************************/
/*     Containers for holding various forms of diff information    */
/*******************************************************************/

/* Reporting options, search filters, and other user specified state */
struct diff_state {
    int use_params;
    int use_attrs;
    int show_params;
    int show_attrs;
    int return_added;
    int return_removed;
    int return_changed;
    int return_unchanged;
    int return_conflicts;
    int have_search_filter;
    long verbosity;
    long quiet;
    int output_mode;
    int merge;
    struct bn_tol *diff_tol;
    struct bu_vls *diff_log;
    struct bu_vls *search_filter;
    struct bu_vls *merge_file;
};

extern void diff_state_init(struct diff_state *state);
extern void diff_state_free(struct diff_state *state);

/*******************************************************************/
/*              summary functions for printing results             */
/*******************************************************************/
extern void diff_summarize(struct bu_vls *diff_log,
	const struct bu_ptbl *results,
	struct diff_state *state);

extern void diff3_summarize(struct bu_vls *diff_log,
	const struct bu_ptbl *results,
	struct diff_state *state);

/*******************************************************************/
/*                     utility functions                           */
/*******************************************************************/
extern struct diff_avp *
diff_ptbl_get(struct bu_ptbl *avp_array, const char *key);

extern int
diff3_merge(struct db_i *left_dbip,
       	    struct db_i *ancestor_dbip,
	    struct db_i *right_dbip,
	    struct diff_state *state,
	    struct bu_ptbl *results);

// Various options for grouping mode
struct gdiff_group_opts {
    long filename_threshold; // editing distance below which we group based on filename
    int use_names;           // Incorporate .g object names into similarity testing
    int use_geometry;        // Incorporate .g object geometry into similarity testing
    long threshold;          // tlsh threshold below which we group .g files
    struct bu_vls fpattern;  // Pattern match to use for a recursive file search
    struct bu_vls ofile;     // Optional output file
    int thread_cnt;          // Number of threads to use for database hashing (defaults to 1)
    // When checking .g geometry, by default we use a slower but better check to spot similar
    // geometry. If this is too slow, there is another option that increases sensitivity to
    // small geometry changes but speeds up processing.  In fast mode objects having ANY
    // difference (anything from added/removed to tiny modifications) are treated as maximally different.
    // .g file will look less similar by this metric, but the user can compensate for that to a
    // degree by increasing geometry_threshold.)
    int geom_fast;
    struct bu_vls hash_infile;   // Optional: read precomputed hashes from this file
    struct bu_vls hash_outfile;  // Optional: write computed hashes to this file
    int path_display_mode;       // 0:auto, 1:relative, 2:absolute
    long verbosity;              // Verbosity level (0 = quiet)
};
#define GDIFF_PATH_DISPLAY_RELATIVE 1
#define GDIFF_PATH_DISPLAY_ABSOLUTE 2
#define GDIFF_GROUP_OPTS_DEFAULT {-1, 0, 0, -1, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, 1, 0, BU_VLS_INIT_ZERO, BU_VLS_INIT_ZERO, 0, 0}

extern int
gdiff_group(int argc, const char **argv, struct gdiff_group_opts *o);

#endif

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
