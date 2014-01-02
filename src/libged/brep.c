/*                         B R E P . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2014 United States Government as represented by
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
/** @file libged/brep.c
 *
 * The brep command.
 *
 */

#include "common.h"

#include <stdlib.h>
#include <string.h>

#include "bio.h"
#include "bu.h"
#include "raytrace.h"
#include "rtgeom.h"
#include "wdb.h"

#include "./ged_private.h"

/* TODO - get rid of the need for brep_specific at this level */
#include "../librt/primitives/brep/brep_local.h"

/* FIXME: how should we set up brep functionality without introducing
 * lots of new public librt functions?  right now, we reach into librt
 * directly and export what we need from brep_debug.cpp which sucks.
 */
RT_EXPORT extern int brep_command(struct bu_vls *vls, const char *solid_name, const struct rt_tess_tol *ttol, const struct bn_tol *tol, struct brep_specific* bs, struct rt_brep_internal* bi, struct bn_vlblock *vbp, int argc, const char *argv[], char *commtag);
RT_EXPORT extern int brep_conversion(struct rt_db_internal* in, struct rt_db_internal* out, const struct db_i *dbip);
RT_EXPORT extern int brep_conversion_comb(struct rt_db_internal *old_internal, const char *name, const char *suffix, struct rt_wdb *wdbp, fastf_t local2mm);
RT_EXPORT extern int brep_intersect_point_point(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
RT_EXPORT extern int brep_intersect_point_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
RT_EXPORT extern int brep_intersect_point_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
RT_EXPORT extern int brep_intersect_curve_curve(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
RT_EXPORT extern int brep_intersect_curve_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j);
RT_EXPORT extern int brep_intersect_surface_surface(struct rt_db_internal *intern1, struct rt_db_internal *intern2, int i, int j, struct bn_vlblock *vbp);
RT_EXPORT extern int rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, const char* operation);
 
static int
selection_command(
	struct ged *gedp,
	struct rt_db_internal *ip,
	int argc,
	const char *argv[])
{
    size_t i;
    struct rt_selection_set *selection_set;
    struct bu_ptbl *selections;
    struct rt_selection *new_selection;
    struct rt_selection_query query;
    const char *cmd, *solid_name, *selection_name;
 
    /*  0         1          2         3
     * brep <solid_name> selection subcommand
     */
    if (argc < 4) {
	return -1;
    }

    solid_name = argv[1];
    cmd = argv[3];

    if (BU_STR_EQUAL(cmd, "append")) {
	/* append to named selection - selection is created if it doesn't exist */
	void (*free_selection)(struct rt_selection *);

	/*        4         5      6      7     8    9    10
	 * selection_name startx starty startz dirx diry dirz
	 */
	if (argc != 11) {
	    return -1;
	}
	selection_name = argv[4];

	/* find matching selections */
	query.start[X] = atof(argv[5]);
	query.start[Y] = atof(argv[6]);
	query.start[Z] = atof(argv[7]);
	query.dir[X] = atof(argv[8]);
	query.dir[Y] = atof(argv[9]);
	query.dir[Z] = atof(argv[10]);
	query.sorting = RT_SORT_CLOSEST_TO_START;

	selection_set = ip->idb_meth->ft_find_selections(ip, &query);
	if (!selection_set) {
	    return -1;
	}

	/* could be multiple options, just grabbing the first and
	 * freeing the rest
	 */
	free_selection = selection_set->free_selection;
	selections = &selection_set->selections;
	new_selection = (struct rt_selection *)BU_PTBL_GET(selections, 0);
	for (i = 1; i < BU_PTBL_LEN(selections); ++i) {
	    long *s = BU_PTBL_GET(selections, i);
	    free_selection((struct rt_selection *)s);
	    bu_ptbl_rm(selections, s);
	}
	bu_ptbl_free(selections);

	/* get existing/new selections set in gedp */
	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	free_selection = selection_set->free_selection;
	selections = &selection_set->selections;

	/* TODO: Need to implement append by passing new and
	 * existing selection to an rt_brep_evaluate_selection.
	 * For now, new selection simply replaces old one.
	 */
	for (i = 1; i < BU_PTBL_LEN(selections); ++i) {
	    long *s = BU_PTBL_GET(selections, i);
	    free_selection((struct rt_selection *)s);
	    bu_ptbl_rm(selections, s);
	}
	bu_ptbl_ins(selections, (long *)new_selection);
    } else if (BU_STR_EQUAL(cmd, "translate")) {
	struct rt_selection_operation operation;

	/*        4       5  6  7
	 * selection_name dx dy dz
	 */
	if (argc != 8) {
	    return -1;
	}
	selection_name = argv[4];

	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	selections = &selection_set->selections;

	if (BU_PTBL_LEN(selections) < 1) {
	    return -1;
	}

	for (i = 0; i < BU_PTBL_LEN(selections); ++i) {
	    int ret;
	    operation.type = RT_SELECTION_TRANSLATION;
	    operation.parameters.tran.dx = atof(argv[5]);
	    operation.parameters.tran.dy = atof(argv[6]);
	    operation.parameters.tran.dz = atof(argv[7]);

	    ret = ip->idb_meth->ft_process_selection(ip,
		    (struct rt_selection *)BU_PTBL_GET(selections, i), &operation);

	    if (ret != 0) {
		return ret;
	    }
	}
    }

    return 0;
}

int
ged_brep(struct ged *gedp, int argc, const char *argv[])
{
    struct bn_vlblock*vbp;
    const char *solid_name;
    static const char *usage = "brep <obj> [command|brepname|suffix] ";
    struct directory *ndp;
    struct rt_db_internal intern;
    struct rt_brep_internal* bi;
    struct brep_specific* bs;
    struct soltab *stp;
    char commtag[64];
    char namebuf[64];
    int i, j, real_flag, valid_command, ret;
    const char *commands[] = {"info", "plot", "translate", "intersect", "u", "i", "-"};
    int num_commands = (int)(sizeof(commands) / sizeof(const char *));

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n\t%s\n", argv[0], usage);
	bu_vls_printf(gedp->ged_result_str, "commands:\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo - return count information for specific BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo S [index] - return information for specific BREP 'surface'\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo F [index] - return information for specific BREP 'face'\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot - plot entire BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot S [index] - plot specific BREP 'surface'\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot F [index] - plot specific BREP 'face'\n");
	bu_vls_printf(gedp->ged_result_str, "\ttranslate SCV index i j dx dy dz - translate a surface control vertex\n");
	bu_vls_printf(gedp->ged_result_str, "\tintersect <obj2> <i> <j> [PP|PC|PS|CC|CS|SS] - BREP intersections\n");
	bu_vls_printf(gedp->ged_result_str, "\tu|i|- <obj2> <output> - BREP boolean evaluations\n");
	bu_vls_printf(gedp->ged_result_str, "\t[brepname] - convert the non-BREP object to BREP form\n");
	bu_vls_printf(gedp->ged_result_str, "\t --no-evaluation [suffix] - convert non-BREP comb to unevaluated BREP form\n");
	return GED_HELP;
    }

    if (argc < 2 || argc > 10) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", argv[0], usage);
	return GED_ERROR;
    }

    solid_name = argv[1];
    if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  solid_name, LOOKUP_NOISY)) == RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", solid_name);
	return GED_ERROR;
    } else {
	real_flag = (ndp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
    }

    if (!real_flag) {
	/* solid doesn't exist - don't kill */
	bu_vls_printf(gedp->ged_result_str, "Error: %s is not a real solid", solid_name);
	return GED_OK;
    }

    GED_DB_GET_INTERNAL(gedp, &intern, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);


    RT_CK_DB_INTERNAL(&intern);
    bi = (struct rt_brep_internal*)intern.idb_ptr;

    if (BU_STR_EQUAL(argv[2], "intersect")) {
	/* handle surface-surface intersection */
	struct rt_db_internal intern2;

	/* we need exactly 6 or 7 arguments */
	if (argc != 6 && argc != 7) {
	    bu_vls_printf(gedp->ged_result_str, "There should be 6 or 7 arguments for intersection.\n");
	    bu_vls_printf(gedp->ged_result_str, "See the usage for help.\n");
	    return GED_ERROR;
	}

	/* get the other solid */
	if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[3], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", argv[3]);
	    return GED_ERROR;
	} else {
	    real_flag = (ndp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	}

	if (!real_flag) {
	    /* solid doesn't exist - don't kill */
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a real solid", argv[3]);
	    return GED_OK;
	}

	GED_DB_GET_INTERNAL(gedp, &intern2, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	i = atoi(argv[4]);
	j = atoi(argv[5]);
	vbp = rt_vlblock_init();

	if (argc == 6 || BU_STR_EQUAL(argv[6], "SS")) {
	    brep_intersect_surface_surface(&intern, &intern2, i, j, vbp);
	} else if (BU_STR_EQUAL(argv[6], "PP")) {
	    brep_intersect_point_point(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "PC")) {
	    brep_intersect_point_curve(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "PS")) {
	    brep_intersect_point_surface(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "CC")) {
	    brep_intersect_curve_curve(&intern, &intern2, i, j);
	} else if (BU_STR_EQUAL(argv[6], "CS")) {
	    brep_intersect_curve_surface(&intern, &intern2, i, j);
	} else {
	    bu_vls_printf(gedp->ged_result_str, "Invalid intersection type %s.\n", argv[6]);
	}

	_ged_cvt_vlblock_to_solids(gedp, vbp, namebuf, 0);
	rt_vlblock_free(vbp);
	vbp = (struct bn_vlblock *)NULL;

	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "u") || BU_STR_EQUAL(argv[2], "i") || BU_STR_EQUAL(argv[2], "-") || BU_STR_EQUAL(argv[2], "x")) {
	/* test booleans on brep.
	 * u: union, i: intersect, -: diff, x: xor
	 */
	struct rt_db_internal intern2, intern_res;
	struct rt_brep_internal *bip;

	if (argc != 5) {
	    bu_vls_printf(gedp->ged_result_str, "Error: There should be exactly 5 params.\n");
	    return GED_ERROR;
	}

	/* get the other solid */
	if ((ndp = db_lookup(gedp->ged_wdbp->dbip,  argv[3], LOOKUP_NOISY)) == RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a solid or does not exist in database", argv[3]);
	    return GED_ERROR;
	} else {
	    real_flag = (ndp->d_addr == RT_DIR_PHONY_ADDR) ? 0 : 1;
	}

	if (!real_flag) {
	    /* solid doesn't exist - don't kill */
	    bu_vls_printf(gedp->ged_result_str, "Error: %s is not a real solid", argv[3]);
	    return GED_OK;
	}

	GED_DB_GET_INTERNAL(gedp, &intern2, ndp, bn_mat_identity, &rt_uniresource, GED_ERROR);

	rt_brep_boolean(&intern_res, &intern, &intern2, argv[2]);
	bip = (struct rt_brep_internal*)intern_res.idb_ptr;
	mk_brep(gedp->ged_wdbp, argv[4], bip->brep);
	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	rt_db_free_internal(&intern_res);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "selection")) {
	ret = selection_command(gedp, &intern, argc, argv);
	rt_db_free_internal(&intern);

	return ret;
    }

    if (!RT_BREP_TEST_MAGIC(bi)) {
	/* The solid is not in brep form. Covert it to brep. */

	struct bu_vls bname, suffix;
	int no_evaluation = 0;

	bu_vls_init(&bname);
	bu_vls_init(&suffix);

	if (argc == 2) {
	    /* brep obj */
	    bu_vls_sprintf(&bname, "%s_brep", solid_name);
	    bu_vls_sprintf(&suffix, "_brep");
	} else if (BU_STR_EQUAL(argv[2], "--no-evaluation")) {
	    no_evaluation = 1;
	    if (argc == 3) {
		/* brep obj --no-evaluation */
		bu_vls_sprintf(&bname, "%s_brep", solid_name);
		bu_vls_sprintf(&suffix, "_brep");
	    } else if (argc == 4) {
		/* brep obj --no-evaluation suffix */
		bu_vls_sprintf(&bname, argv[3]);
		bu_vls_sprintf(&suffix, argv[3]);
	    }
	} else {
	    /* brep obj brepname/suffix */
	    bu_vls_sprintf(&bname, argv[2]);
	    bu_vls_sprintf(&suffix, argv[2]);
	}

	if (no_evaluation && intern.idb_type == ID_COMBINATION) {
	    struct bu_vls bname_suffix;
	    bu_vls_init(&bname_suffix);
	    bu_vls_sprintf(&bname_suffix, "%s%s", solid_name, bu_vls_addr(&suffix));
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_suffix), LOOKUP_QUIET) != RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_suffix));
		bu_vls_free(&bname);
		bu_vls_free(&suffix);
		bu_vls_free(&bname_suffix);
		return GED_OK;
	    }
	    brep_conversion_comb(&intern, bu_vls_addr(&bname_suffix), bu_vls_addr(&suffix), gedp->ged_wdbp, mk_conv2mm);
	    bu_vls_free(&bname_suffix);
	} else {
	    struct rt_db_internal brep_db_internal;
	    ON_Brep* brep;
	    if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname), LOOKUP_QUIET) != RT_DIR_NULL) {
		bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname));
		bu_vls_free(&bname);
		bu_vls_free(&suffix);
		return GED_OK;
	    }
	    ret = brep_conversion(&intern, &brep_db_internal, gedp->ged_wdbp->dbip);
	    brep = ((struct rt_brep_internal *)brep_db_internal.idb_ptr)->brep;
	    if (ret == -1) {
		bu_vls_printf(gedp->ged_result_str, "%s doesn't have a brep-conversion function yet. Type: %s", solid_name, intern.idb_meth->ft_label);
	    } else if (brep == NULL) {
		bu_vls_printf(gedp->ged_result_str, "%s cannot be converted to brep correctly.", solid_name);
	    } else {
		ret = mk_brep(gedp->ged_wdbp, bu_vls_addr(&bname), brep);
		if (ret == 0) {
		    bu_vls_printf(gedp->ged_result_str, "%s is made.", bu_vls_addr(&bname));
		}
	    }
	    rt_db_free_internal(&brep_db_internal);
	}
	bu_vls_free(&bname);
	bu_vls_free(&suffix);
	rt_db_free_internal(&intern);
	return GED_OK;
    }

    BU_ALLOC(stp, struct soltab);

    if (argc == 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	bu_vls_printf(gedp->ged_result_str, "\t%s is in brep form, please input a command.", solid_name);
	return GED_HELP;
    }

    valid_command = 0;
    for (i = 0; i < num_commands; ++i) {
	if (BU_STR_EQUAL(argv[2], commands[i])) {
	    valid_command = 1;
	    break;
	}
    }

    if (!valid_command) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s\n", argv[0], usage);
	bu_vls_printf(gedp->ged_result_str, "\t%s is in brep form, please input a command.", solid_name);
	return GED_HELP;
    }

    if ((bs = (struct brep_specific*)stp->st_specific) == NULL) {
	BU_ALLOC(bs, struct brep_specific);
	bs->brep = bi->brep;
	bi->brep = NULL;
	stp->st_specific = (genptr_t)bs;
    }

    vbp = rt_vlblock_init();

    brep_command(gedp->ged_result_str, solid_name, (const struct rt_tess_tol *)&gedp->ged_wdbp->wdb_ttol, &gedp->ged_wdbp->wdb_tol, bs, bi, vbp, argc, argv, commtag);

    if (BU_STR_EQUAL(argv[2], "translate")) {
	bi->brep = bs->brep;
	GED_DB_PUT_INTERNAL(gedp, ndp, &intern, &rt_uniresource, GED_ERROR);
    }

    snprintf(namebuf, 64, "%s%s_", commtag, solid_name);
    _ged_cvt_vlblock_to_solids(gedp, vbp, namebuf, 0);
    rt_vlblock_free(vbp);
    vbp = (struct bn_vlblock *)NULL;

    rt_db_free_internal(&intern);

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
