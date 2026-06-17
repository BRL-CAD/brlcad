/*                T E S T _ I N T E R A C T I O N . C
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
/** @file libbsg/tests/test_interaction.c
 *
 * Unified interaction record regression tests.
 */

#include "common.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "bsg/feature.h"
#include "bsg/interaction.h"
#include "bsg/pick.h"
#include "bsg/selection.h"
#include "bsg/util.h"

#define PASS(msg) do { printf("  PASS: %s\n", (msg)); } while (0)
#define FAIL(msg) do { printf("  FAIL: %s\n", (msg)); return 1; } while (0)

static struct bsg_view *
make_view(void)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "interaction_view");
    (void)bsg_view_scene_root_ensure(v);
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "interaction view");
}

static int
test_selection_records(void)
{
    printf("=== Test 1: selection stores interaction records ===\n");

    struct bsg_view *v = make_view();
    bsg_feature_ref ref = bsg_feature_create_lines(v, "selectable_feature", 1);
    if (bsg_feature_ref_is_null(ref))
	FAIL("feature create");

    struct bsg_interaction_record *record =
	bsg_interaction_record_create(BSG_INTERACTION_SELECTED_PATH);
    if (!record)
	FAIL("interaction record create");
    record->view = v;
    record->feature = ref;
    record->valid = 1;
    bu_vls_sprintf(&record->source_path, "%s", "selectable_feature");

    struct bsg_selection *sel = bsg_selection_create();
    bsg_selection_add_record(sel, record);
    if (bsg_selection_count(sel) != 1)
	FAIL("selection count after add");
    if (!bsg_selection_contains_record(sel, record))
	FAIL("selection does not contain record");
    if (bsg_selection_record(sel, 0) == record)
	FAIL("selection did not clone record");

    struct bu_vls out = BU_VLS_INIT_ZERO;
    bsg_interaction_record_serialize(bsg_selection_record(sel, 0), &out);
    if (!strstr(bu_vls_cstr(&out), "selected-path") ||
	    !strstr(bu_vls_cstr(&out), "selectable_feature"))
	FAIL("serialized selection record missing expected fields");
    bu_vls_free(&out);

    bsg_selection_remove_record(sel, record);
    if (bsg_selection_count(sel) != 0)
	FAIL("selection count after remove");

    bsg_selection_destroy(sel);
    bsg_interaction_record_free(record);
    free_view(v);

    PASS("selection stores interaction records");
    return 0;
}

static int
test_record_equality_and_result_serialization(void)
{
    printf("=== Test 2: record equality and result serialization ===\n");

    struct bsg_interaction_record *a =
	bsg_interaction_record_create(BSG_INTERACTION_SELECTED_PATH);
    struct bsg_interaction_record *b =
	bsg_interaction_record_create(BSG_INTERACTION_SELECTED_PATH);
    bu_vls_sprintf(&a->source_path, "%s", "/group/solid.s");
    bu_vls_sprintf(&b->source_path, "%s", "/group/solid.s");
    a->valid = 1;
    b->valid = 1;
    if (!bsg_interaction_record_equal(a, b))
	FAIL("records with same semantic path should compare equal");

    struct bsg_interaction_result *result = bsg_interaction_result_create();
    bsg_interaction_result_append(result, bsg_interaction_record_clone(a));
    struct bu_vls out = BU_VLS_INIT_ZERO;
    bsg_interaction_result_serialize(result, &out);
    if (!strstr(bu_vls_cstr(&out), "selected-path") ||
	    !strstr(bu_vls_cstr(&out), "/group/solid.s"))
	FAIL("serialized result missing expected fields");

    bu_vls_free(&out);
    bsg_interaction_result_free(result);
    bsg_interaction_record_free(a);
    bsg_interaction_record_free(b);

    PASS("record equality and result serialization");
    return 0;
}

static int
test_edit_preview_record(void)
{
    printf("=== Test 3: edit preview records ===\n");

    struct bsg_view *v = make_view();
    bsg_feature_ref preview = bsg_feature_create_preview(v, "preview_feature", 1);
    struct bsg_interaction_record *begin =
	bsg_interaction_edit_preview_record(v, preview, BSG_EDIT_PREVIEW_BEGIN,
		"/source.s");
    struct bsg_interaction_record *commit =
	bsg_interaction_edit_preview_record(v, preview, BSG_EDIT_PREVIEW_COMMIT,
		"/source.s");
    if (!begin || !commit)
	FAIL("edit preview record create");
    if (begin->kind != BSG_INTERACTION_EDIT_PREVIEW ||
	    begin->preview_op != BSG_EDIT_PREVIEW_BEGIN)
	FAIL("begin preview record fields");
    if (!bsg_interaction_record_equal(begin, commit))
	FAIL("preview records for same feature should compare equal");

    struct bu_vls out = BU_VLS_INIT_ZERO;
    bsg_interaction_record_serialize(commit, &out);
    if (!strstr(bu_vls_cstr(&out), "edit-preview") ||
	    !strstr(bu_vls_cstr(&out), "commit"))
	FAIL("serialized preview record missing expected fields");
    bu_vls_free(&out);

    bsg_interaction_record_free(begin);
    bsg_interaction_record_free(commit);
    free_view(v);

    PASS("edit preview records");
    return 0;
}

static int
test_pick_record_without_node(void)
{
    printf("=== Test 4: pick record converts without node ===\n");

    struct bsg_view *v = make_view();
    struct bsg_pick_record pick;
    memset(&pick, 0, sizeof(pick));
    BU_VLS_INIT(&pick.pr_source_path);
    BU_VLS_INIT(&pick.pr_instance_path);
    pick.pr_scene = bsg_scene_ref_null();
    pick.pr_feature = (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT;
    pick.pr_valid = 1;
    pick.pr_view = v;
    pick.pr_hit_dist = 12.5;
    pick.pr_primitive_id = 3;
    pick.pr_subelement_id = 7;
    bu_vls_sprintf(&pick.pr_source_path, "%s", "/stable/path.s");
    bu_vls_sprintf(&pick.pr_instance_path, "%s", "/stable/path.s");

    struct bsg_interaction_record *record =
	bsg_interaction_from_pick_record(&pick);
    if (!record)
	FAIL("pick interaction create");
    if (!record->valid)
	FAIL("pick interaction should be valid from semantic identity");
    if (!BU_STR_EQUAL(bsg_interaction_record_path(record), "/stable/path.s"))
	FAIL("pick interaction path");
    if (record->primitive_id != 3 || record->subelement_id != 7)
	FAIL("pick interaction ids");

    bsg_interaction_record_free(record);
    bu_vls_free(&pick.pr_source_path);
    bu_vls_free(&pick.pr_instance_path);
    free_view(v);

    PASS("pick record converts without node");
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc > 1)
	fprintf(stderr, "Unexpected arguments\n");

    int ret = 0;
    ret |= test_selection_records();
    ret |= test_record_equality_and_result_serialization();
    ret |= test_edit_preview_record();
    ret |= test_pick_record_without_node();
    return ret;
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
