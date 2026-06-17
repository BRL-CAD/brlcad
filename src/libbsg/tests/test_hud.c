/*                  T E S T _ H U D . C
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
/** @file libbsg/tests/test_hud.c
 *
 * Unit tests for the BSG HUD root and overlay metadata API.
 */

#include "common.h"
#include "../node_private.h"
#include "../appearance_private.h"
#include "../geometry_private.h"
#include "../material_private.h"
#include "../payload_private.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/ptbl.h"
#include "bu/vls.h"
#include "bsg/defines.h"
#include "bsg/node.h"
#include "../payload_typed_private.h"
#include "bsg/payload.h"
#include "bsg/util.h"
#include "bsg/hud.h"
#include "../hud_private.h"
#include "../object_private.h"
#include "bsg/view_state.h"


/* -----------------------------------------------------------------------
 * Minimal view factory using the proper bsg_view_init / bsg_view_free API.
 * ----------------------------------------------------------------------- */

static struct bsg_view *
_make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "test_hud_view");
    return v;
}

static void
_free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_hud_root_destroy(v);
    bsg_view_free(v);
    bu_free(v, "test_hud view");
}

static int
node_is_a(bsg_node *node, bsg_type_id type)
{
    if (!node)
	return 0;
    return bsg_node_is_a(bsg_node_ref_from_object(bsg_object_ref_from_node(node)),
	    type);
}

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while(0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while(0)
#define TEST(name) do { printf("=== Test %d: %s ===\n", ++_tc, (name)); } while(0)

static void
set_center_dot(struct bsg_view *v, int draw, int color)
{
    struct bsg_other_state s;
    bsg_view_center_dot_get(v, &s);
    s.gos_draw = draw;
    if (color)
	VSET(s.gos_line_color, 1, 2, 3);
    bsg_view_center_dot_set(v, &s);
}

static void
set_axes_draw(struct bsg_view *v, int model, int draw)
{
    struct bsg_axes axes;
    if (model) {
	bsg_view_model_axes_get(v, &axes);
	axes.draw = draw;
	bsg_view_model_axes_set(v, &axes);
    } else {
	bsg_view_view_axes_get(v, &axes);
	axes.draw = draw;
	bsg_view_view_axes_set(v, &axes);
    }
}

static void
set_grid_draw(struct bsg_view *v, int draw)
{
    struct bsg_grid_state grid;
    bsg_view_grid_get(v, &grid);
    grid.draw = draw;
    bsg_view_grid_set(v, &grid);
}

static void
set_params_draw(struct bsg_view *v, int draw, int font_size)
{
    struct bsg_params_state params;
    bsg_view_params_get(v, &params);
    params.draw = draw;
    params.font_size = font_size;
    bsg_view_params_set(v, &params);
}

static void
set_other_draw(struct bsg_view *v, int scale, int draw)
{
    struct bsg_other_state s;
    if (scale) {
	bsg_view_scale_state_get(v, &s);
	s.gos_draw = draw;
	bsg_view_scale_state_set(v, &s);
    } else {
	bsg_view_center_dot_get(v, &s);
	s.gos_draw = draw;
	bsg_view_center_dot_set(v, &s);
    }
}

static void
set_adc_draw(struct bsg_view *v, int draw)
{
    struct bsg_adc_state adc;
    bsg_view_adc_get(v, &adc);
    adc.draw = draw;
    bsg_view_adc_set(v, &adc);
}

static void
set_rect_draw(struct bsg_view *v, int draw)
{
    struct bsg_interactive_rect_state rect;
    bsg_view_interactive_rect_get(v, &rect);
    rect.draw = draw;
    bsg_view_interactive_rect_set(v, &rect);
}

static int _tc = 0;


/* -----------------------------------------------------------------------
 * Test 1: bsg_hud_root_create NULL guard
 * ----------------------------------------------------------------------- */
static int
test_null_guard(void)
{
    TEST("bsg_hud_root_create NULL view");
    bsg_node *r = bsg_hud_root_create(NULL);
    if (r != NULL)
	FAIL("Expected NULL for NULL view");
    PASS("bsg_hud_root_create NULL view");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 2: bsg_hud_root_create creates a valid root
 * ----------------------------------------------------------------------- */
static int
test_create(void)
{
    TEST("bsg_hud_root_create creates root");
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_hud_root_create(v);
    if (!root)
	FAIL("bsg_hud_root_create returned NULL");
    if (v->gv_hud_root != root)
	FAIL("gv_hud_root not set");
    if (!node_is_a(root, bsg_group_type()))
	FAIL("root is not a BSGGroup");
    _free_view(v);
    PASS("bsg_hud_root_create creates root");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 3: root has exactly BSG_HUD_FEATURE_COUNT children
 * ----------------------------------------------------------------------- */
static int
test_child_count(void)
{
    TEST("HUD root has correct child count");
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_hud_root_create(v);
    if (!root)
	FAIL("bsg_hud_root_create returned NULL");
    size_t n = bsg_node_child_count(root);
    if ((int)n != BSG_HUD_FEATURE_COUNT)
	FAIL("Wrong number of HUD children");
    _free_view(v);
    PASS("HUD root has correct child count");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 4: all children start disabled
 * ----------------------------------------------------------------------- */
static int
test_children_initially_down(void)
{
    TEST("HUD children initially DOWN");
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_hud_root_create(v);
    if (!root)
	FAIL("bsg_hud_root_create returned NULL");
    for (int i = 0; i < BSG_HUD_FEATURE_COUNT; i++) {
	bsg_node *c = bsg_node_child_at(root, (size_t)i);
	if (!c)
	    FAIL("NULL child node");
	if (bsg_node_visible(c))
	    FAIL("Child not initially DOWN");
    }
    _free_view(v);
    PASS("HUD children initially DOWN");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 5: each child carries valid HUD meta and payload records
 * ----------------------------------------------------------------------- */
static int
test_children_have_meta(void)
{
    TEST("HUD children have meta and payload snapshots");
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_hud_root_create(v);
    if (!root)
	FAIL("bsg_hud_root_create returned NULL");
    for (int i = 0; i < BSG_HUD_FEATURE_COUNT; i++) {
	bsg_node *c = bsg_node_child_at(root, (size_t)i);
	if (!c)
	    FAIL("NULL child node");
	struct bsg_hud_node_meta *m = bsg_hud_node_get_meta(c);
	if (!m)
	    FAIL("bsg_hud_node_get_meta returned NULL");
	const struct bsg_hud_payload *p = bsg_hud_node_get_payload(c);
	if (!p)
	    FAIL("bsg_hud_node_get_payload returned NULL");
	if ((int)m->feature_type != i)
	    FAIL("feature_type does not match sort order index");
	if ((int)p->feature_type != i)
	    FAIL("payload feature_type does not match sort order index");
	if (m->sort_order != i)
	    FAIL("sort_order does not match index");
    }
    _free_view(v);
    PASS("HUD children have OVERLAY payload, meta, and payload snapshot");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 6: bsg_hud_sync copies faceplate payload state
 * ----------------------------------------------------------------------- */
static int
test_sync_payloads(void)
{
    TEST("bsg_hud_sync updates payload snapshots");
    struct bsg_view *v = _make_view();
    set_center_dot(v, 1, 0);
    set_center_dot(v, 1, 1);
    bsg_view_set_framebuffer_mode(v, 2);

    int rc = bsg_hud_sync(v);
    if (rc != 0)
	FAIL("bsg_hud_sync returned error");

    bsg_node *root = bsg_hud_root_get(v);
    const struct bsg_hud_payload *center =
	bsg_hud_node_get_payload(bsg_node_child_at(root, BSG_HUD_FEATURE_CENTER_DOT));
    const struct bsg_hud_payload *fb =
	bsg_hud_node_get_payload(bsg_node_child_at(root, BSG_HUD_FEATURE_FRAMEBUFFER));
    if (!center || !fb)
	FAIL("missing HUD payload snapshots");
    if (center->data.other.gos_line_color[0] != 1 ||
	center->data.other.gos_line_color[1] != 2 ||
	center->data.other.gos_line_color[2] != 3)
	FAIL("center-dot payload colors not copied");
    if (fb->data.framebuffer.mode != 2)
	FAIL("framebuffer mode not copied");
    if (!bsg_node_visible(bsg_node_child_at(root, BSG_HUD_FEATURE_FRAMEBUFFER)))
	FAIL("framebuffer feature should be enabled");

    _free_view(v);
    PASS("bsg_hud_sync updates payload snapshots");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 7: bsg_hud_sync with center_dot enabled → child UP
 * ----------------------------------------------------------------------- */
static int
test_sync_center_dot(void)
{
    TEST("bsg_hud_sync enables center_dot child");
    struct bsg_view *v = _make_view();
    set_center_dot(v, 1, 0);

    int rc = bsg_hud_sync(v);
    if (rc != 0)
	FAIL("bsg_hud_sync returned error");

    bsg_node *root = bsg_hud_root_get(v);
    if (!root)
	FAIL("HUD root not created by sync");

    /* feature index 0 = CENTER_DOT */
    bsg_node *c = bsg_node_child_at(root, 0);
    if (!c)
	FAIL("NULL center_dot child");
    if (!bsg_node_visible(c))
	FAIL("center_dot child not UP after sync");

    /* All other features should still be DOWN */
    for (int i = 1; i < BSG_HUD_FEATURE_COUNT; i++) {
	bsg_node *other = bsg_node_child_at(root, (size_t)i);
	if (other && bsg_node_visible(other))
	    FAIL("Unexpected UP child after sync");
    }

    _free_view(v);
    PASS("bsg_hud_sync enables center_dot child");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 8: bsg_hud_sync called twice keeps correct state
 * ----------------------------------------------------------------------- */
static int
test_sync_idempotent(void)
{
    TEST("bsg_hud_sync idempotent on repeated calls");
    struct bsg_view *v = _make_view();
    set_center_dot(v, 1, 0);
    set_axes_draw(v, 1, 1);

    bsg_hud_sync(v);
    set_center_dot(v, 0, 0); /* disable center dot */
    bsg_hud_sync(v);

    bsg_node *root = bsg_hud_root_get(v);
    bsg_node *cdot = bsg_node_child_at(root, 0); /* CENTER_DOT */
    bsg_node *maxes = bsg_node_child_at(root, 1); /* MODEL_AXES */

    if (bsg_node_visible(cdot))
	FAIL("center_dot should be DOWN after second sync");
    if (!bsg_node_visible(maxes))
	FAIL("model_axes should remain UP after second sync");

    _free_view(v);
    PASS("bsg_hud_sync idempotent on repeated calls");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 9: bsg_hud_root_create returns existing root on second call
 * ----------------------------------------------------------------------- */
static int
test_create_idempotent(void)
{
    TEST("bsg_hud_root_create idempotent");
    struct bsg_view *v = _make_view();
    bsg_node *r1 = bsg_hud_root_create(v);
    bsg_node *r2 = bsg_hud_root_create(v);
    if (r1 != r2)
	FAIL("Second create returned different pointer");
    _free_view(v);
    PASS("bsg_hud_root_create idempotent");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 10: overlay enums are distinct and in range
 * ----------------------------------------------------------------------- */
static int
test_enum_values(void)
{
    TEST("overlay/hud enum values are distinct");
    if (BSG_OVERLAY_ROLE_MODEL == BSG_OVERLAY_ROLE_SCREEN ||
	BSG_OVERLAY_ROLE_SCREEN == BSG_OVERLAY_ROLE_XRAY)
	FAIL("overlay role enum values collide");
    if (BSG_OVERLAY_CLASS_FACEPLATE == BSG_OVERLAY_CLASS_EDIT_HANDLE ||
	BSG_OVERLAY_CLASS_SELECTION_RUBBER_BAND == BSG_OVERLAY_CLASS_DIAGNOSTIC)
	FAIL("overlay class enum values collide");
    if (BSG_HUD_COORD_SCREEN_PX == BSG_HUD_COORD_NDC ||
	BSG_HUD_COORD_NDC == BSG_HUD_COORD_VIEW_PLANE)
	FAIL("hud coord enum values collide");
    if (BSG_OVERLAY_LC_PERSISTENT == BSG_OVERLAY_LC_PER_FRAME)
	FAIL("lifecycle enum values collide");
    PASS("overlay/hud enum values are distinct");
    return 0;
}


/* -----------------------------------------------------------------------
 * Test 11: bsg_hud_sync NULL guard
 * ----------------------------------------------------------------------- */
static int
test_sync_null(void)
{
    TEST("bsg_hud_sync NULL view");
    int rc = bsg_hud_sync(NULL);
    if (rc != -1)
	FAIL("Expected -1 for NULL view");
    PASS("bsg_hud_sync NULL view");
    return 0;
}

static int
test_typed_payload_realization(void)
{
    TEST("bsg_hud_sync realizes typed payloads");
    struct bsg_view *v = _make_view();
    set_center_dot(v, 1, 0);
    set_axes_draw(v, 1, 1);
    set_axes_draw(v, 0, 1);
    set_grid_draw(v, 1);
    set_params_draw(v, 1, 0);
    set_params_draw(v, 1, 1);
    set_other_draw(v, 1, 1);
    set_adc_draw(v, 1);
    set_rect_draw(v, 1);
    bsg_view_set_framebuffer_mode(v, 1);

    if (bsg_hud_sync(v) != 0)
	FAIL("bsg_hud_sync returned error");

    bsg_node *root = bsg_hud_root_get(v);
    bsg_node *center  = bsg_node_child_at(root, BSG_HUD_FEATURE_CENTER_DOT);
    bsg_node *mAxes   = bsg_node_child_at(root, BSG_HUD_FEATURE_MODEL_AXES);
    bsg_node *vAxes   = bsg_node_child_at(root, BSG_HUD_FEATURE_VIEW_AXES);
    bsg_node *grid    = bsg_node_child_at(root, BSG_HUD_FEATURE_GRID);
    bsg_node *params  = bsg_node_child_at(root, BSG_HUD_FEATURE_VIEW_PARAMS);
    bsg_node *scale   = bsg_node_child_at(root, BSG_HUD_FEATURE_VIEW_SCALE);
    bsg_node *adc     = bsg_node_child_at(root, BSG_HUD_FEATURE_ADC);
    bsg_node *rect    = bsg_node_child_at(root, BSG_HUD_FEATURE_RECT);
    bsg_node *fb      = bsg_node_child_at(root, BSG_HUD_FEATURE_FRAMEBUFFER);

    if (!center || !mAxes || !vAxes || !grid || !params || !scale || !adc || !rect || !fb)
	FAIL("missing HUD feature nodes");
    if (!bsg_node_get_payload(center) || bsg_node_get_payload(center)->pl_type != BSG_PL_LINE_SET)
	FAIL("center dot should realize as line set");
    if (!bsg_node_get_payload(mAxes) || bsg_node_get_payload(mAxes)->pl_type != BSG_PL_AXES)
	FAIL("model axes should realize as axes payload");
    if (!bsg_node_get_payload(vAxes) || bsg_node_get_payload(vAxes)->pl_type != BSG_PL_AXES)
	FAIL("view axes should realize as axes payload");
    if (!bsg_node_get_payload(grid) || bsg_node_get_payload(grid)->pl_type != BSG_PL_GRID)
	FAIL("grid should realize as grid payload");
    if (!bsg_node_get_payload(params) || bsg_node_get_payload(params)->pl_type != BSG_PL_HUD_TEXT)
	FAIL("view params should realize as HUD text");
    if (!bsg_node_get_payload(scale) || bsg_node_get_payload(scale)->pl_type != BSG_PL_LINE_SET)
	FAIL("view scale should realize as line set");
    if (!bsg_node_get_payload(adc) || bsg_node_get_payload(adc)->pl_type != BSG_PL_LINE_SET)
	FAIL("ADC should realize as line set");
    if (!bsg_node_get_payload(rect) || bsg_node_get_payload(rect)->pl_type != BSG_PL_LINE_SET)
	FAIL("selection rect should realize as line set");
    if (!bsg_node_get_payload(fb) || bsg_node_get_payload(fb)->pl_type != BSG_PL_FRAMEBUFFER)
	FAIL("framebuffer should realize as framebuffer payload");
    if (bsg_geometry_node_kind_get(center) != BSG_GEOMETRY_NODE_LINE_SET ||
	    bsg_geometry_node_kind_get(mAxes) != BSG_GEOMETRY_NODE_OVERLAY ||
	    bsg_geometry_node_kind_get(vAxes) != BSG_GEOMETRY_NODE_OVERLAY ||
	    bsg_geometry_node_kind_get(grid) != BSG_GEOMETRY_NODE_OVERLAY ||
	    bsg_geometry_node_kind_get(params) != BSG_GEOMETRY_NODE_HUD ||
	    bsg_geometry_node_kind_get(fb) != BSG_GEOMETRY_NODE_FRAMEBUFFER)
	FAIL("migrated HUD producers should publish geometry node kinds");
    if (!(bsg_node_get_payload_type(center) & BSG_PAYLOAD_OVERLAY))
	FAIL("HUD payload role should be metadata-backed");
    if (!(bsg_node_get_payload_type(center) & BSG_PAYLOAD_OVERLAY) ||
	    (bsg_node_get_payload_type(center) & BSG_PAYLOAD_CSG))
	FAIL("HUD payload role should report metadata only");
    if (!bsg_hud_node_get_meta(center))
	FAIL("HUD payload role should remain metadata-backed");

    _free_view(v);
    PASS("bsg_hud_sync realizes typed payloads");
    return 0;
}

static int
test_hud_fields_ignore_raw_mirrors(void)
{
    TEST("HUD field state ignores stale raw mirrors");
    struct bsg_view *v = _make_view();
    bsg_node *root = bsg_hud_root_create(v);
    if (!root)
	FAIL("bsg_hud_root_create returned NULL");

    if (!bsg_node_name(root) || !BU_STR_EQUAL(bsg_node_name(root), "_hud_root"))
	FAIL("root name should remain field-backed");
    if (!bsg_node_visible(root))
	FAIL("root visibility should remain field-backed");

    bsg_node *center = bsg_node_child_at(root, BSG_HUD_FEATURE_CENTER_DOT);
    if (!center)
	FAIL("missing center-dot feature node");
    if (!bsg_node_name(center) || !BU_STR_EQUAL(bsg_node_name(center), "_hud_center_dot"))
	FAIL("feature name should remain field-backed");
    if (bsg_node_visible(center))
	FAIL("initial feature visibility should remain field-backed");

    set_center_dot(v, 1, 1);
    set_other_draw(v, 1, 1);
    if (bsg_hud_sync(v) != 0)
	FAIL("bsg_hud_sync returned error");

    center = bsg_node_child_at(root, BSG_HUD_FEATURE_CENTER_DOT);
    if (!center)
	FAIL("missing center-dot feature after sync");

    unsigned char r = 0;
    unsigned char g = 0;
    unsigned char b = 0;
    bsg_material_get_rgb(center, &r, &g, &b);
    if (r != 1 || g != 2 || b != 3)
	FAIL("HUD style color should remain field-backed");
    if (bsg_appearance_line_width(center) != 1)
	FAIL("HUD line width should remain field-backed");

    bsg_node *scale = bsg_node_child_at(root, BSG_HUD_FEATURE_VIEW_SCALE);
    bsg_node *scale_label = scale ? bsg_node_child_at(scale, 0) : NULL;
    if (!scale_label)
	FAIL("missing view-scale label child");
    if (!bsg_node_name(scale_label) || !BU_STR_EQUAL(bsg_node_name(scale_label), "_hud_view_scale_zero"))
	FAIL("transient HUD child name should remain field-backed");
    if (!bsg_node_visible(scale_label))
	FAIL("transient HUD child visibility should remain field-backed");

    _free_view(v);
    PASS("HUD field state ignores stale raw mirrors");
    return 0;
}


/* -----------------------------------------------------------------------
 * Main
 * ----------------------------------------------------------------------- */

int
main(int argc, char **argv)
{
    int fail = 0;

    bu_setprogname(argv[0]);
    (void)argc;

    fail += test_null_guard();
    fail += test_create();
    fail += test_child_count();
    fail += test_children_initially_down();
    fail += test_children_have_meta();
    fail += test_sync_payloads();
    fail += test_sync_center_dot();
    fail += test_sync_idempotent();
    fail += test_create_idempotent();
    fail += test_enum_values();
    fail += test_sync_null();
    fail += test_typed_payload_realization();
    fail += test_hud_fields_ignore_raw_mirrors();

    if (fail)
	printf("\n%d TEST(S) FAILED\n", fail);
    else
	printf("\nALL TESTS PASSED\n");

    return fail;
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
