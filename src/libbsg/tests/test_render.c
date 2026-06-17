/*            T E S T _ R E N D E R . C
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
/** @file libbsg/tests/test_render.c
 *
 * Phase 8 unit tests: render-request pre-render traversal.
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bsg/defines.h"
#include "bsg/util.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "bsg/overlay.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/scene_builder.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)


static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "render_test_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "render_test_view");
}


/* ------------------------------------------------------------------ */
/* Test 1: create / destroy                                             */
/* ------------------------------------------------------------------ */

static int
test_create_destroy(void)
{
    printf("=== Test 1: create_destroy ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    if (!req) FAIL("bsg_render_request_create returned NULL");

    if (bsg_render_request_view(req) != v)    FAIL("view pointer mismatch");
    if (!root || !v->gv_scene) FAIL("scene attachment missing");
    if (bsg_render_request_dmp(req) != NULL) FAIL("dmp should be NULL");

    bsg_render_request_destroy(req);
    bsg_render_request_destroy(NULL);  /* must not crash */

    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("create_destroy");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 2: execute on empty subtree returns 0                          */
/* ------------------------------------------------------------------ */

static int
test_empty_subtree(void)
{
    printf("=== Test 2: empty_subtree ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);

    int n = bsg_render_request_execute(req);
    if (n != 0) FAIL("empty subtree should dispatch 0");

    bsg_render_request_destroy(req);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("empty_subtree");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 3: visible-only flag skips DOWN shapes                         */
/* ------------------------------------------------------------------ */

static int
test_visible_only(void)
{
    printf("=== Test 3: visible_only ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    bsg_node_set_name(s1, "batch_s1");
    bsg_node_set_name(s2, "batch_s2");

    /* Attach shapes to root so render traversal can reach them. */
    bsg_node_add_child(root, s1);
    bsg_node_add_child(root, s2);

    bsg_node_set_visible_state(s1, 1);
    bsg_node_set_visible_state(s2, 0);

    struct bsg_render_request *req =
	bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);

    int n = bsg_render_request_execute(req);
    /* bsg_render_request_execute returns the count of shapes that passed all
     * active filters, regardless of whether PAYLOAD_PREPARE is set.  With
     * VISIBLE_ONLY: s1 (UP) passes, s2 (DOWN) is skipped — expect count = 1. */
    if (n != 1) FAIL("only 1 visible shape should be counted");

    bsg_render_request_destroy(req);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("visible_only");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4: NULL request returns -1                                     */
/* ------------------------------------------------------------------ */

static int
test_null_request(void)
{
    printf("=== Test 4: null_request ===\n");

    int n = bsg_render_request_execute(NULL);
    if (n != -1) FAIL("execute(NULL) should return -1");

    PASS("null_request");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 5: retained batch collection + dispatch                         */
/* ------------------------------------------------------------------ */

struct batch_adapter_counts {
    int begin_count;
    int prepare_count;
    int draw_count;
    int end_count;
    unsigned int caps_seen;
};

static int
_batch_adapter_begin(void *ctx, struct bsg_render_request *req)
{
    struct batch_adapter_counts *counts = (struct batch_adapter_counts *)ctx;
    if (!counts || !req)
	return 0;
    counts->begin_count++;
    {
	unsigned char color[3] = {77, 0, 0};
	bsg_render_request_set_geometry_default_color(req, color);
    }
    return 1;
}

static int
_batch_adapter_prepare(void *ctx, const struct bsg_render_item *item)
{
    struct batch_adapter_counts *counts = (struct batch_adapter_counts *)ctx;
    if (!counts || !item)
	return 0;
    counts->prepare_count++;
    counts->caps_seen = item->context.backend_capabilities;
    if (item->context.geometry_default_color[0] != 77)
	return 0;
    return 1;
}

static void
_batch_adapter_draw(void *ctx, const struct bsg_render_item *item)
{
    struct batch_adapter_counts *counts = (struct batch_adapter_counts *)ctx;
    if (!counts || !item)
	return;
    counts->draw_count++;
}

static unsigned int
_batch_adapter_capabilities(void *UNUSED(ctx))
{
    return BSG_ADAPTER_CAP_WIREFRAME | BSG_ADAPTER_CAP_SHADED;
}

static void
_batch_adapter_end(void *ctx, const struct bsg_render_request *UNUSED(req))
{
    struct batch_adapter_counts *counts = (struct batch_adapter_counts *)ctx;
    if (counts)
	counts->end_count++;
}

static int
test_render_batch_collect_dispatch(void)
{
    printf("=== Test 5: render_batch_collect_dispatch ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    bsg_node_add_child(root, s1);
    bsg_node_add_child(root, s2);

    struct batch_adapter_counts counts = {0, 0, 0, 0, 0};
    static struct bsg_backend_adapter adapter = {
	_batch_adapter_begin,
	_batch_adapter_prepare,
	_batch_adapter_draw,
	NULL,
	NULL,
	_batch_adapter_capabilities,
	_batch_adapter_end
    };

    struct bsg_render_request *req = bsg_render_request_create(v, &counts);
    if (!req) FAIL("render request create failed");
    bsg_render_request_set_flags(req, BSG_RENDER_FLAG_VISIBLE_ONLY);
    bsg_render_request_set_adapter(req, &adapter);

    struct bsg_render_batch *batch = bsg_render_batch_create();
    if (!batch) FAIL("render batch create failed");

    int n = bsg_render_request_collect(req, batch);
    if (n != 2) FAIL("expected two collected batch items");
    if (bsg_render_batch_count(batch) != 2) FAIL("batch should own two items");
    if (counts.begin_count != 1) FAIL("adapter begin should run during collection");
    if (counts.prepare_count != 0 || counts.draw_count != 0)
	FAIL("collection must not prepare or draw backend items");

    const struct bsg_render_item *i0 = bsg_render_batch_get(batch, 0);
    const struct bsg_render_item *i1 = bsg_render_batch_get(batch, 1);
    if (!i0 || !i1) FAIL("batch item lookup failed");
    if (i0->sequence != 0 || !i0->source.source_id)
	FAIL("first batch item should preserve phase insertion order");
    if (i1->sequence != 1 || !i1->source.source_id)
	FAIL("second batch item should preserve phase insertion order");

    int dispatched = bsg_render_batch_dispatch(req, batch);
    if (dispatched != 2) FAIL("expected two dispatched batch items");
    if (counts.prepare_count != 2) FAIL("adapter prepare should consume batch items");
    if (counts.draw_count != 2) FAIL("adapter draw should consume batch items");
    if (counts.end_count != 1) FAIL("adapter end should run after batch dispatch");
    if (counts.caps_seen != (BSG_ADAPTER_CAP_WIREFRAME | BSG_ADAPTER_CAP_SHADED))
	FAIL("batch item context should retain backend capabilities");

    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("render_batch_collect_dispatch");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 6: transparency back-to-front sort                             */
/* ------------------------------------------------------------------ */

static void
_make_transparent(bsg_node *s, fastf_t transparency)
{
    bsg_appearance_set_transparency(s, transparency);
    bsg_appearance_set_line_width(s, 1);
}

static int
test_transparency_sort(void)
{
    printf("=== Test 5: transparency_sort ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);

    /* Two transparent shapes at different model depths, attached via
     * transform nodes.  The view's model2view is identity, so the shapes'
     * view-space Z equals their model translation Z.  In BRL-CAD view space
     * more-negative Z is farther from the camera, so the Z=-10 shape must be
     * drawn before the Z=+10 shape (back-to-front). */
    mat_t mfar, mnear;
    MAT_IDN(mfar);
    MAT_IDN(mnear);
    mfar[MDZ]  = -10.0;   /* translation Z = -10 (farther) */
    mnear[MDZ] = 10.0;    /* translation Z = +10 (nearer) */

    bsg_node *tnear = bsg_transform_create(v);
    bsg_node *tfar  = bsg_transform_create(v);
    bsg_transform_set_matrix(tnear, mnear);
    bsg_transform_set_matrix(tfar, mfar);

    bsg_node *snear = bsg_shape_create(v);
    bsg_node *sfar  = bsg_shape_create(v);
    bsg_node_set_name(snear, "near_shape");
    bsg_node_set_name(sfar, "far_shape");
    _make_transparent(snear, 0.5);
    _make_transparent(sfar, 0.5);

    bsg_node_add_child(tnear, snear);
    bsg_node_add_child(tfar, sfar);

    /* Insert near first, far second — the sort must reorder them. */
    bsg_node_add_child(root, tnear);
    bsg_node_add_child(root, tfar);

    struct bu_ptbl items = BU_PTBL_INIT_ZERO;
    bu_ptbl_init(&items, 8, "collected items");

    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_COLLECT_ITEMS |
	    BSG_RENDER_FLAG_SORTED_ALPHA);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 2) FAIL("expected 2 transparent items collected");
    if (BU_PTBL_LEN(&items) != 2) FAIL("items table should hold 2 entries");

    struct bsg_render_item *i0 = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    struct bsg_render_item *i1 = (struct bsg_render_item *)BU_PTBL_GET(&items, 1);
    if (i0->phase != BSG_RENDER_PHASE_TRANSPARENT)
	FAIL("first item should be in the transparent phase");
    if (!(i0->sort_key > i1->sort_key))
	FAIL("farther shape (Z=-10) must be drawn first (back-to-front)");
    if (i1->sort_key >= i0->sort_key)
	FAIL("nearer shape (Z=+10) must be drawn second");

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&items, i));
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);

    bsg_shape_destroy(snear);
    bsg_shape_destroy(sfar);
    bsg_transform_destroy(tnear);
    bsg_transform_destroy(tfar);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("transparency_sort");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 7: overlay ordering                                            */
/* ------------------------------------------------------------------ */

static int
test_overlay_ordering(void)
{
    printf("=== Test 6: overlay_ordering ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_scene_ref root_ref = BSG_SCENE_REF_NULL_INIT;
    root_ref.opaque = root;

    /* Three overlay shapes registered with distinct (ordering, sort_order)
     * keys.  The overlay sort key is ordering*1000 + sort_order and the
     * bucket is sorted ascending, so the expected draw order is b, a, c. */
    bsg_scene_ref oa = bsg_scene_shape_create(v, "overlay_a");  /* SCREEN(1)*1000 + 5 = 1005 */
    bsg_scene_ref ob = bsg_scene_shape_create(v, "overlay_b");  /* MODEL(0)*1000  + 2 =    2 */
    bsg_scene_ref oc = bsg_scene_shape_create(v, "overlay_c");  /* XRAY(2)*1000   + 0 = 2000 */

    if (!bsg_overlay_register_scene_owner(oa, v, BSG_OVERLAY_ROLE_SCREEN,
	    BSG_OVERLAY_CLASS_MEASURE, BSG_OVERLAY_LC_PERSISTENT,
	    BSG_OVERLAY_ORDER_SCREEN, NULL, 5))
	FAIL("register overlay a");
    if (!bsg_overlay_register_scene_owner(ob, v, BSG_OVERLAY_ROLE_MODEL,
	    BSG_OVERLAY_CLASS_MEASURE, BSG_OVERLAY_LC_PERSISTENT,
	    BSG_OVERLAY_ORDER_MODEL, NULL, 2))
	FAIL("register overlay b");
    if (!bsg_overlay_register_scene_owner(oc, v, BSG_OVERLAY_ROLE_XRAY,
	    BSG_OVERLAY_CLASS_MEASURE, BSG_OVERLAY_LC_PERSISTENT,
	    BSG_OVERLAY_ORDER_XRAY, NULL, 0))
	FAIL("register overlay c");

    /* Insert in the unsorted order a, b, c. */
    bsg_scene_append_child(root_ref, oa);
    bsg_scene_append_child(root_ref, ob);
    bsg_scene_append_child(root_ref, oc);

    struct bu_ptbl items = BU_PTBL_INIT_ZERO;
    bu_ptbl_init(&items, 8, "collected items");

    struct bsg_render_request *req = bsg_render_request_create(v, NULL);
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_COLLECT_ITEMS);
    bsg_render_request_set_output_items(req, &items);

    int n = bsg_render_request_execute(req);
    if (n != 3) FAIL("expected 3 overlay items collected");
    if (BU_PTBL_LEN(&items) != 3) FAIL("items table should hold 3 entries");

    struct bsg_render_item *i0 = (struct bsg_render_item *)BU_PTBL_GET(&items, 0);
    struct bsg_render_item *i1 = (struct bsg_render_item *)BU_PTBL_GET(&items, 1);
    struct bsg_render_item *i2 = (struct bsg_render_item *)BU_PTBL_GET(&items, 2);
    if (i0->phase != BSG_RENDER_PHASE_OVERLAY)
	FAIL("collected items should be in the overlay phase");
    if (i0->sort_key != 2) FAIL("overlay b (key 2) should sort first");
    if (i1->sort_key != 1005) FAIL("overlay a (key 1005) should sort second");
    if (i2->sort_key != 2000) FAIL("overlay c (key 2000) should sort third");

    for (size_t i = 0; i < BU_PTBL_LEN(&items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(&items, i));
    bu_ptbl_free(&items);
    bsg_render_request_destroy(req);

    bsg_scene_ref_destroy(oa);
    bsg_scene_ref_destroy(ob);
    bsg_scene_ref_destroy(oc);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("overlay_ordering");
    return 0;
}


/* ------------------------------------------------------------------ */
/* main                                                                 */
/* ------------------------------------------------------------------ */

int
main(int UNUSED(argc), const char **argv)
{
    bu_setprogname(argv[0]);
    int failures = 0;

    failures += test_create_destroy();
    failures += test_empty_subtree();
    failures += test_visible_only();
    failures += test_null_request();
    failures += test_render_batch_collect_dispatch();
    failures += test_transparency_sort();
    failures += test_overlay_ordering();

    if (failures == 0)
	printf("RESULT: all Phase 8 render tests PASSED\n");
    else
	printf("RESULT: %d test(s) FAILED\n", failures);

    return failures;
}

/*
 * Local Variables:
 * tab-width: 8
 * mode: C
 * indent-tabs-mode: t
 * c-file-style: "stroustrup"
 * End:
 * ex: shiftwidth=4 tabstop=8
 */
