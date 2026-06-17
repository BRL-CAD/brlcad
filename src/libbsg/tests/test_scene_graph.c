/*              T E S T _ S C E N E _ G R A P H . C
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
/** @file libbsg/tests/test_scene_graph.c
 *
 * Phase 4 / Phase F regression: unit tests for the libbsg scene-graph
 * lifecycle and query helpers.
 *
 * Modern scene-handle semantics:
 *   gv_scene carries an opaque retained-scene handle.  Internally these tests
 *   still create BSG nodes, but callers assert handle availability and render
 *   behavior instead of public scene-root aliases.
 *
 * Tests (no display manager, no .g file required):
 *   1. create_alias  — bsg_init installs a typed scene anchor, and
 *      bsg_scene_root_create returns the existing handle.
 *   2. create_null   — NULL bsg_view input returns NULL without crashing.
 *   3. sync_noop     — bsg_scene_root_sync is a no-op; calling it does not
 *      change root->children.
 *   4. scene_child_lookup — public scene refs locate field-named children.
 *   5. field_authority — standalone roots, independent scopes, and source
 *      roles ignore stale raw mirrors.
 *   6. view_init_copy — semantic view cloning preserves values without
 *      sharing owned scene/settings/view-state storage.
 *   7. sensor_fire   — registered update sensors are invoked by the public
 *      update-sensor fire API.
 *   8. null_guards   — NULL inputs to sync/detach and empty sensor fire must
 *      not crash.
 *
 * Usage: test_bsg_scene_graph
 *   Returns 0 on success, non-zero on failure.
 */

#include "common.h"
#include "../node_private.h"

#include <string.h>

#include "vmath.h"
#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bsg/defines.h"
#include "bsg/draw_source.h"
#include "bsg/node.h"
#include "bsg/sensor.h"
#include "bsg/scene_builder.h"
#include "bsg/util.h"
#include "bsg/view_state.h"
#include "bsg/scene_object.h"
#include "../node_storage_private.h"
#include "../object_private.h"
#include "../scene_object_private.h"

static int g_fail = 0;

#define BSGCHECK(cond, msg) \
    do { \
	if (!(cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
	    g_fail++; \
	} \
    } while (0)

/* ---- helpers -------------------------------------------------------- */

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_GET(v, struct bsg_view);
    bsg_view_init(v, NULL);
    return v;
}

static void
free_view(struct bsg_view *v)
{
    bsg_view_free(v);
    BU_PUT(v, struct bsg_view);
}

/* Replace the initialized root with a synthetic draw-root group so
 * bsg_scene_root_create can exercise the existing-root path.  The caller owns
 * the returned node and must free it when done. */
static bsg_node *
attach_fake_draw_root(struct bsg_view *v)
{
    bsg_node *old_root = v ? (bsg_node *)v->gv_scene : NULL;
    if (old_root) {
	bsg_node_destroy(old_root);
	v->gv_scene = NULL;
    }
    bsg_node *dr = bsg_group_create(v);
    if (!dr)
	return NULL;
    v->gv_scene  = dr;
    return dr;
}

/* ---- Test 1: create / alias / destroy ------------------------------- */
static void
test_create_alias(void)
{
    bu_log("=== Test 1: create_alias ===\n");

    /* bsg_init creates a typed retained scene anchor. */
    struct bsg_view *v = make_view();
    bsg_node *init_root = (bsg_node *)v->gv_scene;
    BSGCHECK(init_root != NULL, "bsg_init creates a scene root");
    BSGCHECK(bsg_node_is_a(bsg_node_ref_from_object(bsg_object_ref_from_node(init_root)),
		bsg_separator_type()), "initialized root is a separator");
    bsg_node *root = bsg_view_scene_root_ensure(v);
    BSGCHECK(root == init_root, "bsg_scene_root_create returns initialized root");
    BSGCHECK(v->gv_scene == root, "view->gv_scene remains the initialized root");
    bsg_view_scene_root_detach_from_root(root);
    v->gv_scene = NULL;
    bsg_node_destroy(root);

    /* Set up a fake draw root and re-run */
    bsg_node *dr = attach_fake_draw_root(v);
    if (!dr) { g_fail++; free_view(v); return; }

    root = bsg_view_scene_root_ensure(v);
    BSGCHECK(root != NULL,               "bsg_view_scene_root_ensure(with draw root) returns non-NULL");
    BSGCHECK(v->gv_scene == root,        "view->gv_scene == returned root");
    BSGCHECK(v->gv_scene != NULL,        "view carries an opaque scene handle");

    /* Destroy: clears gv_scene but does NOT free the scene node. */
    bsg_view_scene_root_detach_from_root(root);
    BSGCHECK(v->gv_scene == NULL,        "view->gv_scene cleared after destroy");
    BSGCHECK(dr != NULL && bsg_node_child_count(dr) == 0,
	     "scene node remains valid after bsg_scene_root_destroy");

    bu_log("  PASS: create/alias/destroy cycle\n");

    /* Clean up the fake draw root manually (bsg_scene_root_destroy does not
     * free it, as it is owned by the draw-tree lifecycle). */
    v->gv_scene = NULL;
    bsg_group_destroy(dr);
    free_view(v);
}

/* ---- Test 2: NULL bsg_view -------------------------------------------- */
static void
test_create_null(void)
{
    bu_log("=== Test 2: create_null ===\n");
    bsg_node *root = bsg_view_scene_root_ensure(NULL);
    BSGCHECK(root == NULL, "bsg_view_scene_root_ensure(NULL) returns NULL");
    bu_log("  PASS: null bsg_view guard\n");
}

/* ---- Test 3: sync is a no-op --------------------------------------- */
static void
test_sync_noop(void)
{
    bu_log("=== Test 3: sync_noop ===\n");
    struct bsg_view *v = make_view();

    bsg_node *dr = attach_fake_draw_root(v);
    if (!dr) { g_fail++; free_view(v); return; }

    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) { g_fail++; v->gv_scene = NULL; free_view(v); return; }
    BSGCHECK(bsg_node_child_count(root) == 0, "children empty before sync");

    bsg_view_scene_root_sync(root, v);
    BSGCHECK(bsg_node_child_count(root) == 0, "sync is a no-op: children still empty");

    bu_log("  PASS: sync_noop\n");

    bsg_view_scene_root_detach_from_root(root);
    v->gv_scene = NULL;
    bsg_group_destroy(dr);
    free_view(v);
}

/* ---- Test 4: scene_child_lookup ------------------------------------ */
static void
test_scene_child_lookup(void)
{
    bu_log("=== Test 4: scene_child_lookup ===\n");
    struct bsg_view *v = make_view();

    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) { g_fail++; free_view(v); return; }

    bsg_scene_ref root_ref = BSG_SCENE_REF_NULL_INIT;
    root_ref.opaque = root;
    bsg_scene_ref group = bsg_scene_group_create(v, "lookup_group");
    bsg_scene_ref child = bsg_scene_shape_create(v, "lookup_shape");
    if (bsg_scene_ref_is_null(group) || bsg_scene_ref_is_null(child)) {
	g_fail++;
	bsg_view_scene_root_detach_from_root(root);
	free_view(v);
	return;
    }

    bsg_scene_append_child(root_ref, group);
    bsg_scene_append_child(root_ref, child);

    bsg_scene_ref found = bsg_scene_find_child(root_ref, "lookup_shape");
    BSGCHECK(bsg_scene_ref_equal(found, child),
	    "bsg_scene_find_child finds the field-named child");

    bsg_scene_ref notfound = bsg_scene_find_child(root_ref, "missing_shape");
    BSGCHECK(bsg_scene_ref_is_null(notfound),
	    "bsg_scene_find_child returns NULL for absent names");

    if (bsg_scene_ref_equal(found, child) && bsg_scene_ref_is_null(notfound))
	bu_log("  PASS: scene_child_lookup\n");

    bsg_scene_ref_destroy(child);
    bsg_scene_ref_destroy(group);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
}

/* ---- Test 5: field_authority --------------------------------------- */
static void
test_field_authority(void)
{
    bu_log("=== Test 5: field_authority ===\n");
    struct bsg_view *v = make_view();

    bsg_node *init_root = (bsg_node *)v->gv_scene;
    if (init_root) {
	bsg_node_destroy(init_root);
	v->gv_scene = NULL;
    }

    bsg_node *root = bsg_view_scene_root_ensure(v);
    BSGCHECK(root != NULL, "standalone root is created");
    BSGCHECK(BU_STR_EQUAL(bsg_node_name(root), "_draw_root"),
	    "standalone root name is field-backed");
    BSGCHECK(bsg_node_visible(root) == 1,
	    "standalone root visibility is field-backed");

    BSGCHECK(BU_STR_EQUAL(bsg_node_name(root), "_draw_root"),
	    "standalone root keeps field-backed name");
    BSGCHECK(bsg_node_visible(root) == 1,
	    "standalone root keeps field-backed visibility");
    BSGCHECK(bsg_node_is_internal_child_source(root) == 1,
	    "standalone root ignores stale raw source mirror");

    bsg_node *shape = bsg_shape_create(v);
    BSGCHECK(shape != NULL, "generic shape is created");
    BSGCHECK(!bsg_node_is_local_source(shape),
	    "generic shape does not inherit allocation-local source");
    BSGCHECK(!bsg_node_is_database_source(shape),
	    "generic shape is not database-sourced without metadata");
    BSGCHECK(!bsg_node_has_geometry_role(shape, BSG_GEOMETRY_DB_OBJECT),
	    "generic shape has no database-object geometry role without metadata");
    bsg_shape_mark_db_object(shape);
    BSGCHECK(bsg_node_is_database_source(shape) == 1,
	    "database shape source metadata is authoritative");
    BSGCHECK(bsg_node_has_geometry_role(shape, BSG_GEOMETRY_DB_OBJECT),
	    "database shape geometry metadata is authoritative");
    bsg_node_destroy(shape);

    bsg_scene_ref scope_ref = bsg_view_independent_scope_ref(v, 1);
    bsg_node *scope = (bsg_node *)scope_ref.opaque;
    BSGCHECK(scope != NULL, "independent scope is created");
    BSGCHECK(BU_STR_EQUAL(bsg_node_name(scope), "_independent_db_scope"),
	    "independent scope name is field-backed");
    BSGCHECK(bsg_node_visible(scope) == 1,
	    "independent scope visibility is field-backed");

    BSGCHECK(bsg_view_is_independent(v) == 1,
	    "independent view detection uses canonical scope metadata");
    BSGCHECK(bsg_scene_ref_equal(bsg_view_independent_scope_ref(v, 0), scope_ref),
	    "independent scope lookup uses canonical name and view metadata");
    BSGCHECK(bsg_node_visible(scope) == 1,
	    "independent scope keeps field-backed visibility");
    BSGCHECK(bsg_node_is_local_source(scope) == 1,
	    "independent scope ignores stale raw local-source mirror");

    bsg_view_independent_scope_destroy(v);
    bsg_view_scene_root_detach_from_root(root);
    bsg_group_destroy(root);
    free_view(v);

    bu_log("  PASS: field_authority\n");
}

/* ---- Test 6: view_init_copy ---------------------------------------- */
static void
test_view_init_copy(void)
{
    bu_log("=== Test 6: view_init_copy ===\n");

    struct bsg_view *src = make_view();
    bu_vls_sprintf(&src->gv_name, "source_view");
    src->gv_scale = 42.0;
    src->gv_size = 84.0;
    src->gv_isize = 1.0 / 84.0;
    src->gv_base2local = 2.0;
    src->gv_local2base = 0.5;
    src->gv_width = 640;
    src->gv_height = 480;
    V2SET(src->gv_wmin, -1.0, -2.0);
    V2SET(src->gv_wmax, 3.0, 4.0);
    src->gv_mouse_x = 12;
    src->gv_mouse_y = 34;
    src->gv_frame_rev = 17;
    bsg_view_set_zclip(src, 1);

    struct bsg_view *dst;
    BU_GET(dst, struct bsg_view);
    bsg_view_init_copy(dst, src, NULL);

    BSGCHECK(BU_STR_EQUAL(bu_vls_cstr(&dst->gv_name), "source_view"),
	    "view copy preserves name");
    BSGCHECK(NEAR_EQUAL(dst->gv_scale, 42.0, SMALL_FASTF),
	    "view copy preserves scale");
    BSGCHECK(NEAR_EQUAL(dst->gv_local2base, 0.5, SMALL_FASTF),
	    "view copy preserves local2base");
    BSGCHECK(NEAR_EQUAL(dst->gv_wmin[X], -1.0, SMALL_FASTF) &&
	    NEAR_EQUAL(dst->gv_wmax[Y], 4.0, SMALL_FASTF),
	    "view copy preserves window bounds");
    BSGCHECK(dst->gv_mouse_x == 12 && dst->gv_mouse_y == 34,
	    "view copy preserves mouse state");
    BSGCHECK(dst->gv_frame_rev == 17,
	    "view copy preserves frame revision");
    BSGCHECK(bsg_view_zclip(dst) == 1,
	    "view copy preserves active view settings");

    BSGCHECK(dst->gv_objs.i != src->gv_objs.i,
	    "view copy does not share scene-store storage");
    BSGCHECK(dst->settings_local != src->settings_local,
	    "view copy does not share settings storage");
    BSGCHECK(dst->settings_active == dst->settings_local,
	    "view copy uses its own local settings");
    BSGCHECK(dst->gv_state != src->gv_state,
	    "view copy does not share view-state storage");
    BSGCHECK(dst->gv_scene != src->gv_scene,
	    "view copy does not share standalone retained scene roots");

    free_view(dst);
    free_view(src);

    bu_log("  PASS: view_init_copy\n");
}

/* ---- Test 7: sensor_fire ------------------------------------------- */
static int g_sensor_fired = 0;

static int
sensor_callback(bsg_update_sensor_ref UNUSED(sensor), void *UNUSED(data))
{
    g_sensor_fired++;
    return 0;
}

static void
test_sensor_fire(void)
{
    bu_log("=== Test 7: sensor_fire ===\n");
    g_sensor_fired = 0;

    struct bsg_view *v = make_view();

    bsg_node *dr = attach_fake_draw_root(v);
    if (!dr) { g_fail++; free_view(v); return; }

    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) { g_fail++; v->gv_scene = NULL; free_view(v); return; }

    bsg_update_sensor_ref sensor = bsg_update_sensor_ref_create(sensor_callback, NULL);
    BSGCHECK(!bsg_sensor_ref_is_null(bsg_update_sensor_ref_as_sensor(sensor)),
	    "update sensor created");

    (void)bsg_update_sensors_fire();

    BSGCHECK(g_sensor_fired == 1, "sensor callback invoked exactly once");
    if (g_sensor_fired == 1)
	bu_log("  PASS: sensor_fire invoked callback\n");

    /* Fire again — counter should increment */
    (void)bsg_update_sensors_fire();
    BSGCHECK(g_sensor_fired == 2, "sensor callback invoked again on second fire");

    bsg_update_sensor_ref_destroy(sensor);
    (void)bsg_update_sensors_fire();
    BSGCHECK(g_sensor_fired == 2, "destroyed sensor not invoked");

    /* Cleanup: same pattern as test 4 — clear gv_scene, reset draw-root
     * pointer and destroy the synthetic nodes. */
    bsg_view_scene_root_detach_from_root(root);
    v->gv_scene = NULL;
    bsg_group_destroy(dr);
    free_view(v);
}

/* ---- Test 8: null guards ------------------------------------------- */
static void
test_null_guards(void)
{
    bu_log("=== Test 8: null_guards ===\n");
    int fails_before = g_fail;

    /* These must not crash */
    bsg_view_scene_root_sync(NULL, NULL);
    bsg_view_scene_root_detach_from_root(NULL);

    BSGCHECK(bsg_update_sensors_fire() == 0,
	    "update sensor fire with no sensors returns zero");

    if (g_fail == fails_before)
	bu_log("  PASS: all null guards\n");
}

/* -------------------------------------------------------------------- */
int
main(int UNUSED(argc), char *argv[])
{
    bu_setprogname(argv[0]);

    test_create_alias();
    test_create_null();
    test_sync_noop();
    test_scene_child_lookup();
    test_field_authority();
    test_view_init_copy();
    test_sensor_fire();
    test_null_guards();

    if (g_fail) {
	bu_log("RESULT: %d failure(s)\n", g_fail);
	return 1;
    }
    bu_log("RESULT: all scene-graph tests PASSED\n");
    return 0;
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
