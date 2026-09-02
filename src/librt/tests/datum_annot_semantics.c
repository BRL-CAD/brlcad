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
#include <string.h>

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

enum { LAYER_TEST_MAX_SEGMENTS = 2 };


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


static void
write_layer_annotation(struct rt_wdb *wdbp, const char *name,
    fastf_t anchor_x, uint32_t space, size_t segment_count,
    const unsigned char colors[][4])
{
    struct rt_annot_internal annot = {0};
    point2d_t vertices[2];
    struct line_seg lines[LAYER_TEST_MAX_SEGMENTS] = {{0}};
    void *segments[LAYER_TEST_MAX_SEGMENTS] = {NULL, NULL};
    int reverse[LAYER_TEST_MAX_SEGMENTS] = {0, 0};
    struct rt_annot_seg_style styles[LAYER_TEST_MAX_SEGMENTS] = {{0}};
    size_t i;

    if (!wdbp || !name || !colors || segment_count < 1 ||
	    segment_count > LAYER_TEST_MAX_SEGMENTS)
	bu_exit(1, "invalid ordered-layer test annotation\n");
    annot.magic = RT_ANNOT_INTERNAL_MAGIC;
    VSET(annot.V, anchor_x, 0.0, 0.0);
    annot.flags = space;
    VSET(annot.u_vec, 0.0, 1.0, 0.0);
    VSET(annot.v_vec, 0.0, 0.0, 1.0);
    annot.vert_count = 2;
    annot.verts = vertices;
    V2SET(vertices[0], -1.0, 0.0);
    V2SET(vertices[1], 1.0, 0.0);
    annot.ant.count = segment_count;
    annot.ant.reverse = reverse;
    annot.ant.segments = segments;
    annot.styles = styles;
    for (i = 0; i < segment_count; ++i) {
	lines[i].magic = CURVE_LSEG_MAGIC;
	lines[i].start = 0;
	lines[i].end = 1;
	segments[i] = &lines[i];
	styles[i].flags = RT_ANNOT_STYLE_COLOR;
	styles[i].line_pattern = RT_ANNOT_LINE_CONTINUOUS;
	memcpy(styles[i].color, colors[i], sizeof(styles[i].color));
    }
    if (rt_annot_validate(&annot, NULL) || mk_annot(wdbp, name, &annot) < 0)
	bu_exit(1, "unable to write ordered-layer test annotation\n");
}


int
main(int argc, char **argv)
{
    char path[MAXPATHLEN];
    char osifont_path[MAXPATHLEN];
    struct db_i *dbip;
    struct rt_wdb *wdbp;
    struct rt_datum_internal datum = {0};
    struct rt_annot_internal annot = {0};
    struct line_seg line = {0};
    struct line_seg fill_lines[8] = {{0}};
    struct txt_seg text = {0};
    struct fill_seg fill = {0};
    int fill_loop_ends[2] = {4, 8};
    int fill_points[8] = {2, 3, 4, 5, 6, 7, 8, 9};
    struct rt_db_internal intern;
    struct directory *dp;
    mat_t transform = MAT_INIT_IDN;
    vect_t expected;

    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    if (!bu_dir(osifont_path, sizeof(osifont_path), BU_DIR_DATA, "fonts",
	    "osifont-lgpl3fe.ttf", (const char *)NULL) ||
	    !bu_file_exists(osifont_path, NULL))
	bu_exit(1, "installed OSIFont test data could not be resolved\n");

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
    annot.vert_count = 10;
    annot.verts = (point2d_t *)bu_calloc(10, sizeof(point2d_t), "vertices");
    V2SET(annot.verts[0], 0.0, 0.0);
    V2SET(annot.verts[1], 4.0, 5.0);
    V2SET(annot.verts[2], -5.0, -5.0);
    V2SET(annot.verts[3],  5.0, -5.0);
    V2SET(annot.verts[4],  5.0,  5.0);
    V2SET(annot.verts[5], -5.0,  5.0);
    V2SET(annot.verts[6], -2.0, -2.0);
    V2SET(annot.verts[7], -2.0,  2.0);
    V2SET(annot.verts[8],  2.0,  2.0);
    V2SET(annot.verts[9],  2.0, -2.0);
    annot.ant.count = 11;
    annot.ant.reverse = (int *)bu_calloc(11, sizeof(int), "reverse");
    annot.ant.segments = (void **)bu_calloc(11, sizeof(void *), "segments");
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
    {
	int i;
	for (i = 0; i < 8; ++i) {
	    fill_lines[i].magic = CURVE_LSEG_MAGIC;
	    fill_lines[i].start = fill_points[i];
	    fill_lines[i].end = fill_points[(i == 3) ? 0 :
		((i == 7) ? 4 : i + 1)];
	    annot.ant.segments[2 + i] = &fill_lines[i];
	}
    }
    fill.magic = ANN_FSEG_MAGIC;
    fill.loop_count = 2;
    fill.point_count = 8;
    fill.loop_ends = fill_loop_ends;
    fill.points = fill_points;
    fill.legacy_start = 2;
    fill.legacy_count = 8;
    annot.ant.segments[10] = &fill;
    annot.styles = (struct rt_annot_seg_style *)bu_calloc(11,
	sizeof(struct rt_annot_seg_style), "styles");
    annot.styles[0].role = RT_ANNOT_ROLE_CENTERMARK;
    annot.styles[0].flags = RT_ANNOT_STYLE_WIDTH | RT_ANNOT_STYLE_COLOR;
    annot.styles[0].line_pattern = RT_ANNOT_LINE_DASHED;
    annot.styles[0].line_width = 2.5;
    annot.styles[0].color[0] = 12;
    annot.styles[0].color[1] = 34;
    annot.styles[0].color[2] = 56;
    annot.styles[0].color[3] = 255;
    annot.styles[0].font = (char *)"ProFont.ttf";
    annot.styles[0].symbol = (char *)"centermark";
    annot.styles[1].role = RT_ANNOT_ROLE_TEXT;
    annot.styles[1].font = (char *)"osifont";
	annot.styles[1].flags = RT_ANNOT_STYLE_SCALE |
	    RT_ANNOT_STYLE_UNDERLINE | RT_ANNOT_STYLE_STRIKETHROUGH |
	    RT_ANNOT_STYLE_BOLD | RT_ANNOT_STYLE_ITALIC;
	annot.styles[1].x_scale = 1.5;
	annot.styles[1].xy_scale = 0.25;
	annot.styles[1].yx_scale = 0.1;
	annot.styles[1].y_scale = 0.75;
    {
	int i;
	for (i = 2; i < 10; ++i)
	    annot.styles[i].role = RT_ANNOT_ROLE_MASK;
    }
    annot.styles[10].role = RT_ANNOT_ROLE_MASK;
    annot.styles[10].flags = RT_ANNOT_STYLE_FILLED;
    annot.styles[10].symbol = (char *)"owned fill with hole";
    {
	const size_t segment_count = annot.ant.count;
	annot.ant.count = 1;
	annot.styles[0].line_pattern = RT_ANNOT_LINE_CONTINUOUS;
	annot.styles[0].line_width = 1.0;
	if (rt_annot_validate(&annot, NULL) ||
		mk_annot(wdbp, "thin.annot", &annot) < 0)
	    bu_exit(1, "unable to write default-width annotation\n");
	annot.styles[0].line_width = 2.5;
	if (rt_annot_validate(&annot, NULL) ||
		mk_annot(wdbp, "wide.annot", &annot) < 0)
	    bu_exit(1, "unable to write wide annotation\n");
	annot.ant.count = segment_count;
	annot.styles[0].line_pattern = RT_ANNOT_LINE_DASHED;
    }
    if (rt_annot_validate(&annot, NULL) ||
	    mk_annot(wdbp, "leader.annot", &annot) < 0)
	bu_exit(1, "unable to write enhanced annotation\n");
    {
	struct wmember members;
	mat_t member_transform = MAT_INIT_IDN;
	unsigned char inherited_color[3] = {90, 80, 70};
	BU_LIST_INIT(&members.l);
	MAT_DELTAS(member_transform, 100.0, 0.0, 0.0);
	if (!mk_addmember("leader.annot", &members.l, member_transform,
		WMOP_UNION) ||
		mk_lcomb(wdbp, "annotation-group", &members, 0, NULL, NULL,
		    inherited_color, 0))
	    bu_exit(1, "unable to write annotation instance combination\n");
    }
    {
	struct wmember members;
	unsigned char inherited_color[3] = {21, 31, 41};
	BU_LIST_INIT(&members.l);
	if (!mk_addmember("annotation-group", &members.l, NULL, WMOP_UNION) ||
		mk_lcomb(wdbp, "inherited-annotation-group", &members, 0,
		    NULL, NULL, inherited_color, 1))
	    bu_exit(1, "unable to write inherited annotation combination\n");
    }
    {
	struct wmember members;
	mat_t member_transform = MAT_INIT_IDN;
	BU_LIST_INIT(&members.l);
	MAT_SCALE_ALL(member_transform, 1000.0);
	if (!mk_addmember("leader.annot", &members.l, member_transform,
		WMOP_UNION) ||
		mk_lcomb(wdbp, "scaled-annotation-group", &members, 0, NULL,
		    NULL, NULL, 0))
	    bu_exit(1, "unable to write scaled annotation instance combination\n");
    }
    {
	point_t saved_anchor;
	VMOVE(saved_anchor, annot.V);
	VSETALL(annot.V, 0.0);
	annot.flags = RT_ANNOT_SCREEN_SPACE;
	if (mk_annot(wdbp, "screen.annot", &annot) < 0)
	    bu_exit(1, "unable to write screen annotation\n");
	VMOVE(annot.V, saved_anchor);
	annot.flags = RT_ANNOT_MODEL_SPACE;
    }
    {
	const unsigned char far_color[][4] = {{0, 0, 255, 128}};
	const unsigned char near_colors[][4] = {
	    {255, 0, 0, 128}, {0, 255, 0, 128}
	};
	const unsigned char screen_color[][4] = {{255, 255, 255, 128}};
	write_layer_annotation(wdbp, "alpha-far.annot", 20.0,
	    RT_ANNOT_MODEL_SPACE, 1, far_color);
	write_layer_annotation(wdbp, "alpha-near.annot", 10.0,
	    RT_ANNOT_MODEL_SPACE, 2, near_colors);
	write_layer_annotation(wdbp, "alpha-screen.annot", 0.0,
	    RT_ANNOT_SCREEN_SPACE, 1, screen_color);
    }

    /* ANP2 is deliberately appended after the main-branch ANT2 body.  A
     * reader which knows only ANT2 must still get usable fill outlines,
     * unscaled text, and roles from the pre-extension enum range. */
    {
	struct rt_db_internal source;
	struct rt_db_internal legacy_intern;
	struct bu_external full = BU_EXTERNAL_INIT_ZERO;
	struct bu_external legacy = BU_EXTERNAL_INIT_ZERO;
	size_t presentation_offset = SIZE_MAX;
	size_t i;
	RT_DB_INTERNAL_INIT(&source);
	source.idb_major_type = DB5_MAJORTYPE_BRLCAD;
	source.idb_type = ID_ANNOT;
	source.idb_meth = &OBJ[ID_ANNOT];
	source.idb_ptr = &annot;
	if (OBJ[ID_ANNOT].ft_export5(&full, &source, 1.0, NULL) < 0)
	    bu_exit(1, "unable to export annotation compatibility stream\n");
	for (i = 0; i + 4 <= full.ext_nbytes; ++i)
	    if (!memcmp(full.ext_buf + i, "ANP2", 4))
		presentation_offset = i;
	if (presentation_offset == SIZE_MAX)
	    bu_exit(1, "annotation presentation extension was not exported\n");
	legacy.ext_nbytes = presentation_offset;
	legacy.ext_buf = (uint8_t *)bu_malloc(legacy.ext_nbytes,
	    "annotation compatibility stream");
	memcpy(legacy.ext_buf, full.ext_buf, legacy.ext_nbytes);
	RT_DB_INTERNAL_INIT(&legacy_intern);
	if (OBJ[ID_ANNOT].ft_import5(&legacy_intern, &legacy,
		bn_mat_identity, NULL) < 0)
	    bu_exit(1, "main-era annotation stream could not be imported\n");
	{
	    struct rt_annot_internal *loaded =
		(struct rt_annot_internal *)legacy_intern.idb_ptr;
	    if (!(loaded->flags & RT_ANNOT_MODEL_SPACE) || !loaded->styles ||
		    loaded->ant.count != 10 ||
		    loaded->styles[0].role != RT_ANNOT_ROLE_GEOMETRY ||
		    loaded->styles[1].role != RT_ANNOT_ROLE_TEXT ||
		    (loaded->styles[1].flags & (RT_ANNOT_STYLE_SCALE |
		    RT_ANNOT_STYLE_UNDERLINE | RT_ANNOT_STYLE_STRIKETHROUGH |
		    RT_ANNOT_STYLE_BOLD | RT_ANNOT_STYLE_ITALIC)))
		bu_exit(1, "main-era annotation compatibility view changed\n");
	    for (i = 0; i < loaded->ant.count; ++i)
		if (*(uint32_t *)loaded->ant.segments[i] == ANN_FSEG_MAGIC)
		    bu_exit(1, "main-era reader unexpectedly loaded a fill segment\n");
	}
	rt_db_free_internal(&legacy_intern);
	bu_free_external(&legacy);
	bu_free_external(&full);
    }

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
		loaded->styles[0].role != RT_ANNOT_ROLE_CENTERMARK ||
		loaded->styles[0].line_pattern != RT_ANNOT_LINE_DASHED ||
		!loaded->styles[0].font ||
		!BU_STR_EQUAL(loaded->styles[0].font, "ProFont.ttf") ||
		!loaded->styles[0].symbol ||
		!BU_STR_EQUAL(loaded->styles[0].symbol, "centermark") ||
		loaded->styles[1].role != RT_ANNOT_ROLE_TEXT ||
		!loaded->styles[1].font ||
		!BU_STR_EQUAL(loaded->styles[1].font, "osifont") ||
		!(loaded->styles[1].flags & RT_ANNOT_STYLE_SCALE) ||
		!(loaded->styles[1].flags & RT_ANNOT_STYLE_UNDERLINE) ||
		!(loaded->styles[1].flags & RT_ANNOT_STYLE_STRIKETHROUGH) ||
		!(loaded->styles[1].flags & RT_ANNOT_STYLE_BOLD) ||
		!(loaded->styles[1].flags & RT_ANNOT_STYLE_ITALIC) ||
		!NEAR_EQUAL(loaded->styles[1].x_scale, 1.5, SMALL_FASTF) ||
		!NEAR_EQUAL(loaded->styles[1].xy_scale, 0.25, SMALL_FASTF) ||
		!NEAR_EQUAL(loaded->styles[1].yx_scale, 0.1, SMALL_FASTF) ||
		!NEAR_EQUAL(loaded->styles[1].y_scale, 0.75, SMALL_FASTF) ||
		loaded->ant.count != 11 ||
		*(uint32_t *)loaded->ant.segments[10] != ANN_FSEG_MAGIC ||
		loaded->styles[10].role != RT_ANNOT_ROLE_MASK ||
		!loaded->styles[10].symbol ||
		!BU_STR_EQUAL(loaded->styles[10].symbol,
		    "owned fill with hole")) {
	    bu_log("count=%zu magic=%x scale=%g/%g/%g/%g flags=%x symbol=%s\n",
		loaded->ant.count,
		loaded->ant.count > 10 ?
		*(uint32_t *)loaded->ant.segments[10] : 0,
		loaded->styles[1].x_scale, loaded->styles[1].xy_scale,
		loaded->styles[1].yx_scale, loaded->styles[1].y_scale,
		loaded->styles[1].flags,
		loaded->ant.count > 10 && loaded->styles[10].symbol ?
		loaded->styles[10].symbol : "(null)");
	    bu_exit(1, "enhanced annotation fields did not round trip\n");
	}
	VMOVE(original_anchor, loaded->V);
	BU_LIST_INIT(&vhead);
	if (OBJ[ID_ANNOT].ft_plot(&vhead, &intern, &ttol, &tol, NULL) < 0)
	    bu_exit(1, "unable to plot model-space annotation\n");
	if (bv_vlist_cmd_cnt((struct bv_vlist *)&vhead) < 20)
	    bu_exit(1, "annotation font outlines were not plotted\n");
	{
	    struct bv_vlist *vp;
	    size_t polygon_starts = 0;
	    size_t width_commands = 0;
	    for (BU_LIST_FOR(vp, bv_vlist, &vhead)) {
		size_t i;
		for (i = 0; i < vp->nused; ++i)
		    if (vp->cmd[i] == BV_VLIST_POLY_START)
			++polygon_starts;
		    else if (vp->cmd[i] == BV_VLIST_LINE_WIDTH)
			++width_commands;
	    }
	    if (!polygon_starts)
		bu_exit(1, "annotation fill was not triangulated for plotting\n");
	    if (width_commands < 2)
		bu_exit(1, "bold annotation text did not set and reset line width\n");
	}
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

    {
	const char *paths[] = {"leader.annot"};
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct rt_annot_scene *scene = rt_annot_scene_create(dbip, 1, paths,
	    &ttol, &tol);
	struct xray ray;
	struct rt_annot_hit hit = {0};
	struct rt_annot_hit all_hit = {0};
	point_t render_min, render_max, width_sample;
	const fastf_t line_offset = 0.12;
	const fastf_t inverse_line_length = 1.0 / sqrt(41.0);
	const fastf_t offset_x = 2.0 - 5.0 * line_offset * inverse_line_length;
	const fastf_t offset_y = 2.5 + 4.0 * line_offset * inverse_line_length;
	VSET(ray.r_pt, 0.0, 24.0, 37.5);
	VSET(ray.r_dir, 1.0, 0.0, 0.0);
	if (!scene || rt_annot_scene_count(scene) != 1)
	    bu_exit(1, "annotation render scene did not collect its leaf\n");
	if (!rt_annot_scene_bounds(scene, render_min, render_max) ||
		!NEAR_EQUAL(render_min[X], 10.0, SMALL_FASTF) ||
		!NEAR_EQUAL(render_max[X], 10.0, SMALL_FASTF) ||
		render_min[Y] >= render_max[Y] ||
		render_min[Z] >= render_max[Z])
	    bu_exit(1, "annotation render scene bounds are invalid\n");
	if (!rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		INFINITY, &hit) || hit.screen_space || !hit.visible ||
		!NEAR_EQUAL(hit.distance, 10.0, SMALL_FASTF) ||
		hit.color[0] != 12 || hit.color[1] != 34 || hit.color[2] != 56)
	    bu_exit(1, "model annotation coverage query failed\n");
	if (rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0, 5.0, &hit))
	    bu_exit(1, "occluded annotation was returned as visible\n");
	if (!rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		9.99975, &hit))
	    bu_exit(1, "annotation visibility ignored model tolerance\n");
	if (rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		9.999, &hit))
	    bu_exit(1, "annotation visibility exceeded model tolerance\n");
	if (rt_annot_scene_query_model(scene, &ray, 5.0, &all_hit, 1) != 1 ||
		all_hit.visible)
	    bu_exit(1, "annotation diagnostic query lost occluded coverage\n");
	VSET(ray.r_pt, 0.0, 12.0, 18.0);
	if (!rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		INFINITY, &hit) || hit.segment != 10)
	    bu_exit(1, "filled annotation coverage was not returned\n");
	VSET(ray.r_pt, 0.0, 17.0, 25.5);
	if (rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		INFINITY, &hit))
	    bu_exit(1, "annotation fill hole unexpectedly reported coverage\n");
	rt_annot_scene_destroy(scene);
	VSET(width_sample, 0.0, 20.0 + 2.0 * offset_x,
	    30.0 + 3.0 * offset_y);
	paths[0] = "wide.annot";
	scene = rt_annot_scene_create(dbip, 1, paths, &ttol, &tol);
	VMOVE(ray.r_pt, width_sample);
	if (!scene || !rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		INFINITY, &hit) || hit.segment != 0)
	    bu_exit(1, "annotation line width did not expand ray coverage\n");
	rt_annot_scene_destroy(scene);
	paths[0] = "thin.annot";
	scene = rt_annot_scene_create(dbip, 1, paths, &ttol, &tol);
	VMOVE(ray.r_pt, width_sample);
	if (!scene || rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		INFINITY, &hit))
	    bu_exit(1, "default line width unexpectedly covered wide sample\n");
	rt_annot_scene_destroy(scene);
    }

    {
	const char *paths[] = {"scaled-annotation-group"};
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct rt_annot_scene *scene = rt_annot_scene_create(dbip, 1, paths,
	    &ttol, &tol);
	struct xray ray;
	struct rt_annot_hit hit = {0};
	VSET(ray.r_pt, 0.0, 0.028, 0.045);
	VSET(ray.r_dir, 1.0, 0.0, 0.0);
	if (!scene || rt_annot_scene_count(scene) != 1 ||
		!rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		    INFINITY, &hit) ||
		!NEAR_EQUAL(hit.distance, 0.01, SMALL_FASTF))
	    bu_exit(1, "scaled annotation instance coverage failed\n");
	rt_annot_scene_destroy(scene);
    }

    {
	const char *paths[] = {"annotation-group"};
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct rt_annot_scene *scene = rt_annot_scene_create(dbip, 1, paths,
	    &ttol, &tol);
	struct xray ray;
	struct rt_annot_hit hit = {0};
	VSET(ray.r_pt, 0.0, 28.0, 45.0);
	VSET(ray.r_dir, 1.0, 0.0, 0.0);
	if (!scene || rt_annot_scene_count(scene) != 1 ||
		!rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		    INFINITY, &hit) ||
		!NEAR_EQUAL(hit.distance, 110.0, SMALL_FASTF) ||
		hit.segment != 10 || hit.color[0] != 90 ||
		hit.color[1] != 80 || hit.color[2] != 70)
	    bu_exit(1, "annotation instance transform was not applied\n");
	VSET(ray.r_pt, 0.0, 24.0, 37.5);
	if (!rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		INFINITY, &hit) || hit.segment != 0 || hit.color[0] != 12 ||
		hit.color[1] != 34 || hit.color[2] != 56)
	    bu_exit(1, "annotation segment color did not override inherited color\n");
	rt_annot_scene_destroy(scene);
    }

    {
	const char *paths[] = {"inherited-annotation-group"};
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct rt_annot_scene *scene = rt_annot_scene_create(dbip, 1, paths,
	    &ttol, &tol);
	struct xray ray;
	struct rt_annot_hit hit = {0};
	VSET(ray.r_pt, 0.0, 28.0, 45.0);
	VSET(ray.r_dir, 1.0, 0.0, 0.0);
	if (!scene || !rt_annot_scene_query(scene, NULL, &ray, 0.0, 0.0,
		INFINITY, &hit) || hit.segment != 10 || hit.color[0] != 21 ||
		hit.color[1] != 31 || hit.color[2] != 41)
	    bu_exit(1, "annotation color did not follow tree inheritance\n");
	rt_annot_scene_destroy(scene);
    }

    {
	const char *paths[] = {"screen.annot"};
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct rt_annot_scene *scene = rt_annot_scene_create(dbip, 1, paths,
	    &ttol, &tol);
	struct rt_annot_view view = {MAT_INIT_IDN, 100, 100, 0.0};
	struct xray ray;
	struct rt_annot_hit hit = {0};
	VSET(ray.r_pt, 0.0, 0.0, 10.0);
	VSET(ray.r_dir, 0.0, 0.0, -1.0);
	if (!scene || !rt_annot_scene_query(scene, &view, &ray, 50.0, 50.0,
		0.0, &hit) || !hit.screen_space || !hit.visible)
	    bu_exit(1, "screen annotation coverage query failed\n");
	rt_annot_scene_destroy(scene);
    }

    {
	const char *paths[] = {
	    "alpha-far.annot", "alpha-near.annot", "alpha-screen.annot"
	};
	struct bg_tess_tol ttol = BG_TESS_TOL_INIT_TOL;
	struct bn_tol tol = BN_TOL_INIT_TOL;
	struct rt_annot_scene *scene = rt_annot_scene_create(dbip, 3, paths,
	    &ttol, &tol);
	struct rt_annot_view view = {MAT_INIT_IDN, 100, 100, 0.0};
	struct rt_annot_hit layers[4] = {{0}};
	struct rt_annot_hit diagnostic_hits[2] = {{0}};
	struct rt_annot_hit top = {0};
	struct xray ray;
	size_t layer_count;
	fastf_t color[3] = {0.0, 0.0, 0.0};
	const fastf_t alpha = 128.0 / 255.0;
	const fastf_t remainder = 1.0 - alpha;
	const fastf_t expected_color[3] = {
	    alpha + remainder * remainder * alpha,
	    alpha + remainder * alpha,
	    alpha + remainder * remainder * remainder * alpha
	};
	VSET(ray.r_pt, 0.0, 0.0, 0.0);
	VSET(ray.r_dir, 1.0, 0.0, 0.0);
	layer_count = rt_annot_scene_query_layers(scene, &view, &ray,
	    50.0, 50.0, INFINITY, layers, 2);
	if (!scene || layer_count != 4 ||
		bu_strcmp(layers[0].path, "/alpha-far.annot") ||
		bu_strcmp(layers[1].path, "/alpha-near.annot") ||
		layers[1].segment != 0)
	    bu_exit(1, "annotation layer count or capacity handling failed: "
		"count=%zu first=%s second=%s/%zu\n", layer_count,
		layers[0].path ? layers[0].path : "(null)",
		layers[1].path ? layers[1].path : "(null)", layers[1].segment);
	if (rt_annot_scene_query_layers(scene, &view, &ray, 50.0, 50.0,
		INFINITY, layers, 4) != 4 ||
		!NEAR_EQUAL(layers[0].distance, 20.0, SMALL_FASTF) ||
		!NEAR_EQUAL(layers[1].distance, 10.0, SMALL_FASTF) ||
		layers[1].segment != 0 ||
		layers[2].segment != 1 || !layers[3].screen_space)
	    bu_exit(1, "annotation layers are not back-to-front\n");
	if (!rt_annot_scene_query(scene, &view, &ray, 50.0, 50.0,
		INFINITY, &top) || !top.screen_space ||
		bu_strcmp(top.path, "/alpha-screen.annot"))
	    bu_exit(1, "annotation top-layer compatibility query changed\n");
	if (rt_annot_scene_query_layers(scene, &view, &ray, 50.0, 50.0,
		15.0, NULL, 0) != 3)
	    bu_exit(1, "annotation layer visibility filtering failed\n");
	if (rt_annot_scene_query_model(scene, &ray, INFINITY,
		diagnostic_hits, 2) != 2 ||
		bu_strcmp(diagnostic_hits[0].path, "/alpha-near.annot") ||
		diagnostic_hits[0].segment != 1 ||
		bu_strcmp(diagnostic_hits[1].path, "/alpha-far.annot"))
	    bu_exit(1, "annotation diagnostic hits changed layer semantics\n");
	if (!rt_annot_scene_composite(scene, &view, &ray, 50.0, 50.0,
		INFINITY, color, &top) ||
		!VNEAR_EQUAL(color, expected_color, SMALL_FASTF) ||
		!top.screen_space)
	    bu_exit(1, "annotation alpha compositing order failed\n");
	MAT_DELTAS(view.model2view, 0.0, 0.0, -2.0);
	view.width = 160;
	view.height = 90;
	view.perspective = 60.0;
	if (rt_annot_scene_query_layers(scene, &view, &ray, 80.0, 45.0,
		INFINITY, NULL, 0) != 4)
	    bu_exit(1, "perspective screen annotation coverage failed\n");
	rt_annot_scene_destroy(scene);
    }

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
