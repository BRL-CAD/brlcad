/*           T E S T _ V I E W _ S C O P E . C
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
/** @file libbsg/tests/test_view_scope.c
 *
 * Phase V1 unit tests: BSG_NODE_VIEW_SCOPE lifecycle and visibility predicate.
 *
 * Tests (no display manager, no .g file required):
 *   1. create         — bsg_scene_view_scope_create returns a view-scope scene ref.
 *   2. visible_match  — scope visibility allows the owner view.
 *   3. visible_nomatch — scope visibility rejects a different view.
 *   4. visible_shared — node with NULL owner metadata is visible to any view.
 *   5. null_guards    — NULL inputs must not crash.
 *   6. destroy        — bsg_scene_ref_destroy clears children and frees the scope.
 *   7. effective_visibility — ancestors and view scopes affect visibility.
 *
 * Usage: test_bsg_view_scope
 *   Returns 0 on success, non-zero on failure.
 */

#include "common.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bsg/appearance.h"
#include "bsg/defines.h"
#include "bsg/draw_source.h"
#include "bsg/node.h"
#include "bsg/scene_builder.h"
#include "bsg/util.h"

#include "../node_private.h"
#include "../node_private.h"
#include "../appearance_private.h"
#include "bsg/visibility.h"
#include "../visibility_private.h"
#include "bsg/view_scope.h"

static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
	if (!(cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
	    g_fail++; \
	} \
    } while (0)


/* ---- helpers -------------------------------------------------------- */

static struct bsg_view *
make_view(const char *name)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "%s", name);
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "test view");
}

static bsg_node *
scope_node(bsg_scene_ref ref)
{
    return (bsg_node *)ref.opaque;
}


/* ================================================================== */
/* Tests                                                               */
/* ================================================================== */

/* Test 1: create */
static int
test_create(void)
{
    printf("=== Test 1: create ===\n");
    struct bsg_view *v = make_view("t1");

    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(v, "scope");
    bsg_node *scope = scope_node(scope_ref);
    CHECK(scope != NULL, "bsg_scene_view_scope_create returned non-NULL");

    CHECK(bsg_scene_view(scope_ref) == v, "owner view set to the creating view");
    CHECK(bsg_visibility_scope_allows_view(scope, v) == 1,
	  "created scope is visible to the owner view");
    CHECK(bsg_visibility_local(scope), "scope local visibility is enabled");
    CHECK(bsg_visibility_local(scope),
	  "scope local visibility remains canonical");

    bsg_scene_ref_destroy(scope_ref);
    free_view(v);
    return 0;
}


/* Test 2: visible_match — visible when view matches */
static int
test_visible_match(void)
{
    printf("=== Test 2: visible_match ===\n");
    struct bsg_view *v = make_view("t2");

    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(v, "scope");
    bsg_node *scope = scope_node(scope_ref);
    CHECK(scope != NULL, "create");

    CHECK(bsg_visibility_scope_allows_view(scope, v) == 1,
	  "visible to owner view");

    bsg_scene_ref_destroy(scope_ref);
    free_view(v);
    return 0;
}


/* Test 3: visible_nomatch — not visible when view differs */
static int
test_visible_nomatch(void)
{
    printf("=== Test 3: visible_nomatch ===\n");
    struct bsg_view *v1 = make_view("t3a");
    struct bsg_view *v2 = make_view("t3b");

    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(v1, "scope");
    bsg_node *scope = scope_node(scope_ref);
    CHECK(scope != NULL, "create");

    CHECK(bsg_visibility_scope_allows_view(scope, v2) == 0,
	  "not visible to a different view");
    CHECK(bsg_scene_view(scope_ref) == v1,
	  "canonical owner remains the creating view");
    CHECK(bsg_visibility_scope_allows_view(scope, v1) == 1,
	  "owner view remains visible");
    CHECK(bsg_visibility_scope_allows_view(scope, v2) == 0,
	  "non-owner view remains hidden");

    bsg_scene_ref_destroy(scope_ref);
    free_view(v1);
    free_view(v2);
    return 0;
}


/* Test 4: visible_shared — NULL owner metadata means shared, visible to all views */
static int
test_visible_shared(void)
{
    printf("=== Test 4: visible_shared ===\n");
    struct bsg_view *v1 = make_view("t4a");
    struct bsg_view *v2 = make_view("t4b");

    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(v1, "scope");
    bsg_node *scope = scope_node(scope_ref);
    CHECK(scope != NULL, "create");

    /* Clear the owner view to simulate a shared scope. */
    bsg_scene_set_view(scope_ref, NULL);

    CHECK(bsg_visibility_scope_allows_view(scope, v1) == 1,
	  "shared scope visible to v1");
    CHECK(bsg_visibility_scope_allows_view(scope, v2) == 1,
	  "shared scope visible to v2");
    CHECK(bsg_visibility_scope_allows_view(scope, NULL) == 1,
	  "shared scope visible to NULL view");

    bsg_scene_ref_destroy(scope_ref);
    free_view(v1);
    free_view(v2);
    return 0;
}


/* Test 5: null_guards */
static int
test_null_guards(void)
{
    printf("=== Test 5: null_guards ===\n");
    struct bsg_view *v = make_view("t5");

    /* create with NULL view must return NULL without crash. */
    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(NULL, "scope");
    CHECK(bsg_scene_ref_is_null(scope_ref), "create(NULL) returns NULL");

    /* visible with NULL node must return 0 without crash. */
    CHECK(bsg_visibility_scope_allows_view(NULL, v) == 0,
	  "visible(NULL, v) returns 0");

    /* visible with NULL view on a valid scope. */
    bsg_scene_ref s2_ref = bsg_scene_view_scope_create(v, "scope");
    bsg_node *s2 = scope_node(s2_ref);
    CHECK(s2 != NULL, "create non-NULL");
    CHECK(bsg_visibility_scope_allows_view(s2, NULL) == 0,
	  "visible(scope, NULL) returns 0 for per-view scope");
    bsg_scene_ref_destroy(s2_ref);

    /* destroy with NULL must not crash. */
    bsg_scene_ref null_ref = BSG_SCENE_REF_NULL_INIT;
    bsg_scene_ref_destroy(null_ref);

    free_view(v);
    return 0;
}


/* Test 6: destroy — children ptbl is reset, node is freed */
static int
test_destroy(void)
{
    printf("=== Test 6: destroy ===\n");
    struct bsg_view *v = make_view("t6");

    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(v, "scope");
    bsg_node *scope = scope_node(scope_ref);
    CHECK(scope != NULL, "create");

    /* Add a dummy child reference (just the pointer — we own it). */
    bsg_scene_ref child_ref = bsg_scene_group_create(v, "child");
    bsg_node *child = (bsg_node *)child_ref.opaque;
    CHECK(child != NULL, "child create");
    bsg_scene_append_child(scope_ref, child_ref);

    /* destroy resets children and frees the node. */
    bsg_scene_ref_destroy(scope_ref);

    /* Clean up the child separately. */
    bsg_scene_ref_destroy(child_ref);

    free_view(v);
    return 0;
}


/* Test 7: effective visibility considers ancestors and view scope */
static int
test_effective_visibility(void)
{
    printf("=== Test 7: effective_visibility ===\n");
    struct bsg_view *v1 = make_view("t8a");
    struct bsg_view *v2 = make_view("t8b");

    bsg_node *root = bsg_group_create(v1);
    bsg_scene_ref scope_ref = bsg_scene_view_scope_create(v1, "scope");
    bsg_node *scope = scope_node(scope_ref);
    bsg_node *group = bsg_group_create(v1);
    bsg_node *shape = bsg_shape_create(v1);
    CHECK(root != NULL, "root create");
    CHECK(scope != NULL, "scope create");
    CHECK(group != NULL, "group create");
    CHECK(shape != NULL, "shape create");

    bsg_group_add_child(root, scope);
    bsg_group_add_child(scope, group);
    bsg_group_add_child(group, shape);

    CHECK(bsg_visibility_local(shape) == 1,
	  "shape local visibility defaults on");
    CHECK(bsg_visibility_scope_allows_view(scope, v1) == 1,
	  "scope allows owner view");
    CHECK(bsg_visibility_scope_allows_view(scope, v2) == 0,
	  "scope rejects non-owner view");
    CHECK(bsg_visibility_visible_in_view(shape, v1) == 1,
	  "shape visible in owner view");
    CHECK(bsg_visibility_hidden_in_view(shape, v1) == 0,
	  "hidden inverse is false for visible shape");
    CHECK(bsg_visibility_visible_in_view(shape, v2) == 0,
	  "shape hidden from non-owner view");

    bsg_appearance_set_visible(group, 0);
    CHECK(bsg_visibility_visible_in_view(shape, v1) == 0,
	  "hidden ancestor hides visible child");
    CHECK(bsg_visibility_hidden_in_view(shape, v1) == 1,
	  "hidden inverse is true for hidden child");
    CHECK(bsg_visibility_inherited_visible(shape, v1, 0) == 0,
	  "inherited hidden state keeps child hidden");

    bsg_shape_destroy(shape);
    bsg_group_destroy(group);
    bsg_scene_ref_destroy(scope_ref);
    bsg_group_destroy(root);
    free_view(v1);
    free_view(v2);
    return 0;
}


/* ================================================================== */
/* main                                                                */
/* ================================================================== */

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    (void)argc;

    test_create();
    test_visible_match();
    test_visible_nomatch();
    test_visible_shared();
    test_null_guards();
    test_destroy();
    test_effective_visibility();

    if (g_fail == 0)
	printf("All tests PASSED\n");
    else
	printf("%d test(s) FAILED\n", g_fail);

    return (g_fail != 0) ? 1 : 0;
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
