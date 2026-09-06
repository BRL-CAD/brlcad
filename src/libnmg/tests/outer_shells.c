/*                   O U T E R _ S H E L L S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
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

#include "bu/log.h"
#include "nmg.h"

int
main(int UNUSED(argc), char **UNUSED(argv))
{
    const struct bn_tol tol = BN_TOL_INIT_TOL;
    struct bu_list vlfree = BU_LIST_INIT_ZERO;
    struct model *m = nmg_mmr();
    struct nmgregion *r = BU_LIST_FIRST(nmgregion, &m->r_hd);
    struct bu_ptbl table = BU_PTBL_INIT_ZERO;
    struct bu_ptbl *table_p = &table;
    struct bu_ptbl **shells = &table_p;
    int count;
    int result = 0;

    BU_LIST_INIT(&vlfree);
    /* The empty result must replace the caller's non-NULL output pointer. */
    count = nmg_find_outer_and_void_shells(r, &shells, &vlfree, &tol);
    if (count != 0 || shells != NULL) {
	bu_log("Empty region returned %d outer shells and array %p\n", count, (void *)shells);
	result = 1;
    }
    nmg_km(m);
    return result;
}

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * c-basic-offset: 4
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
