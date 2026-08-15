/*                        G R I D . C
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
/** @file libged/view/faceplate/grid.c
 *
 * Commands for HUD grid overlay
 *
 */

#include "common.h"

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include "bu/cmd.h"
#include "bu/cmdschema.h"
#include "bu/color.h"
#include "bu/vls.h"
#include "bv.h"

#include "../../ged_private.h"
#include "../ged_view.h"
#include "./faceplate.h"

struct _ged_fp_grid_info {
    struct _ged_view_info *gd;
    struct bv_grid_state *g;
};

int
_fp_grid_cmd_draw(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->draw);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, "grid", "draw");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0]) || (val != 0 && val != 1)) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    g->draw = val;

    return BRLCAD_OK;
}

int
_fp_grid_cmd_snap(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->snap);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, "grid", "snap");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0]) || (val != 0 && val != 1)) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    g->snap = val;

    return BRLCAD_OK;
}

int
_fp_grid_cmd_anchor(void *bs, int argc, const char **argv)
{
    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
     if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%g %g %g\n", V3ARGS(g->anchor));
	return BRLCAD_OK;
    }

    if (argc != 1 && argc != 3) {
	_fp_cmd_schema_help(gedp, "grid", "anchor");
	return BRLCAD_ERROR;
    }
    vect_t val;
    int ret = bu_cmd_vector3_from_argv(val, (size_t)argc, (const char * const *)argv);
    if (ret != 1 && ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid specification\n");
	return BRLCAD_ERROR;
    }

    VMOVE(g->anchor, val);

    return BRLCAD_OK;
}

int
_fp_grid_cmd_res_h(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%g\n", g->res_h);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, "grid", "res_h");
	return BRLCAD_ERROR;
    }
    fastf_t val;
    if (!bu_cmd_number_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < BN_TOL_DIST) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is %f\n", BN_TOL_DIST);
	return BRLCAD_ERROR;
    }

    g->res_h = val;

    return BRLCAD_OK;
}

int
_fp_grid_cmd_res_v(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%g\n", g->res_v);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, "grid", "res_v");
	return BRLCAD_ERROR;
    }
    fastf_t val;
    if (!bu_cmd_number_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < BN_TOL_DIST) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is %f\n", BN_TOL_DIST);
	return BRLCAD_ERROR;
    }

    g->res_v = val;

    return BRLCAD_OK;
}

int
_fp_grid_cmd_res_major_h(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->res_major_h);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, "grid", "res_major_h");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return BRLCAD_ERROR;
    }

    g->res_major_h = val;

    return BRLCAD_OK;
}

int
_fp_grid_cmd_res_major_v(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d\n", g->res_major_v);
	return BRLCAD_OK;
    }

    if (argc != 1) {
	_fp_cmd_schema_help(gedp, "grid", "res_major_v");
	return BRLCAD_ERROR;
    }
    int val;
    if (!bu_cmd_integer_from_str(&val, argv[0])) {
	bu_vls_printf(gedp->ged_result_str, "Invalid argument %s\n", argv[0]);
	return BRLCAD_ERROR;
    }

    if (val < 1) {
	bu_vls_printf(gedp->ged_result_str, "Smallest supported value is 1\n");
	return BRLCAD_ERROR;
    }

    g->res_major_v = val;

    return BRLCAD_OK;
}

int
_fp_grid_cmd_color(void *bs, int argc, const char **argv)
{

    struct _ged_fp_grid_info *ginfo = (struct _ged_fp_grid_info *)bs;
    struct _ged_view_info *gd = ginfo->gd;
    struct ged *gedp = gd->gedp;
    if (_fp_cmd_schema_msgs(gedp, argc, argv, "grid"))
	return BRLCAD_OK;

    argc--; argv++;

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    struct bv_grid_state *g = ginfo->g;
    if (argc == 0) {
	bu_vls_printf(gedp->ged_result_str, "%d %d %d\n", g->color[0], g->color[1], g->color[2]);
	return BRLCAD_OK;
    }

    // For color need either 1 or 3 non-subcommand args
    if (argc != 1 && argc != 3) {
	_fp_cmd_schema_help(gedp, "grid", "color");
	return BRLCAD_ERROR;
    }

    struct bu_color c;
    int opt_ret = bu_cmd_color_from_argv(&c, (size_t)argc, (const char * const *)argv);
    if (opt_ret != 1 && opt_ret != 3) {
	bu_vls_printf(gedp->ged_result_str, "Invalid color specifier\n");
	return BRLCAD_ERROR;
    }

    bu_color_to_rgb_ints(&c, &g->color[0], &g->color[1], &g->color[2]);

    return BRLCAD_OK;
}

const struct bu_cmdtab _fp_grid_cmds[] = {
    { "draw",        _fp_grid_cmd_draw},
    { "snap",        _fp_grid_cmd_snap},
    { "anchor",      _fp_grid_cmd_anchor},
    { "res_h",       _fp_grid_cmd_res_h},
    { "res_v",       _fp_grid_cmd_res_v},
    { "res_major_h", _fp_grid_cmd_res_major_h},
    { "res_major_v", _fp_grid_cmd_res_major_v},
    { "color",       _fp_grid_cmd_color},
    { (char *)NULL,      NULL}
};

int
_fp_cmd_grid(void *bs, int argc, const char **argv)
{
    struct ged_faceplate_subcommand_args args = {0};
    struct _ged_view_info *gd = (struct _ged_view_info *)bs;
    struct ged *gedp = gd->gedp;
    struct bview *v = gedp->ged_gvp;

    if (_fp_cmd_schema_msgs(gedp, argc, argv, NULL))
	return BRLCAD_OK;

    if (!gedp->ged_gvp) {
	bu_vls_printf(gedp->ged_result_str, ": no view current in GED");
	return BRLCAD_ERROR;
    }

    // We know we're the grid command - start processing args
    argc--; argv++;

    if (argc == 1) {
	if (BU_STR_EQUAL("1", argv[0])) {
	    v->gv_s->gv_grid.draw = 1;
	    return BRLCAD_OK;
	}
	if (BU_STR_EQUAL("0", argv[0])) {
	    v->gv_s->gv_grid.draw = 0;
	    return BRLCAD_OK;
	}
    }

    struct bu_vls parse_msgs = BU_VLS_INIT_ZERO;
    int subcommand_index = bu_cmd_schema_parse(&ged_faceplate_subcommand_schema,
	&args, &parse_msgs, argc, argv);
    if (subcommand_index < 0) {
	bu_vls_printf(gedp->ged_result_str, "%s", bu_vls_addr(&parse_msgs));
	bu_vls_free(&parse_msgs);
	return BRLCAD_ERROR;
    }
    bu_vls_free(&parse_msgs);
    argc -= subcommand_index;
    argv += subcommand_index;

    struct _ged_fp_grid_info ginfo;
    ginfo.gd = gd;
    ginfo.g = &v->gv_s->gv_grid;

    return _fp_subcmd_exec(gedp, &ged_faceplate_subcommand_schema,
	_fp_grid_cmds, "view faceplate grid", "[options] subcommand [args]",
	(void *)&ginfo, argc, argv, args.help);
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
