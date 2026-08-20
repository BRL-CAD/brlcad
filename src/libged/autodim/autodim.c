/*                     A U T O D I M . C
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
/** @file libged/autodim/autodim.c
 *
 * The autodim command - automatically compute the overall bounding-box
 * dimensions of an object (or the currently displayed objects) and place
 * an annotation object that labels the overall X/Y/Z extents and draws
 * the three coordinate axes emanating from the model origin.
 *
 * This provides the "automatic calculation and placement" of dimension
 * annotations for hidden-line / wireframe drawings that previously had to
 * be entered by hand: the bounding-box math (rt_obj_bounds, as used by the
 * bb command) is reused, and the resulting extents are written into a
 * standard annot primitive via mk_annot so they draw on the current view.
 *
 */

#include "common.h"

#include <string.h>

#include "bu/cmd.h"
#include "bu/getopt.h"
#include "bu/units.h"
#include "bu/vls.h"
#include "bu/magic.h"
#include "bu/malloc.h"
#include "rt/geom.h"
#include "wdb.h"

#include "../ged_private.h"


/* Build and persist an annotation object describing the overall bounding
 * box of the region [rpp_min, rpp_max] (model units).  Text labels carry
 * the extents in local units (str is the unit name); line segments trace
 * the three coordinate axes from the model origin toward each extent.
 */
static int
autodim_make_annot(struct ged *gedp, const char *name,
		   const point_t rpp_min, const point_t rpp_max,
		   double xlen, double ylen, double zlen, const char *str)
{
    struct rt_wdb *wdbp;
    struct rt_annot_internal *aip;
    struct txt_seg *tx;
    struct txt_seg *ty;
    struct txt_seg *tz;
    struct line_seg *lx;
    struct line_seg *ly;
    struct line_seg *lz;
    int ret;
    fastf_t txtsize;
    point_t mid;

    /* Overall model-space diagonal, used to scale text sensibly. */
    txtsize = DIST_PNT_PNT(rpp_min, rpp_max) * 0.05;
    if (txtsize < SMALL_FASTF)
	txtsize = 1.0;

    BU_ALLOC(aip, struct rt_annot_internal);
    aip->magic = RT_ANNOT_INTERNAL_MAGIC;

    /* Anchor the annotation's 2D coordinate system at the model origin. */
    VSETALL(aip->V, 0.0);

    /* Control points (model units, relative to V):
     *   0: origin
     *   1: +X extent (max X, min Y, min Z)
     *   2: +Y extent (min X, max Y, min Z)
     *   3: +Z extent (min X, min Y, max Z)
     *   4: X label anchor (midpoint of X axis)
     *   5: Y label anchor (midpoint of Y axis)
     *   6: Z label anchor (midpoint of Z axis)
     */
    aip->vert_count = 7;
    aip->verts = (point2d_t *)bu_calloc(aip->vert_count, sizeof(point2d_t), "autodim verts");

    V2SET(aip->verts[0], 0.0, 0.0);
    V2SET(aip->verts[1], rpp_max[X], rpp_min[Y]);
    V2SET(aip->verts[2], rpp_min[X], rpp_max[Y]);
    V2SET(aip->verts[3], rpp_min[X], rpp_min[Y]);

    VADD2SCALE(mid, rpp_min, rpp_max, 0.5);
    V2SET(aip->verts[4], mid[X], rpp_min[Y]);
    V2SET(aip->verts[5], rpp_min[X], mid[Y]);
    V2SET(aip->verts[6], rpp_min[X], rpp_min[Y]);

    /* Six segments: three axis lines + three dimension labels. */
    aip->ant.count = 6;
    aip->ant.reverse = (int *)bu_calloc(aip->ant.count, sizeof(int), "autodim reverse");
    aip->ant.segments = (void **)bu_calloc(aip->ant.count, sizeof(void *), "autodim segments");

    /* X axis line: origin -> +X extent */
    BU_ALLOC(lx, struct line_seg);
    lx->magic = CURVE_LSEG_MAGIC;
    lx->start = 0;
    lx->end = 1;
    aip->ant.segments[0] = (void *)lx;

    /* Y axis line: origin -> +Y extent */
    BU_ALLOC(ly, struct line_seg);
    ly->magic = CURVE_LSEG_MAGIC;
    ly->start = 0;
    ly->end = 2;
    aip->ant.segments[1] = (void *)ly;

    /* Z axis line: origin -> +Z extent */
    BU_ALLOC(lz, struct line_seg);
    lz->magic = CURVE_LSEG_MAGIC;
    lz->start = 0;
    lz->end = 3;
    aip->ant.segments[2] = (void *)lz;

    /* X dimension label */
    BU_ALLOC(tx, struct txt_seg);
    tx->magic = ANN_TSEG_MAGIC;
    tx->ref_pt = 4;
    tx->rel_pos = RT_TXT_POS_BC;
    bu_vls_init(&tx->label);
    bu_vls_sprintf(&tx->label, "X: %g %s", xlen, str);
    tx->txt_size = txtsize;
    tx->txt_rot_angle = 0.0;
    aip->ant.segments[3] = (void *)tx;

    /* Y dimension label */
    BU_ALLOC(ty, struct txt_seg);
    ty->magic = ANN_TSEG_MAGIC;
    ty->ref_pt = 5;
    ty->rel_pos = RT_TXT_POS_ML;
    bu_vls_init(&ty->label);
    bu_vls_sprintf(&ty->label, "Y: %g %s", ylen, str);
    ty->txt_size = txtsize;
    ty->txt_rot_angle = 0.0;
    aip->ant.segments[4] = (void *)ty;

    /* Z dimension label */
    BU_ALLOC(tz, struct txt_seg);
    tz->magic = ANN_TSEG_MAGIC;
    tz->ref_pt = 6;
    tz->rel_pos = RT_TXT_POS_MR;
    bu_vls_init(&tz->label);
    bu_vls_sprintf(&tz->label, "Z: %g %s", zlen, str);
    tz->txt_size = txtsize;
    tz->txt_rot_angle = 0.0;
    aip->ant.segments[5] = (void *)tz;

    wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    ret = mk_annot(wdbp, name, aip);

    /* mk_annot deep-copies the struct; free our temporaries. */
    bu_vls_free(&tx->label);
    bu_vls_free(&ty->label);
    bu_vls_free(&tz->label);
    BU_PUT(tx, struct txt_seg);
    BU_PUT(ty, struct txt_seg);
    BU_PUT(tz, struct txt_seg);
    BU_PUT(lx, struct line_seg);
    BU_PUT(ly, struct line_seg);
    BU_PUT(lz, struct line_seg);
    bu_free(aip->ant.segments, "autodim segments");
    bu_free(aip->ant.reverse, "autodim reverse");
    bu_free(aip->verts, "autodim verts");
    BU_PUT(aip, struct rt_annot_internal);

    return ret;
}


int
ged_autodim_core(struct ged *gedp, int argc, const char *argv[])
{
    point_t rpp_min, rpp_max;
    point_t obj_min, obj_max;
    int c;
    int use_air = 1;
    int i;
    int draw = 1;
    int ret;
    const char *str;
    double xlen, ylen, zlen;
    struct bu_vls annot_name = BU_VLS_INIT_ZERO;
    const char *name = NULL;
    static const char *usage = "[-u] [-n name] [-D] [object1 object2 ...]";

    /* Collected display-list object names when none are supplied. */
    struct bu_vls disp = BU_VLS_INIT_ZERO;
    char **disp_argv = NULL;
    int disp_argc = 0;

    GED_CHECK_DATABASE_OPEN(gedp, BRLCAD_ERROR);
    GED_CHECK_READ_ONLY(gedp, BRLCAD_ERROR);
    GED_CHECK_ARGC_GT_0(gedp, argc, BRLCAD_ERROR);

    /* initialize result */
    bu_vls_trunc(gedp->ged_result_str, 0);

    bu_optind = 1;	/* re-init bu_getopt() */
    while ((c = bu_getopt(argc, (char * const *)argv, "un:D")) != -1) {
	switch (c) {
	    case 'u':
		use_air = 0;
		break;
	    case 'n':
		name = bu_optarg;
		break;
	    case 'D':
		draw = 0;
		break;
	    default:
		bu_vls_printf(gedp->ged_result_str, "Unrecognized option - %c", c);
		return BRLCAD_ERROR;
	}
    }

    /* skip options processed plus command name, should just leave object names */
    argc -= bu_optind;
    argv += bu_optind;

    /* No objects named: fall back to the currently displayed objects,
     * mirroring the ged_dl() walk used by the who command.
     */
    if (argc == 0) {
	struct display_list *gdlp;
	for (BU_LIST_FOR(gdlp, display_list, (struct bu_list *)ged_dl(gedp))) {
	    if (((struct directory *)gdlp->dl_dp)->d_addr == RT_DIR_PHONY_ADDR)
		continue;
	    if (disp_argc)
		bu_vls_putc(&disp, ' ');
	    bu_vls_printf(&disp, "%s", bu_vls_addr(&gdlp->dl_path));
	    disp_argc++;
	}
	if (disp_argc == 0) {
	    bu_vls_printf(gedp->ged_result_str,
			  "Usage: autodim %s\n(no objects specified and none currently displayed)", usage);
	    bu_vls_free(&disp);
	    return GED_HELP;
	}
	disp_argv = (char **)bu_calloc(disp_argc + 1, sizeof(char *), "autodim disp_argv");
	disp_argc = bu_argv_from_string(disp_argv, disp_argc, bu_vls_addr(&disp));
	argc = disp_argc;
	argv = (const char **)disp_argv;
    }

    /* Compute the axis-aligned bounding box, exactly as the bb command does. */
    VSETALL(rpp_min, INFINITY);
    VSETALL(rpp_max, -INFINITY);
    for (i = 0; i < argc; i++) {
	if (rt_obj_bounds(gedp->ged_result_str, gedp->dbip, argc - i, (const char **)argv + i, use_air, obj_min, obj_max) & BRLCAD_ERROR) {
	    if (disp_argv)
		bu_free(disp_argv, "autodim disp_argv");
	    bu_vls_free(&disp);
	    return BRLCAD_ERROR;
	}
	VMINMAX(rpp_min, rpp_max, (double *)obj_min);
	VMINMAX(rpp_min, rpp_max, (double *)obj_max);
    }

    str = bu_units_string(gedp->dbip->dbi_local2base);
    if (!str)
	str = "Unknown_unit";

    xlen = fabs(rpp_max[X] - rpp_min[X]) * gedp->dbip->dbi_base2local;
    ylen = fabs(rpp_max[Y] - rpp_min[Y]) * gedp->dbip->dbi_base2local;
    zlen = fabs(rpp_max[Z] - rpp_min[Z]) * gedp->dbip->dbi_base2local;

    /* Derive an annotation name if the caller did not supply one. */
    if (!name) {
	bu_vls_sprintf(&annot_name, "%s.autodim", argv[0]);
	name = bu_vls_addr(&annot_name);
    }

    if (db_lookup(gedp->dbip, name, LOOKUP_QUIET) != RT_DIR_NULL) {
	bu_vls_printf(gedp->ged_result_str, "Error: object '%s' already exists; use -n to choose another name", name);
	bu_vls_free(&annot_name);
	if (disp_argv)
	    bu_free(disp_argv, "autodim disp_argv");
	bu_vls_free(&disp);
	return BRLCAD_ERROR;
    }

    ret = autodim_make_annot(gedp, name, rpp_min, rpp_max, xlen, ylen, zlen, str);
    if (ret) {
	bu_vls_printf(gedp->ged_result_str, "Error: failed to create annotation '%s'", name);
	bu_vls_free(&annot_name);
	if (disp_argv)
	    bu_free(disp_argv, "autodim disp_argv");
	bu_vls_free(&disp);
	return BRLCAD_ERROR;
    }

    bu_vls_printf(gedp->ged_result_str,
		  "Created annotation '%s': X %g %s, Y %g %s, Z %g %s",
		  name, xlen, str, ylen, str, zlen, str);

    /* Draw the resulting annotation on the current view. */
    if (draw) {
	const char *draw_argv[3];
	draw_argv[0] = "draw";
	draw_argv[1] = name;
	draw_argv[2] = NULL;
	(void)ged_exec(gedp, 2, draw_argv);
    }

    bu_vls_free(&annot_name);
    if (disp_argv)
	bu_free(disp_argv, "autodim disp_argv");
    bu_vls_free(&disp);

    return BRLCAD_OK;
}


#include "../include/plugin.h"

#define GED_AUTODIM_COMMANDS(X, XID) \
    X(autodim, ged_autodim_core, GED_CMD_DEFAULT)

GED_DECLARE_COMMAND_SET(GED_AUTODIM_COMMANDS)
GED_DECLARE_PLUGIN_MANIFEST("libged_autodim", 1, GED_AUTODIM_COMMANDS)

/*
 * Local Variables:
 * mode: C
 * tab-width: 8
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
