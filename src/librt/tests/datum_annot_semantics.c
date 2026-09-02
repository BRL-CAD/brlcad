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
#include "nmg.h"
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


struct annot_ray_result {
    fastf_t in_dist;
    fastf_t out_dist;
    fastf_t color[3];
    int color_valid;
    int partitions;
};


static int
annot_ray_hit(struct application *ap, struct partition *part_head,
	struct seg *UNUSED(finished_segs))
{
    struct annot_ray_result *result = (struct annot_ray_result *)ap->a_uptr;
    struct partition *part;

    for (part = part_head->pt_forw; part != part_head;
	    part = part->pt_forw) {
	if (!result->partitions) {
	    result->in_dist = part->pt_inhit->hit_dist;
	    result->out_dist = part->pt_outhit->hit_dist;
	    result->color_valid = part->pt_regionp->reg_mater.ma_color_valid;
	    VMOVE(result->color, part->pt_regionp->reg_mater.ma_color);
	}
	result->partitions++;
    }
    return result->partitions > 0;
}


static int
annot_ray_miss(struct application *UNUSED(ap))
{
    return 0;
}


static void
check_annot_ray(struct db_i *dbip, const char *object,
	const fastf_t *expected_color)
{
    struct rt_i *rtip = rt_i_create(dbip);
    struct resource resource = RT_RESOURCE_INIT_ZERO;
    struct application ap;
    struct annot_ray_result result = {0};
    const fastf_t expected_thickness = 0.2;
    int shot_result;

    if (!rtip || rt_gettree(rtip, object))
	bu_exit(1, "unable to prepare annotation raytrace object %s\n", object);
    rt_prep(rtip);
    rt_init_resource(&resource, 0, rtip);
    RT_APPLICATION_INIT(&ap);
    ap.a_rt_i = rtip;
    ap.a_resource = &resource;
    ap.a_hit = annot_ray_hit;
    ap.a_miss = annot_ray_miss;
    ap.a_uptr = &result;
    VSET(ap.a_ray.r_pt, 0.0, -2.0, 0.03);
    VSET(ap.a_ray.r_dir, 1.0, 0.0, 0.0);
    shot_result = rt_shootray(&ap);
    if (shot_result != 1 || result.partitions != 1 ||
	    !result.color_valid ||
	    !NEAR_EQUAL(result.out_dist - result.in_dist,
		expected_thickness, rtip->rti_tol.dist) ||
	    !VNEAR_EQUAL(result.color, expected_color, VUNITIZE_TOL)) {
	bu_log("%s: shot=%d partitions=%d LOS=%g color-valid=%d color=%g/%g/%g\n",
	    object, shot_result, result.partitions,
	    result.out_dist - result.in_dist, result.color_valid,
	    V3ARGS(result.color));
	bu_exit(1, "annotation did not produce the expected colored partition\n");
    }
    rt_i_destroy(rtip);
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
    if (rt_annot_validate(&annot, NULL) ||
	    mk_annot(wdbp, "leader.annot", &annot) < 0)
	bu_exit(1, "unable to write enhanced annotation\n");

    {
	struct rt_annot_internal ray_annot = {0};
	point2d_t ray_vertices[2];
	struct line_seg ray_line = {0};
	void *ray_segments[1];
	int ray_reverse[1] = {0};
	struct rt_annot_seg_style ray_style[1] = {{0}};
	struct wmember member;
	unsigned char region_color[3] = {90, 80, 70};

	ray_annot.magic = RT_ANNOT_INTERNAL_MAGIC;
	VSET(ray_annot.V, 10.0, 0.0, 0.0);
	ray_annot.flags = RT_ANNOT_MODEL_SPACE;
	VSET(ray_annot.u_vec, 0.0, 1.0, 0.0);
	VSET(ray_annot.v_vec, 0.0, 0.0, 1.0);
	ray_annot.vert_count = 2;
	ray_annot.verts = ray_vertices;
	V2SET(ray_vertices[0], -5.0, 0.0);
	V2SET(ray_vertices[1], 5.0, 0.0);
	ray_line.magic = CURVE_LSEG_MAGIC;
	ray_line.start = 0;
	ray_line.end = 1;
	ray_segments[0] = &ray_line;
	ray_annot.ant.count = 1;
	ray_annot.ant.reverse = ray_reverse;
	ray_annot.ant.segments = ray_segments;
	ray_style[0].flags = RT_ANNOT_STYLE_WIDTH;
	ray_style[0].line_pattern = RT_ANNOT_LINE_CONTINUOUS;
	ray_style[0].line_width = 2.0;
	ray_annot.styles = ray_style;
	if (rt_annot_validate(&ray_annot, NULL) ||
		mk_annot(wdbp, "ray.annot", &ray_annot) < 0)
	    bu_exit(1, "unable to write raytrace annotation\n");
	BU_LIST_INIT(&member.l);
	if (!mk_addmember("ray.annot", &member.l, NULL, WMOP_UNION) ||
		mk_lcomb(wdbp, "ray.r", &member, 1, NULL, NULL,
		region_color, 0))
	    bu_exit(1, "unable to write raytrace annotation region\n");
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
	{
	    struct model *model = nmg_mm();
	    struct nmgregion *region = NULL;
	    if (rt_obj_tess(&region, model, &intern, &ttol, &tol) || !region)
		bu_exit(1, "unable to tessellate model-space annotation\n");
	    nmg_km(model);
	}
    }
    rt_db_free_internal(&intern);

    {
	const fastf_t default_color[3] = {1.0, 1.0, 1.0};
	const fastf_t annotation_color[3] = {
	    12.0 / 255.0, 34.0 / 255.0, 56.0 / 255.0
	};
	const fastf_t region_color[3] = {
	    90.0 / 255.0, 80.0 / 255.0, 70.0 / 255.0
	};

	/* m35.g has this broad table entry, which includes the synthetic ID 0
	 * assigned to bare solids.  It must not replace annotation colors. */
	db_mater_add(dbip, 0, 15000, 210, 146, 1, MATER_NO_ADDR);
	check_annot_ray(dbip, "ray.annot", default_color);
	if (db5_update_attribute("ray.annot", "rgb", "12/34/56", dbip))
	    bu_exit(1, "unable to set annotation raytrace color\n");
	check_annot_ray(dbip, "ray.annot", annotation_color);
	check_annot_ray(dbip, "ray.r", region_color);
    }

    dp = lookup(dbip, "leader.annot");
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
