/*                      U T I L I T Y . C
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

#include "./gdiff2.h"

void
diff_state_init(struct diff_state *state)
{
    state->use_params = 1;
    state->use_attrs = 1;
    state->return_added = -1;
    state->return_removed = -1;
    state->return_changed = -1;
    state->return_conflicts = 1;
    state->return_unchanged = 0;
    state->have_search_filter = 0;
    state->verbosity = 2;
    state->output_mode = 0;
    BU_GET(state->diff_tol, struct bn_tol);
    state->diff_tol->dist = RT_LEN_TOL;
    BU_GET(state->diff_log, struct bu_vls);
    BU_GET(state->search_filter, struct bu_vls);
    bu_vls_init(state->diff_log);
    bu_vls_init(state->search_filter);
}

void
diff_state_free(struct diff_state *state)
{
    bu_vls_free(state->diff_log);
    bu_vls_free(state->search_filter);
    BU_PUT(state->diff_tol, struct bn_tol);
    BU_PUT(state->diff_log, struct bu_vls);
    BU_PUT(state->search_filter, struct bu_vls);
}

struct diff_avp *
diff_ptbl_get(struct bu_ptbl *avp_array, const char *key)
{
    int i = 0;
    for (i = 0; i < (int)BU_PTBL_LEN(avp_array); i++) {
	struct diff_avp *avp = (struct diff_avp *)BU_PTBL_GET(avp_array, i);
	if (BU_STR_EQUAL(avp->name, key)) return avp;
    }
    return NULL;
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
