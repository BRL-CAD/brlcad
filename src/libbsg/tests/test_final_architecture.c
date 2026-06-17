/*          T E S T _ F I N A L _ A R C H I T E C T U R E . C
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
/** @file libbsg/tests/test_final_architecture.c
 *
 * End-to-end retained drawing architecture acceptance test.
 */

#include "common.h"
#include "../lod_private.h"
#include "../node_private.h"

#include <stdio.h>
#include <string.h>

#include "bu/app.h"
#include "bu/malloc.h"
#include "bu/ptbl.h"
#include "bu/str.h"
#include "bu/vls.h"
#include "vmath.h"
#include "bsg/action.h"
#include "bsg/appearance.h"
#include "../appearance_private.h"
#include "bsg/backend_scene.h"
#include "bsg/draw_intent.h"
#include "../draw_intent_private.h"
#include "bsg/draw_source.h"
#include "../draw_source_private.h"
#include "bsg/export.h"
#include "bsg/feature.h"
#include "bsg/geometry.h"
#include "bsg/hud.h"
#include "../hud_private.h"
#include "bsg/interaction.h"
#include "bsg/lod.h"
#include "bsg/measure.h"
#include "bsg/node.h"
#include "../node_private.h"
#include "../node_storage_private.h"
#include "../object_private.h"
#include "bsg/pick.h"
#include "bsg/render.h"
#include "bsg/render_item.h"
#include "bsg/selection.h"
#include "bsg/snap_action.h"
#include "bsg/util.h"
#include "bsg/view_state.h"

#define CHECK(_cond, _msg) do { if (!(_cond)) { printf("FAIL: %s\n", (_msg)); return 1; } } while (0)

struct lod_state {
    int level;
};

static struct bsg_view *
make_view(const char *name)
{
    struct bsg_view *v;
    BU_ALLOC(v, struct bsg_view);
    bsg_view_init(v, NULL);
    bu_vls_sprintf(&v->gv_name, "%s", name);
    v->gv_width = 640;
    v->gv_height = 480;
    (void)bsg_view_scene_root_ensure(v);
    return v;
}

static void
free_view(struct bsg_view *v)
{
    if (!v)
	return;
    bsg_view_free(v);
    bu_free(v, "final architecture view");
}

static bsg_node *
make_database_line(struct bsg_view *v, const char *name)
{
    bsg_node *root = (bsg_node *)v->gv_scene;
    bsg_line_set_ref lines = bsg_line_set_ref_create(v, name);
    bsg_node *shape = bsg_object_ref_node(
	    bsg_node_ref_object(bsg_line_set_ref_as_node(lines)));
    point_t pts[2] = {VINIT_ZERO, VINIT_ZERO};
    int cmds[2] = {BSG_GEOMETRY_LINE_MOVE, BSG_GEOMETRY_LINE_DRAW};
    point_t bmin = VINIT_ZERO;
    point_t bmax = VINIT_ZERO;

    if (!shape) {
	printf("FAIL: database line allocated\n");
	return NULL;
    }
    VSET(pts[1], 1.0, 0.0, 0.0);
    if (!bsg_line_set_ref_set_points(lines, (const point_t *)pts, cmds, 2)) {
	printf("FAIL: database line geometry\n");
	return NULL;
    }

    bsg_node_add_child(root, shape);
    bsg_node_set_name(shape, name);
    bsg_node_set_draw_intent(shape, bsg_draw_intent_create(name, BSG_DRAW_MODE_SHADED_BOTS));
    bsg_shape_mark_db_object(shape);
    bsg_node_set_visible_state(shape, 1);
    bsg_appearance_set_transparency(shape, 0.5);
    bsg_appearance_set_dmode(shape, 1);

    VSET(bmin, -0.1, -0.1, -0.1);
    VSET(bmax, 1.1, 0.1, 0.1);
    bsg_node_set_bounds(shape, bmin, bmax, 1);
    bsg_node_set_draw_center(shape, pts[0]);
    bsg_node_set_draw_size(shape, 1.0);
    return shape;
}

static int
lod_select(const struct bsg_lod_source_record *UNUSED(source),
	   struct bsg_view *UNUSED(view),
	   void *data)
{
    struct lod_state *st = (struct lod_state *)data;
    return st ? st->level : 0;
}

static void
lod_activate(const struct bsg_lod_source_record *UNUSED(source),
	     struct bsg_view *UNUSED(view),
	     int level,
	     struct bsg_lod_view_state *state,
	     void *UNUSED(data))
{
    if (!state)
	return;
    state->level = level;
    state->policy_tag = 66;
}

static int
lod_stale(const struct bsg_lod_source_record *UNUSED(source),
	  struct bsg_view *UNUSED(view),
	  const struct bsg_lod_view_state *state,
	  void *UNUSED(data))
{
    return (!state || state->policy_tag != 66);
}

static int
collect_items(struct bsg_view *v, struct bu_ptbl *items)
{
    struct bsg_render_request *req;
    struct bsg_render_batch *batch;
    int ret;

    bu_ptbl_init(items, 8, "final architecture render items");
    req = bsg_render_request_create(v, NULL);
    batch = bsg_render_batch_create();
    if (!req || !batch) {
	bsg_render_request_destroy(req);
	bsg_render_batch_destroy(batch);
	return -1;
    }
    bsg_render_request_set_flags(req,
	    BSG_RENDER_FLAG_VISIBLE_ONLY |
	    BSG_RENDER_FLAG_PAYLOAD_PREPARE |
	    BSG_RENDER_FLAG_SORTED_ALPHA);
    ret = bsg_render_request_collect(req, batch);
    if (ret >= 0)
	(void)bsg_render_batch_move_items(batch, items);
    bsg_render_batch_destroy(batch);
    bsg_render_request_destroy(req);
    return ret;
}

static void
free_items(struct bu_ptbl *items)
{
    if (!items)
	return;
    for (size_t i = 0; i < BU_PTBL_LEN(items); i++)
	bsg_render_item_free((struct bsg_render_item *)BU_PTBL_GET(items, i));
    bu_ptbl_free(items);
}

static const struct bsg_render_item *
find_item_by_path(const struct bu_ptbl *items, const char *path)
{
    if (!items || !path)
	return NULL;
    for (size_t i = 0; i < BU_PTBL_LEN(items); i++) {
	const struct bsg_render_item *item =
	    (const struct bsg_render_item *)BU_PTBL_GET(items, i);
	if (item && item->source.name && BU_STR_EQUAL(item->source.name, path))
	    return item;
    }
    return NULL;
}

static const struct bsg_export_record *
find_export_by_path(const struct bsg_export_result *result, const char *path)
{
    if (!result || !path)
	return NULL;
    for (size_t i = 0; i < bsg_export_result_count(result); i++) {
	const struct bsg_export_record *rec = bsg_export_result_get(result, i);
	if (rec && BU_STR_EQUAL(bu_vls_cstr(&rec->path), path))
	    return rec;
    }
    return NULL;
}

static int
test_final_architecture_records(void)
{
    struct bsg_view *view_a = make_view("final_architecture_A");
    struct bsg_view *view_b = make_view("final_architecture_B");
    struct lod_state lod = {2};
    struct bsg_lod_source_policy policy = BSG_LOD_SOURCE_POLICY_INIT;
    struct bsg_lod_view_state lod_state = BSG_LOD_VIEW_STATE_INIT;
    struct bu_ptbl items = BU_PTBL_INIT_ZERO;
    struct bsg_backend_scene *scene = NULL;
    struct bsg_export_result *exports = NULL;
    struct bsg_pick_result *pick = NULL;
    struct bsg_snap_result snap = {NULL, 0};
    struct bsg_measure_result measure = {0.0, 0.0, 0.0, 0};
    const struct bsg_render_item *item = NULL;
    const struct bsg_export_record *rec = NULL;
    const struct bsg_backend_scene_node *node = NULL;
    bsg_node *shape = NULL;
    bsg_lod_source_ref lod_ref = BSG_LOD_SOURCE_REF_NULL_INIT;
    bsg_feature_ref overlay = BSG_FEATURE_REF_NULL_INIT;
    bsg_feature_ref label = BSG_FEATURE_REF_NULL_INIT;
    bsg_feature_ref preview = BSG_FEATURE_REF_NULL_INIT;
    struct bsg_interaction_record *sel = NULL;
    struct bsg_interaction_record *preview_ir = NULL;
    struct bsg_feature_record feature_record;
    struct bsg_hud_payload const *hud_payload = NULL;
    unsigned long long hash_before_metadata_check = 0;
    unsigned long long hash_after_metadata_check = 0;
    point_t sample = VINIT_ZERO;
    point_t a = VINIT_ZERO;
    point_t b = VINIT_ZERO;

    CHECK(view_a && view_b, "views created");
    CHECK(view_a != view_b, "multiple views are distinct records");
    CHECK(!bsg_view_settings_shared(view_a, view_b),
	  "multiple views keep independent view-state records");

    shape = make_database_line(view_a, "scene/primary.s");
    hash_before_metadata_check = bsg_hash(view_a);
    CHECK(hash_before_metadata_check != 0, "semantic hash populated before metadata check");
    CHECK(bsg_node_has_geometry_role(shape, BSG_GEOMETRY_DB_OBJECT),
	  "database geometry role is metadata-backed");
    hash_after_metadata_check = bsg_hash(view_a);
    CHECK(hash_before_metadata_check == hash_after_metadata_check,
	  "semantic hash remains stable across metadata readback");

    policy.kind = BSG_LOD_SOURCE_TEST;
    policy.identity = 0x6601;
    policy.name = "final-architecture-lod";
    policy.policy_data = &lod;
    policy.select = lod_select;
    policy.activate = lod_activate;
    policy.stale = lod_stale;
    lod_ref = bsg_lod_source_insert_above(shape, view_a, &policy);
    CHECK(!bsg_lod_source_ref_is_null(lod_ref), "LoD source ref created");
    bsg_lod_sources_update((bsg_node *)view_a->gv_scene, view_a);
    CHECK(bsg_lod_source_view_state_ensure(lod_ref, view_a, &lod_state),
	  "LoD source produces per-view state");
    CHECK(lod_state.level == 2, "LoD level selected by policy");

    overlay = bsg_feature_create_lines(view_a, "final_overlay", 0);
    label = bsg_feature_create_label(view_a, "final_label", 0);
    preview = bsg_feature_create_preview(view_a, "final_preview", 1);
    CHECK(!bsg_feature_ref_is_null(overlay), "overlay feature created");
    CHECK(!bsg_feature_ref_is_null(label), "label feature created");
    CHECK(!bsg_feature_ref_is_null(preview), "preview feature created");
    CHECK(bsg_feature_record_get(preview, &feature_record) &&
	    feature_record.family == BSG_FEATURE_TRANSIENT_PREVIEW &&
	    feature_record.scope == BSG_FEATURE_SCOPE_LOCAL,
	  "edit preview is a local feature record");

    bsg_view_set_framebuffer_mode(view_a, 7);
    CHECK(bsg_hud_sync(view_a) == 0, "HUD sync succeeds");
    hud_payload = bsg_hud_node_get_payload(
	    bsg_node_child_at(bsg_hud_root_get(view_a), BSG_HUD_FEATURE_FRAMEBUFFER));
    CHECK(hud_payload && hud_payload->feature_type == BSG_HUD_FEATURE_FRAMEBUFFER,
	  "framebuffer is represented by HUD payload record");
    CHECK(hud_payload->data.framebuffer.mode == 7,
	  "framebuffer mode carried by HUD record");

    sel = bsg_interaction_record_create_ref(view_a,
	    BSG_INTERACTION_SELECTED_PATH,
	    (bsg_feature_ref)BSG_FEATURE_REF_NULL_INIT,
	    "scene/primary.s", NULL);
    CHECK(sel != NULL, "selection interaction record created");
    bsg_selection_add_record(bsg_view_selection(view_a), sel);
    CHECK(bsg_selection_count(bsg_view_selection(view_a)) == 1,
	  "selection stores interaction record");

    preview_ir = bsg_interaction_edit_preview_record(view_a, preview,
	    BSG_EDIT_PREVIEW_UPDATE, "scene/primary.s");
    CHECK(preview_ir && preview_ir->kind == BSG_INTERACTION_EDIT_PREVIEW,
	  "edit preview interaction record created");

    CHECK(collect_items(view_a, &items) > 0, "render batch collected");
    item = find_item_by_path(&items, "scene/primary.s");
    CHECK(item != NULL, "database item present in render batch");
    CHECK(item->source.scope == BSG_RENDER_SOURCE_SCOPE_DATABASE,
	  "render item keeps database source scope");
    CHECK(item->source.geometry_role == BSG_RENDER_GEOMETRY_ROLE_DATABASE_OBJECT,
	  "render item keeps database geometry role");
    CHECK(item->source.lod_id == 0x6601, "render item carries LoD identity");
    CHECK(item->lod_level == 2, "render item carries selected LoD level");
    CHECK(item->selected && item->highlighted,
	  "render item derives selection/highlight from interaction record");
    CHECK(item->appearance.transparency < 1.0,
	  "render item carries transparent material");
    CHECK(item->phase == BSG_RENDER_PHASE_TRANSPARENT,
	  "transparent material routes to transparent phase");

    exports = bsg_export_scene(view_a,
	    BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE);
    CHECK(exports != NULL, "export records created");
    rec = find_export_by_path(exports, "scene/primary.s");
    CHECK(rec != NULL, "export record present");
    CHECK(rec->cache_identity == item->cache_identity,
	  "export and render agree on cache identity");
    CHECK(rec->source.source_id == item->source.source_id,
	  "export and render agree on source identity");
    CHECK(rec->payload_revision == item->payload_revision,
	  "export and render agree on payload revision");
    CHECK(rec->geometry_revision == item->geometry.revision,
	  "export and render agree on geometry revision");
    CHECK(rec->selected == item->selected && rec->highlighted == item->highlighted,
	  "export and render agree on interaction state");
    CHECK(rec->lod_identity == item->source.lod_id,
	  "export carries LoD identity");

    bsg_action_ref backend_action = bsg_backend_scene_action_create(NULL,
	    view_a, BSG_RENDER_FLAG_VISIBLE_ONLY | BSG_RENDER_FLAG_PAYLOAD_PREPARE,
	    NULL, NULL);
    CHECK(!bsg_action_ref_is_null(backend_action), "backend scene action created");
    CHECK(bsg_action_apply(backend_action, bsg_node_ref_null()) > 0,
	  "backend scene action consumed render stream");
    struct bsg_backend_scene_update_result backend_action_result;
    scene = bsg_backend_scene_action_result(backend_action, &backend_action_result);
    CHECK(scene != NULL && backend_action_result.scene == scene,
	  "backend scene action produced retained scene result");
    CHECK(backend_action_result.rendered > 0,
	  "backend scene action result reports rendered items");
    bsg_action_ref_destroy(backend_action);
    node = bsg_backend_scene_find(scene, rec->cache_identity);
    CHECK(node != NULL, "backend scene retained matching record");
    CHECK(node->source_identity == rec->source.source_id,
	  "backend scene agrees on source identity");
    CHECK(node->source.lod_id == rec->lod_identity,
	  "backend scene agrees on LoD identity");
    CHECK(node->selection.selected == rec->selected &&
	    node->selection.highlighted == rec->highlighted,
	  "backend scene agrees on selection/highlight");
    CHECK(NEAR_EQUAL(node->material.transparency, rec->transparency, BN_TOL_DIST),
	  "backend scene agrees on transparency");

    pick = bsg_pick_semantic_path(view_a, "scene/primary.s");
    CHECK(pick && bsg_pick_result_count(pick) == 1,
	  "semantic pick returns selected database source");
    CHECK(BU_STR_EQUAL(bu_vls_cstr(&bsg_pick_result_get(pick, 0)->pr_source_path),
		"scene/primary.s"),
	  "pick record carries source path");

    CHECK(bsg_snap_candidates(view_a, sample, 1.0, BSG_SNAP_KIND_ENDPOINT, &snap) >= 0,
	  "snap candidate query succeeds");
    CHECK(snap.sr_cnt > 0, "snap query returns endpoint candidates");
    CHECK(bu_vls_strlen(&snap.sr_candidates[0].sc_source_path) > 0,
	  "snap candidate carries source identity");

    VSET(b, 1.0, 0.0, 0.0);
    CHECK(bsg_measure_candidates(view_a, a, b, &measure) == 1,
	  "measure candidate query succeeds");
    CHECK(measure.mr_valid && NEAR_EQUAL(measure.mr_distance, 1.0, BN_TOL_DIST),
	  "measure record carries expected distance");

    bsg_interaction_record_free(preview_ir);
    bsg_interaction_record_free(sel);
    bsg_pick_result_free(pick);
    bsg_snap_result_free(&snap);
    bsg_backend_scene_destroy(scene);
    bsg_export_result_free(exports);
    free_items(&items);
    free_view(view_a);
    free_view(view_b);
    return 0;
}

int
main(int argc, char **argv)
{
    bu_setprogname(argv[0]);
    if (argc != 1) {
	printf("Usage: %s\n", argv[0]);
	return 1;
    }
    if (test_final_architecture_records())
	return 1;
    printf("PASS: final retained drawing architecture records agree\n");
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
