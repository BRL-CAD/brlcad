/*                         G E T _ O B J _ B O U N D S . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2011 United States Government as represented by
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
 * The orot command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include "bio.h"

#include "ged.h"

#include "./ged_private.h"


int
_ged_get_obj_bounds(struct ged *gedp,
		    int argc,
		    const char *argv[],
		    int use_air,
		    point_t rpp_min,
		    point_t rpp_max)
{
    int i;
    struct rt_i *rtip;
    struct db_full_path path;
    struct region *regp;

    /* Make a new rt_i instance from the existing db_i sructure */
    if ((rtip=rt_new_rti(gedp->ged_wdbp->dbip)) == RTI_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "rt_new_rti failure for %s\n", gedp->ged_wdbp->dbip->dbi_filename);
	return GED_ERROR;
    }

    rtip->useair = use_air;

    /* Get trees for list of objects/paths */
    for (i = 0; i < argc; i++) {
	int gottree;

	/* Get full_path structure for argument */
	db_full_path_init(&path);
	if (db_string_to_path(&path,  rtip->rti_dbip, argv[i])) {
	    bu_vls_printf(&gedp->ged_result_str, "db_string_to_path failed for %s\n", argv[i]);
	    rt_free_rti(rtip);
	    return GED_ERROR;
	}

	/* check if we already got this tree */
	gottree = 0;
	for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	    struct db_full_path tmp_path;

	    db_full_path_init(&tmp_path);
	    if (db_string_to_path(&tmp_path, rtip->rti_dbip, regp->reg_name)) {
		bu_vls_printf(&gedp->ged_result_str, "db_string_to_path failed for %s\n", regp->reg_name);
		rt_free_rti(rtip);
		db_free_full_path(&path);
		return GED_ERROR;
	    }
	    if (path.fp_names[0] == tmp_path.fp_names[0])
		gottree = 1;
	    db_free_full_path(&tmp_path);
	    if (gottree)
		break;
	}

	/* if we don't already have it, get it */
	if (!gottree && rt_gettree(rtip, path.fp_names[0]->d_namep)) {
	    bu_vls_printf(&gedp->ged_result_str, "rt_gettree failed for %s\n", argv[i]);
	    rt_free_rti(rtip);
	    db_free_full_path(&path);
	    return GED_ERROR;
	}
	db_free_full_path(&path);
    }

    /* prep calculates bounding boxes of solids */
    rt_prep(rtip);

    /* initialize RPP bounds */
    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);
    for (i = 0; i < argc; i++) {
	vect_t reg_min, reg_max;
	const char *reg_name;

	/* check if input name is a region */
	for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
	    reg_name = regp->reg_name;
	    if (*argv[i] != '/' && *reg_name == '/')
		reg_name++;

	    if (BU_STR_EQUAL(reg_name, argv[i]))
		goto found;
	}
	goto not_found;

    found:
	if (regp != REGION_NULL) {
	    /* input name was a region */
	    if (rt_bound_tree(regp->reg_treetop, reg_min, reg_max)) {
		bu_vls_printf(&gedp->ged_result_str, "rt_bound_tree failed for %s\n", regp->reg_name);
		rt_free_rti(rtip);
		return TCL_ERROR;
	    }
	    VMINMAX(rpp_min, rpp_max, reg_min);
	    VMINMAX(rpp_min, rpp_max, reg_max);
	} else {
	    size_t name_len;
	not_found:

	    /* input name may be a group, need to check all regions under
	     * that group
	     */
	    name_len = strlen(argv[i]);
	    for (BU_LIST_FOR(regp, region, &(rtip->HeadRegion))) {
		reg_name = regp->reg_name;
		if (*argv[i] != '/' && *reg_name == '/')
		    reg_name++;

		if (strncmp(argv[i], reg_name, name_len))
		    continue;

		/* This is part of the group */
		if (rt_bound_tree(regp->reg_treetop, reg_min, reg_max)) {
		    bu_vls_printf(&gedp->ged_result_str, "rt_bound_tree failed for %s\n", regp->reg_name);
		    rt_free_rti(rtip);
		    return TCL_ERROR;
		}
		VMINMAX(rpp_min, rpp_max, reg_min);
		VMINMAX(rpp_min, rpp_max, reg_max);
	    }
	}
    }

    rt_free_rti(rtip);

    return TCL_OK;
}


/**
 *
 *
 */
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

	av0 = strdup(argv[0]);
	tok = strtok(av0, "/");
	while (tok) {
	    if ((gtdp->gtd_obj[gtdp->gtd_objpos++] =
		 db_lookup(gedp->ged_wdbp->dbip, tok, LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(&gedp->ged_result_str, "get_objpath_mat: Failed to find %s", tok);
		free(av0);
		return GED_ERROR;
	    }

	    tok = strtok((char *)0, "/");
	}

	free(av0);
    } else {
	gtdp->gtd_objpos = argc;

	/* build directory pointer array for desired path */
	for (i=0; i<gtdp->gtd_objpos; i++) {
	    if ((gtdp->gtd_obj[i] =
		 db_lookup(gedp->ged_wdbp->dbip, argv[pos_in+i], LOOKUP_NOISY)) == RT_DIR_NULL) {
		bu_vls_printf(&gedp->ged_result_str, "get_objpath_mat: Failed to find %s", argv[pos_in+i]);
		return GED_ERROR;
	    }
	}
    }

    MAT_IDN(gtdp->gtd_xform);
    _ged_trace(gtdp->gtd_obj[0], 0, bn_mat_identity, gtdp);

    return GED_OK;
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
    struct soltab *stp;
    mat_t imat;

    /* initialize RPP bounds */
    VSETALL(rpp_min, MAX_FASTF);
    VREVERSE(rpp_max, rpp_min);

    if (get_objpath_mat(gedp, argc, argv, gtdp) == TCL_ERROR)
	return GED_ERROR;

    dp = gtdp->gtd_obj[gtdp->gtd_objpos-1];
    GED_DB_GET_INTERNAL(gedp, &intern, dp, gtdp->gtd_xform, &rt_uniresource, GED_ERROR);

    /* Make a new rt_i instance from the existing db_i structure */
    if ((rtip=rt_new_rti(gedp->ged_wdbp->dbip)) == RTI_NULL) {
	bu_vls_printf(&gedp->ged_result_str, "rt_new_rti failure for %s", gedp->ged_wdbp->dbip->dbi_filename);
	return GED_ERROR;
    }

    BU_GETSTRUCT(stp, soltab);
    stp->l.magic = RT_SOLTAB_MAGIC;
    stp->l2.magic = RT_SOLTAB2_MAGIC;
    stp->st_dp = dp;
    MAT_IDN(imat);
    stp->st_matp = imat;

    /* Get bounds from internal object */
    VMOVE(stp->st_min, rpp_min);
    VMOVE(stp->st_max, rpp_max);
    if (intern.idb_meth->ft_prep)
	intern.idb_meth->ft_prep(stp, &intern, rtip);
    VMOVE(rpp_min, stp->st_min);
    VMOVE(rpp_max, stp->st_max);

    rt_free_rti(rtip);
    rt_db_free_internal(&intern);
    bu_free((char *)stp, "struct soltab");

    return GED_OK;
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
