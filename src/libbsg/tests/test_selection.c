/*            T E S T _ S E L E C T I O N . C
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
/** @file libbsg/tests/test_selection.c
 *
 * Phase 6 unit tests: first-class selection model.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"
#include "bsg/defines.h"
#include "bsg/feature.h"
#include "bsg/interaction.h"
#include "bsg/util.h"
#include "../node_private.h"
#include "bsg/selection.h"
#include "bsg/view_state.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)


/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "sel_test_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v) return;
    bsg_view_free(v);
    bu_free(v, "sel_test_view");
}

static struct bsg_interaction_record *
record_from_path(struct bsg_view *v, const char *path)
{
    return bsg_interaction_record_create_ref(v, BSG_INTERACTION_SELECTED_PATH,
	    (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT, path, NULL);
}


/* ------------------------------------------------------------------ */
/* Test 1: create / destroy lifecycle                                   */
/* ------------------------------------------------------------------ */

static int
test_create_destroy(void)
{
    printf("=== Test 1: create_destroy ===\n");

    struct bsg_selection *sel = bsg_selection_create();
    if (!sel) FAIL("bsg_selection_create returned NULL");

    if (bsg_selection_count(sel) != 0) FAIL("initial count should be 0");

    bsg_selection_destroy(sel);
    bsg_selection_destroy(NULL);   /* must not crash */

    PASS("create_destroy");
    return 0;
}

static int
test_view_selection_storage(void)
{
    printf("=== Test 1b: view_selection_storage ===\n");

    struct bsg_view *v = make_view();
    if (!bsg_view_selection(v)) FAIL("view selection storage should be initialized");
    if (bsg_selection_count(bsg_view_selection(v)) != 0) FAIL("view selection storage should start empty");

    free_view(v);
    PASS("view_selection_storage");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 2: add / contains / count                                       */
/* ------------------------------------------------------------------ */

static int
test_add_contains_count(void)
{
    printf("=== Test 2: add_contains_count ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    if (!root) FAIL("scene_root_create");

    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);
    if (!s1 || !s2) FAIL("shape_create");

    struct bsg_selection *sel = bsg_selection_create();

    struct bsg_interaction_record *r1 = record_from_path(v, "s1");
    struct bsg_interaction_record *r2 = record_from_path(v, "s2");

    bsg_selection_add_record(sel, r1);
    if (bsg_selection_count(sel) != 1) FAIL("count should be 1 after first add");
    if (!bsg_selection_contains_record(sel, r1)) FAIL("s1 record not found after add");
    if (bsg_selection_contains_record(sel, r2))  FAIL("s2 record should not be in sel");

    /* Duplicate add is a no-op */
    bsg_selection_add_record(sel, r1);
    if (bsg_selection_count(sel) != 1) FAIL("duplicate add should be no-op");

    bsg_selection_add_record(sel, r2);
    if (bsg_selection_count(sel) != 2) FAIL("count should be 2");

    bsg_interaction_record_free(r1);
    bsg_interaction_record_free(r2);
    bsg_selection_destroy(sel);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("add_contains_count");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 3: remove                                                       */
/* ------------------------------------------------------------------ */

static int
test_remove(void)
{
    printf("=== Test 3: remove ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);

    struct bsg_selection *sel = bsg_selection_create();
    struct bsg_interaction_record *r1 = record_from_path(v, "s1");
    struct bsg_interaction_record *r2 = record_from_path(v, "s2");
    bsg_selection_add_record(sel, r1);
    bsg_selection_add_record(sel, r2);

    bsg_selection_remove_record(sel, r1);
    if (bsg_selection_contains_record(sel, r1)) FAIL("s1 should be gone after remove");
    if (bsg_selection_count(sel) != 1)   FAIL("count should be 1 after remove");

    /* remove of non-member is a no-op */
    bsg_selection_remove_record(sel, r1);
    if (bsg_selection_count(sel) != 1) FAIL("remove of absent node should be no-op");

    bsg_interaction_record_free(r1);
    bsg_interaction_record_free(r2);
    bsg_selection_destroy(sel);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("remove");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 4: clear                                                        */
/* ------------------------------------------------------------------ */

static int
test_clear(void)
{
    printf("=== Test 4: clear ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);
    bsg_node *s2 = bsg_shape_create(v);

    struct bsg_selection *sel = bsg_selection_create();
    struct bsg_interaction_record *r1 = record_from_path(v, "s1");
    struct bsg_interaction_record *r2 = record_from_path(v, "s2");
    bsg_selection_add_record(sel, r1);
    bsg_selection_add_record(sel, r2);
    bsg_selection_clear(sel);
    if (bsg_selection_count(sel) != 0) FAIL("count should be 0 after clear");

    bsg_interaction_record_free(r1);
    bsg_interaction_record_free(r2);
    bsg_selection_destroy(sel);
    bsg_shape_destroy(s1);
    bsg_shape_destroy(s2);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("clear");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 5: selection does not mutate node highlight storage              */
/* ------------------------------------------------------------------ */

static int
test_record_highlight_semantics(void)
{
    printf("=== Test 5: record_highlight_semantics ===\n");

    struct bsg_view *v = make_view();
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *s1 = bsg_shape_create(v);

    struct bsg_selection *sel = bsg_selection_create();
    struct bsg_interaction_record *r1 = record_from_path(v, "s1");
    bsg_selection_add_record(sel, r1);

    bsg_appearance_set_highlighted(s1, 0);
    if (bsg_appearance_is_highlighted(s1)) FAIL("selection add should not mutate appearance highlight");
    if (!bsg_selection_contains_record(sel, r1)) FAIL("record match should resolve selected record");

    bsg_interaction_record_free(r1);
    bsg_selection_destroy(sel);
    bsg_shape_destroy(s1);
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    PASS("record_highlight_semantics");
    return 0;
}


/* ------------------------------------------------------------------ */
/* Test 6: NULL guards                                                  */
/* ------------------------------------------------------------------ */

static int
test_null_guards(void)
{
    printf("=== Test 6: null_guards ===\n");

    bsg_selection_add_record(NULL, NULL);
    bsg_selection_remove_record(NULL, NULL);
    bsg_selection_clear(NULL);

    if (bsg_selection_contains_record(NULL, NULL) != 0) FAIL("contains_record(NULL,NULL) should be 0");
    if (bsg_selection_count(NULL) != 0)          FAIL("count(NULL) should be 0");

    PASS("null_guards");
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
    failures += test_view_selection_storage();
    failures += test_add_contains_count();
    failures += test_remove();
    failures += test_clear();
    failures += test_record_highlight_semantics();
    failures += test_null_guards();

    if (failures == 0)
	printf("RESULT: all Phase 6 selection tests PASSED\n");
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
