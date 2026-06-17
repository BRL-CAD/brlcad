/*        T E S T _ B A C K E N D _ D R A W _ I T E M . C
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
/** @file libdm/tests/test_backend_draw_item.c
 *
 * Regression coverage for the modern libdm backend draw contract.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/log.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/geometry.h"
#include "bsg/node.h"
#include "bsg/render_item.h"
#include "bsg/separator.h"
#include "bsg/util.h"
#include "dm.h"
#include "../include/private.h"

static int g_fail = 0;
static int g_draw_item_count = 0;
static int g_resource_free_count = 0;

static int file_has_token(const char *path, const char *token);

#define DMCHECK(_cond, _msg) \
    do { \
	if (!(_cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (_msg)); \
	    g_fail++; \
	} \
    } while (0)

struct draw_capture {
    int line2d_count;
    int line3d_count;
    int string2d_count;
    point_t first_3d_a;
    point_t first_3d_b;
    char first_string[64];
    fastf_t first_string_x;
    fastf_t first_string_y;
    int first_string_size;
    int first_string_use_aspect;
    int string2d_rot_count;
    fastf_t first_string_rotation;
};

static struct draw_capture g_capture;

static int
capture_draw_line_2d(struct dm *UNUSED(dmp), fastf_t UNUSED(x1),
	fastf_t UNUSED(y1), fastf_t UNUSED(x2), fastf_t UNUSED(y2))
{
    g_capture.line2d_count++;
    return 0;
}

static int
capture_draw_line_3d(struct dm *UNUSED(dmp), point_t pt1, point_t pt2)
{
    if (!g_capture.line3d_count) {
	VMOVE(g_capture.first_3d_a, pt1);
	VMOVE(g_capture.first_3d_b, pt2);
    }
    g_capture.line3d_count++;
    return 0;
}

static int
capture_draw_string_2d(struct dm *UNUSED(dmp), const char *str,
	fastf_t x, fastf_t y, int size, int use_aspect)
{
    if (!g_capture.string2d_count) {
	snprintf(g_capture.first_string, sizeof(g_capture.first_string), "%s",
		str ? str : "");
	g_capture.first_string_x = x;
	g_capture.first_string_y = y;
	g_capture.first_string_size = size;
	g_capture.first_string_use_aspect = use_aspect;
    }
    g_capture.string2d_count++;
    return 0;
}

static int
capture_draw_string_2d_rot(struct dm *UNUSED(dmp), const char *str,
	fastf_t x, fastf_t y, int size, int use_aspect, fastf_t angle)
{
    if (!g_capture.string2d_count) {
	snprintf(g_capture.first_string, sizeof(g_capture.first_string), "%s",
		str ? str : "");
	g_capture.first_string_x = x;
	g_capture.first_string_y = y;
	g_capture.first_string_size = size;
	g_capture.first_string_use_aspect = use_aspect;
	g_capture.first_string_rotation = angle;
    }
    g_capture.string2d_count++;
    g_capture.string2d_rot_count++;
    return 0;
}

static struct dm *
open_null_dm(void)
{
    const char *av0 = "attach";
    return dm_open(NULL, NULL, "nu", 1, &av0);
}

static int
count_draw_item(struct dm *UNUSED(dmp), const struct bsg_render_item *item)
{
    if (!item || item->geometry.kind == BSG_RENDER_GEOMETRY_NONE ||
	    !item->source.source_id)
	return -1;
    g_draw_item_count++;
    return 0;
}

static void
test_requires_draw_item(void)
{
    bu_log("=== backend draw item: no node-only fallback ===\n");

    struct dm *dmp = open_null_dm();
    DMCHECK(dmp != NULL, "opened null display manager");
    if (!dmp)
	return;

    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);

    bsg_shape_ref shape = bsg_shape_ref_create(v, "dm_backend_draw_item_shape");
    bsg_node_ref shape_node = bsg_shape_ref_as_node(shape);
    DMCHECK(!bsg_node_ref_is_null(shape_node), "created test shape");
    if (bsg_node_ref_is_null(shape_node)) {
	bsg_view_free(v);
	bu_free(v, "test view");
	dm_close(dmp);
	return;
    }

    struct bsg_render_item *item = bsg_render_item_create();
    item->view = v;
    item->source.source_id = 42;
    item->source.name = "node-free-item";
    item->geometry.kind = BSG_RENDER_GEOMETRY_LINE_SET;
    item->geometry.source_identity = 84;
    item->geometry.revision = 1;

    dm_set_backend_ops(dmp, NULL);
    int ret = dm_backend_draw_item(dmp, item);
    DMCHECK(ret != 0, "draw_item without backend op must fail");
    DMCHECK(g_draw_item_count == 0, "missing draw_item must not draw");

    static const struct dm_backend_ops count_ops = {
	0,
	NULL,
	count_draw_item,
	NULL,
	NULL,
	NULL
    };
    dm_set_backend_ops(dmp, &count_ops);
    ret = dm_backend_draw_item(dmp, item);
    DMCHECK(ret == 0, "registered draw_item op is called");
    DMCHECK(g_draw_item_count == 1, "draw_item count incremented once");

    dm_set_backend_ops(dmp, NULL);
    bsg_render_item_free(item);
    bsg_node_ref_destroy(shape_node);
    bsg_view_free(v);
    bu_free(v, "test view");
    dm_close(dmp);
}

static void
count_resource_free(struct dm *UNUSED(dmp), struct dm_backend_resource *r)
{
    g_resource_free_count++;
    r->handle = NULL;
}

static void
test_backend_resource_cache_contract(void)
{
    bu_log("=== backend resource cache: render/backend record keys ===\n");

    struct dm *dmp = open_null_dm();
    DMCHECK(dmp != NULL, "opened null display manager");
    if (!dmp)
	return;

    g_resource_free_count = 0;

    struct dm_backend_resource *r1 = dm_backend_resource_get(dmp,
	    0x1001, 0x501, 0x9001, 1, count_resource_free);
    DMCHECK(r1 != NULL, "created first backend resource");
    r1->handle = (void *)0x1;

    struct dm_backend_resource *r2 = dm_backend_resource_get(dmp,
	    0x1001, 0x501, 0x9001, 0, count_resource_free);
    DMCHECK(r2 == r1, "same cache/source/backend identities reuse resource");

    struct dm_backend_resource *r3 = dm_backend_resource_get(dmp,
	    0x1001, 0x501, 0x9002, 1, count_resource_free);
    DMCHECK(r3 != NULL && r3 != r1, "backend capability/context identity separates resources");
    r3->handle = (void *)0x1;

    struct dm_backend_resource *r4 = dm_backend_resource_get(dmp,
	    0x1001, 0x502, 0x9001, 1, count_resource_free);
    DMCHECK(r4 != NULL && r4 != r1, "source identity separates resources");
    r4->handle = (void *)0x1;

    dm_backend_resource_invalidate(dmp, 0x501);
    DMCHECK(r1->stale == 1, "invalidate marks source resources stale");
    DMCHECK(r3->stale == 1, "invalidate marks same-source backend variants stale");
    DMCHECK(r4->stale == 0, "invalidate does not stale unrelated source");

    struct dm_backend_resource *stale = dm_backend_resource_get(dmp,
	    0x1001, 0x501, 0x9001, 0, count_resource_free);
    DMCHECK(stale == NULL, "stale resource is not returned without create");

    struct dm_backend_resource *r5 = dm_backend_resource_get(dmp,
	    0x1001, 0x501, 0x9001, 1, count_resource_free);
    DMCHECK(r5 != NULL, "create after stale invalidation creates replacement");
    DMCHECK(r5->stale == 0, "replacement resource is current");
    DMCHECK(g_resource_free_count >= 1, "stale resource was released on replacement");
    r5->handle = (void *)0x1;

    dm_backend_resource_release_source(dmp, 0x501);
    DMCHECK(g_resource_free_count >= 3, "release_source frees all source variants");
    dm_backend_resource_release_source(dmp, 0x502);

    dm_close(dmp);
}

static void
test_annotation_curve_dm_output(void)
{
    bu_log("=== backend draw item: annotation curve DM output ===\n");

    struct dm *dmp = open_null_dm();
    DMCHECK(dmp != NULL, "opened null display manager");
    if (!dmp)
	return;

    int (*save_line2d)(struct dm *, fastf_t, fastf_t, fastf_t, fastf_t) =
	dmp->i->dm_drawLine2D;
    int (*save_line3d)(struct dm *, point_t, point_t) =
	dmp->i->dm_drawLine3D;
    int (*save_string2d)(struct dm *, const char *, fastf_t, fastf_t, int, int) =
	dmp->i->dm_drawString2D;
    dmp->i->dm_drawLine2D = capture_draw_line_2d;
    dmp->i->dm_drawLine3D = capture_draw_line_3d;
    dmp->i->dm_drawString2D = capture_draw_string_2d;

    dm_set_width(dmp, 512);
    dm_set_height(dmp, 512);
    dmp->i->dm_aspect = 1.0;

    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    v->dmp = dmp;
    v->gv_width = 512;
    v->gv_height = 512;
    MAT_IDN(v->gv_model2view);
    MAT_IDN(v->gv_view2model);

    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_annotation_ref annotation =
	bsg_annotation_ref_create(v, "dm_annotation_curves");
    DMCHECK(!bsg_node_ref_is_null(bsg_annotation_ref_as_node(annotation)),
	    "created annotation geometry");

    point_t pts[7] = {VINIT_ZERO, VINIT_ZERO, VINIT_ZERO, VINIT_ZERO,
	VINIT_ZERO, VINIT_ZERO, VINIT_ZERO};
    VSET(pts[0], 0.0, 0.0, 0.0);
    VSET(pts[1], 1.0, 0.0, 0.0);
    VSET(pts[2], 0.0, 1.0, 0.0);
    VSET(pts[3], 1.0, 1.0, 0.0);
    VSET(pts[4], 2.0, 0.0, 0.0);
    VSET(pts[5], 2.0, 1.0, 0.0);
    VSET(pts[6], 3.0, 0.0, 0.0);

    fastf_t knots[6] = {0.0, 0.0, 0.0, 1.0, 1.0, 1.0};
    int nurb_controls[3] = {2, 3, 4};
    int bezier_controls[4] = {3, 4, 5, 6};
    struct bsg_annotation_segment segs[4];
    memset(segs, 0, sizeof(segs));
    segs[0].kind = BSG_ANNOTATION_SEGMENT_LINE;
    segs[0].data.line.start = 0;
    segs[0].data.line.end = 1;
    segs[1].kind = BSG_ANNOTATION_SEGMENT_CARC;
    segs[1].data.carc.start = 0;
    segs[1].data.carc.end = 1;
    segs[1].data.carc.radius = 1.0;
    segs[1].data.carc.center_is_left = 1;
    segs[2].kind = BSG_ANNOTATION_SEGMENT_NURB;
    segs[2].data.nurb.order = 3;
    segs[2].data.nurb.knot_count = 6;
    segs[2].data.nurb.knots = knots;
    segs[2].data.nurb.control_point_count = 3;
    segs[2].data.nurb.control_points = nurb_controls;
    segs[3].kind = BSG_ANNOTATION_SEGMENT_BEZIER;
    segs[3].data.bezier.degree = 3;
    segs[3].data.bezier.control_point_count = 4;
    segs[3].data.bezier.control_points = bezier_controls;

    mat_t model_mat, display_mat;
    MAT_IDN(model_mat);
    MAT_IDN(display_mat);
    DMCHECK(bsg_annotation_ref_set_record(annotation, "curve annotation",
		BSG_ANNOTATION_SPACE_MODEL, pts[0], model_mat, display_mat,
		(const point_t *)pts, 7, segs, 4),
	    "configured model-space annotation curves");
    bsg_separator_ref_append_child(root, bsg_annotation_ref_as_node(annotation));

    memset(&g_capture, 0, sizeof(g_capture));
    dm_draw_objs(v);

    DMCHECK(g_capture.line3d_count >= 60,
	    "annotation CARC/NURB/BEZIER emitted 3D line output");
    DMCHECK(VNEAR_EQUAL(g_capture.first_3d_a, pts[0], SMALL_FASTF) &&
	    VNEAR_EQUAL(g_capture.first_3d_b, pts[1], SMALL_FASTF),
	    "annotation line segment preserved model-space endpoints");

    bsg_node_ref_destroy(bsg_annotation_ref_as_node(annotation));
    bsg_view_free(v);
    bu_free(v, "test view");
    dmp->i->dm_drawLine2D = save_line2d;
    dmp->i->dm_drawLine3D = save_line3d;
    dmp->i->dm_drawString2D = save_string2d;
    dm_close(dmp);
}

static void
test_annotation_display_text_position(void)
{
    bu_log("=== backend draw item: annotation display text position ===\n");

    struct dm *dmp = open_null_dm();
    DMCHECK(dmp != NULL, "opened null display manager");
    if (!dmp)
	return;

    int (*save_line2d)(struct dm *, fastf_t, fastf_t, fastf_t, fastf_t) =
	dmp->i->dm_drawLine2D;
    int (*save_line3d)(struct dm *, point_t, point_t) =
	dmp->i->dm_drawLine3D;
    int (*save_string2d)(struct dm *, const char *, fastf_t, fastf_t, int, int) =
	dmp->i->dm_drawString2D;
    int (*save_string2d_rot)(struct dm *, const char *, fastf_t, fastf_t, int, int, fastf_t) =
	dmp->i->dm_drawString2DRot;
    dmp->i->dm_drawLine2D = capture_draw_line_2d;
    dmp->i->dm_drawLine3D = capture_draw_line_3d;
    dmp->i->dm_drawString2D = capture_draw_string_2d;
    dmp->i->dm_drawString2DRot = capture_draw_string_2d_rot;

    dm_set_width(dmp, 200);
    dm_set_height(dmp, 200);
    dmp->i->dm_aspect = 1.0;

    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    v->dmp = dmp;
    v->gv_width = 200;
    v->gv_height = 200;
    MAT_IDN(v->gv_model2view);
    MAT_IDN(v->gv_view2model);

    bsg_separator_ref root = bsg_view_scene_separator_ref(v, 1);
    bsg_annotation_ref annotation =
	bsg_annotation_ref_create(v, "dm_annotation_text");
    DMCHECK(!bsg_node_ref_is_null(bsg_annotation_ref_as_node(annotation)),
	    "created text annotation geometry");

    point_t pts[1] = {VINIT_ZERO};
    VSET(pts[0], 0.0, 0.0, 0.0);

    struct bsg_annotation_segment seg;
    memset(&seg, 0, sizeof(seg));
    seg.kind = BSG_ANNOTATION_SEGMENT_TEXT;
    seg.data.text.ref_pt = 0;
    seg.data.text.relative_position = BSG_ANNOTATION_TEXT_POS_TOP_RIGHT;
    seg.data.text.text = (char *)"ABCD";
    seg.data.text.size = 20.0;
    seg.data.text.rotation = 45.0;

    mat_t model_mat, display_mat;
    MAT_IDN(model_mat);
    MAT_IDN(display_mat);
    DMCHECK(bsg_annotation_ref_set_record(annotation, "display text",
		BSG_ANNOTATION_SPACE_DISPLAY, pts[0], model_mat, display_mat,
		(const point_t *)pts, 1, &seg, 1),
	    "configured display-space annotation text");
    bsg_separator_ref_append_child(root, bsg_annotation_ref_as_node(annotation));

    memset(&g_capture, 0, sizeof(g_capture));
    dm_draw_objs(v);

    fastf_t expected_width = 2.0 * 20.0 * 0.6 * 4.0 / 200.0;
    fastf_t expected_height = 2.0 * 20.0 / 200.0;
    DMCHECK(g_capture.string2d_count == 1,
	    "annotation text emitted one native string draw");
    DMCHECK(strcmp(g_capture.first_string, "ABCD") == 0,
	    "annotation text preserves string");
    DMCHECK(NEAR_EQUAL(g_capture.first_string_x, -expected_width, 1.0e-6) &&
	    NEAR_EQUAL(g_capture.first_string_y, -expected_height, 1.0e-6),
	    "annotation top-right relative position offsets text from anchor");
    DMCHECK(g_capture.first_string_size == 20 &&
	    g_capture.first_string_use_aspect == 1,
	    "annotation text forwards size and aspect intent");
    DMCHECK(g_capture.string2d_rot_count == 1 &&
	    NEAR_EQUAL(g_capture.first_string_rotation, 45.0, SMALL_FASTF),
	    "annotation text forwards rotation intent");
    DMCHECK(g_capture.line2d_count == 0 && g_capture.line3d_count == 0,
	    "annotation text draw does not emit line fallback geometry");

    bsg_node_ref_destroy(bsg_annotation_ref_as_node(annotation));
    bsg_view_free(v);
    bu_free(v, "test view");
    dmp->i->dm_drawLine2D = save_line2d;
    dmp->i->dm_drawLine3D = save_line3d;
    dmp->i->dm_drawString2D = save_string2d;
    dmp->i->dm_drawString2DRot = save_string2d_rot;
    dm_close(dmp);
}

static void
test_generic_rotated_text_stroke_fallback(void)
{
    bu_log("=== generic DM rotated text stroke fallback ===\n");

    struct dm *dmp = open_null_dm();
    DMCHECK(dmp != NULL, "opened null display manager");
    if (!dmp)
	return;

    int (*save_line2d)(struct dm *, fastf_t, fastf_t, fastf_t, fastf_t) =
	dmp->i->dm_drawLine2D;
    int (*save_string2d)(struct dm *, const char *, fastf_t, fastf_t, int, int) =
	dmp->i->dm_drawString2D;
    int (*save_string2d_rot)(struct dm *, const char *, fastf_t, fastf_t, int, int, fastf_t) =
	dmp->i->dm_drawString2DRot;
    dmp->i->dm_drawLine2D = capture_draw_line_2d;
    dmp->i->dm_drawString2D = capture_draw_string_2d;
    dmp->i->dm_drawString2DRot = NULL;

    dm_set_width(dmp, 320);
    dm_set_height(dmp, 240);
    dmp->i->dm_aspect = 320.0 / 240.0;
    dm_set_fontsize(dmp, 12);

    memset(&g_capture, 0, sizeof(g_capture));
    int ret = dm_draw_string_2d_rot(dmp, "A", 0.0, 0.0, 20, 1, 30.0);
    DMCHECK(ret == BRLCAD_OK, "stroke fallback reports success");
    DMCHECK(g_capture.line2d_count > 0,
	    "rotated fallback emits vector line strokes");
    DMCHECK(g_capture.string2d_count == 0,
	    "rotated fallback does not emit unrotated native text");

    memset(&g_capture, 0, sizeof(g_capture));
    ret = dm_draw_string_2d_rot(dmp, "A", 0.0, 0.0, 20, 1, 0.0);
    DMCHECK(ret == BRLCAD_OK, "zero-degree fallback reports success");
    DMCHECK(g_capture.string2d_count == 1 && g_capture.line2d_count == 0,
	    "zero-degree text keeps native backend string path");

    dmp->i->dm_drawLine2D = save_line2d;
    dmp->i->dm_drawString2D = save_string2d;
    dmp->i->dm_drawString2DRot = save_string2d_rot;
    dm_close(dmp);
}

static void
test_postscript_rotated_text_output(const char *source_root)
{
    if (!source_root)
	return;

    bu_log("=== postscript backend: rotated native text output ===\n");

    struct bu_vls path = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&path, "%s/src/libdm/postscript/dm-ps.c", source_root);

    DMCHECK(file_has_token(bu_vls_cstr(&path), "ps_drawString2DRot") == 1,
	    "PostScript backend defines native rotated string draw");
    DMCHECK(file_has_token(bu_vls_cstr(&path), "translate %g rotate") == 1,
	    "PostScript backend writes native rotate transform");
    DMCHECK(file_has_token(bu_vls_cstr(&path), "ps_drawString2DRot,") == 1,
	    "PostScript backend registers rotated string draw hook");

    bu_vls_free(&path);
}

static void
test_plot_rotated_text_output(const char *source_root)
{
    if (!source_root)
	return;

    bu_log("=== plot backend: rotated vector text output ===\n");

    struct bu_vls path = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&path, "%s/src/libdm/plot/dm-plot.c", source_root);

    DMCHECK(file_has_token(bu_vls_cstr(&path), "plot_drawString2DRot") == 1,
	    "Plot backend defines native rotated string draw");
    DMCHECK(file_has_token(bu_vls_cstr(&path), "pd_symbol") == 1,
	    "Plot backend writes rotated vector text");
    DMCHECK(file_has_token(bu_vls_cstr(&path), "plot_drawString2DRot,") == 1,
	    "Plot backend registers rotated string draw hook");

    bu_vls_free(&path);
}

static int
file_has_token(const char *path, const char *token)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
	return -1;

    int found = 0;
    char buf[4096];
    while (bu_fgets(buf, sizeof(buf), fp)) {
	if (strstr(buf, token)) {
	    found = 1;
	    break;
	}
    }
    fclose(fp);
    return found;
}

static void
test_retained_gl_source_hygiene(const char *source_root)
{
    if (!source_root)
	return;

    bu_log("=== retained GL source hygiene ===\n");

    struct bu_vls path = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&path, "%s/src/libdm/dm-gl_lod.cpp", source_root);

    const char *needles[] = {
	"dm_draw_device_vlist(",
	"dm_draw_device_vlist_hidden_line(",
	"dlist",
	"display list",
	"fallback"
    };
    for (size_t i = 0; i < sizeof(needles) / sizeof(needles[0]); i++) {
	int ret = file_has_token(bu_vls_cstr(&path), needles[i]);
	if (ret < 0) {
	    DMCHECK(0, "could not open retained GL source file");
	    break;
	}
	if (ret) {
	    struct bu_vls msg = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&msg, "retained GL source contains forbidden token: %s", needles[i]);
	    DMCHECK(0, bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	}
    }

    bu_vls_free(&path);
}

static void
test_public_dm_header_hygiene(const char *source_root)
{
    if (!source_root)
	return;

    bu_log("=== public dm.h vlist hygiene ===\n");

    struct bu_vls path = BU_VLS_INIT_ZERO;
    bu_vls_sprintf(&path, "%s/include/dm.h", source_root);

    const char *needles[] = {
	"bsg/vlist.h",
	"bsg_vlist",
	"drawVList",
	"dm_draw("
    };
    for (size_t i = 0; i < sizeof(needles) / sizeof(needles[0]); i++) {
	int ret = file_has_token(bu_vls_cstr(&path), needles[i]);
	if (ret < 0) {
	    DMCHECK(0, "could not open public dm.h");
	    break;
	}
	if (ret) {
	    struct bu_vls msg = BU_VLS_INIT_ZERO;
	    bu_vls_sprintf(&msg, "public dm.h contains legacy vlist token: %s", needles[i]);
	    DMCHECK(0, bu_vls_cstr(&msg));
	    bu_vls_free(&msg);
	}
    }

    bu_vls_sprintf(&path, "%s/include/dm/vlist.h", source_root);
    DMCHECK(file_has_token(bu_vls_cstr(&path), "dm_draw(") == 1,
	    "legacy dm_draw prototype is isolated in dm/vlist.h");
    DMCHECK(file_has_token(bu_vls_cstr(&path), "drawVList") == 0,
	    "direct drawVList hooks remain backend-local and out of dm/vlist.h");

    bu_vls_free(&path);
}

int
main(int argc, const char **argv)
{
    bu_setprogname(argv[0]);

    test_requires_draw_item();
    test_backend_resource_cache_contract();
    test_annotation_curve_dm_output();
    test_annotation_display_text_position();
    test_generic_rotated_text_stroke_fallback();
    test_postscript_rotated_text_output(argc > 1 ? argv[1] : NULL);
    test_plot_rotated_text_output(argc > 1 ? argv[1] : NULL);
    test_retained_gl_source_hygiene(argc > 1 ? argv[1] : NULL);
    test_public_dm_header_hygiene(argc > 1 ? argv[1] : NULL);

    if (g_fail) {
	bu_log("%d backend draw item test failure(s)\n", g_fail);
	return 1;
    }

    bu_log("backend draw item tests passed\n");
    return 0;
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
