/*                     G D I F F 2 . H
 * BRL-CAD
 *
 * Copyright (c) 2014 United States Government as represented by
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

#include "tcl.h"

#include "bu/getopt.h"
#include "mater.h"
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
    int verbosity;
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
