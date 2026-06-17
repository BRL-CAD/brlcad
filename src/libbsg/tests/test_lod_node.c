/*              T E S T _ L O D _ N O D E . C
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
/** @file libbsg/tests/test_lod_node.c
 *
 * Unit tests for the public typed LoD source API.  The CTest target name is
 * retained for continuity with earlier BSG_NODE_LOD structural coverage, but
 * these tests intentionally avoid the retired node callback API.
 */

#include "common.h"
#include "../draw_source_private.h"
#include "../lod_private.h"
#include "../node_private.h"
#include "../object_private.h"
#include "../scene_object_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/log.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bsg/defines.h"
#include "bsg/lod.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../node_private.h"
#include "bsg/util.h"

static int g_fail = 0;

static int
node_is_a(bsg_node *node, bsg_type_id type)
{
    if (!node)
	return 0;
    return bsg_node_is_a(bsg_node_ref_from_object(bsg_object_ref_from_node(node)),
	    type);
}

#define CHECK(cond, msg) \
    do { \
	if (!(cond)) { \
	    bu_log("FAIL [%s:%d] %s\n", __FILE__, __LINE__, (msg)); \
	    g_fail++; \
	} \
    } while (0)

struct synth_state {
    int select_calls;
    int activate_calls;
    int stale_calls;
    int free_calls;
    int stale_result;
    int select_result;
    int last_level;
};

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
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "test view");
}

static int
synth_select(const struct bsg_lod_source_record *source,
	     struct bsg_view *UNUSED(v),
	     void *policy_data)
{
    struct synth_state *st = (struct synth_state *)policy_data;
    CHECK(source != NULL, "select received source record");
    CHECK(source->kind == BSG_LOD_SOURCE_TEST, "select source kind");
    CHECK(source->identity == 0x5601, "select source identity");
    st->select_calls++;
    return st->select_result;
}

static void
synth_activate(const struct bsg_lod_source_record *source,
	       struct bsg_view *v,
	       int level,
	       struct bsg_lod_view_state *state,
	       void *policy_data)
{
    struct synth_state *st = (struct synth_state *)policy_data;
    CHECK(source != NULL, "activate received source record");
    CHECK(v != NULL, "activate received view");
    CHECK(state != NULL, "activate received view state");
    st->activate_calls++;
    st->last_level = level;
    state->level = level;
    state->policy_tag = level + 100;
}

static int
synth_stale(const struct bsg_lod_source_record *source,
	    struct bsg_view *UNUSED(v),
	    const struct bsg_lod_view_state *state,
	    void *policy_data)
{
    struct synth_state *st = (struct synth_state *)policy_data;
    CHECK(source != NULL, "stale received source record");
    CHECK(state != NULL, "stale received view state");
    st->stale_calls++;
    return st->stale_result;
}

static void
synth_free(void *policy_data)
{
    struct synth_state *st = (struct synth_state *)policy_data;
    st->free_calls++;
}

static bsg_lod_source_ref
make_source(struct bsg_view *v, struct synth_state *st, bsg_node **root_out, bsg_node **leaf_out)
{
    bsg_node *root = bsg_view_scene_root_ensure(v);
    bsg_node *leaf = bsg_shape_create(v);
    CHECK(root != NULL, "root created");
    CHECK(leaf != NULL, "leaf created");
    bsg_node_add_child(root, leaf);

    struct bsg_lod_source_policy policy = BSG_LOD_SOURCE_POLICY_INIT;
    policy.kind = BSG_LOD_SOURCE_TEST;
    policy.identity = 0x5601;
    policy.name = "test-source";
    policy.policy_data = st;
    policy.select = synth_select;
    policy.activate = synth_activate;
    policy.stale = synth_stale;
    policy.free = synth_free;

    bsg_lod_source_ref ref = bsg_lod_source_insert_above(leaf, v, &policy);
    CHECK(!bsg_lod_source_ref_is_null(ref), "source ref created");

    if (root_out)
	*root_out = root;
    if (leaf_out)
	*leaf_out = leaf;
    return ref;
}

static int
test_null_guards(void)
{
    printf("=== Test 1: null guards ===\n");
    struct bsg_lod_source_record record;
    struct bsg_lod_view_state state;
    bsg_lod_source_ref null_ref = BSG_LOD_SOURCE_REF_NULL_INIT;

    CHECK(bsg_lod_source_ref_is_null(null_ref), "null ref recognized");
    CHECK(!bsg_lod_source_record_get(null_ref, &record), "null record rejected");
    CHECK(!bsg_lod_source_view_state_ensure(null_ref, NULL, &state), "null state rejected");
    CHECK(!bsg_lod_source_view_state_invalidate(null_ref, NULL), "null invalidate rejected");
    CHECK(!bsg_lod_source_view_policy_tag_sync(null_ref, NULL, 1), "null policy tag rejected");
    CHECK(!bsg_lod_source_stale(null_ref, NULL), "null stale rejected");
    bsg_lod_sources_update(NULL, NULL);
    return 0;
}

static int
test_public_lod_fields(void)
{
    printf("=== Test 2: public LoD fields ===\n");
    struct bsg_view *v = make_view("public-fields");
    bsg_lod_ref lod = bsg_lod_ref_create(v, "field-lod");
    bsg_shape_ref fine = bsg_shape_ref_create(v, "fine");
    bsg_shape_ref coarse = bsg_shape_ref_create(v, "coarse");

    CHECK(!bsg_node_ref_is_null(bsg_lod_ref_as_node(lod)), "LoD ref created");
    CHECK(!bsg_field_ref_is_null(bsg_lod_ref_selection_field(lod)), "selection field");
    CHECK(!bsg_field_ref_is_null(bsg_lod_ref_active_level_field(lod)), "active-level field");
    CHECK(!bsg_field_ref_is_null(bsg_lod_ref_ranges_field(lod)), "ranges field");
    CHECK(bsg_lod_ref_selection(lod) == BSG_LOD_SELECT_AUTO, "default selection");
    CHECK(bsg_lod_ref_active_level(lod) == -1, "default active level");

    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(fine));
    bsg_lod_ref_append_level(lod, bsg_shape_ref_as_node(coarse));
    CHECK(bsg_lod_ref_level_count(lod) == 2, "level count");
    CHECK(bsg_node_ref_equal(bsg_lod_ref_level_at(lod, 1), bsg_shape_ref_as_node(coarse)),
	    "level lookup");

    CHECK(bsg_lod_ref_set_selection(lod, BSG_LOD_SELECT_FORCE_LEVEL), "set selection");
    CHECK(bsg_lod_ref_selection(lod) == BSG_LOD_SELECT_FORCE_LEVEL, "read selection");
    CHECK(bsg_lod_ref_set_active_level(lod, 1), "set active level");
    CHECK(bsg_lod_ref_active_level(lod) == 1, "read active level");
    CHECK(bsg_lod_ref_append_range(lod, 42.0), "append range");
    CHECK(bsg_lod_ref_range_count(lod) == 1, "range count");
    double range = 0.0;
    CHECK(bsg_lod_ref_range_at(lod, 0, &range) && NEAR_ZERO(range - 42.0, SMALL_FASTF),
	    "range lookup");
    CHECK(bsg_lod_ref_clear_ranges(lod) && bsg_lod_ref_range_count(lod) == 0,
	    "clear ranges");

    bsg_lod_ref_remove_level(lod, bsg_shape_ref_as_node(fine));
    CHECK(bsg_lod_ref_level_count(lod) == 1, "remove level");

    bsg_node_ref_destroy(bsg_shape_ref_as_node(fine));
    bsg_node_ref_destroy(bsg_shape_ref_as_node(coarse));
    bsg_node_ref_destroy(bsg_lod_ref_as_node(lod));
    free_view(v);
    return 0;
}

static int
test_insert_and_record(void)
{
    printf("=== Test 3: insert and record ===\n");
    struct bsg_view *v = make_view("insert");
    struct synth_state st;
    bsg_node *root = NULL;
    bsg_node *leaf = NULL;
    memset(&st, 0, sizeof(st));
    st.stale_result = 1;

    bsg_lod_source_ref ref = make_source(v, &st, &root, &leaf);
    bsg_node *lod = bsg_node_parent(leaf);

    CHECK(bsg_node_child_count(root) == 1, "root child count preserved");
    CHECK(bsg_node_child_at(root, 0) == lod, "source wrapper replaced leaf slot");
    CHECK(node_is_a(lod, bsg_lod_type()), "wrapper is LoD node");
    CHECK(bsg_node_child_count(lod) == 1, "source has one representation child");
    CHECK(bsg_node_child_at(lod, 0) == leaf, "leaf is source representation child");

    struct bsg_lod_source_record record;
    memset(&record, 0, sizeof(record));
    CHECK(bsg_lod_source_record_get(ref, &record), "source record available");
    CHECK(record.kind == BSG_LOD_SOURCE_TEST, "record kind");
    CHECK(record.identity == 0x5601, "record identity");
    CHECK(record.name && BU_STR_EQUAL(record.name, "test-source"), "record name");
    CHECK(record.level_count == 1, "record level count");

    bsg_node_remove_child(root, lod);
    bsg_node_destroy(lod);
    CHECK(st.free_calls == 1, "policy free callback fired");
    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    return 0;
}

static int
test_view_state(void)
{
    printf("=== Test 4: view state ===\n");
    struct bsg_view *va = make_view("va");
    struct bsg_view *vb = make_view("vb");
    struct synth_state st;
    bsg_node *root = NULL;
    memset(&st, 0, sizeof(st));

    bsg_lod_source_ref ref = make_source(va, &st, &root, NULL);
    struct bsg_lod_view_state sa;
    struct bsg_lod_view_state sb;
    memset(&sa, 0, sizeof(sa));
    memset(&sb, 0, sizeof(sb));

    CHECK(bsg_lod_source_view_state_ensure(ref, va, &sa), "view A state");
    CHECK(sa.view == va, "view A stored");
    CHECK(sa.level == -1, "view A starts unresolved");
    CHECK(bsg_lod_source_view_state_ensure(ref, vb, &sb), "view B state");
    CHECK(sb.view == vb, "view B stored");
    CHECK(sb.level == -1, "view B starts unresolved");

    CHECK(bsg_lod_source_view_policy_tag_sync(ref, va, 7), "policy tag change invalidates");
    CHECK(!bsg_lod_source_view_policy_tag_sync(ref, va, 7), "same policy tag is unchanged");
    CHECK(bsg_lod_source_view_state_ensure(ref, va, &sa), "view A state after tag");
    CHECK(sa.policy_tag == 7, "policy tag recorded");
    CHECK(sa.level == -1, "policy tag invalidated level");

    bsg_view_scene_root_detach_from_root(root);
    free_view(va);
    free_view(vb);
    return 0;
}

static int
test_update_cycle(void)
{
    printf("=== Test 5: update cycle ===\n");
    struct bsg_view *v = make_view("update");
    struct synth_state st;
    bsg_node *root = NULL;
    bsg_node *leaf = NULL;
    memset(&st, 0, sizeof(st));
    st.stale_result = 1;
    st.select_result = 0;

    bsg_lod_source_ref ref = make_source(v, &st, &root, &leaf);
    bsg_node *lod = bsg_node_parent(leaf);
    CHECK(lod != NULL, "lod parent available");
    CHECK(bsg_lod_source_stale(ref, v), "stale check uses runtime LoD type");
    point_t bmin, bmax, got_min, got_max;
    VSET(bmin, 1.0, 2.0, 3.0);
    VSET(bmax, 4.0, 5.0, 6.0);
    bsg_node_set_bounds(leaf, bmin, bmax, 1);
    CHECK(bsg_scene_node_update_bounds(lod, v),
	    "LoD bounds update uses runtime LoD type");
    CHECK(bsg_node_bounds(lod, got_min, got_max) &&
	    VNEAR_EQUAL(got_min, bmin, SMALL_FASTF) &&
	    VNEAR_EQUAL(got_max, bmax, SMALL_FASTF),
	    "LoD bounds copied from selected child with runtime LoD type");

    bsg_lod_sources_update(root, v);
    CHECK(st.select_calls == 1, "select called once");
    CHECK(st.activate_calls == 1, "activate called once");
    CHECK(st.last_level == 0, "activated level 0");

    struct bsg_lod_view_state state;
    memset(&state, 0, sizeof(state));
    CHECK(bsg_lod_source_view_state_ensure(ref, v, &state), "state after update");
    CHECK(state.level == 0, "state level after update");
    CHECK(state.policy_tag == 100, "state policy tag after update");

    st.select_result = 4;
    bsg_lod_source_view_state_invalidate(ref, v);
    bsg_lod_sources_update(bsg_node_parent(leaf), v);
    CHECK(st.select_calls == 2, "select called after invalidation");
    CHECK(st.activate_calls == 2, "activate called after invalidation");
    CHECK(st.last_level == 4, "logical level can exceed representation count");

    st.stale_result = 0;
    bsg_lod_sources_update(root, v);
    CHECK(st.select_calls == 2, "select skipped when source fresh");
    CHECK(st.activate_calls == 2, "activate skipped when source fresh");

    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    return 0;
}

static int
test_mesh_role_cached_bounds(void)
{
    printf("=== Test 6: mesh role cached bounds ===\n");
    struct bsg_view *v = make_view("mesh-bounds");
    bsg_node *shape = bsg_shape_create(v);
    point_t bmin, bmax, got_min, got_max;

    CHECK(shape != NULL, "shape created");
    VSET(bmin, -3.0, -2.0, -1.0);
    VSET(bmax,  4.0,  5.0,  6.0);
    bsg_node_set_bounds(shape, bmin, bmax, 1);
    bsg_node_source_realization_set_roles(shape, 0, 1);

    CHECK(bsg_shape_is_mesh(shape), "shape classified as mesh realization");
    CHECK(bsg_scene_node_update_bounds(shape, v),
	    "mesh realization without mesh payload uses cached bounds");
    CHECK(bsg_node_bounds(shape, got_min, got_max) &&
	    VNEAR_EQUAL(got_min, bmin, SMALL_FASTF) &&
	    VNEAR_EQUAL(got_max, bmax, SMALL_FASTF),
	    "cached mesh-role bounds preserved");

    bsg_node_destroy(shape);
    free_view(v);
    return 0;
}

static int
test_bounds_use_active_lod_child(void)
{
    printf("=== Test 7: active LoD child bounds ===\n");
    struct bsg_view *v = make_view("active-lod-bounds");
    struct synth_state st;
    bsg_node *root = NULL;
    bsg_node *fine = NULL;
    bsg_node *coarse = NULL;
    memset(&st, 0, sizeof(st));
    st.stale_result = 1;
    st.select_result = 1;

    bsg_lod_source_ref ref = make_source(v, &st, &root, &fine);
    bsg_node *lod = bsg_node_parent(fine);
    coarse = bsg_shape_create(v);
    CHECK(lod != NULL && coarse != NULL, "LoD bounds setup");
    if (lod && coarse) {
	bsg_lod_ref lod_ref = BSG_LOD_REF_NULL_INIT;
	lod_ref.node = bsg_node_ref_from_object(bsg_object_ref_from_node(lod));
	bsg_lod_ref_append_level(lod_ref,
		bsg_node_ref_from_object(bsg_object_ref_from_node(coarse)));
    }

    point_t fine_min, fine_max, coarse_min, coarse_max, got_min, got_max;
    VSET(fine_min, -1.0, -1.0, -1.0);
    VSET(fine_max,  1.0,  1.0,  1.0);
    VSET(coarse_min, 20.0, 30.0, 40.0);
    VSET(coarse_max, 22.0, 35.0, 48.0);
    bsg_node_set_bounds(fine, fine_min, fine_max, 1);
    bsg_node_set_bounds(coarse, coarse_min, coarse_max, 1);

    bsg_lod_sources_update(root, v);
    CHECK(st.select_calls == 1 && st.activate_calls == 1,
	    "LoD update selected active representation");

    CHECK(bsg_scene_node_update_bounds(lod, v),
	    "LoD bounds update uses active child");
    CHECK(bsg_node_bounds(lod, got_min, got_max) &&
	    VNEAR_EQUAL(got_min, coarse_min, SMALL_FASTF) &&
	    VNEAR_EQUAL(got_max, coarse_max, SMALL_FASTF),
	    "LoD bounds copied from active child");
    CHECK(!VNEAR_EQUAL(got_min, fine_min, SMALL_FASTF),
	    "LoD bounds did not use inactive first child");

    struct bsg_lod_view_state state;
    memset(&state, 0, sizeof(state));
    CHECK(bsg_lod_source_view_state_ensure(ref, v, &state) &&
	    state.level == 1,
	    "LoD cursor records active level");

    bsg_view_scene_root_detach_from_root(root);
    free_view(v);
    return 0;
}

int
main(int argc, char *argv[])
{
    bu_setprogname(argv[0]);
    (void)argc;

    test_null_guards();
    test_public_lod_fields();
    test_insert_and_record();
    test_view_state();
    test_update_cycle();
    test_mesh_role_cached_bounds();
    test_bounds_use_active_lod_child();

    if (g_fail == 0)
	printf("\nAll typed LoD source tests PASSED.\n");
    else
	printf("\n%d typed LoD source assertion(s) FAILED.\n", g_fail);

    return (g_fail == 0) ? 0 : 1;
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
