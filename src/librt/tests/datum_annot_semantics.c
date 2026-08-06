/*              D A T U M _ A N N O T _ S E M A N T I C S . C
 * BRL-CAD
 *
 * Copyright (c) 2026 United States Government as represented by
 * the U.S. Army Research Laboratory.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 2.1 as published by the Free Software Foundation.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/file.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bv/vlist.h"
#include "raytrace.h"
#include "rt/geom.h"
#include "rt/primitives/annot.h"
#include "rt/primitives/datum.h"
#include "wdb.h"


static void
check_vector(const char *name, const fastf_t *actual, const fastf_t *expected)
{
    if (!VNEAR_EQUAL(actual, expected, SMALL_FASTF))
	bu_exit(1, "%s mismatch: (%g %g %g), expected (%g %g %g)\n",
	    name, V3ARGS(actual), V3ARGS(expected));
}


static struct directory *
lookup(struct db_i *dbip, const char *name)
{
    struct directory *dp = db_lookup(dbip, name, LOOKUP_QUIET);
    if (dp == RT_DIR_NULL)
	bu_exit(1, "unable to find %s\n", name);
    return dp;
}


int
main(int argc, char **argv)
{
    char path[MAXPATHLEN];
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct rt_datum_internal datum = {0};
    struct rt_annot_internal annot = {0};
    struct line_seg line = {0};
    struct txt_seg text = {0};
    struct rt_db_internal intern;
    struct directory *dp;
    mat_t transform = MAT_INIT_IDN;
    vect_t expected;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;

    {
	FILE *fp = bu_temp_file(path, MAXPATHLEN);
	if (!fp)
	    bu_exit(1, "unable to create temporary database path\n");
	fclose(fp);
    }
    dbip = db_create(path, 5);
    if (dbip == DBI_NULL)
	bu_exit(1, "unable to create temporary database\n");
    wdbp = wdb_dbopen(dbip, RT_WDB_TYPE_DB_DISK);
    if (wdbp == RT_WDB_NULL)
	bu_exit(1, "unable to open temporary database for writing\n");

    datum.magic = RT_DATUM_INTERNAL_MAGIC;
    datum.type = RT_DATUM_FRAME;
    datum.role = RT_DATUM_ROLE_REFERENCE_FRAME;
    VSET(datum.pnt, 1.0, 2.0, 3.0);
    VSET(datum.dir, 0.0, 0.0, 4.0);
    VSET(datum.xdir, 2.0, 0.0, 0.0);
    VSET(datum.ydir, 0.0, 3.0, 0.0);
    datum.dimensions[0] = 5.0;
    datum.dimensions[1] = 6.0;
    datum.identifier = (char *)"A";
    datum.description = (char *)"primary frame";
    if (rt_datum_validate(&datum, NULL) ||
	    mk_datums(wdbp, "frame.datum", &datum) < 0)
	bu_exit(1, "unable to write enhanced datum\n");

    annot.magic = RT_ANNOT_INTERNAL_MAGIC;
    VSET(annot.V, 10.0, 20.0, 30.0);
    annot.flags = RT_ANNOT_MODEL_SPACE;
    VSET(annot.u_vec, 0.0, 2.0, 0.0);
    VSET(annot.v_vec, 0.0, 0.0, 3.0);
    annot.vert_count = 2;
    annot.verts = (point2d_t *)bu_calloc(2, sizeof(point2d_t), "vertices");
    V2SET(annot.verts[0], 0.0, 0.0);
    V2SET(annot.verts[1], 4.0, 5.0);
    annot.ant.count = 2;
    annot.ant.reverse = (int *)bu_calloc(2, sizeof(int), "reverse");
    annot.ant.segments = (void **)bu_calloc(2, sizeof(void *), "segments");
    line.magic = CURVE_LSEG_MAGIC;
    line.start = 0;
    line.end = 1;
    annot.ant.segments[0] = &line;
    text.magic = ANN_TSEG_MAGIC;
    text.ref_pt = 0;
    text.rel_pos = RT_TXT_POS_BL;
    text.txt_size = 3.0;
    text.txt_rot_angle = 15.0;
    bu_vls_init(&text.label);
    bu_vls_strcpy(&text.label, "A \xe2\x8c\x80 25");
    annot.ant.segments[1] = &text;
    annot.styles = (struct rt_annot_seg_style *)bu_calloc(2,
	sizeof(struct rt_annot_seg_style), "styles");
    annot.styles[0].role = RT_ANNOT_ROLE_LEADER;
    annot.styles[0].flags = RT_ANNOT_STYLE_WIDTH | RT_ANNOT_STYLE_COLOR;
    annot.styles[0].line_pattern = RT_ANNOT_LINE_DASHED;
    annot.styles[0].line_width = 2.5;
    annot.styles[0].color[0] = 12;
    annot.styles[0].color[1] = 34;
    annot.styles[0].color[2] = 56;
    annot.styles[0].color[3] = 255;
    annot.styles[0].font = (char *)"ProFont.ttf";
    annot.styles[0].symbol = (char *)"leader";
    annot.styles[1].role = RT_ANNOT_ROLE_TEXT;
    annot.styles[1].font = (char *)"osifont";
    if (rt_annot_validate(&annot, NULL) ||
	    mk_annot(wdbp, "leader.annot", &annot) < 0)
	bu_exit(1, "unable to write enhanced annotation\n");

    dp = lookup(dbip, "frame.datum");
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) != ID_DATUM)
	bu_exit(1, "unable to reload enhanced datum\n");
    {
	struct rt_datum_internal *loaded =
	    (struct rt_datum_internal *)intern.idb_ptr;
	if (loaded->type != RT_DATUM_FRAME ||
		loaded->role != RT_DATUM_ROLE_REFERENCE_FRAME ||
		!loaded->identifier || !BU_STR_EQUAL(loaded->identifier, "A") ||
		!loaded->description ||
		!BU_STR_EQUAL(loaded->description, "primary frame") ||
		!NEAR_EQUAL(loaded->dimensions[0], 5.0, SMALL_FASTF) ||
		!NEAR_EQUAL(loaded->dimensions[1], 6.0, SMALL_FASTF))
	    bu_exit(1, "enhanced datum fields did not round trip\n");
    }
    rt_db_free_internal(&intern);

    MAT_DELTAS(transform, 100.0, 200.0, 300.0);
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, transform) != ID_DATUM)
	bu_exit(1, "unable to transform enhanced datum\n");
    {
	struct rt_datum_internal *loaded =
	    (struct rt_datum_internal *)intern.idb_ptr;
	VSET(expected, 101.0, 202.0, 303.0);
	check_vector("datum anchor", loaded->pnt, expected);
	VSET(expected, 0.0, 0.0, 4.0);
	check_vector("datum Z direction", loaded->dir, expected);
	VSET(expected, 2.0, 0.0, 0.0);
	check_vector("datum X direction", loaded->xdir, expected);
	VSET(expected, 0.0, 3.0, 0.0);
	check_vector("datum Y direction", loaded->ydir, expected);
    }
    rt_db_free_internal(&intern);

    dp = lookup(dbip, "leader.annot");
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, NULL) != ID_ANNOT)
	bu_exit(1, "unable to reload enhanced annotation\n");
    {
	struct rt_annot_internal *loaded =
	    (struct rt_annot_internal *)intern.idb_ptr;
	point_t original_anchor;
	struct bu_list vhead;
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	if (!(loaded->flags & RT_ANNOT_MODEL_SPACE) || !loaded->styles ||
		loaded->styles[0].role != RT_ANNOT_ROLE_LEADER ||
		loaded->styles[0].line_pattern != RT_ANNOT_LINE_DASHED ||
		!loaded->styles[0].font ||
		!BU_STR_EQUAL(loaded->styles[0].font, "ProFont.ttf") ||
		!loaded->styles[0].symbol ||
		!BU_STR_EQUAL(loaded->styles[0].symbol, "leader") ||
		loaded->styles[1].role != RT_ANNOT_ROLE_TEXT ||
		!loaded->styles[1].font ||
		!BU_STR_EQUAL(loaded->styles[1].font, "osifont"))
	    bu_exit(1, "enhanced annotation fields did not round trip\n");
	VMOVE(original_anchor, loaded->V);
	BU_LIST_INIT(&vhead);
	if (OBJ[ID_ANNOT].ft_plot(&vhead, &intern, &ttol, &tol, NULL) < 0)
	    bu_exit(1, "unable to plot model-space annotation\n");
	if (bv_vlist_cmd_cnt((struct bv_vlist *)&vhead) < 20)
	    bu_exit(1, "annotation font outlines were not plotted\n");
	{
	    point_t bmin, bmax;
	    VSET(bmin, INFINITY, INFINITY, INFINITY);
	    VSET(bmax, -INFINITY, -INFINITY, -INFINITY);
	    (void)bv_vlist_bbox(&vhead, &bmin, &bmax, NULL, NULL);
	    if (!NEAR_EQUAL(bmin[X], 10.0, SMALL_FASTF) ||
		    !NEAR_EQUAL(bmax[X], 10.0, SMALL_FASTF) ||
		    bmax[Y] <= bmin[Y] || bmax[Z] <= bmin[Z])
		bu_exit(1, "annotation outlines left their model-space plane\n");
	}
	check_vector("annotation plot anchor", loaded->V, original_anchor);
	BV_FREE_VLIST(&rt_vlfree, &vhead);
    }
    rt_db_free_internal(&intern);

    MAT_DELTAS(transform, -5.0, 6.0, 7.0);
    RT_DB_INTERNAL_INIT(&intern);
    if (rt_db_get_internal(&intern, dp, dbip, transform) != ID_ANNOT)
	bu_exit(1, "unable to transform enhanced annotation\n");
    {
	struct rt_annot_internal *loaded =
	    (struct rt_annot_internal *)intern.idb_ptr;
	VSET(expected, 5.0, 26.0, 37.0);
	check_vector("annotation anchor", loaded->V, expected);
	VSET(expected, 0.0, 2.0, 0.0);
	check_vector("annotation U direction", loaded->u_vec, expected);
	VSET(expected, 0.0, 0.0, 3.0);
	check_vector("annotation V direction", loaded->v_vec, expected);
    }
    rt_db_free_internal(&intern);

    bu_free(annot.styles, "styles");
    bu_free(annot.ant.segments, "segments");
    bu_free(annot.ant.reverse, "reverse");
    bu_free(annot.verts, "vertices");
    bu_vls_free(&text.label);
    db_close(dbip);
    bu_file_delete(path);
    return 0;
}
