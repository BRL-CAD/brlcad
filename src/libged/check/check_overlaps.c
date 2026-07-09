/*                C H E C K _ O V E R L A P S . C
 * BRL-CAD
 *
 * Copyright (c) 2018-2026 United States Government as represented by
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
#include "ged.h"

#include <stdlib.h>
#include <string.h>

#include "../ged_private.h"
#include "./check_private.h"


/**
 * Format overlaps results from an analyze_results object.
 *
 * When options->rpt_overlap_flag is set (the default), prints an
 * "OVERLAP PAIRS" section that groups ordered pairs, shows the
 * reverse pair if present, and adds a SUMMARY.
 *
 * When rpt_overlap_flag is clear (the -R option), the per-occurrence
 * immediate output was already printed by the render hook; here we
 * only print the final count.
 */
int check_format_overlaps(struct ged *gedp,
			  struct analyze_results *res,
			  struct check_parameters *options)
{
    size_t i, j;
    size_t total_overlaps = 0;
    size_t n_pairs;

    /* Total occurrences = sum of all per-pair counts. */
    n_pairs = BU_PTBL_LEN(&res->overlaps);
    for (i = 0; i < n_pairs; i++) {
	struct analyze_overlap_record *r =
	    (struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, i);
	total_overlaps += r->count;
    }

    if (options->rpt_overlap_flag) {
	/* ---- OVERLAP PAIRS section ---- */
	size_t unique_pairs = 0;
	size_t object_counter = 0;
	struct bu_vls str = BU_VLS_INIT_ZERO;

	bu_vls_printf(&str,
		      "OVERLAP PAIRS\n"
		      "------------------------------------------\n");

	/* Iterate; for each ordered pair (A,B) that has not already
	 * been printed as a reverse of an earlier pair (B,A), print
	 * both (A,B) and – if present – (B,A). */
	for (i = 0; i < n_pairs; i++) {
	    struct analyze_overlap_record *rec =
		(struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, i);
	    struct analyze_overlap_record *fwd_rev = NULL;
	    int reverse_seen = 0;

	    /* Was this pair already printed as a reverse of an earlier one? */
	    for (j = 0; j < i; j++) {
		struct analyze_overlap_record *prev =
		    (struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, j);
		if (BU_STR_EQUAL(prev->region1, rec->region2)
		    && rec->region1 && prev->region2
		    && BU_STR_EQUAL(prev->region2, rec->region1)) {
		    reverse_seen = 1;
		    break;
		}
	    }
	    if (reverse_seen)
		continue;

	    unique_pairs++;

	    /* Look for the forward-reverse (B,A) later in the table. */
	    for (j = i + 1; j < n_pairs; j++) {
		struct analyze_overlap_record *nxt =
		    (struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, j);
		if (rec->region2 && nxt->region1
		    && BU_STR_EQUAL(nxt->region1, rec->region2)
		    && rec->region1 && nxt->region2
		    && BU_STR_EQUAL(nxt->region2, rec->region1)) {
		    fwd_rev = nxt;
		    break;
		}
	    }

	    bu_vls_printf(&str, "%s and %s overlap\n",
			  rec->region1, rec->region2 ? rec->region2 : "");
	    bu_vls_printf(&str,
			  "\t<%s, %s>: %lu overlap%c detected, "
			  "maximum depth is %g %s\n",
			  rec->region1, rec->region2 ? rec->region2 : "",
			  rec->count, rec->count > 1 ? 's' : (char)0,
			  rec->max_dist / options->units[LINE]->val,
			  options->units[LINE]->name);

	    if (fwd_rev) {
		bu_vls_printf(&str,
			      "\t<%s, %s>: %lu overlap%c detected, "
			      "maximum depth is %g %s\n",
			      fwd_rev->region1,
			      fwd_rev->region2 ? fwd_rev->region2 : "",
			      fwd_rev->count,
			      fwd_rev->count > 1 ? 's' : (char)0,
			      fwd_rev->max_dist / options->units[LINE]->val,
			      options->units[LINE]->name);
	    }
	}

	if (total_overlaps) {
	    bu_vls_printf(&str,
			  "==========================================\n"
			  "SUMMARY\n"
			  "\t%zu overlap%s detected\n"
			  "\t%zu unique overlapping pair%s (%zu ordered pair%s)\n",
			  total_overlaps,
			  (total_overlaps == 1) ? "" : "s",
			  unique_pairs,
			  (unique_pairs == 1) ? "" : "s",
			  n_pairs, (n_pairs == 1) ? "" : "s");

	    /* Collect unique overlapping object names. */
	    bu_vls_printf(&str, "\tOverlapping objects: ");
	    for (i = 0; i < n_pairs; i++) {
		struct analyze_overlap_record *rec =
		    (struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, i);
		/* Print region1 if not seen before. */
		int seen = 0;
		for (j = 0; j < i && !seen; j++) {
		    struct analyze_overlap_record *prev =
			(struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, j);
		    if (BU_STR_EQUAL(prev->region1, rec->region1)
			|| (prev->region2 && BU_STR_EQUAL(prev->region2, rec->region1)))
			seen = 1;
		}
		if (!seen) {
		    bu_vls_printf(&str, "%s  ", rec->region1);
		    object_counter++;
		}

		/* Print region2 if not seen before. */
		if (rec->region2) {
		    seen = 0;
		    for (j = 0; j < n_pairs && !seen; j++) {
			struct analyze_overlap_record *prev =
			    (struct analyze_overlap_record *)BU_PTBL_GET(&res->overlaps, j);
			if (j < i) {
			    if (BU_STR_EQUAL(prev->region1, rec->region2)
				|| (prev->region2 && BU_STR_EQUAL(prev->region2, rec->region2)))
				seen = 1;
			}
			if (j == i)
			    break;
		    }
		    if (!seen) {
			bu_vls_printf(&str, "%s  ", rec->region2);
			object_counter++;
		    }
		}
	    }
	    bu_vls_printf(&str,
			  "\n\t%zu unique overlapping object%s detected\n",
			  object_counter,
			  (object_counter == 1) ? "" : "s");
	} else {
	    bu_vls_printf(&str, "%zu overlap%s detected\n\n",
			  total_overlaps, (total_overlaps == 1) ? "" : "s");
	}

	bu_vls_vlscat(gedp->ged_result_str, &str);
	bu_vls_free(&str);

    } else {
	/* Non-rpt mode: individual occurrences already printed by render
	 * hook; just output the final count. */
	bu_vls_printf(gedp->ged_result_str, "%zu overlap%s detected\n",
		      total_overlaps, (total_overlaps == 1) ? "" : "s");
    }

    return BRLCAD_OK;
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

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
