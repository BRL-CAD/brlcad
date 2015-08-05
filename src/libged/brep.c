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

#include "bu.h"

#include "raytrace.h"
#include "rt/geom.h"
#include "wdb.h"

#include "./ged_private.h"
#include "dplot_reader.h"

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
RT_EXPORT extern int rt_brep_boolean(struct rt_db_internal *out, const struct rt_db_internal *ip1, const struct rt_db_internal *ip2, db_op_t operation);

static int
selection_command(
	struct ged *gedp,
	struct rt_db_internal *ip,
	int argc,
	const char *argv[])
{
    int i;
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
	    bu_log("wrong args for selection append");
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
	    bu_log("no matching selections");
	    return -1;
	}

	/* could be multiple options, just grabbing the first and
	 * freeing the rest
	 */
	selections = &selection_set->selections;
	new_selection = (struct rt_selection *)BU_PTBL_GET(selections, 0);

	free_selection = selection_set->free_selection;
	for (i = BU_PTBL_LEN(selections) - 1; i > 0; --i) {
	    long *s = BU_PTBL_GET(selections, i);
	    free_selection((struct rt_selection *)s);
	    bu_ptbl_rm(selections, s);
	}
	bu_ptbl_free(selections);
	BU_FREE(selection_set, struct rt_selection_set);

	/* get existing/new selections set in gedp */
	selection_set = ged_get_selection_set(gedp, solid_name, selection_name);
	selection_set->free_selection = free_selection;
	selections = &selection_set->selections;

	/* TODO: Need to implement append by passing new and
	 * existing selection to an rt_brep_evaluate_selection.
	 * For now, new selection simply replaces old one.
	 */
	for (i = BU_PTBL_LEN(selections) - 1; i >= 0; --i) {
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

	for (i = 0; i < (int)BU_PTBL_LEN(selections); ++i) {
	    int ret;
	    operation.type = RT_SELECTION_TRANSLATION;
	    operation.parameters.tran.dx = atof(argv[5]);
	    operation.parameters.tran.dy = atof(argv[6]);
	    operation.parameters.tran.dz = atof(argv[7]);

	    ret = ip->idb_meth->ft_process_selection(ip, gedp->ged_wdbp->dbip,
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
    const char *commands[] = {"info", "plot", "translate", "intersect", "csg", "u", "i", "-"};
    int num_commands = (int)(sizeof(commands) / sizeof(const char *));
    db_op_t op = DB_OP_NULL;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* must be wanting help */
    if (argc < 2) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s\n\t%s\n", argv[0], usage);
	bu_vls_printf(gedp->ged_result_str, "commands:\n");
	bu_vls_printf(gedp->ged_result_str, "\tvalid          - report on validity of specific BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo           - return count information for specific BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo S [index] - return information for specific BREP 'surface'\n");
	bu_vls_printf(gedp->ged_result_str, "\tinfo F [index] - return information for specific BREP 'face'\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot           - plot entire BREP\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot S [index] - plot specific BREP 'surface'\n");
	bu_vls_printf(gedp->ged_result_str, "\tplot F [index] - plot specific BREP 'face'\n");
	bu_vls_printf(gedp->ged_result_str, "\tcsg            - convert BREP to implicit primitive CSG tree\n");
	bu_vls_printf(gedp->ged_result_str, "\ttranslate SCV index i j dx dy dz - translate a surface control vertex\n");
	bu_vls_printf(gedp->ged_result_str, "\tintersect <obj2> <i> <j> [PP|PC|PS|CC|CS|SS] - BREP intersections\n");
	bu_vls_printf(gedp->ged_result_str, "\tu|i|- <obj2> <output>     - BREP boolean evaluations\n");
	bu_vls_printf(gedp->ged_result_str, "\t[brepname]                - convert the non-BREP object to BREP form\n");
	bu_vls_printf(gedp->ged_result_str, "\t --no-evaluation [suffix] - convert non-BREP comb to unevaluated BREP form\n");
	return GED_HELP;
    }

    if (argc < 2 || argc > 11) {
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

    if (BU_STR_EQUAL(argv[2], "valid")) {
	int valid = rt_brep_valid(&intern, gedp->ged_result_str);
	return (valid) ? GED_OK : GED_ERROR;
    }

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
	bn_vlblock_free(vbp);
	vbp = (struct bn_vlblock *)NULL;

	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "csg")) {
	/* Call csg conversion routine */
	struct bu_vls bname_csg;
	bu_vls_init(&bname_csg);
	bu_vls_sprintf(&bname_csg, "csg_%s", solid_name);
	if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_csg), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_csg));
	    bu_vls_free(&bname_csg);
	    return GED_OK;
	}
	bu_vls_free(&bname_csg);
	return _ged_brep_to_csg(gedp, argv[1], 0);
    }

    if (BU_STR_EQUAL(argv[2], "csgv")) {
	/* Call csg conversion routine */
	struct bu_vls bname_csg;
	bu_vls_init(&bname_csg);
	bu_vls_sprintf(&bname_csg, "csg_%s", solid_name);
	if (db_lookup(gedp->ged_wdbp->dbip, bu_vls_addr(&bname_csg), LOOKUP_QUIET) != RT_DIR_NULL) {
	    bu_vls_printf(gedp->ged_result_str, "%s already exists.", bu_vls_addr(&bname_csg));
	    bu_vls_free(&bname_csg);
	    return GED_OK;
	}
	bu_vls_free(&bname_csg);
	return _ged_brep_to_csg(gedp, argv[1], 1);
    }

    /* make sure arg isn't --no-evaluate */
    if (argc > 2 && argv[2][1] != '-') {
	op = db_str2op(argv[2]);
    }

    if (op != DB_OP_NULL) {
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

	rt_brep_boolean(&intern_res, &intern, &intern2, op);
	bip = (struct rt_brep_internal*)intern_res.idb_ptr;
	mk_brep(gedp->ged_wdbp, argv[4], bip->brep);
	rt_db_free_internal(&intern);
	rt_db_free_internal(&intern2);
	rt_db_free_internal(&intern_res);
	return GED_OK;
    }

    if (BU_STR_EQUAL(argv[2], "selection")) {
	ret = selection_command(gedp, &intern, argc, argv);
	if (BU_STR_EQUAL(argv[3], "translate") && ret == 0) {
	    GED_DB_PUT_INTERNAL(gedp, ndp, &intern, &rt_uniresource, GED_ERROR);
	}
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
	    if (ret == -1) {
		bu_vls_printf(gedp->ged_result_str, "%s doesn't have a "
			"brep-conversion function yet. Type: %s", solid_name,
			intern.idb_meth->ft_label);
	    } else if (ret == -2) {
		bu_vls_printf(gedp->ged_result_str, "%s cannot be converted "
			"to brep correctly.", solid_name);
	    } else {
		brep = ((struct rt_brep_internal *)brep_db_internal.idb_ptr)->brep;
		ret = mk_brep(gedp->ged_wdbp, bu_vls_addr(&bname), brep);
		if (ret == 0) {
		    bu_vls_printf(gedp->ged_result_str, "%s is made.", bu_vls_addr(&bname));
		}
		rt_db_free_internal(&brep_db_internal);
	    }
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
	stp->st_specific = (void *)bs;
    }

    vbp = rt_vlblock_init();

    brep_command(gedp->ged_result_str, solid_name, (const struct rt_tess_tol *)&gedp->ged_wdbp->wdb_ttol, &gedp->ged_wdbp->wdb_tol, bs, bi, vbp, argc, argv, commtag);

    if (BU_STR_EQUAL(argv[2], "translate")) {
	bi->brep = bs->brep;
	GED_DB_PUT_INTERNAL(gedp, ndp, &intern, &rt_uniresource, GED_ERROR);
    }

    snprintf(namebuf, 64, "%s%s_", commtag, solid_name);
    _ged_cvt_vlblock_to_solids(gedp, vbp, namebuf, 0);
    bn_vlblock_free(vbp);
    vbp = (struct bn_vlblock *)NULL;

    rt_db_free_internal(&intern);

    return GED_OK;
}

enum {
    DPLOT_INITIAL,
    DPLOT_SSX_FIRST,
    DPLOT_SSX,
    DPLOT_SSX_EVENTS,
    DPLOT_ISOCSX_FIRST,
    DPLOT_ISOCSX,
    DPLOT_ISOCSX_EVENTS,
    DPLOT_FACE_CURVES,
    DPLOT_LINKED_CURVES,
    DPLOT_SPLIT_FACES
};

struct dplot_info {
    struct dplot_data fdata;
    int mode;
    struct ged *gedp;
    FILE *logfile;
    char *prefix;
    int ssx_idx;
    int isocsx_idx;
    int brep1_surf_idx;
    int brep2_surf_idx;
    int brep1_surf_count;
    int brep2_surf_count;
    int event_idx;
    int event_count;
    int brep1_isocsx_count;
    int isocsx_count;
};

#define CLEANUP \
    fclose(info.logfile); \
    bu_free(info.prefix, "prefix"); \
    info.prefix = NULL; \
    if (info.fdata.ssx) bu_free(info.fdata.ssx, "ssx array");

#define RETURN_MORE \
    CLEANUP \
    return GED_MORE;

#define RETURN_ERROR \
    CLEANUP \
    info.mode = DPLOT_INITIAL; \
    return GED_ERROR;

HIDDEN int
dplot_overlay(
    struct ged *gedp,
    const char *prefix,
    const char *infix,
    int idx,
    const char *name)
{
    const char *cmd_av[] = {"overlay", "[filename]", "1.0", "[name]"};
    int ret, cmd_ac = sizeof(cmd_av) / sizeof(char *);
    struct bu_vls overlay_name = BU_VLS_INIT_ZERO;

    bu_vls_printf(&overlay_name, "%s%s%d.plot3", prefix, infix, idx);
    cmd_av[1] = cmd_av[3] = bu_vls_cstr(&overlay_name);
    if (name) {
	cmd_av[3] = name;
    }
    ret = ged_overlay(gedp, cmd_ac, cmd_av);
    bu_vls_free(&overlay_name);

    if (ret != GED_OK) {
	bu_vls_printf(gedp->ged_result_str, "error overlaying plot\n");
	return GED_ERROR;
    }
    return GED_OK;
}

HIDDEN int
dplot_erase_overlay(
    struct dplot_info *info,
    const char *name)
{
    const int NUM_EMPTY_PLOTS = 13;
    int i;

    /* We can't actually erase the old plot without its real name,
     * which is unknown. Instead, we'll write a plot with the same
     * base name and color, which will overwrite the old one. We
     * don't actually know the color either, so we resort to writing
     * an empty plot with the given name using every color we created
     * plots with.
     */
    for (i = 0; i < NUM_EMPTY_PLOTS; ++i) {
	int ret = dplot_overlay(info->gedp, info->prefix, "_empty", i, name);
	if (ret != GED_OK) {
	    return ret;
	}
    }
    return GED_OK;
}

HIDDEN int
dplot_ssx(
    struct dplot_info *info)
{
    int i, ret;

    /* draw surfaces, skipping intersecting surfaces if in SSI mode */
    /* TODO: need to name these overlays so I can selectively erase them */
    for (i = 0; i < info->brep1_surf_count; ++i) {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	bu_vls_printf(&name, "%s_brep1_surface%d", info->prefix, i);
	dplot_erase_overlay(info, bu_vls_cstr(&name));
	bu_vls_free(&name);
    }
    for (i = 0; i < info->brep2_surf_count; ++i) {
	struct bu_vls name = BU_VLS_INIT_ZERO;
	bu_vls_printf(&name, "%s_brep2_surface%d", info->prefix, i);
	dplot_erase_overlay(info, bu_vls_cstr(&name));
	bu_vls_free(&name);
    }
    if (info->mode == DPLOT_SSX_FIRST || info->mode == DPLOT_SSX || info->mode == DPLOT_SSX_EVENTS) {
	for (i = 0; i < info->brep1_surf_count; ++i) {
	    if (info->mode != DPLOT_SSX_FIRST && i == info->brep1_surf_idx) {
		continue;
	    }
	    ret = dplot_overlay(info->gedp, info->prefix, "_brep1_surface", i, NULL);
	    if (ret != GED_OK) {
		return GED_ERROR;
	    }
	}
	for (i = 0; i < info->brep2_surf_count; ++i) {
	    if (info->mode != DPLOT_SSX_FIRST && i == info->brep2_surf_idx) {
		continue;
	    }
	    ret = dplot_overlay(info->gedp, info->prefix, "_brep2_surface", i, NULL);
	    if (ret != GED_OK) {
		return GED_ERROR;
	    }
	}
    }

    /* draw highlighted surface-surface intersection pair */
    if (info->mode == DPLOT_SSX || info->mode == DPLOT_SSX_EVENTS) {
	ret = dplot_overlay(info->gedp, info->prefix, "_highlight_brep1_surface",
		info->brep1_surf_idx, "dplot_ssx1");
	if (ret != GED_OK) {
	    return GED_ERROR;
	}
	ret = dplot_overlay(info->gedp, info->prefix, "_highlight_brep2_surface",
		info->brep2_surf_idx, "dplot_ssx2");
	if (ret != GED_OK) {
	    return GED_ERROR;
	}
	if (info->mode == DPLOT_SSX) {
	    /* advance past the completed pair */
	    ++info->ssx_idx;
	}
    }
    if (info->mode == DPLOT_SSX_FIRST) {
	info->mode = DPLOT_SSX;
    }
    if (info->mode == DPLOT_SSX && info->ssx_idx < info->fdata.ssx_count) {
	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show surface-"
		"surface intersection %d", info->ssx_idx);
	return GED_MORE;
    }
    return GED_OK;
}

void
dplot_print_event_legend(struct dplot_info *info)
{
    bu_vls_printf(info->gedp->ged_result_str, "yellow = transverse\n");
    bu_vls_printf(info->gedp->ged_result_str, "white  = tangent\n");
    bu_vls_printf(info->gedp->ged_result_str, "green  = overlap\n");
}

HIDDEN int
dplot_ssx_events(
    struct dplot_info *info)
{
    int ret;

    /* erase old event plots */
    ret = dplot_erase_overlay(info, "curr_event");
    if (ret != GED_OK) {
	return ret;
    }

    if (info->mode != DPLOT_SSX_EVENTS) {
	return GED_OK;
    }

    if (info->event_count > 0) {
	/* convert event ssx_idx to string */
	struct bu_vls infix = BU_VLS_INIT_ZERO;
	bu_vls_printf(&infix, "_ssx%d_event", info->ssx_idx);

	/* plot overlay */
	ret = dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&infix),
		info->event_idx, "curr_event");
	bu_vls_free(&infix);

	if (ret != GED_OK) {
	    return ret;
	}
	if (info->event_idx == 0) {
	    dplot_print_event_legend(info);
	}
    }
    /* advance to next event, or return to initial state */
    if (++info->event_idx < info->event_count) {
	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show next event\n");
	return GED_MORE;
    }
    info->mode = DPLOT_INITIAL;
    return GED_OK;
}

HIDDEN int
dplot_isocsx(
    struct dplot_info *info)
{
    if (info->mode != DPLOT_ISOCSX &&
	info->mode != DPLOT_ISOCSX_FIRST &&
	info->mode != DPLOT_ISOCSX_EVENTS)
    {
	return GED_OK;
    }

    if (info->fdata.ssx[info->ssx_idx].isocsx_events == NULL) {
	bu_vls_printf(info->gedp->ged_result_str, "The isocurves of neither "
		"surface intersected the opposing surface in surface-surface"
		" intersection %d.\n", info->ssx_idx);
	info->mode = DPLOT_INITIAL;
	return GED_OK;
    }

    dplot_overlay(info->gedp, info->prefix, "_brep1_surface",
	    info->brep1_surf_idx, "isocsx_b1");
    dplot_overlay(info->gedp, info->prefix, "_brep2_surface",
	    info->brep2_surf_idx, "isocsx_b2");

    if (info->mode == DPLOT_ISOCSX) {
	struct bu_vls infix = BU_VLS_INIT_ZERO;
	/* plot surface and the isocurve that intersects it */
	if (info->isocsx_idx < info->brep1_isocsx_count) {
	    dplot_overlay(info->gedp, info->prefix, "_highlight_brep2_surface",
		    info->brep2_surf_idx, "isocsx_b2");
	} else {
	    dplot_overlay(info->gedp, info->prefix, "_highlight_brep1_surface",
		    info->brep1_surf_idx, "isocsx_b1");
	}
	bu_vls_printf(&infix, "_highlight_ssx%d_isocurve", info->ssx_idx);
	dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&infix), info->isocsx_idx, "isocsx_isocurve");
	bu_vls_free(&infix);
    }

    if (info->mode == DPLOT_ISOCSX_FIRST ||
	info->mode == DPLOT_ISOCSX)
    {
	if (info->mode == DPLOT_ISOCSX_FIRST ||
	    ++info->isocsx_idx < info->isocsx_count)
	{
	    bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show "
		    "isocurve-surface intersection %d", info->isocsx_idx);
	    info->mode = DPLOT_ISOCSX;
	    return GED_MORE;
	} else {
	    info->mode = DPLOT_INITIAL;
	}
    }
    return GED_OK;
}

HIDDEN int
dplot_isocsx_events(struct dplot_info *info)
{
    int ret;

    if (info->mode != DPLOT_ISOCSX_EVENTS) {
	return GED_OK;
    }
    ret = dplot_erase_overlay(info, "curr_event");
    if (ret != GED_OK) {
	return ret;
    }
    if (info->event_count > 0) {
	/* convert event ssx_idx to string */
	struct bu_vls infix = BU_VLS_INIT_ZERO;
	bu_vls_printf(&infix, "_ssx%d_isocsx%d_event", info->ssx_idx,
		info->isocsx_idx);

	/* plot overlay */
	ret = dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&infix),
		info->event_idx, "curr_event");
	bu_vls_free(&infix);

	if (ret != GED_OK) {
	    bu_vls_printf(info->gedp->ged_result_str,
		    "error overlaying plot\n");
	    return ret;
	}
	if (info->event_idx == 0) {
	    dplot_print_event_legend(info);
	}
    }
    /* advance to next event, or return to initial state */
    if (++info->event_idx < info->event_count) {
	bu_vls_printf(info->gedp->ged_result_str,
		"Press [Enter] to show next event\n");
	return GED_MORE;
    }

    info->mode = DPLOT_INITIAL;
    return GED_OK;
}

HIDDEN int
dplot_face_curves(struct dplot_info *info)
{
    int f1_curves, f2_curves;
    if (info->mode != DPLOT_FACE_CURVES) {
	return GED_OK;
    }

    f1_curves = info->fdata.ssx[info->ssx_idx].face1_clipped_curves;
    f2_curves = info->fdata.ssx[info->ssx_idx].face2_clipped_curves;
    info->event_count = f1_curves + f2_curves;

    if (info->event_count == 0) {
	bu_vls_printf(info->gedp->ged_result_str, "No clipped curves for ssx"
		" pair %d.\n", info->ssx_idx);
	return GED_OK;
    }

    if (info->event_idx < info->event_count) {
	struct bu_vls prefix;

	dplot_overlay(info->gedp, info->prefix, "_brep1_surface",
		info->brep1_surf_idx, "face_b1");
	dplot_overlay(info->gedp, info->prefix, "_brep2_surface",
		info->brep2_surf_idx, "face_b2");

	BU_VLS_INIT(&prefix);
	bu_vls_printf(&prefix, "%s_ssx%d", info->prefix, info->ssx_idx);
	dplot_erase_overlay(info, "clipped_fcurve");
	if (info->event_idx < f1_curves) {
	    bu_vls_printf(&prefix, "_brep1face_clipped_curve");
	    dplot_overlay(info->gedp, bu_vls_cstr(&prefix), "",
		    info->event_idx, "clipped_fcurve");
	} else {
	    bu_vls_printf(&prefix, "_brep2face_clipped_curve");
	    dplot_overlay(info->gedp, bu_vls_cstr(&prefix), "",
		    info->event_idx - f1_curves, "clipped_fcurve");
	}
	++info->event_idx;
	if (info->event_idx < info->event_count) {
	    bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show the"
		    " next curve.");
	    return GED_MORE;
	}
    }

    info->mode = DPLOT_INITIAL;
    return GED_OK;
}

HIDDEN int
dplot_split_faces(
    struct dplot_info *info)
{
    int i, j;
    struct bu_vls name = BU_VLS_INIT_ZERO;
    struct bu_vls short_name = BU_VLS_INIT_ZERO;
    struct split_face split_face;

    if (info->mode != DPLOT_SPLIT_FACES) {
	return GED_OK;
    }

    if (info->event_idx >= info->fdata.split_face_count) {
	for (i = 0; i < info->fdata.split_face_count; ++i) {
	    split_face = info->fdata.face[i];

	    bu_vls_trunc(&name, 0);
	    bu_vls_printf(&name, "_split_face%d_outerloop_curve", i);
	    for (j = 0; j < split_face.outerloop_curves; ++j) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sf%do%d", i, j);
		dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), j,
			bu_vls_cstr(&short_name));
	    }

	    bu_vls_trunc(&name, 0);
	    bu_vls_printf(&name, "_split_face%d_innerloop_curve", i);
	    for (j = 0; j < split_face.innerloop_curves; ++j) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sf%di%d", i, j);
		dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), j,
			bu_vls_cstr(&short_name));
	    }
	}
    } else {
	if (info->event_idx > 0) {
	    /* erase curves of previous split face */
	    split_face = info->fdata.face[info->event_idx - 1];
	    for (i = 0; i < split_face.outerloop_curves; ++i) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sfo%d", i);
		dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    }
	    for (i = 0; i < split_face.innerloop_curves; ++i) {
		bu_vls_trunc(&short_name, 0);
		bu_vls_printf(&short_name, "sfi%d", i);
		dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    }
	}

	split_face = info->fdata.face[info->event_idx];
	bu_vls_printf(&name, "_split_face%d_outerloop_curve", info->event_idx);
	for (i = 0; i < info->fdata.face[info->event_idx].outerloop_curves; ++i) {
	    bu_vls_trunc(&short_name, 0);
	    bu_vls_printf(&short_name, "sfo%d", i);

	    dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), i,
		    bu_vls_cstr(&short_name));
	}

	bu_vls_trunc(&name, 0);
	bu_vls_printf(&name, "_split_face%d_innerloop_curve", info->event_idx);
	for (i = 0; i < info->fdata.face[info->event_idx].innerloop_curves; ++i) {
	    bu_vls_trunc(&short_name, 0);
	    bu_vls_printf(&short_name, "sfi%d", i);

	    dplot_erase_overlay(info, bu_vls_cstr(&short_name));
	    dplot_overlay(info->gedp, info->prefix, bu_vls_cstr(&name), i,
		    bu_vls_cstr(&short_name));
	}

	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show "
		"split face %d", ++info->event_idx);
	return GED_MORE;
    }
    return GED_OK;
}

HIDDEN int
dplot_linked_curves(
    struct dplot_info *info)
{
    int i;
    if (info->mode != DPLOT_LINKED_CURVES) {
	return GED_OK;
    }

    if (info->event_idx >= info->fdata.linked_curve_count) {
	for (i = 0; i < info->fdata.linked_curve_count; ++i) {
	    dplot_overlay(info->gedp, info->prefix, "_linked_curve", i, NULL);
	}
    } else {
	dplot_overlay(info->gedp, info->prefix, "_linked_curve",
		info->event_idx, "linked_curve");
	bu_vls_printf(info->gedp->ged_result_str, "Press [Enter] to show "
		"linked curve %d", ++info->event_idx);
	return GED_MORE;
    }
    return GED_OK;
}

HIDDEN void *
dplot_malloc(size_t s) {
    return bu_malloc(s, "dplot_malloc");
}

HIDDEN void
dplot_free(void *p) {
    bu_free(p, "dplot_free");
}

HIDDEN void
dplot_load_file_data(struct dplot_info *info)
{
    int i, j;
    struct ssx *curr_ssx;
    struct isocsx *curr_isocsx;
    int token_id;
    perplex_t scanner;
    void *parser;

    /* initialize scanner and parser */
    parser = ParseAlloc(dplot_malloc);
    scanner = perplexFileScanner(info->logfile);

    info->fdata.brep1_surface_count = info->fdata.brep2_surface_count = 0;
    info->fdata.ssx_count = 0;
    BU_LIST_INIT(&info->fdata.ssx_list);
    BU_LIST_INIT(&info->fdata.isocsx_list);
    perplexSetExtra(scanner, (void *)&info->fdata);

    /* parse */
    while ((token_id = yylex(scanner)) != YYEOF) {
	Parse(parser, token_id, info->fdata.token_data, &info->fdata);
    }
    Parse(parser, 0, info->fdata.token_data, &info->fdata);

    /* clean up */
    ParseFree(parser, dplot_free);
    perplexFree(scanner);

    /* move ssx to dynamic array for easy access */
    info->fdata.ssx = NULL;
    if (info->fdata.ssx_count > 0) {

	info->fdata.ssx = (struct ssx *)bu_malloc(
		sizeof(struct ssx) * info->fdata.ssx_count, "ssx array");

	i = info->fdata.ssx_count - 1;
	while (BU_LIST_WHILE(curr_ssx, ssx, &info->fdata.ssx_list)) {
	    BU_LIST_DEQUEUE(&curr_ssx->l);

	    curr_ssx->isocsx_events = NULL;
	    if (curr_ssx->intersecting_isocurves > 0) {
		curr_ssx->isocsx_events = (int *)bu_malloc(sizeof(int) *
			curr_ssx->intersecting_isocurves, "isocsx array");

		j = curr_ssx->intersecting_isocurves - 1;
		while (BU_LIST_WHILE(curr_isocsx, isocsx, &curr_ssx->isocsx_list)) {
		    BU_LIST_DEQUEUE(&curr_isocsx->l);
		    curr_ssx->isocsx_events[j--] = curr_isocsx->events;
		    BU_PUT(curr_isocsx, struct isocsx);
		}
	    }
	    info->fdata.ssx[i--] = *curr_ssx;
	    BU_PUT(curr_ssx, struct ssx);
	}
    }
}

int
ged_dplot(struct ged *gedp, int argc, const char *argv[])
{
    static struct dplot_info info;
    int ret;
    const char *filename, *cmd;
    char *dot;

    info.gedp = gedp;
    bu_vls_trunc(gedp->ged_result_str, 0);

    if (argc < 3) {
	bu_vls_printf(gedp->ged_result_str, "usage: %s logfile cmd\n",
		argv[0]);
	bu_vls_printf(gedp->ged_result_str, "  where cmd is one of:\n");
	bu_vls_printf(gedp->ged_result_str,
		"      ssx     (show intersecting surface pairs)\n");
	bu_vls_printf(gedp->ged_result_str,
		"      ssx N   (show intersections of ssx pair N)\n");
	bu_vls_printf(gedp->ged_result_str,
		"   isocsx N   (show intersecting isocurve-surface pairs of ssx pair N)\n");
	bu_vls_printf(gedp->ged_result_str,
		"   isocsx N M (show intersections of ssx pair N, isocsx pair M)\n");
	bu_vls_printf(gedp->ged_result_str,
		"  fcurves N   (show clipped face curves of ssx pair N\n");
	bu_vls_printf(gedp->ged_result_str,
		"  lcurves     (show linked ssx curves used to split faces\n");
	bu_vls_printf(gedp->ged_result_str,
		"    faces     (show split faces used to construct result\n");
	return GED_HELP;
    }
    filename = argv[1];
    cmd = argv[2];

    if (info.mode == DPLOT_INITIAL) {
	if (BU_STR_EQUAL(cmd, "ssx") && argc == 3) {
	    info.mode = DPLOT_SSX_FIRST;
	    info.ssx_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "ssx") && argc == 4) {
	    /* parse surface pair index */
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return GED_ERROR;
	    }
	    info.mode = DPLOT_SSX_EVENTS;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "isocsx") && argc == 4) {
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return GED_ERROR;
	    }
	    info.mode = DPLOT_ISOCSX_FIRST;
	    info.isocsx_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "isocsx") && argc == 5) {
	    /* parse surface pair index */
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return GED_ERROR;
	    }
	    idx_str = argv[4];
	    ret = bu_sscanf(idx_str, "%d", &info.isocsx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"isocurve-surface pair (must be a non-negative integer)\n", idx_str);
		return GED_ERROR;
	    }
	    info.mode = DPLOT_ISOCSX_EVENTS;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "fcurves") && argc == 4) {
	    const char *idx_str = argv[3];
	    ret = bu_sscanf(idx_str, "%d", &info.ssx_idx);
	    if (ret != 1) {
		bu_vls_printf(gedp->ged_result_str, "%s is not a valid "
			"surface pair (must be a non-negative integer)\n", idx_str);
		return GED_ERROR;
	    }
	    info.mode = DPLOT_FACE_CURVES;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "lcurves") && argc == 3) {
	    info.mode = DPLOT_LINKED_CURVES;
	    info.event_idx = 0;
	} else if (BU_STR_EQUAL(cmd, "faces") && argc == 3) {
	    info.mode = DPLOT_SPLIT_FACES;
	    info.event_idx = 0;
	} else {
	    bu_vls_printf(gedp->ged_result_str, "%s is not a recognized "
		    "command or was given the wrong number of arguments\n",
		    cmd);
	    return GED_ERROR;
	}
    }

    /* open dplot log file */
    info.logfile = fopen(filename, "r");
    if (!info.logfile) {
	bu_vls_printf(gedp->ged_result_str, "couldn't open log file \"%s\"\n", filename);
	return GED_ERROR;
    }

    /* filename before '.' is assumed to be the prefix for all
     * plot-file names
     */
    info.prefix = bu_strdup(filename);
    dot = strchr(info.prefix, '.');
    if (dot) {
	*dot = '\0';
    }

    dplot_load_file_data(&info);

    if (info.mode == DPLOT_SSX_FIRST	||
	info.mode == DPLOT_SSX		||
	info.mode == DPLOT_SSX_EVENTS	||
	info.mode == DPLOT_ISOCSX_FIRST	||
	info.mode == DPLOT_ISOCSX	||
	info.mode == DPLOT_ISOCSX_EVENTS)
    {
	if (info.fdata.ssx_count == 0) {
	    bu_vls_printf(info.gedp->ged_result_str, "no surface surface"
		    "intersections");
	    RETURN_ERROR;
	} else if (info.ssx_idx < 0 ||
		   info.ssx_idx > (info.fdata.ssx_count - 1))
	{
	    bu_vls_printf(info.gedp->ged_result_str, "no surface pair %d (valid"
		    " range is [0, %d])\n", info.ssx_idx, info.fdata.ssx_count - 1);
	    RETURN_ERROR;
	}
    }
    if (info.fdata.ssx_count > 0) {
	info.brep1_surf_idx = info.fdata.ssx[info.ssx_idx].brep1_surface;
	info.brep2_surf_idx = info.fdata.ssx[info.ssx_idx].brep2_surface;
	info.event_count = info.fdata.ssx[info.ssx_idx].final_curve_events;
	info.brep1_isocsx_count =
	    info.fdata.ssx[info.ssx_idx].intersecting_brep1_isocurves;
	info.isocsx_count =
	    info.fdata.ssx[info.ssx_idx].intersecting_isocurves;
    }
    info.brep1_surf_count = info.fdata.brep1_surface_count;
    info.brep2_surf_count = info.fdata.brep2_surface_count;

    if (info.mode == DPLOT_ISOCSX_EVENTS) {
	int *isocsx_events = info.fdata.ssx[info.ssx_idx].isocsx_events;

	info.event_count = 0;
	if (isocsx_events) {
	    info.event_count = isocsx_events[info.event_idx];
	}
    }

    ret = dplot_ssx(&info);
    if (ret == GED_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_ssx_events(&info);
    if (ret == GED_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_isocsx(&info);
    if (ret == GED_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_isocsx_events(&info);
    if (ret == GED_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_face_curves(&info);
    if (ret == GED_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_split_faces(&info);
    if (ret == GED_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    ret = dplot_linked_curves(&info);
    if (ret == GED_ERROR) {
	RETURN_ERROR;
    } else if (ret == GED_MORE) {
	RETURN_MORE;
    }

    info.mode = DPLOT_INITIAL;
    CLEANUP;

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
