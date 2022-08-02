/*                         G E T _ O B J _ B O U N D S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2022 United States Government as represented by
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
/** @file libged/get_obj_bounds.c
 *
 * Calculate object bounds.
 *
 * TODO - why are there two versions of this?
 *
 * TODO - this belongs at the librt level, and probably
 * lower than that once libg is split out...
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bu/str.h"
#include "ged.h"

#include "./ged_private.h"

int
ged_get_obj_bounds(struct ged *gedp,
                   int argc,
                   const char *argv[],
                   int use_air,
                   point_t rpp_min,
                   point_t rpp_max)
{
    return rt_obj_bounds(gedp->ged_result_str, gedp->dbip, argc, argv, use_air, rpp_min, rpp_max);
}

static int
get_objpath_mat(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    struct _ged_trace_data *gtdp)
{
    int i, pos_in;

    /*
     * paths are matched up to last input member
     * ANY path the same up to this point is considered as matching
     */

    /* initialize gtd */
    gtdp->gtd_gedp = gedp;
    gtdp->gtd_flag = _GED_EVAL_ONLY;
    gtdp->gtd_prflag = 0;

    pos_in = 0;

    if (argc == 1 && strchr(argv[0], '/')) {
	char *tok;
	char *av0;
	gtdp->gtd_objpos = 0;

	av0 = bu_strdup(argv[0]);
	tok = strtok(av0, "/");
	while (tok) {
	    if ((gtdp->gtd_obj[gtdp->gtd_objpos++] =
		 db_lookup(gedp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "get_objpath_mat: Failed to find %s", tok);
		free(av0);
		return BRLCAD_ERROR;
	    }

	    tok = strtok((char *)0, "/");
	}

	free(av0);
    } else {
	gtdp->gtd_objpos = argc;

	/* build directory pointer array for desired path */
	for (i = 0; i < gtdp->gtd_objpos; i++) {
	    if ((gtdp->gtd_obj[i] =
		 db_lookup(gedp->dbip, argv[pos_in+i], LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "get_objpath_mat: Failed to find %s", argv[pos_in+i]);
		return BRLCAD_ERROR;
	    }
	}
    }

    MAT_IDN(gtdp->gtd_xform);
    ged_trace(gtdp->gtd_obj[0], 0, bn_mat_identity, gtdp, 1);

    return BRLCAD_OK;
}


/**
 * @brief
 * This version works if the last member of the path is a primitive.
 */
int
_ged_get_obj_bounds2(struct ged *gedp,
		     int argc,
		     const char *argv[],
		     struct _ged_trace_data *gtdp,
		     point_t rpp_min,
		     point_t rpp_max)
{
    struct directory *dp;
    struct rt_db_internal intern;
    struct rt_i *rtip;
    struct soltab st;
    mat_t imat;

    /* initialize RPP bounds */
    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);

    if (get_objpath_mat(gedp, argc, argv, gtdp) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    dp = gtdp->gtd_obj[gtdp->gtd_objpos-1];
    GED_DB_GET_INTERNAL(gedp, &intern, dp, gtdp->gtd_xform, &rt_uniresource, BRLCAD_ERROR);

    /* Make a new rt_i instance from the existing db_i structure */
    rtip = rt_new_rti(gedp->dbip);
    if (rtip == RTI_NULL) {
	bu_vls_printf(gedp->ged_result_str, "rt_new_rti failure for %s", gedp->dbip->dbi_filename);
	return BRLCAD_ERROR;
    }

    memset(&st, 0, sizeof(struct soltab));

    st.l.magic = RT_SOLTAB_MAGIC;
    st.l2.magic = RT_SOLTAB2_MAGIC;
    st.st_dp = dp;
    MAT_IDN(imat);
    st.st_matp = imat;
    st.st_meth = intern.idb_meth;

    /* Get bounds from internal object */
    VMOVE(st.st_min, rpp_min);
    VMOVE(st.st_max, rpp_max);
    if (intern.idb_meth->ft_prep)
	intern.idb_meth->ft_prep(&st, &intern, rtip);
    VMOVE(rpp_min, st.st_min);
    VMOVE(rpp_max, st.st_max);

    rt_free_rti(rtip);
    rt_db_free_internal(&intern);

    return BRLCAD_OK;
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
