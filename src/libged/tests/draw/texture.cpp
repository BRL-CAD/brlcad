/*                         T E X T U R E . C P P
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
/** @file texture.cpp
 *
 * Exercise shaded BoT UVs through GED and the software OpenGL display manager.
 */

#include "common.h"

#include <cstdio>
#include <string>

#include "bu/app.h"
#include "bu/env.h"
#include "bu/file.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bv/lod.h"
#include "dm.h"
#include "ged.h"
#include "wdb.h"

#include "../../dbi.h"

static const int image_size = 192;
static const fastf_t view_scale = 120.0;
static const int coarse_face_count = 4;
static const int source_face_count = 8;
static const char *const mesh_name = "seam.s";

static const unsigned char image_a[] = {
    255, 0, 0, 0, 0, 0, 255, 255, 0,
    0, 255, 0, 0, 0, 0, 0, 0, 255
};
static const unsigned char image_b[] = {
    0, 255, 255, 0, 0, 0, 0, 0, 255,
    255, 0, 255, 0, 0, 0, 255, 0, 0
};

static bool
write_image(struct rt_wdb *wdbp, const char *name, const unsigned char *data)
{
    return mk_binunif(wdbp, name, data, WDB_BINUNIF_UINT8, sizeof(image_a)) == 0;
}

static bool
write_region(struct rt_wdb *wdbp, const char *name, const char *shader,
	const char *params, fastf_t x, fastf_t y, const unsigned char *color, bool rotate = false)
{
    struct wmember member;
    BU_LIST_INIT(&member.l);
    mat_t transform;
    MAT_IDN(transform);
    if (rotate) {
	// Turn the mesh around Z to exercise transformed bounds on both axes.
	transform[0] = transform[5] = -1.0;
    }
    MAT_DELTAS(transform, x, y, 0);
    return mk_addmember(mesh_name, &member.l, transform, WMOP_UNION) &&
	mk_lrcomb(wdbp, name, &member, 1, shader, params, color, 0, 0, 0, 100, 0) == 0;
}

static bool
write_fixture(const char *path)
{
    struct rt_wdb *wdbp = wdb_fopen(path);
    if (!wdbp)
	return false;

    // Tiny faces precede the four coarse triangles in source order. At
    // reduced detail the coarse triangles must still use their own UVs.
    const point_t vertices[] = {
	{-66,-52,0}, {-65.98,-52,0}, {-66,-51.98,0},
	{-63,-52,0}, {-62.98,-52,0}, {-63,-51.98,0},
	{-60,-52,0}, {-59.98,-52,0}, {-60,-51.98,0},
	{-57,-52,0}, {-56.98,-52,0}, {-57,-51.98,0},
	{-90,-36,0}, {-50,-36,0}, {-10,-36,0},
	{-90,12,0}, {-50,12,0}, {-10,12,0}
    };
    const int faces[] = {
	0,2,1, 3,5,4, 6,8,7, 9,11,10,
	12,16,13, 12,15,16, 13,17,14, 13,16,17
    };
    // The two panels share position indices at the seam. The right panel
    // uses a different image column, with the same vertical UV range.
    point_t uvs[] = {
	{0.5,0.5,0}, {1.0/6,0.25,0}, {1.0/6,0.75,0},
	{5.0/6,0.25,0}, {5.0/6,0.75,0}
    };
    int face_uvs[] = {
	0,0,0, 0,0,0, 0,0,0, 0,0,0,
	1,2,1, 1,2,2, 3,4,3, 3,4,4
    };

    const unsigned char plain[] = {255, 128, 32};
    const bool ok = mk_bot_w_normals_and_uvs(wdbp, mesh_name, RT_BOT_SURFACE, RT_BOT_CW,
	RT_BOT_HAS_TEXTURE_UVS, sizeof(vertices)/sizeof(vertices[0]), source_face_count,
	vertices[0], faces, NULL, NULL, 0, NULL, NULL, sizeof(uvs)/sizeof(uvs[0]),
	uvs[0], face_uvs) == 0 &&
	write_image(wdbp, "image_a", image_a) && write_image(wdbp, "image_b", image_b) &&
	write_region(wdbp, "left.r", "texture", "obj=image_a w=3 n=2", 0, 0, NULL) &&
	write_region(wdbp, "right.r", "texture", "obj=image_b w=3 n=2", 0, -24, NULL, true) &&
	write_region(wdbp, "plain.r", NULL, NULL, 50, 75, plain);
    db_close(wdbp->dbip);
    return ok;
}

struct sample {
    fastf_t x;
    fastf_t y;
    unsigned char rgb[3];
};

static bool
draw_frame(struct bview *view)
{
    struct dm *display = (struct dm *)view->dmp;
    if (dm_draw_begin(display) != BRLCAD_OK)
	return false;
    dm_draw_objs(view, NULL, NULL);
    return dm_draw_end(display) == BRLCAD_OK;
}

static bool
check_frame(struct bview *view, const char *stage, bool image_updated = false)
{
    const sample samples[] = {
	{-72, -32, {255, 0, 0}},
	{-72,   8, {0, 255, 0}},
	{-30,   8, {0, 0, 255}},
	{ 28, -32, {255, 0, 0}},
	{ 70,   8, {0, 255, 255}},
	{-20,  64, {255, 128, 32}}
    };
    struct dm *display = (struct dm *)view->dmp;
    if (!draw_frame(view))
	return false;
    unsigned char *pixels = NULL;
    if (dm_get_display_image(display, &pixels, 0, 0) != BRLCAD_OK || !pixels)
	return false;

    bool ok = true;
    for (size_t i = 0; i < sizeof(samples)/sizeof(samples[0]); i++) {
	const sample &s = samples[i];
	const int x = (int)((s.x / view_scale + 1.0) * image_size / 2);
	const int y = (int)((s.y / view_scale + 1.0) * image_size / 2);
	const unsigned char *actual = &pixels[(y * image_size + x) * 3];
	const unsigned char *expected = s.rgb;
	if (image_updated && i < 3) {
	    const int texels[] = {0, 3, 5};
	    expected = &image_b[texels[i]*3];
	}
	// Samples near the edge still blend slightly with the other row.
	const int tolerance = 33;
	for (int channel = 0; channel < 3; channel++) {
	    if (abs((int)actual[channel] - expected[channel]) <= tolerance)
		continue;
	    bu_log("%s pixel (%d,%d): got %u/%u/%u, expected %u/%u/%u\n",
		stage, x, y, actual[0], actual[1], actual[2],
		expected[0], expected[1], expected[2]);
	    ok = false;
	    break;
	}
    }
    bu_free(pixels, "texture test pixels");
    return ok;
}

static struct bv_scene_obj *
local_object(struct bview *view, const char *path)
{
    return bv_obj_for_view(bv_find_obj(view, path), view);
}

static bool
check_lod(struct bview *view, int expected_faces, bool simplified)
{
    auto *local = local_object(view, "left.r/seam.s");
    auto *lod = local ? (struct bv_mesh_lod *)local->draw_data : NULL;
    if (!lod || lod->fcnt != expected_faces || (lod->face_sources != NULL) != simplified) {
	bu_log("Expected %d %s faces, got %d\n", expected_faces,
	    simplified ? "simplified" : "full-detail", lod ? lod->fcnt : -1);
	return false;
    }
    if (simplified) {
	for (int i = 0; i < lod->fcnt; i++) {
	    if (lod->face_sources[i] < source_face_count - coarse_face_count) {
		bu_log("Coarse triangle mapped to a discarded source face\n");
		return false;
	    }
	}
    }
    return true;
}

static bool
check_uv_fit(struct bview *view)
{
    auto *local = local_object(view, "left.r/seam.s");
    auto *lod = local ? (struct bv_mesh_lod *)local->draw_data : NULL;
    const point2d_t *uvs = bv_mesh_lod_snapped_uvs(lod);
    if (!uvs)
	return false;

    // For both coplanar panels, V is a linear function of Y. Equal
    // positions at the seam are mapped to different image columns.
    const fastf_t panel_bottom = -36.0;
    const fastf_t panel_height = 48.0;
    const fastf_t bottom_v = 0.25;
    const fastf_t uv_height = 0.5;
    const fastf_t uv_tolerance = 1.0e-12;
    bool moved = false;
    for (int f = 0; f < lod->fcnt; f++) {
	for (int j = 0; j < 3; j++) {
	    const size_t corner = (size_t)f * 3 + j;
	    const int uv_index = lod->face_uvs[(size_t)lod->face_sources[f] * 3 + j];
	    const fastf_t *original = lod->uvs[uv_index];
	    const fastf_t y = lod->points[lod->faces[corner]][Y];
	    const fastf_t expected_v = bottom_v + uv_height * (y - panel_bottom) / panel_height;
	    if (!NEAR_EQUAL(uvs[corner][X], original[X], uv_tolerance) ||
		!NEAR_EQUAL(uvs[corner][Y], expected_v, uv_tolerance)) {
		bu_log("Snapped UV failed to preserve an image column or the planar texture mapping\n");
		return false;
	    }
	    moved |= !NEAR_EQUAL(uvs[corner][Y], original[Y], uv_tolerance);
	}
    }
    if (!moved)
	bu_log("UV fit did not compensate for snapping\n");
    return moved;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1)
	return 1;
    char path[MAXPATHLEN] = {};
    FILE *tmp = bu_temp_file(path, MAXPATHLEN);
    if (!tmp)
	return 1;
    std::fclose(tmp);
    const std::string cache = std::string(path) + ".cache";
    bu_mkdir(cache.c_str());
    bu_setenv("BU_DIR_CACHE", cache.c_str(), 1);
    bu_setenv("DM_SWRAST", "1", 1);
    if (!write_fixture(path))
	bu_exit(EXIT_FAILURE, "Could not write the texture fixture\n");

    struct ged *gedp = ged_open("db", path, 1);
    if (!gedp)
	bu_exit(EXIT_FAILURE, "Could not open the texture fixture\n");
    auto *dbis = new DbiState(gedp);
    gedp->dbi_state = dbis;
    gedp->new_cmd_forms = 1;
    gedp->ged_lod = bv_mesh_lod_context_create(path);

    const char *attach[] = {"dm", "attach", "swrast", "SW"};
    if (ged_exec_dm(gedp, 4, attach) != BRLCAD_OK)
	bu_exit(EXIT_FAILURE, "Could not open swrast: %s\n", bu_vls_cstr(gedp->ged_result_str));
    struct bview *view = gedp->ged_gvp;
    struct dm *display = (struct dm *)view->dmp;
    view->gv_width = view->gv_height = image_size;
    dm_set_width(display, image_size);
    dm_set_height(display, image_size);
    dm_configure_win(display, 0);
    dm_set_zbuffer(display, 1);
    dm_set_light(display, 0);
    fastf_t bounds[6] = {-1, 1, -1, 1, -100, 100};
    dm_set_win_bounds(display, bounds);
    dm_set_vp(display, &view->gv_scale);

    const char *draw[] = {"draw", "-m1", "left.r", "right.r", "plain.r"};
    if (ged_exec_draw(gedp, 5, draw) != BRLCAD_OK)
	bu_exit(EXIT_FAILURE, "Could not draw the texture fixture\n");
    view->gv_scale = view_scale;
    view->gv_size = 2 * view_scale;
    MAT_IDN(view->gv_rotation);
    MAT_IDN(view->gv_center);
    bv_update(view);

    bool ok = check_frame(view, "Full detail");
    const char *enable[] = {"view", "lod", "mesh", "1"};
    const char *disable[] = {"view", "lod", "mesh", "0"};
    const char *error_off[] = {"view", "lod", "texture_error", "0"};
    const char *error_default[] = {"view", "lod", "texture_error", "2"};
    const char *error_query[] = {"view", "lod", "texture_error"};
    const char *error_invalid[] = {"view", "lod", "texture_error", "nan"};
    ok &= ged_exec_view(gedp, 3, error_query) == BRLCAD_OK &&
	EQUAL(atof(bu_vls_cstr(gedp->ged_result_str)), BV_LOD_TEXTURE_ERROR_DEFAULT);
    ok &= ged_exec_view(gedp, 4, error_invalid) == BRLCAD_ERROR;
    ok &= ged_exec_view(gedp, 4, error_off) == BRLCAD_OK;
    const fastf_t coarse_scale = 0.8;
    view->gv_s->lod_scale = coarse_scale;
    ok &= ged_exec_view(gedp, 4, enable) == BRLCAD_OK;
    ok &= check_frame(view, "LoD");
    ok &= check_lod(view, coarse_face_count, true);
    ok &= check_uv_fit(view);
    ok &= check_frame(view, "Cached display list");

    const fastf_t fine_scale = 2.0;
    view->gv_s->lod_scale = fine_scale;
    ok &= check_frame(view, "LoD full-detail transition");
    ok &= check_lod(view, source_face_count, false);
    view->gv_s->lod_scale = coarse_scale;
    ok &= check_frame(view, "LoD after returning from full detail");
    ok &= check_lod(view, coarse_face_count, true);

    // Rewrite the image without changing geometry or the material string.
    auto *wdbp = wdb_dbopen(gedp->dbip, RT_WDB_TYPE_DB_DEFAULT);
    ok &= write_image(wdbp, "image_a", image_b);
    dbis->update();
    ok &= check_frame(view, "Updated image with cached geometry", true);

    ok &= ged_exec_view(gedp, 4, disable) == BRLCAD_OK;
    ok &= check_frame(view, "LoD disabled", true);
    ok &= ged_exec_view(gedp, 4, enable) == BRLCAD_OK;
    ok &= check_frame(view, "LoD from disk cache", true);
    ok &= check_lod(view, coarse_face_count, true);

    // Exercise regeneration after the renderer releases CPU mesh data.
    auto *local = local_object(view, "left.r/seam.s");
    if (local) {
	bv_mesh_lod_memshrink(local);
	bv_obj_stale(local);
	ok &= check_frame(view, "LoD after releasing mesh buffers", true);
	ok &= check_lod(view, coarse_face_count, true);
    } else {
	ok = false;
    }

    // A coarser requested level exceeds the pixel budget for the textured
    // instance. Switching the limit on and off must refine and restore it.
    const fastf_t aggressive_scale = 0.4;
    view->gv_s->lod_scale = aggressive_scale;
    ok &= draw_frame(view);
    auto *plain = local_object(view, "plain.r/seam.s");
    const int coarse_level = bv_mesh_lod_level(local, -1, 0);
    const int plain_level = bv_mesh_lod_level(plain, -1, 0);
    ok &= ged_exec_view(gedp, 4, error_default) == BRLCAD_OK;
    ok &= check_frame(view, "With texture error limit", true);
    if (coarse_level < 0 || bv_mesh_lod_level(local, -1, 0) <= coarse_level ||
	plain_level < 0 || bv_mesh_lod_level(plain, -1, 0) != plain_level) {
	bu_log("The pixel budget should refine only the textured instance\n");
	ok = false;
    }
    ok &= ged_exec_view(gedp, 4, error_off) == BRLCAD_OK;
    ok &= draw_frame(view);
    ok &= bv_mesh_lod_level(local, -1, 0) == coarse_level;
    view->gv_s->lod_scale = coarse_scale;

    const char *zap[] = {"zap"};
    ged_exec_zap(gedp, 1, zap);
    const char *draw_all_shaded[] = {"draw", "-m2", "-R", "left.r", "right.r", "plain.r"};
    ok &= ged_exec_draw(gedp, 6, draw_all_shaded) == BRLCAD_OK;
    ok &= check_frame(view, "LoD in all-shaded mode", true);
    ok &= check_lod(view, coarse_face_count, true);
    ged_exec_zap(gedp, 1, zap);
    view->dmp = NULL;
    dm_close(display);
    ged_close(gedp);
    bu_file_delete(path);
    bu_dirclear(cache.c_str());
    return ok ? 0 : 1;
}

// Local Variables:
// tab-width: 8
// mode: C++
// c-basic-offset: 4
// indent-tabs-mode: t
// c-file-style: "stroustrup"
// End:
// ex: shiftwidth=4 tabstop=8
