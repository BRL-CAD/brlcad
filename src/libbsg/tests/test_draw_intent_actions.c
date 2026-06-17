/*        T E S T _ D R A W _ I N T E N T _ A C T I O N S . C
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
/** @file libbsg/tests/test_draw_intent_actions.c
 *
 * Phase D2 draw-intent action API regression tests.
 *
 * Tests: revalidate (modified/removed/renamed), erase_by_path, erase,
 * simplify (subsumption and duplicates), collect_visible,
 * collect_for_export (flags), and match (glob pattern).
 *
 * Usage: test_bsg_draw_intent_actions
 */

#include "common.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bsg/defines.h"
#include "bsg/draw_source.h"
#include "../draw_source_private.h"
#include "bsg/draw_intent.h"
#include "../draw_intent_private.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "bsg/util.h"

/* ---- Minimal test harness ------------------------------------------- */

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
	if ((cond)) { \
	    printf("  PASS: %s\n", (msg)); \
	    g_pass++; \
	} else { \
	    printf("  FAIL: %s (line %d)\n", (msg), __LINE__); \
	    g_fail++; \
	} \
    } while (0)

/* ---- Helpers --------------------------------------------------------- */

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_GET(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "test_intent_view");
    return v;
}

static void
free_view(struct bsg_view *v)
{
    bsg_view_free(v);
    BU_PUT(v, struct bsg_view);
}

static size_t
intent_count(bsg_node *root)
{
    struct bu_ptbl out;
    bu_ptbl_init(&out, 8, "intent count");
    bsg_draw_intent_collect_for_export(root, &out, BSG_EXPORT_INCLUDE_OVERLAYS);
    size_t cnt = BU_PTBL_LEN(&out);
    bu_ptbl_free(&out);
    return cnt;
}

/*
 * Create a group node with a draw-intent for @p path attached, and
 * append it as a child of @p root.  Returns the new group.
 */
static bsg_node *
add_intent_group(bsg_node *root, struct bsg_view *v,
		 const char *path, bsg_draw_mode mode)
{
    bsg_node *g = bsg_group_create(v);
    if (!g) return NULL;
    struct bsg_draw_intent *di = bsg_draw_intent_create(path, mode);
    if (!di) { bsg_group_destroy(g); return NULL; }
    bsg_node_set_draw_intent(g, di);
    bsg_appearance_set_visible(g, 1);
    bsg_group_add_child(root, g);
    return g;
}

/* ---- Test: appearance retention ------------------------------------ */
static void
test_appearance(void)
{
    printf("=== appearance retention ===\n");

    struct bsg_draw_intent *di =
	bsg_draw_intent_create("styled", BSG_DRAW_MODE_WIRE);
    CHECK(di != NULL, "appearance: create intent");

    struct bsg_appearance_settings settings = BSG_APPEARANCE_SETTINGS_INIT;
    settings.draw_mode = BSG_DRAW_MODE_HIDDEN_LINE;
    settings.mixed_modes = 1;
    settings.s_line_width = 9;
    settings.strict_fallback = 1;
    settings.color_override = 1;
    settings.color[0] = 11;
    settings.color[1] = 22;
    settings.color[2] = 33;
    bsg_draw_intent_set_appearance(di, &settings);

    struct bsg_appearance_settings out = BSG_APPEARANCE_SETTINGS_INIT;
    CHECK(bsg_draw_intent_appearance(di, &out) == 1,
	  "appearance: accessor succeeds");
    CHECK(bsg_draw_intent_mode(di) == BSG_DRAW_MODE_HIDDEN_LINE,
	  "appearance: mode synchronized");
    CHECK(di->di_mixed == 1, "appearance: mixed flag synchronized");
    CHECK(out.s_line_width == 9, "appearance: line width retained");
    CHECK(out.strict_fallback == 1, "appearance: strict fallback retained");
    CHECK(out.color_override == 1 && out.color[0] == 11 &&
	    out.color[1] == 22 && out.color[2] == 33,
	  "appearance: color override retained");

    bsg_draw_intent_set_mode(di, BSG_DRAW_MODE_WIRE);
    CHECK(bsg_draw_intent_mode(di) == BSG_DRAW_MODE_WIRE,
	  "appearance: set_mode updates mode");
    CHECK(bsg_draw_intent_appearance(di, &out) == 1 &&
	    out.draw_mode == BSG_DRAW_MODE_WIRE &&
	    out.s_line_width == 9,
	  "appearance: set_mode preserves other style fields");

    CHECK(bsg_draw_intent_appearance(NULL, &out) == 0,
	  "appearance: NULL intent rejected");
    CHECK(bsg_draw_intent_appearance(di, NULL) == 0,
	  "appearance: NULL output rejected");
    bsg_draw_intent_free(di);
}

/* ---- Test: revalidate MODIFIED -------------------------------------- */
static void
test_revalidate_modified(struct bsg_view *v)
{
    printf("=== revalidate: MODIFIED ===\n");

    bsg_node *root = bsg_group_create(v);
    bsg_node *g    = add_intent_group(root, v, "myobj", BSG_DRAW_MODE_WIRE);

    struct bsg_db_event ev;
    ev.dbe_path     = "myobj";
    ev.dbe_old_path = NULL;
    ev.dbe_kind     = BSG_DB_EVENT_MODIFIED;

    int n = bsg_draw_intent_revalidate(root, &ev);
    CHECK(n == 1, "MODIFIED: matched one intent");
    (void)g; /* g still alive — just marked stale */

    CHECK(intent_count(root) == 1, "MODIFIED: group not removed");

    bsg_group_destroy(root);
}

/* ---- Test: revalidate REMOVED --------------------------------------- */
static void
test_revalidate_removed(struct bsg_view *v)
{
    printf("=== revalidate: REMOVED ===\n");

    bsg_node *root = bsg_group_create(v);
    add_intent_group(root, v, "deleted", BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "kept",    BSG_DRAW_MODE_WIRE);

    struct bsg_db_event ev;
    ev.dbe_path     = "deleted";
    ev.dbe_old_path = NULL;
    ev.dbe_kind     = BSG_DB_EVENT_REMOVED;

    int n = bsg_draw_intent_revalidate(root, &ev);
    CHECK(n == 1, "REMOVED: matched and removed one intent");
    CHECK(intent_count(root) == 1, "REMOVED: one group remains");

    bsg_node *remaining = bsg_draw_intent_find(root, "kept");
    const struct bsg_draw_intent *di = bsg_node_get_draw_intent(remaining);
    CHECK(di != NULL, "REMOVED: remaining group has an intent");
    if (di)
	CHECK(BU_STR_EQUAL(bsg_draw_intent_path(di), "kept"),
	      "REMOVED: remaining group is 'kept'");

    bsg_group_destroy(root);
}

/* ---- Test: revalidate RENAMED --------------------------------------- */
static void
test_revalidate_renamed(struct bsg_view *v)
{
    printf("=== revalidate: RENAMED ===\n");

    bsg_node *root = bsg_group_create(v);
    bsg_node *g    = add_intent_group(root, v, "old_name", BSG_DRAW_MODE_WIRE);

    struct bsg_db_event ev;
    ev.dbe_path     = "new_name";
    ev.dbe_old_path = "old_name";
    ev.dbe_kind     = BSG_DB_EVENT_RENAMED;

    int n = bsg_draw_intent_revalidate(root, &ev);
    CHECK(n == 1, "RENAMED: matched one intent");
    CHECK(intent_count(root) == 1, "RENAMED: group not removed");

    const struct bsg_draw_intent *di = bsg_node_get_draw_intent(g);
    CHECK(di != NULL && BU_STR_EQUAL(bsg_draw_intent_path(di), "new_name"),
	  "RENAMED: path updated to new_name");

    bsg_group_destroy(root);
}

/* ---- Test: erase_by_path ------------------------------------------- */
static void
test_erase_by_path(struct bsg_view *v)
{
    printf("=== erase_by_path ===\n");

    bsg_node *root = bsg_group_create(v);
    add_intent_group(root, v, "keep_me",  BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "erase_me", BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "also_keep", BSG_DRAW_MODE_WIRE);

    CHECK(bsg_draw_intent_erase_by_path(root, "erase_me") == 1,
	  "erase_by_path: found and erased");
    CHECK(intent_count(root) == 2,
	  "erase_by_path: two groups remain");
    CHECK(bsg_draw_intent_find(root, "erase_me") == NULL,
	  "erase_by_path: erased group not found");

    /* No-match returns 0. */
    CHECK(bsg_draw_intent_erase_by_path(root, "no_such") == 0,
	  "erase_by_path: no-match returns 0");

    bsg_group_destroy(root);
}

/* ---- Test: erase ---------------------------------------------------- */
static void
test_erase(struct bsg_view *v)
{
    printf("=== erase ===\n");

    bsg_node *root = bsg_group_create(v);
    add_intent_group(root, v, "alpha", BSG_DRAW_MODE_WIRE);
    bsg_node *target = add_intent_group(root, v, "beta", BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "gamma", BSG_DRAW_MODE_WIRE);

    struct bsg_draw_intent *target_di = bsg_node_get_draw_intent(target);
    CHECK(target_di != NULL, "erase: target has intent");

    CHECK(bsg_draw_intent_erase(root, target_di) == 1,
	  "erase: erased by intent pointer");
    CHECK(intent_count(root) == 2,
	  "erase: two groups remain");
    CHECK(bsg_draw_intent_find(root, "beta") == NULL,
	  "erase: beta not found after erase");

    /* NULL args are no-ops. */
    CHECK(bsg_draw_intent_erase(NULL, target_di) == 0,
	  "erase: NULL root returns 0");
    CHECK(bsg_draw_intent_erase(root, NULL) == 0,
	  "erase: NULL intent returns 0");

    bsg_group_destroy(root);
}

/* ---- Test: simplify ------------------------------------------------- */
static void
test_simplify(struct bsg_view *v)
{
    printf("=== simplify ===\n");

    /* Set up: "a" drawn, then "a/b" drawn, then "b" drawn, then "a" again */
    bsg_node *root = bsg_group_create(v);
    add_intent_group(root, v, "a",   BSG_DRAW_MODE_WIRE); /* idx 0 */
    add_intent_group(root, v, "a/b", BSG_DRAW_MODE_WIRE); /* idx 1 — subsumed by "a" */
    add_intent_group(root, v, "b",   BSG_DRAW_MODE_WIRE); /* idx 2 — not subsumed */
    add_intent_group(root, v, "a",   BSG_DRAW_MODE_WIRE); /* idx 3 — duplicate of "a" */

    int removed = bsg_draw_intent_simplify(root);
    CHECK(removed == 2, "simplify: removed 2 redundant groups (a/b and dup a)");
    CHECK(intent_count(root) == 2,
	  "simplify: two groups remain (a, b)");

    bsg_node *found_a = bsg_draw_intent_find(root, "a");
    bsg_node *found_b = bsg_draw_intent_find(root, "b");
    bsg_node *found_ab = bsg_draw_intent_find(root, "a/b");
    CHECK(found_a  != NULL, "simplify: 'a' retained");
    CHECK(found_b  != NULL, "simplify: 'b' retained");
    CHECK(found_ab == NULL, "simplify: 'a/b' removed (subsumed by 'a')");

    bsg_group_destroy(root);

    /* Edge case: sibling paths are NOT subsumed by each other. */
    bsg_node *root2 = bsg_group_create(v);
    add_intent_group(root2, v, "ab",   BSG_DRAW_MODE_WIRE);
    add_intent_group(root2, v, "a/b",  BSG_DRAW_MODE_WIRE);
    int removed2 = bsg_draw_intent_simplify(root2);
    CHECK(removed2 == 0,   "simplify: 'ab' does NOT subsume 'a/b'");
    CHECK(intent_count(root2) == 2, "simplify: both kept when paths just share prefix string");
    bsg_group_destroy(root2);
}

/* ---- Test: collect_visible ----------------------------------------- */
static void
test_collect_visible(struct bsg_view *v)
{
    printf("=== collect_visible ===\n");

    bsg_node *root    = bsg_group_create(v);
    bsg_node *vis     = add_intent_group(root, v, "visible",   BSG_DRAW_MODE_WIRE);
    bsg_node *hidden  = add_intent_group(root, v, "hidden",    BSG_DRAW_MODE_WIRE);
    bsg_node *scoped  = add_intent_group(root, v, "other_view",BSG_DRAW_MODE_WIRE);

    /* Mark one as invisible. */
    bsg_appearance_set_visible(hidden, 0);

    /* Scope one to a different view. */
    struct bsg_view *v2;
    BU_GET(v2, struct bsg_view);
    bsg_view_init(v2, NULL);
    bsg_node_set_view(scoped, v2);

    (void)vis;

    struct bu_ptbl out;
    bu_ptbl_init(&out, 8, "collect_visible out");
    bsg_draw_intent_collect_visible(root, &out, v);

    CHECK(BU_PTBL_LEN(&out) == 1,
	  "collect_visible: only one group visible in v");
    if (BU_PTBL_LEN(&out) == 1) {
	bsg_node *g0 = (bsg_node *)BU_PTBL_GET(&out, 0);
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(g0);
	CHECK(di && BU_STR_EQUAL(bsg_draw_intent_path(di), "visible"),
	      "collect_visible: visible group is 'visible'");
    }

    bu_ptbl_free(&out);
    bsg_view_free(v2);
    BU_PUT(v2, struct bsg_view);
    bsg_group_destroy(root);
}

/* ---- Test: collect_for_export -------------------------------------- */
static void
test_collect_for_export(struct bsg_view *v)
{
    printf("=== collect_for_export ===\n");

    bsg_node *root = bsg_group_create(v);
    add_intent_group(root, v, "wire",   BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "shaded", BSG_DRAW_MODE_SHADED);
    /* overlay group */
    bsg_node *ov = bsg_group_create(v);
    struct bsg_draw_intent *ov_di = bsg_draw_intent_create_overlay("_overlays");
    bsg_node_set_draw_intent(ov, ov_di);
    bsg_group_add_child(root, ov);

    /* Default (no flags): exclude overlays, include all draw modes. */
    struct bu_ptbl out1;
    bu_ptbl_init(&out1, 8, "export out1");
    bsg_draw_intent_collect_for_export(root, &out1, 0);
    CHECK(BU_PTBL_LEN(&out1) == 2,
	  "collect_for_export(0): wire + shaded, no overlay");
    bu_ptbl_free(&out1);

    /* SHADED_ONLY: only shaded group. */
    struct bu_ptbl out2;
    bu_ptbl_init(&out2, 8, "export out2");
    bsg_draw_intent_collect_for_export(root, &out2, BSG_EXPORT_SHADED_ONLY);
    CHECK(BU_PTBL_LEN(&out2) == 1,
	  "collect_for_export(SHADED_ONLY): one group");
    if (BU_PTBL_LEN(&out2) == 1) {
	bsg_node *g0 = (bsg_node *)BU_PTBL_GET(&out2, 0);
	const struct bsg_draw_intent *di = bsg_node_get_draw_intent(g0);
	CHECK(di && BU_STR_EQUAL(bsg_draw_intent_path(di), "shaded"),
	      "collect_for_export(SHADED_ONLY): group is 'shaded'");
    }
    bu_ptbl_free(&out2);

    /* INCLUDE_OVERLAYS: wire + shaded + overlay. */
    struct bu_ptbl out3;
    bu_ptbl_init(&out3, 8, "export out3");
    bsg_draw_intent_collect_for_export(root, &out3, BSG_EXPORT_INCLUDE_OVERLAYS);
    CHECK(BU_PTBL_LEN(&out3) == 3,
	  "collect_for_export(INCLUDE_OVERLAYS): 3 groups");
    bu_ptbl_free(&out3);

    bsg_group_destroy(root);
}

/* ---- Test: match --------------------------------------------------- */
static void
test_match(struct bsg_view *v)
{
    printf("=== match ===\n");

    bsg_node *root = bsg_group_create(v);
    add_intent_group(root, v, "hull/frame",  BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "hull/plating",BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "engine/main", BSG_DRAW_MODE_WIRE);
    add_intent_group(root, v, "engine/aux",  BSG_DRAW_MODE_WIRE);

    /* Wildcard hull slash star should match hull/frame and hull/plating. */
    struct bu_ptbl out1;
    bu_ptbl_init(&out1, 8, "match out1");
    bsg_draw_intent_match(root, "hull/*", &out1);
    CHECK(BU_PTBL_LEN(&out1) == 2,
	  "match('hull/*'): two hull groups");
    bu_ptbl_free(&out1);

    /* Wildcard star slash main should match engine/main. */
    struct bu_ptbl out2;
    bu_ptbl_init(&out2, 8, "match out2");
    bsg_draw_intent_match(root, "*/main", &out2);
    CHECK(BU_PTBL_LEN(&out2) == 1,
	  "match('*/main'): one group");
    bu_ptbl_free(&out2);

    /* Exact match "engine/aux". */
    struct bu_ptbl out3;
    bu_ptbl_init(&out3, 8, "match out3");
    bsg_draw_intent_match(root, "engine/aux", &out3);
    CHECK(BU_PTBL_LEN(&out3) == 1,
	  "match('engine/aux'): exact match");
    bu_ptbl_free(&out3);

    /* "*" matches all four. */
    struct bu_ptbl out4;
    bu_ptbl_init(&out4, 8, "match out4");
    bsg_draw_intent_match(root, "*/*", &out4);
    CHECK(BU_PTBL_LEN(&out4) == 4,
	  "match('*/*'): all four groups");
    bu_ptbl_free(&out4);

    /* No match. */
    struct bu_ptbl out5;
    bu_ptbl_init(&out5, 8, "match out5");
    bsg_draw_intent_match(root, "no_such*", &out5);
    CHECK(BU_PTBL_LEN(&out5) == 0,
	  "match('no_such*'): zero groups");
    bu_ptbl_free(&out5);

    bsg_group_destroy(root);
}

/* ---- Test: null-safety --------------------------------------------- */
static void
test_null_safety(void)
{
    printf("=== null safety ===\n");

    /* All action functions must survive NULL root / NULL args. */
    struct bsg_db_event ev = { "path", NULL, BSG_DB_EVENT_MODIFIED };
    CHECK(bsg_draw_intent_revalidate(NULL, &ev) == 0,
	  "revalidate: NULL root returns 0");
    CHECK(bsg_draw_intent_revalidate((bsg_node *)1, NULL) == 0,
	  "revalidate: NULL event returns 0");
    CHECK(bsg_draw_intent_erase_by_path(NULL, "x") == 0,
	  "erase_by_path: NULL root returns 0");
    CHECK(bsg_draw_intent_erase(NULL, NULL) == 0,
	  "erase: NULL root returns 0");
    CHECK(bsg_draw_intent_simplify(NULL) == 0,
	  "simplify: NULL root returns 0");
    /* collect functions are void — just verify they don't crash. */
    bsg_draw_intent_collect_visible(NULL, NULL, NULL);
    bsg_draw_intent_collect_for_export(NULL, NULL, 0);
    bsg_draw_intent_match(NULL, NULL, NULL);
    CHECK(1, "null safety: no crashes");
}

/* ---- main ---------------------------------------------------------- */
int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    (void)argc;

    printf("--- Phase D2 draw-intent action tests ---\n");

    struct bsg_view *v = make_view();

    test_appearance();
    test_revalidate_modified(v);
    test_revalidate_removed(v);
    test_revalidate_renamed(v);
    test_erase_by_path(v);
    test_erase(v);
    test_simplify(v);
    test_collect_visible(v);
    test_collect_for_export(v);
    test_match(v);
    test_null_safety();

    free_view(v);

    printf("\n--- Results: %d passed, %d failed ---\n", g_pass, g_fail);
    return (g_fail == 0) ? 0 : 1;
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
