/*                         S E L E C T . C
 * BRL-CAD
 *
 * Copyright (c) 2008-2026 United States Government as represented by
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


#include "bu/cmdschema.h"
#include "../ged_private.h"


struct select_args {
    const char *bot_path;
    int partial;
    fastf_t vminz;
};


static const struct bu_cmd_option select_native_options[] = {
    BU_CMD_STRING("b", NULL, struct select_args, bot_path, "bot",
	"Select points from a BOT object or path"),
    BU_CMD_FLAG("p", NULL, struct select_args, partial,
	"Select partial display paths"),
    BU_CMD_NUMBER("z", NULL, struct select_args, vminz, "vminz",
	"Minimum view-space Z coordinate"),
    BU_CMD_OPTION_NULL
};
static const struct bu_cmd_operand select_native_operands[] = {
    BU_CMD_OPERAND("coordinates", BU_CMD_VALUE_NUMBER, 3, 4,
	"View X, Y, and radius or width/height", NULL),
    BU_CMD_OPERAND_NULL
};
GED_EXPORT const struct bu_cmd_schema ged_select_legacy_schema = {
    "select", "Select displayed objects within a view region",
    select_native_options, select_native_operands, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};
GED_EXPORT const struct bu_cmd_schema ged_rselect_legacy_schema = {
    "rselect", "Select displayed objects within the current rubber-band region",
    select_native_options, NULL, BU_CMD_PARSE_OPTIONS_FIRST,
    BU_CMD_SCHEMA_CONSTRAINTS(NULL, NULL)
};


static int
select_import_bot(struct ged *gedp, const char *cmd, const char *bot_path,
	struct rt_wdb *wdbp, struct rt_db_internal *intern,
	struct rt_bot_internal **botip)
{
    mat_t mat;

    if (!bot_path)
	return BRLCAD_OK;
    if (wdb_import_from_path2(gedp->ged_result_str, intern, bot_path, wdbp,
	mat) & BRLCAD_ERROR) {
	bu_vls_printf(gedp->ged_result_str, "%s: failed to find %s", cmd, bot_path);
	return BRLCAD_ERROR;
    }
    if (intern->idb_major_type != DB5_MAJORTYPE_BRLCAD ||
	intern->idb_minor_type != DB5_MINORTYPE_BRLCAD_BOT) {
	bu_vls_printf(gedp->ged_result_str, "%s: %s is not a BOT", cmd, bot_path);
	rt_db_free_internal(intern);
	return BRLCAD_ERROR;
    }

    *botip = (struct rt_bot_internal *)intern->idb_ptr;
    return BRLCAD_OK;
}


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

    double vx, vy, vw, vh, vr;
    static const char *usage = "[-b bot] [-p] [-z vminz] vx vy {vr | vw vh}";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = NULL;
    struct select_args args = {NULL, 0, -1000.0};
    int operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&ged_select_legacy_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }
    argc -= operand_index + 1;
    argv += operand_index + 1;

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (select_import_bot(gedp, cmd, args.bot_path, wdbp, &intern, &botip) & BRLCAD_ERROR)
	return BRLCAD_ERROR;


    if (argc == 3) {
	if (sscanf(argv[0], "%lf", &vx) != 1 ||
	    sscanf(argv[1], "%lf", &vy) != 1 ||
	    sscanf(argv[2], "%lf", &vr) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return BRLCAD_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vr, vr, args.vminz, 1);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (args.partial)
		return dl_select_partial(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vr, vr, 1);
	    else
		return dl_select(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vr, vr, 1);
	}
    } else {
	if (sscanf(argv[0], "%lf", &vx) != 1 ||
	    sscanf(argv[1], "%lf", &vy) != 1 ||
	    sscanf(argv[2], "%lf", &vw) != 1 ||
	    sscanf(argv[3], "%lf", &vh) != 1) {
	    bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	    return BRLCAD_ERROR;
	}

	if (botip != (struct rt_bot_internal *)NULL) {
	    int ret;

	    ret = _ged_select_botpts(gedp, botip, vx, vy, vw, vh, args.vminz, 0);
	    rt_db_free_internal(&intern);

	    return ret;
	} else {
	    if (args.partial)
		return dl_select_partial(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vw, vh, 0);
	    else
		return dl_select(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, vx, vy, vw, vh, 0);
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
    static const char *usage = "[-b bot] [-p] [-z vminz]";
    const char *cmd = argv[0];
    struct rt_db_internal intern;
    struct rt_bot_internal *botip = (struct rt_bot_internal *)NULL;
    struct select_args args = {NULL, 0, -1000.0};
    int operand_index;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_DRAWABLE(gedp, BRLCAD_ERROR);
    GED_CHECK_VIEW(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    operand_index = bu_cmd_schema_parse_complete(&ged_rselect_legacy_schema, &args,
	gedp->ged_result_str, argc - 1, argv + 1);
    if (operand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "Usage: %s %s", cmd, usage);
	return BRLCAD_ERROR;
    }

    struct rt_wdb *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    if (select_import_bot(gedp, cmd, args.bot_path, wdbp, &intern, &botip) & BRLCAD_ERROR)
	return BRLCAD_ERROR;

    if (botip != (struct rt_bot_internal *)NULL) {
	int ret;

	ret = _ged_select_botpts(gedp, botip,
				  gedp->ged_gvp->gv_s->gv_rect.x,
				  gedp->ged_gvp->gv_s->gv_rect.y,
				  gedp->ged_gvp->gv_s->gv_rect.width,
				  gedp->ged_gvp->gv_s->gv_rect.height,
				  args.vminz,
				  0);

	rt_db_free_internal(&intern);
	return ret;
    } else {
	if (args.partial)
	    return dl_select_partial(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, 				       gedp->ged_gvp->gv_s->gv_rect.x,
				     gedp->ged_gvp->gv_s->gv_rect.y,
				     gedp->ged_gvp->gv_s->gv_rect.width,
				     gedp->ged_gvp->gv_s->gv_rect.height,
				     0);
	else
	    return dl_select(gedp->i->ged_gdp->gd_headDisplay, gedp->ged_gvp->gv_model2view, gedp->ged_result_str, 				       gedp->ged_gvp->gv_s->gv_rect.x,
			     gedp->ged_gvp->gv_s->gv_rect.y,
			     gedp->ged_gvp->gv_s->gv_rect.width,
			     gedp->ged_gvp->gv_s->gv_rect.height,
			     0);
    }
}


#include "../include/plugin.h"

extern GED_EXPORT const struct ged_cmd_grammar ged_select_grammar;
extern GED_EXPORT const struct ged_cmd_grammar ged_rselect_grammar;

#define GED_SELECT_COMMANDS(X, XID) \
    X(select, ged_select_core, GED_CMD_DEFAULT, &ged_select_grammar) \
    X(rselect, ged_rselect_core, GED_CMD_DEFAULT, &ged_rselect_grammar) \

GED_DECLARE_COMMAND_SET_WITH_GRAMMAR(GED_SELECT_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST_WITH_GRAMMAR("libged_select", 1, GED_SELECT_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
