/*                         S E L E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2025 United States Government as represented by
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


#include "bu/getopt.h"
#include "../ged_private.h"

static int
_ged_select_botpts(struct ged *gedp, struct rt_bot_internal *botip, double vx, double vy, double vwidth, double vheight, double vminz, int rflag)
{
    size_t i;
    fastf_t vr = 0.0;
    fastf_t vmin_x = 0.0;
    fastf_t vmin_y = 0.0;
    fastf_t vmax_x = 0.0;
    fastf_t vmax_y = 0.0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);

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

    if (rflag) {
	for (i = 0; i < botip->num_vertices; i++) {
	    point_t vloc;
	    point_t vpt;
	    vect_t diff;
	    fastf_t mag;

	    MAT4X3PNT(vpt, gedp->ged_gvp->gv_model2view, &botip->vertices[i*3]);

	    if (vpt[Z] < vminz)
		continue;

	    VSET(vloc, vx, vy, vpt[Z]);
	    VSUB2(diff, vpt, vloc);
	    mag = MAGNITUDE(diff);

	    if (mag > vr)
		continue;

	    bu_vls_printf(gedp->ged_result_str, "%zu ", i);
	}
    } else {
	for (i = 0; i < botip->num_vertices; i++) {
	    point_t vpt;

	    MAT4X3PNT(vpt, gedp->ged_gvp->gv_model2view, &botip->vertices[i*3]);

	    if (vpt[Z] < vminz)
		continue;

	    if (vmin_x <= vpt[X] && vpt[X] <= vmax_x &&
		vmin_y <= vpt[Y] && vpt[Y] <= vmax_y) {
		bu_vls_printf(gedp->ged_result_str, "%zu ", i);
	    }
	}
    }

    return BRLCAD_OK;
}



int
dl_select(struct bu_list *hdlp, mat_t model2view, struct bu_vls *vls, double vx, double vy, double vwidth, double vheight, int rflag)
{
    struct display_list *gdlp = NULL;
    struct display_list *next_gdlp = NULL;
    struct bv_scene_obj *sp = NULL;
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

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    point_t vmin, vmax;
	    struct bv_vlist *vp;

	    vmax[X] = vmax[Y] = vmax[Z] = -INFINITY;
	    vmin[X] = vmin[Y] = vmin[Z] =  INFINITY;

	    for (BU_LIST_FOR(vp, bv_vlist, &(sp->s_vlist))) {
		size_t j;
		size_t nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		point_t vpt;
		for (j = 0; j < nused; j++, cmd++, pt++) {
		    switch (*cmd) {
			case BV_VLIST_POLY_START:
			case BV_VLIST_POLY_VERTNORM:
			case BV_VLIST_TRI_START:
			case BV_VLIST_TRI_VERTNORM:
			case BV_VLIST_POINT_SIZE:
			case BV_VLIST_LINE_WIDTH:
			    /* attribute, not location */
			    break;
			case BV_VLIST_LINE_MOVE:
			case BV_VLIST_LINE_DRAW:
			case BV_VLIST_POLY_MOVE:
			case BV_VLIST_POLY_DRAW:
			case BV_VLIST_POLY_END:
			case BV_VLIST_TRI_MOVE:
			case BV_VLIST_TRI_DRAW:
			case BV_VLIST_TRI_END:
			    MAT4X3PNT(vpt, model2view, *pt);
			    V_MIN(vmin[X], vpt[X]);
			    V_MAX(vmax[X], vpt[X]);
			    V_MIN(vmin[Y], vpt[Y]);
			    V_MAX(vmax[Y], vpt[Y]);
			    V_MIN(vmin[Z], vpt[Z]);
			    V_MAX(vmax[Z], vpt[Z]);
			    break;
			default: {
			    bu_vls_printf(vls, "unknown vlist op %d\n", *cmd);
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

		db_path_to_vls(vls, &bdata->s_fullpath);
		bu_vls_printf(vls, "\n");
	    } else {
		if (vmin_x <= vmin[X] && vmax[X] <= vmax_x &&
		    vmin_y <= vmin[Y] && vmax[Y] <= vmax_y) {
		    db_path_to_vls(vls, &bdata->s_fullpath);
		    bu_vls_printf(vls, "\n");
		}
	    }
	}

        gdlp = next_gdlp;
    }

    return BRLCAD_OK;
}


int
dl_select_partial(struct bu_list *hdlp, mat_t model2view, struct bu_vls *vls, double vx, double vy, double vwidth, double vheight, int rflag)
{
    struct display_list *gdlp = NULL;
    struct display_list *next_gdlp = NULL;
    struct bv_scene_obj *sp = NULL;
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

    gdlp = BU_LIST_NEXT(display_list, hdlp);
    while (BU_LIST_NOT_HEAD(gdlp, hdlp)) {
        next_gdlp = BU_LIST_PNEXT(display_list, gdlp);

	for (BU_LIST_FOR(sp, bv_scene_obj, &gdlp->dl_head_scene_obj)) {
	    if (!sp->s_u_data)
		continue;
	    struct ged_bv_data *bdata = (struct ged_bv_data *)sp->s_u_data;

	    struct bv_vlist *vp;

	    for (BU_LIST_FOR(vp, bv_vlist, &(sp->s_vlist))) {
		size_t j;
		size_t nused = vp->nused;
		int *cmd = vp->cmd;
		point_t *pt = vp->pt;
		point_t vpt;
		for (j = 0; j < nused; j++, cmd++, pt++) {
		    switch (*cmd) {
			case BV_VLIST_POLY_START:
			case BV_VLIST_POLY_VERTNORM:
			case BV_VLIST_TRI_START:
			case BV_VLIST_TRI_VERTNORM:
			    /* Has normal vector, not location */
			    break;
			case BV_VLIST_LINE_MOVE:
			case BV_VLIST_LINE_DRAW:
			case BV_VLIST_POLY_MOVE:
			case BV_VLIST_POLY_DRAW:
			case BV_VLIST_POLY_END:
			case BV_VLIST_TRI_MOVE:
			case BV_VLIST_TRI_DRAW:
			case BV_VLIST_TRI_END:
			    MAT4X3PNT(vpt, model2view, *pt);

			    if (rflag) {
				point_t vloc;
				vect_t diff;
				fastf_t mag;

				VSET(vloc, vx, vy, vpt[Z]);
				VSUB2(diff, vpt, vloc);
				mag = MAGNITUDE(diff);

				if (mag > vr)
				    continue;

				db_path_to_vls(vls, &bdata->s_fullpath);
				bu_vls_printf(vls, "\n");

				goto solid_done;
			    } else {
				if (vmin_x <= vpt[X] && vpt[X] <= vmax_x &&
				    vmin_y <= vpt[Y] && vpt[Y] <= vmax_y) {
				    db_path_to_vls(vls, &bdata->s_fullpath);
				    bu_vls_printf(vls, "\n");

				    goto solid_done;
				}
			    }

			    break;
			default: {
			    bu_vls_printf(vls, "unknown vlist op %d\n", *cmd);
			}
		    }
		}
	    }

	solid_done:
	    ;
	}

        gdlp = next_gdlp;
    }

    return BRLCAD_OK;
}



/*
 * Returns a list of items within the specified rectangle or circle.
 * If bot is specified, the bot points within the specified area are returned.
 *
 * Usage:
 * select [-b bot] [-p] [-z vminz] vx vy {vr | vw vh}
 *
 */
extern int ged_select2_core(struct ged *gedp, int argc, const char *argv[]);
int
ged_select_core(struct ged *gedp, int argc, const char *argv[])
{
    if (gedp->new_cmd_forms)
	return ged_select2_core(gedp, argc, argv);

    int c;
    double vx, vy, vw, vh, vr;
    static const char *usage = "[-b bot] [-p] [-z vminz] vx vy {vr | vw vh}";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = NULL;
    int pflag = 0;
    double vminz = -1000.0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
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

	    if (wdb_import_from_path2(gedp->ged_result_str, &intern, bu_optarg, wdbp, mat) & BRLCAD_ERROR) {
		bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, bu_optarg);
		return BRLCAD_ERROR;
	    }

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
		intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT", cmd, bu_optarg);
		rt_db_free_internal(&intern);

		return BRLCAD_ERROR;
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
	    return BRLCAD_ERROR;
	}
    }

    argc -= (bu_optind - 1);
    argv += (bu_optind - 1);

    if (argc < 4 || 5 < argc) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    if (argc == 4) {
	if (sscanf(argv[1], "%lf", &vx) != 1 ||
	    sscanf(argv[2], "%lf", &vy) != 1 ||
	    sscanf(argv[3], "%lf", &vr) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return BRLCAD_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vr, vr, vminz, 1);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (pflag)
		return dl_select_partial(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vr, vr, 1);
	    else
		return dl_select(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vr, vr, 1);
	}
    } else {
	if (sscanf(argv[1], "%lf", &vx) != 1 ||
	    sscanf(argv[2], "%lf", &vy) != 1 ||
	    sscanf(argv[3], "%lf", &vw) != 1 ||
	    sscanf(argv[4], "%lf", &vh) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return BRLCAD_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vw, vh, vminz, 0);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (pflag)
		return dl_select_partial(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vw, vh, 0);
	    else
		return dl_select(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vw, vh, 0);
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
ged_rselect_core(struct ged *gedp, int argc, const char *argv[])
{
    int c;
    static const char *usage = "[-b bot] [-p] [-z vminz]";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = (struct rt_bot_internal *)NULL;
    int pflag = 0;
    double vminz = -1000.0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);

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

	    if (wdb_import_from_path2(gedp->ged_result_str, &intern, bu_optarg, wdbp, mat) == BRLCAD_ERROR) {
		bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, bu_optarg);
		return BRLCAD_ERROR;
	    }

	    if (intern.idb_major_type != DB5_MAJORTYPE_BRLCAD ||
		intern.idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
		bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT", cmd, bu_optarg);
		rt_db_free_internal(&intern);

		return BRLCAD_ERROR;
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
	    return BRLCAD_ERROR;
	}
    }

    argc -= (bu_optind - 1);

    if (argc != 1) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    if (botip != (struct rt_bot_internal *)NULL) {
	int ret;

	ret = _ged_select_botpts(gedp, botip,
				  gedp->ged_gvp->gv_s->gv_rect.x,
				  gedp->ged_gvp->gv_s->gv_rect.y,
				  gedp->ged_gvp->gv_s->gv_rect.width,
				  gedp->ged_gvp->gv_s->gv_rect.height,
				  vminz,
				  0);

	rt_db_free_internal(&intern);
	return ret;
    } else {
	if (pflag)
	    return dl_select_partial(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, 				       gedp->ged_gvp->gv_s->gv_rect.x,
				     gedp->ged_gvp->gv_s->gv_rect.y,
				     gedp->ged_gvp->gv_s->gv_rect.width,
				     gedp->ged_gvp->gv_s->gv_rect.height,
				     0);
	else
	    return dl_select(gedp->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, 				       gedp->ged_gvp->gv_s->gv_rect.x,
			     gedp->ged_gvp->gv_s->gv_rect.y,
			     gedp->ged_gvp->gv_s->gv_rect.width,
			     gedp->ged_gvp->gv_s->gv_rect.height,
			     0);
    }
}

#ifdef GED_PLUGIN
#include "../include/plugin.h"
struct ged_cmd_impl select_cmd_impl = {"select", ged_select_core, GED_CMD_DEFAULT};
const struct ged_cmd select_cmd = { &select_cmd_impl };

struct ged_cmd_impl rselect_cmd_impl = {"rselect", ged_rselect_core, GED_CMD_DEFAULT};
const struct ged_cmd rselect_cmd = { &rselect_cmd_impl };

const struct ged_cmd *select_cmds[] = { &select_cmd, &rselect_cmd, NULL };

static const struct ged_plugin pinfo = { GED_API,  select_cmds, 2 };

COMPILER_DLLEXPORT const struct ged_plugin *ged_plugin_info(void)
{
    return &pinfo;
}
#endif /* GED_PLUGIN */

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
