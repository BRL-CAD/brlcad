/*                         S E L E C T . C
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
/** @file libged/select.c
 *
 * The select command.
 *
 */

#include "common.h"

#include <string.h>
#include "bio.h"
#include "solid.h"

#include "./ged_private.h"

int
_ged_select(struct ged *gedp, double vx, double vy, double vwidth, double vheight, int rflag)
{
    struct ged_display_list *gdlp = NULL;
    struct ged_display_list *next_gdlp = NULL;
    struct solid *sp = NULL;
    fastf_t vr = 0.0;
    fastf_t vmin_x = 0.0;
    fastf_t vmin_y = 0.0;
    fastf_t vmax_x = 0.0;
    fastf_t vmax_y = 0.0;

    if (rflag) {
	vr = vwidth;
    } else {
	vmin_x = vx;
	vmin_y = vy;

	if (vwidth > 0)
	    vmax_x = vx + vwidth;
	else {
	    vmin_x = vx + vwidth;
	    vmax_x = vx;
	}

	if (vheight > 0)
	    vmax_y = vy + vheight;
	else {
	    vmin_y = vy + vheight;
	    vmax_y = vy;
	}
    }

    gdlp = BU_LIST_NEXT(ged_display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    point_t vmin, vmax;
	    struct bn_vlist *vp;

	    vmax[X] = vmax[Y] = vmax[Z] = -INFINITY;
	    vmin[X] = vmin[Y] = vmin[Z] =  INFINITY;

	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int j;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		point_t vpt;
		for (j = 0; j < nused; j++, cmd++, pt++) {
		    switch (*cmd) {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
			case BN_VLIST_TRI_START:
			case BN_VLIST_TRI_VERTNORM:
			case BN_VLIST_POINT_SIZE:
			case BN_VLIST_LINE_WIDTH:
			    /* attribute, not location */
			    break;
			case BN_VLIST_LINE_MOVE:
			case BN_VLIST_LINE_DRAW:
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
			case BN_VLIST_TRI_MOVE:
			case BN_VLIST_TRI_DRAW:
			case BN_VLIST_TRI_END:
			    MAT4X3PNT(vpt, gedp->ged_gvp->gv_model2view, *pt);
			    V_MIN(vmin[X], vpt[X]);
			    V_MAX(vmax[X], vpt[X]);
			    V_MIN(vmin[Y], vpt[Y]);
			    V_MAX(vmax[Y], vpt[Y]);
			    V_MIN(vmin[Z], vpt[Z]);
			    V_MAX(vmax[Z], vpt[Z]);
			    break;
			default: {
			    bu_vls_printf(gedp->ged_result_str, "unknown vlist op %d\n", *cmd);
			}
		    }
		}
	    }

	    if (rflag) {
		point_t vloc;
		vect_t diff;
		fastf_t mag;

		VSET(vloc, vx, vy, vmin[Z]);
		VSUB2(diff, vmin, vloc);
		mag = MAGNITUDE(diff);

		if (mag > vr)
		    continue;

		VSET(vloc, vx, vy, vmax[Z]);
		VSUB2(diff, vmax, vloc);
		mag = MAGNITUDE(diff);

		if (mag > vr)
		    continue;

		db_path_to_vls(gedp->ged_result_str, &sp->s_fullpath);
		bu_vls_printf(gedp->ged_result_str, "\n");
	    } else {
		if (vmin_x <= vmin[X] && vmax[X] <= vmax_x &&
		    vmin_y <= vmin[Y] && vmax[Y] <= vmax_y) {
		    db_path_to_vls(gedp->ged_result_str, &sp->s_fullpath);
		    bu_vls_printf(gedp->ged_result_str, "\n");
		}
	    }
	}

	gdlp = next_gdlp;
    }

    return GED_OK;
}


int
_ged_select_partial(struct ged *gedp, double vx, double vy, double vwidth, double vheight, int rflag)
{
    struct ged_display_list *gdlp = NULL;
    struct ged_display_list *next_gdlp = NULL;
    struct solid *sp = NULL;
    fastf_t vr = 0.0;
    fastf_t vmin_x = 0.0;
    fastf_t vmin_y = 0.0;
    fastf_t vmax_x = 0.0;
    fastf_t vmax_y = 0.0;

    if (rflag) {
	vr = vwidth;
    } else {
	vmin_x = vx;
	vmin_y = vy;

	if (vwidth > 0)
	    vmax_x = vx + vwidth;
	else {
	    vmin_x = vx + vwidth;
	    vmax_x = vx;
	}

	if (vheight > 0)
	    vmax_y = vy + vheight;
	else {
	    vmin_y = vy + vheight;
	    vmax_y = vy;
	}
    }

    gdlp = BU_LIST_NEXT(ged_display_list, gedp->ged_gdp->gd_headDisplay);
    while (BU_LIST_NOT_HEAD(gdlp, gedp->ged_gdp->gd_headDisplay)) {
	next_gdlp = BU_LIST_PNEXT(ged_display_list, gdlp);

	FOR_ALL_SOLIDS(sp, &gdlp->gdl_headSolid) {
	    struct bn_vlist *vp;

	    for (BU_LIST_FOR(vp, bn_vlist, &(sp->s_vlist))) {
		int j;
		int nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		point_t vpt;
		for (j = 0; j < nused; j++, cmd++, pt++) {
		    switch (*cmd) {
			case BN_VLIST_POLY_START:
			case BN_VLIST_POLY_VERTNORM:
			case BN_VLIST_TRI_START:
			case BN_VLIST_TRI_VERTNORM:
			    /* Has normal vector, not location */
			    break;
			case BN_VLIST_LINE_MOVE:
			case BN_VLIST_LINE_DRAW:
			case BN_VLIST_POLY_MOVE:
			case BN_VLIST_POLY_DRAW:
			case BN_VLIST_POLY_END:
			case BN_VLIST_TRI_MOVE:
			case BN_VLIST_TRI_DRAW:
			case BN_VLIST_TRI_END:
			    MAT4X3PNT(vpt, gedp->ged_gvp->gv_model2view, *pt);

			    if (rflag) {
				point_t vloc;
				vect_t diff;
				fastf_t mag;

				VSET(vloc, vx, vy, vpt[Z]);
				VSUB2(diff, vpt, vloc);
				mag = MAGNITUDE(diff);

				if (mag > vr)
				    continue;

				db_path_to_vls(gedp->ged_result_str, &sp->s_fullpath);
				bu_vls_printf(gedp->ged_result_str, "\n");

				goto solid_done;
			    } else {
				if (vmin_x <= vpt[X] && vpt[X] <= vmax_x &&
				    vmin_y <= vpt[Y] && vpt[Y] <= vmax_y) {
				    db_path_to_vls(gedp->ged_result_str, &sp->s_fullpath);
				    bu_vls_printf(gedp->ged_result_str, "\n");

				    goto solid_done;
				}
			    }

			    break;
			default: {
			    bu_vls_printf(gedp->ged_result_str, "unknown vlist op %d\n", *cmd);
			}
		    }
		}
	    }

	    solid_done:
	    ;
	}

	gdlp = next_gdlp;
    }

    return GED_OK;
}


/*
 * Returns a list of items within the specified rectangle or circle.
 * If bot is specified, the bot points within the specified area are returned.
 *
 * Usage:
 * select [-b bot] [-p] [-z vminz] vx vy {vr | vw vh}
 *
 */
int
ged_select(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    double vx, vy, vw, vh, vr;
    static const char *usage = "[-b bot] [-p] [-z vminz] vx vy {vr | vw vh}";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = NULL;
    int pflag = 0;
    double vminz = -1000.0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Get command line options. */
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "b:pz:")) != -1) {
	switch (c) {
	case 'b':
	{
	    mat_t mat;

	    /* skip subsequent bot specifications */
	    if (botip != (struct rt_bot_internal *)NULL)
		break;

	    if (wdb_import_from_path2(gedp->ged_result_str, &intern, bu_optarg, gedp->ged_wdbp, mat) == GED_ERROR) {
		bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, bu_optarg);
		return GED_ERROR;
	    }

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
		intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT", cmd, bu_optarg);
		rt_db_free_internal(&intern);

		return GED_ERROR;
	    }

	    botip = (struct rt_bot_internal *)intern.idb_ptr;
	}

	break;
	case 'p':
	    pflag = 1;
	    break;
	case 'z':
	    if (sscanf(bu_optarg, "%lf", &vminz) != 1) {
		if (botip != (struct rt_bot_internal *)NULL)
		    rt_db_free_internal(&intern);

		bu_vls_printf(gedp->ged_result_str, "%s: bad vminz - %s", cmd, bu_optarg);
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    }

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    if (argc == 4) {
	if (sscanf(argv[1], "%lf", &vx) != 1 ||
	    sscanf(argv[2], "%lf", &vy) != 1 ||
	    sscanf(argv[3], "%lf", &vr) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vr, vr, vminz, 1);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (pflag)
		return _ged_select_partial(gedp, vx, vy, vr, vr, 1);
	    else
		return _ged_select(gedp, vx, vy, vr, vr, 1);
	}
    } else {
	if (sscanf(argv[1], "%lf", &vx) != 1 ||
	    sscanf(argv[2], "%lf", &vy) != 1 ||
	    sscanf(argv[3], "%lf", &vw) != 1 ||
	    sscanf(argv[4], "%lf", &vh) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vw, vh, vminz, 0);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (pflag)
		return _ged_select_partial(gedp, vx, vy, vw, vh, 0);
	    else
		return _ged_select(gedp, vx, vy, vw, vh, 0);
	}
    }
}


/*
 * Returns a list of items within the previously defined rectangle.
 *
 * Usage:
 * rselect [-b bot] [-p] [-z vminz]
 *
 */
int
ged_rselect(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    static const char *usage = "[-b bot] [-p] [-z vminz]";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = (struct rt_bot_internal *)NULL;
    int pflag = 0;
    double vminz = -1000.0;

    GED_CHECK_DATABASE_OPEN(gedp, GED_ERROR);
    GED_CHECK_DRAWABLE(gedp, GED_ERROR);
    GED_CHECK_VIEW(gedp, GED_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, GED_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    /* Get command line options. */
    bu_optind = 1;
    while ((c = bu_getopt(argc, (char * const *)argv, "b:pz:")) != -1) {
	switch (c) {
	case 'b':
	{
	    mat_t mat;

	    /* skip subsequent bot specifications */
	    if (botip != (struct rt_bot_internal *)NULL)
		break;

	    if (wdb_import_from_path2(gedp->ged_result_str, &intern, bu_optarg, gedp->ged_wdbp, mat) == GED_ERROR) {
		bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, bu_optarg);
		return GED_ERROR;
	    }

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
		intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT", cmd, bu_optarg);
		rt_db_free_internal(&intern);

		return GED_ERROR;
	    }

	    botip = (struct rt_bot_internal *)intern.idb_ptr;
	}

	break;
	case 'p':
	    pflag = 1;
	    break;
	case 'z':
	    if (sscanf(bu_optarg, "%lf", &vminz) != 1) {
		if (botip != (struct rt_bot_internal *)NULL)
		    rt_db_free_internal(&intern);

		bu_vls_printf(gedp->ged_result_str, "%s: bad vminz - %s", cmd, bu_optarg);
		bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    }

	    break;
	default:
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return GED_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return GED_ERROR;
    }

    if (botip != (struct rt_bot_internal *)NULL) {
	int ret;

	ret = _ged_select_botpts(gedp, botip,
				  gedp->ged_gvp->gv_rect.grs_x,
				  gedp->ged_gvp->gv_rect.grs_y,
				  gedp->ged_gvp->gv_rect.grs_width,
				  gedp->ged_gvp->gv_rect.grs_height,
				  vminz,
				  0);

	rt_db_free_internal(&intern);
	return ret;
    } else {
	if (pflag)
	    return _ged_select_partial(gedp,
				       gedp->ged_gvp->gv_rect.grs_x,
				       gedp->ged_gvp->gv_rect.grs_y,
				       gedp->ged_gvp->gv_rect.grs_width,
				       gedp->ged_gvp->gv_rect.grs_height,
				       0);
	else
	    return _ged_select(gedp,
			       gedp->ged_gvp->gv_rect.grs_x,
			       gedp->ged_gvp->gv_rect.grs_y,
			       gedp->ged_gvp->gv_rect.grs_width,
			       gedp->ged_gvp->gv_rect.grs_height,
			       0);
    }
}

struct rt_object_selections *
ged_get_object_selections(struct ged *gedp, const char *object_name)
{
    int new;
    struct bu_hash_entry *entry;

    entry = bu_hash_tbl_add(gedp->ged_selections, (unsigned char *)object_name,
	    strlen(object_name), &new);

    return (struct rt_object_selections *)bu_get_hash_value(entry);
}

struct rt_selection_set *
ged_get_selection_set(struct ged *gedp, const char *object_name, const char *selection_name)
{
    struct rt_object_selections *obj_selections;
    struct bu_hash_entry *entry;
    int new;

    obj_selections = ged_get_object_selections(gedp, object_name);
    entry = bu_hash_tbl_add(obj_selections->sets,
		(const unsigned char *)selection_name, strlen(selection_name), &new);

    return (struct rt_selection_set *)bu_get_hash_value(entry);
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
